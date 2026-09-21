#pragma once

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "sd_patterns.h"
#include "remote_page.h"
#include "../../shared/remote_control.h"

// Called by the web server when a pattern is renamed so the on-device
// display cache can be refreshed immediately.
extern void OnPresetRenamed(int slot);

class RemoteWeb {
  public:
    void Begin()
    {
        char token[33];
        snprintf(token, sizeof(token), "%08lx%08lx%08lx%08lx",
                 static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()),
                 static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
        token_ = token;
        nextId_ = esp_random() & 0x7fffffff;
        WiFi.persistent(false);
        WiFi.mode(WIFI_AP);
        WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
        if (!WiFi.softAP("DCO-ONE", nullptr, 6, false, 2))
        {
            Serial.println("[WIFI] Access point failed");
            return;
        }
        WiFi.setTxPower(WIFI_POWER_13dBm);
        const char* headers[] = {"X-DCO-Token"};
        server_.collectHeaders(headers, 1);
        server_.on("/", HTTP_GET, [this]() {
            server_.sendHeader("Cache-Control", "no-store");
            server_.sendHeader("X-Frame-Options", "DENY");
            server_.sendHeader("X-Content-Type-Options", "nosniff");
            server_.send_P(200, "text/html; charset=utf-8", kRemotePage);
        });
        server_.on("/api/state", HTTP_GET, [this]() { State(); });
        server_.on("/api/patterns", HTTP_GET, [this]() { Patterns(); });
        server_.on("/api/command", HTTP_POST, [this]() { Command(); });
        server_.onNotFound([this]() { Error(404, "Ressource introuvable"); });
        server_.begin();
        enabled_ = true;
        PrintConnection();
    }

    void Observe(const char* line)
    {
        if (!strncmp(line, "RMP,", 4))
        {
            int slot, group, consumed = 0;
            if (sscanf(line, "RMP,%d,%d,%n", &slot, &group, &consumed) != 2 || !consumed
                || slot < 0 || slot > 128 || group < 0 || group > 4) return;
            if (group == 0 || slot != parameterSlot_) parameterMask_ = 0;
            if (!dco::DecodeHex(line + consumed, reinterpret_cast<uint8_t*>(pendingParameters_[group]),
                                sizeof(pendingParameters_[group]))) { parameterMask_ = 0; return; }
            parameterSlot_ = slot;
            parameterMask_ |= 1U << group;
        }
        else if (!strncmp(line, "RMS,", 4))
        {
            int slot, playing, count, head, bpm, busy, consumed = 0;
            if (sscanf(line, "RMS,%d,%d,%d,%d,%d,%d,%n", &slot, &playing, &count,
                       &head, &bpm, &busy, &consumed) != 6 || !consumed
                || slot < 0 || slot > 128 || count < 1 || count > 32
                || head < -1 || head >= count || (playing != 0 && playing != 1)
                || (busy != 0 && busy != 1)) return;
            dco::PatchPolyStep steps[32];
            if (!dco::DecodeHex(line + consumed, reinterpret_cast<uint8_t*>(steps), sizeof(steps))) return;
            for (const auto& step : steps)
                if (step.state > 4 || step.degree < -14 || step.degree > 14
                    || step.fixedTranspose < -24 || step.fixedTranspose > 24) return;
            slot_ = slot; playing_ = playing; count_ = count; head_ = head; bpm_ = bpm; busy_ = busy;
            parametersReady_ = parameterMask_ == 31 && parameterSlot_ == slot;
            if (parametersReady_) memcpy(parameters_, pendingParameters_, sizeof(parameters_));
            parameterMask_ = 0;
            memcpy(steps_, steps, sizeof(steps_));
            lastState_ = millis();
            receivedState_ = true;
        }
        else if (!strncmp(line, "RMR,", 4))
        {
            unsigned id;
            int consumed = 0;
            if (sscanf(line, "RMR,%u,%n", &id, &consumed) != 1 || !consumed
                || !pending_ || id != commandId_) return;
            const char* reply = line + consumed;
            result_ = !strcmp(reply, "OK") ? "ok"
                : !strcmp(reply, "ERR,BUSY") ? "Synthesiseur occupe. Reessayer."
                : !strcmp(reply, "ERR,STALE") ? "Le pattern a change. Actualiser avant de modifier."
                : !strcmp(reply, "ERR,SD") ? "Echec SD : verifier la carte et le pattern."
                : "Commande refusee par la Daisy.";
            pending_ = false;
        }
    }

    void Tick()
    {
        if (!enabled_) return;
        server_.handleClient();
        uint32_t now = millis();
        if (WiFi.softAPgetStationNum() && now - lastPoll_ >= 400)
        {
            lastPoll_ = now;
            SendSdReply("RMC,0,STATE", nullptr);
        }
        if (pending_ && now - sentAt_ >= 24000)
        {
            pending_ = false;
            result_ = "Delai depasse. Verifier le synthese avant de reessayer.";
        }
        while (Serial.available())
        {
            char value = Serial.read();
            if (value == '\n')
            {
                if (usbCommand_ == "WIFI?") PrintConnection();
                usbCommand_ = "";
            }
            else if (value != '\r')
            {
                if (usbCommand_.length() < 16) usbCommand_ += value;
                else usbCommand_ = "";
            }
        }
    }

  private:
    WebServer server_{80};
    bool enabled_ = false, receivedState_ = false, pending_ = false;
    int slot_ = 0, playing_ = 0, count_ = 32, head_ = -1, bpm_ = 120, busy_ = 0;
    dco::PatchPolyStep steps_[32] = {};
    int32_t parameters_[5][24] = {}, pendingParameters_[5][24] = {};
    int parameterSlot_ = 0;
    unsigned parameterMask_ = 0;
    bool parametersReady_ = false;
    uint32_t lastState_ = 0, lastPoll_ = 0, sentAt_ = 0;
    unsigned nextId_ = 0, commandId_ = 0;
    String token_, usbCommand_, result_ = "idle";

    bool Connected() const { return receivedState_ && millis() - lastState_ < 2500; }
    void PrintConnection()
    {
        Serial.println("[WIFI] SSID=DCO-ONE SECURITY=OPEN URL=http://192.168.4.1");
    }
    void Json(int code, const JsonDocument& document)
    {
        String body;
        serializeJson(document, body);
        server_.sendHeader("Cache-Control", "no-store");
        server_.send(code, "application/json; charset=utf-8", body);
    }
    void Error(int code, const char* message)
    {
        JsonDocument document;
        document["error"] = message;
        Json(code, document);
    }
    void State()
    {
        JsonDocument document;
        document["token"] = token_;
        document["connected"] = Connected();
        document["slot"] = slot_;
        document["playing"] = bool(playing_);
        document["count"] = count_;
        document["head"] = head_;
        document["bpm"] = bpm_;
        document["busy"] = bool(busy_) || pending_;
        document["parametersReady"] = parametersReady_;
        auto parameters = document["parameters"].to<JsonArray>();
        for (const auto& group : parameters_)
        {
            auto values = parameters.add<JsonArray>();
            for (int32_t value : group) values.add(value);
        }
        document["command"]["id"] = commandId_;
        document["command"]["status"] = result_;
        auto steps = document["steps"].to<JsonArray>();
        for (const auto& step : steps_)
        {
            auto item = steps.add<JsonObject>();
            item["state"] = step.state;
            item["degree"] = step.degree;
            item["transpose"] = step.fixedTranspose;
        }
        Json(200, document);
    }
    void Patterns()
    {
        uint8_t used[16] = {};
        if (!s_sd_disk.List(used)) { Error(503, "Carte SD absente ou illisible (FAT32 requise)."); return; }
        JsonDocument document;
        auto patterns = document["patterns"].to<JsonArray>();
        for (int slot = 1; slot <= dco::kPatchSlotCount; ++slot)
        {
            if (!(used[(slot - 1) / 8] & (1U << ((slot - 1) % 8)))) continue;
            auto item = patterns.add<JsonObject>();
            item["slot"] = slot;
            item["name"] = s_sd_disk.Name(slot);
        }
        Json(200, document);
    }
    void Command()
    {
        if (server_.header("X-DCO-Token") != token_) { Error(403, "Session invalide. Recharger la page."); return; }
        if (!Connected()) { Error(503, "Daisy hors ligne : verifier le cable UART et son firmware."); return; }
        if (pending_ || busy_) { Error(409, "Transfert en cours. Reessayer."); return; }
        if (server_.arg("plain").length() > 512) { Error(413, "Commande trop longue."); return; }
        JsonDocument request;
        if (deserializeJson(request, server_.arg("plain")) || !request["action"].is<const char*>())
        { Error(400, "Commande invalide."); return; }
        String action = request["action"].as<String>();
        int slot = request["slot"].is<int>() ? request["slot"].as<int>() : -1;
        if (action == "rename")
        {
            if (slot < 1 || slot > 128 || !request["name"].is<const char*>())
            { Error(400, "Nom ou emplacement invalide."); return; }
            String name = request["name"].as<String>();
            name.trim();
            if (!name.length() || name.length() > 64 || name.length() != strlen(name.c_str()))
            { Error(400, "Nom requis, 64 octets UTF-8 maximum."); return; }
            for (size_t index = 0; index < name.length(); ++index)
                if (static_cast<uint8_t>(name[index]) < 32 || name[index] == 127)
                { Error(400, "Caractere interdit dans le nom."); return; }
            if (!s_sd_disk.Rename(slot, name)) { Error(503, "Impossible de renommer ce pattern sur la SD."); return; }
            OnPresetRenamed(slot);
            JsonDocument result;
            result["ok"] = true;
            Json(200, result);
            return;
        }
        char line[128];
        unsigned id = ++nextId_;
        if (!id) id = ++nextId_;
        if (action == "param")
        {
            if (!parametersReady_) { Error(409, "Parametres indisponibles. Actualiser."); return; }
            for (const char* field : {"group", "parameter", "value"})
                if (!request[field].is<int>()) { Error(400, "Parametre invalide."); return; }
            snprintf(line, sizeof(line), "RMC,%u,PARAM,%d,%d,%d,%d", id, slot,
                     request["group"].as<int>(), request["parameter"].as<int>(), request["value"].as<int>());
        }
        else if (action == "step")
        {
            for (const char* field : {"step", "state", "degree", "transpose"})
                if (!request[field].is<int>()) { Error(400, "Valeur de pastille invalide."); return; }
            snprintf(line, sizeof(line), "RMC,%u,STEP,%d,%d,%d,%d,%d", id, slot,
                     request["step"].as<int>(), request["state"].as<int>(),
                     request["degree"].as<int>(), request["transpose"].as<int>());
        }
        else if (action == "stop" || action == "play")
            snprintf(line, sizeof(line), "RMC,%u,%s", id, action == "stop" ? "STOP" : "PLAY");
        else if (action == "load" || action == "save" || action == "delete")
        {
            action.toUpperCase();
            snprintf(line, sizeof(line), "RMC,%u,%s,%d", id, action.c_str(), slot);
        }
        else { Error(400, "Action inconnue."); return; }
        dco::RemoteCommand command;
        if (!dco::ParseRemoteCommand(line, command)) { Error(400, "Valeur hors limites."); return; }
        if (!SendSdReply(line, nullptr)) { Error(503, "Echec UART."); return; }
        commandId_ = id;
        pending_ = true;
        result_ = "pending";
        sentAt_ = millis();
        JsonDocument result;
        result["id"] = id;
        Json(202, result);
    }
};

static RemoteWeb s_remote_web;