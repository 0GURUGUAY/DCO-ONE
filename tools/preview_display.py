"""Render the firmware's drawing functions on the host using its actual bitmap fonts.

Requires clang++, and ESP32 libraries installed by `pio run -d esp32`.
The software rasterizer checks layout, not QSPI timing or indexed-palette behavior.
Outputs PNG previews in build/display-preview without connecting to hardware.
"""

from pathlib import Path
import ast
import struct
import subprocess
import tempfile
import zlib


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "build" / "display-preview"

SUPPORT = r'''
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
using std::min;
using std::max;
#define PROGMEM
#define _ADAFRUIT_GFX_H
#include "gfxfont.h"
#include "Fonts/FreeSans9pt7b.h"
#include "Fonts/FreeSans12pt7b.h"
#include "Fonts/FreeSansBold12pt7b.h"
#include "Fonts/FreeSans18pt7b.h"
#include "Fonts/FreeSans24pt7b.h"
#define LCD_WIDTH 466
#define LCD_HEIGHT 466
#define PI M_PI
#define RGB565_BLACK 0x0000
#define RGB565_RED 0xF800
#define RGB565_GREEN 0x07E0
#define RGB565_BLUE 0x001F
#define RGB565_CYAN 0x07FF
#define RGB565_YELLOW 0xFFE0
#define RGB565_MAGENTA 0xF81F
#define RGB565_WHITE 0xFFFF
template<class Value, class Low, class High>
Value constrain(Value value, Low low, High high) {
    return max(Value(low), min(value, Value(high)));
}
float radians(float angle) { return angle * float(M_PI) / 180.0f; }
uint32_t millis() { return 0; }
class String {
    std::string value;
public:
    String(const char* text = "") : value(text) {}
    String(std::string text) : value(text) {}
    int indexOf(const char* text, int start = 0) const {
        auto position = value.find(text, start);
        return position == std::string::npos ? -1 : int(position);
    }
    int indexOf(char character, int start = 0) const {
        auto position = value.find(character, start);
        return position == std::string::npos ? -1 : int(position);
    }
    String substring(int start, int end = -1) const {
        return value.substr(start, end < 0 ? std::string::npos : end - start);
    }
    long toInt() const { return strtol(value.c_str(), nullptr, 10); }
    const char* c_str() const { return value.c_str(); }
};
class PreviewCanvas {
    const GFXfont* font = &FreeSans9pt7b;
    int cursorX = 0, cursorY = 0;
    uint16_t ink = 0xFFFF;
    struct TextBox { int left, top, right, bottom; };
    std::vector<TextBox> textBoxes;
public:
    std::vector<uint16_t> pixels = std::vector<uint16_t>(466 * 466);
    std::string scene;
    int errors = 0;
    void drawPixel(int posX, int posY, uint16_t color) {
        if (posX >= 0 && posX < 466 && posY >= 0 && posY < 466)
            pixels[posY * 466 + posX] = color;
    }
    void fillScreen(uint16_t color) { std::fill(pixels.begin(), pixels.end(), color); textBoxes.clear(); }
    void setFont(const GFXfont* value) { font = value; }
    void setTextSize(int) {}
    void setTextColor(uint16_t color) { ink = color; }
    void setCursor(int posX, int posY) { cursorX = posX; cursorY = posY; }
    void getTextBounds(const char* text, int posX, int posY, int16_t* left, int16_t* top,
                       uint16_t* width, uint16_t* height) {
        int minX = 32767, minY = 32767, maxX = -32768, maxY = -32768;
        for (const char* character = text; *character; ++character) {
            auto code = static_cast<unsigned char>(*character);
            if (code < font->first || code > font->last) continue;
            const auto& glyph = font->glyph[code - font->first];
            if (glyph.width && glyph.height) {
                minX = min(minX, posX + glyph.xOffset);
                minY = min(minY, posY + glyph.yOffset);
                maxX = max(maxX, posX + glyph.xOffset + glyph.width - 1);
                maxY = max(maxY, posY + glyph.yOffset + glyph.height - 1);
            }
            posX += glyph.xAdvance;
        }
        *left = maxX >= minX ? minX : posX;
        *top = maxY >= minY ? minY : posY;
        *width = maxX >= minX ? maxX - minX + 1 : 0;
        *height = maxY >= minY ? maxY - minY + 1 : 0;
    }
    void print(const char* text) {
        int16_t left, top;
        uint16_t width, height;
        getTextBounds(text, cursorX, cursorY, &left, &top, &width, &height);
        if (width && height) {
            TextBox box{left, top, left + width, top + height};
            for (const auto& previous : textBoxes)
                if (box.left < previous.right && box.right > previous.left &&
                    box.top < previous.bottom && box.bottom > previous.top) {
                    fprintf(stderr, "%s: text overlap: %s\n", scene.c_str(), text);
                    ++errors;
                }
            textBoxes.push_back(box);
        }
        for (const char* character = text; *character; ++character) {
            auto code = static_cast<unsigned char>(*character);
            if (code < font->first || code > font->last) continue;
            const auto& glyph = font->glyph[code - font->first];
            for (int row = 0; row < glyph.height; ++row)
                for (int column = 0; column < glyph.width; ++column) {
                    int bitIndex = row * glyph.width + column;
                    if (font->bitmap[glyph.bitmapOffset + bitIndex / 8] & (0x80 >> (bitIndex % 8))) {
                        int posX = cursorX + glyph.xOffset + column;
                        int posY = cursorY + glyph.yOffset + row;
                        if ((posX - 233) * (posX - 233) + (posY - 233) * (posY - 233) > 233 * 233) {
                            fprintf(stderr, "%s: clipped text: %s\n", scene.c_str(), text);
                            ++errors;
                            return;
                        }
                        drawPixel(posX, posY, ink);
                    }
                }
            cursorX += glyph.xAdvance;
        }
    }
    void drawLine(int fromX, int fromY, int toX, int toY, uint16_t color) {
        int steps = max(abs(toX - fromX), abs(toY - fromY));
        for (int step = 0; step <= steps; ++step) {
            float amount = steps ? float(step) / steps : 0;
            drawPixel(lround(fromX + amount * (toX - fromX)), lround(fromY + amount * (toY - fromY)), color);
        }
    }
    void drawFastHLine(int posX, int posY, int width, uint16_t color) { drawLine(posX, posY, posX + width - 1, posY, color); }
    void drawFastVLine(int posX, int posY, int height, uint16_t color) { drawLine(posX, posY, posX, posY + height - 1, color); }
    void fillRect(int posX, int posY, int width, int height, uint16_t color) {
        for (int row = posY; row < posY + height; ++row) drawFastHLine(posX, row, width, color);
    }
    void drawRect(int posX, int posY, int width, int height, uint16_t color) {
        drawFastHLine(posX, posY, width, color); drawFastHLine(posX, posY + height - 1, width, color);
        drawFastVLine(posX, posY, height, color); drawFastVLine(posX + width - 1, posY, height, color);
    }
    void fillRoundRect(int posX, int posY, int width, int height, int, uint16_t color) { fillRect(posX, posY, width, height, color); }
    void fillCircle(int centerX, int centerY, int radius, uint16_t color) {
        for (int row = -radius; row <= radius; ++row)
            for (int column = -radius; column <= radius; ++column)
                if (row * row + column * column <= radius * radius) drawPixel(centerX + column, centerY + row, color);
    }
    void drawCircle(int centerX, int centerY, int radius, uint16_t color) {
        for (float angle = 0; angle < 360; angle += 0.2f)
            drawPixel(lround(centerX + radius * cos(radians(angle))), lround(centerY + radius * sin(radians(angle))), color);
    }
    void fillArc(int centerX, int centerY, int outer, int inner, float start, float end, uint16_t color) {
        for (int row = -outer; row <= outer; ++row)
            for (int column = -outer; column <= outer; ++column) {
                int distance = row * row + column * column;
                if (distance < inner * inner || distance > outer * outer) continue;
                float angle = atan2(float(row), float(column)) * 180.0f / float(M_PI);
                while (angle < start) angle += 360;
                if (angle <= end) drawPixel(centerX + column, centerY + row, color);
            }
    }
    void drawArc(int centerX, int centerY, int outer, int inner, float start, float end, uint16_t color) {
        fillArc(centerX, centerY, outer, inner, start, end, color);
    }
    void drawTriangle(int firstX, int firstY, int secondX, int secondY, int thirdX, int thirdY, uint16_t color) {
        drawLine(firstX, firstY, secondX, secondY, color); drawLine(secondX, secondY, thirdX, thirdY, color);
        drawLine(thirdX, thirdY, firstX, firstY, color);
    }
    void fillTriangle(int firstX, int firstY, int secondX, int secondY, int thirdX, int thirdY, uint16_t color) {
        for (int step = 0; step <= 500; ++step) {
            float amount = step / 500.0f;
            drawLine(firstX, firstY, lround(secondX + amount * (thirdX - secondX)), lround(secondY + amount * (thirdY - secondY)), color);
        }
    }
    void flush() {}
    void save(const char* name) {
        std::ofstream output(std::string(name) + ".rgb", std::ios::binary);
        int lit = 0;
        for (uint16_t pixel : pixels) {
            unsigned char rgb[] = {static_cast<unsigned char>(((pixel >> 11) & 31) * 255 / 31),
                static_cast<unsigned char>(((pixel >> 5) & 63) * 255 / 63), static_cast<unsigned char>((pixel & 31) * 255 / 31)};
            output.write(reinterpret_cast<char*>(rgb), 3);
            lit += pixel != 0;
        }
        assert(lit > 500);
    }
};
PreviewCanvas previewCanvas;
PreviewCanvas* canvas = &previewCanvas;
'''

PRESET_TEST_SUPPORT = r'''
namespace presetTests {
#define DEBUG_LOG(...) ((void)0)
namespace dco {
constexpr int kPatchSlotCount = 128;
struct PatchData {};
struct PatternClient {
    enum class Operation { None, List, Load, Save };
    bool accepting = true;
    Operation requested = Operation::None;
    bool Start(Operation operation, int, uint32_t, const PatchData* = nullptr) {
        if (!accepting) return false;
        requested = operation;
        return true;
    }
};
}
dco::PatternClient s_sd_client;
bool s_patch_used[dco::kPatchSlotCount] = {};
int32_t s_current_patch = 0;
bool s_patch_error = false;
bool s_playing = true, s_midi_clock_running = true;
bool s_440hz_test_active = true, s_poly_wheel_active = false;
int s_poly_cursor = 3, s_poly_play_step = 5;
int s_prog_transpose_degrees = 2, s_auto_steps_until_change = 0;
int applied = 0, dirty = 0, silenced = 0;
struct Param { int value; } kPlayAutoParam{4}, loadParam{3};
struct Node { Param* numeric; void (*onConfirm)(int32_t); };
Node* s_editing_node = nullptr;
bool s_editing_numeric = true;
struct Midi { void SendStop() {} void SendStart() {} } s_midi_out;
float s_midi_clock_phase = 0;
struct System { static int GetNow() { return 0; } };
std::vector<std::string> events;
void PanicSilence() { ++silenced; }
void TriggerPolyStep(int) {}
void ApplyPatchData(const dco::PatchData&) { ++applied; }
void CopySettingsToPatchData(dco::PatchData&) {}
void ApplyAllEngineSettings() {}
void MarkSettingsDirty(int) { ++dirty; }
void SendPlayStatus() { events.push_back("STAT"); }
void SendPolyState() { events.push_back("POLY"); }
void SendMenuPath() { events.push_back("NAV"); }
'''

PRESET_TEST_CASES = r'''
void run() {
    assert(NextUsedPatchSlot(1, 0) == 0);
    s_patch_used[2] = true; s_patch_used[8] = true; s_patch_used[127] = true;
    assert(NextUsedPatchSlot(1, 0) == 3);
    assert(NextUsedPatchSlot(3, 1) == 9);
    assert(NextUsedPatchSlot(3, 2) == 128);
    assert(NextUsedPatchSlot(128, -1) == 9);
    assert(NextUsedPatchSlot(128, 99) == 128);
    assert(NextUsedPatchSlot(3, -99) == 3);
    Node loadNode{&loadParam, ApplyPatchLoad};
    s_editing_node = &loadNode;
    confirm();
    assert(!s_poly_wheel_active && !s_editing_numeric && !s_editing_node);
    assert(s_playing && applied == 0 && silenced == 0 && s_current_patch == 0);
    assert(s_sd_client.requested == dco::PatternClient::Operation::Load);
    events.clear();
    CompletePatchLoad(3, dco::PatchData{});
    assert(s_poly_wheel_active && s_playing && s_midi_clock_running && !s_440hz_test_active);
    assert(s_current_patch == 3 && applied == 1 && dirty == 1 && silenced == 1);
    assert(s_poly_cursor == 0 && s_poly_play_step == 0);
    assert(events.size() == 2 && events.back() == "POLY");
    events.clear();
    s_sd_client.accepting = false;
    s_playing = true; s_poly_wheel_active = false;
    s_editing_numeric = true; s_editing_node = &loadNode;
    confirm();
    assert(s_playing && !s_poly_wheel_active && s_patch_error);
    assert(applied == 1 && silenced == 1 && s_current_patch == 3);
    assert(events.back() == "NAV");
    ApplyPatchLoad(0);
    assert(s_playing && applied == 1);
    s_sd_client.accepting = true;
    ApplyPatchSave(7);
    assert(s_current_patch == 3 && !s_patch_used[6] && !s_patch_error);
    assert(s_sd_client.requested == dco::PatternClient::Operation::Save);
    s_sd_client.accepting = false;
    ApplyPatchSave(8);
    assert(s_current_patch == 3 && !s_patch_used[7] && s_patch_error);
    puts("SD LOAD/SAVE wait for completion; successful LOAD starts POLY; rejected requests preserve playback.");
}
}
'''

SCENES = r'''
int main() {
    presetTests::run();
    for (int tick = 0; tick < 35; ++tick) {
        if (tick % 5 == 0) SendPlayStatus();
        SendNextStatusFrame();
    }
    assert(std::find(sentFrames.begin(), sentFrames.end(), 6) != sentFrames.end());
    puts("OSC status remains reachable during navigation heartbeats.");
    s_status_valid = true;
    s_status_note = 60;
    s_osc_valid = true;
    s_osc_fm_ratio = 2;
    s_osc_fm_amount = 45;
    s_audio_valid = true;
    for (int sample = 0; sample < kAudioSampleCount; ++sample)
        s_audio_samples[sample] = int8_t(100 * sin(sample * 0.8f));
    auto wheel = [&](const char* name, const char* path) {
        canvas->scene = name;
        applyNavPath(String(path));
        drawCurrentLevel();
        canvas->save(name);
    };
    wheel("01-home", "0");
    wheel("02-osc", "1.0");
    s_osc_wave = 4;
    wheel("03-fm", "1.6.0");
    wheel("04-env", "3.0");
    wheel("05-vcf", "2.1");
    wheel("06-lfo", "5.0");
    s_mat1_src = 1; s_mat1_dst = 1; s_mat1_amt = 75;
    wheel("07-matrix", "6.0");
    canvas->scene = "08-edit";
    drawParamEditRing("Cutoff", 5000, 20, 20000, "Hz");
    canvas->save("08-edit");
    canvas->scene = "09-env-edit";
    drawEnvelopeEditScreen("Attack", 5000, 0, 5000, "ms");
    canvas->save("09-env-edit");
    s_status_bpm = 200; s_status_scale = 3; s_status_note = 127;
    s_status_t440 = true; s_midi_in_active = true; s_midi_out_active = true;
    wheel("10-home-max", "8");
    s_osc_fm_ratio = 32; s_osc_fm_fine = 50; s_osc_fm_amount = 100; s_osc_fm_wave = 3;
    wheel("11-fm-max", "1.6.2");
    s_lfo1_amp = 100; s_lfo1_phase = 360;
    wheel("12-lfo-max", "5.4");
    assert(applyPresetState(String("PRST,C=3,M=0,S=0,U=55000000000000000000000000000008,W=4,B=200,R=11,G=3,F=4,H=20000,N=32,E=0,SD=1,IO=0")));
    assert(s_preset.sdReady && !s_preset.busy);
    assert(s_preset.used[0] && s_preset.used[2] && s_preset.used[4] && s_preset.used[6] && s_preset.used[127]);
    assert(!s_preset.used[1] && !s_preset.used[126]);
    wheel("13-presets", "8.0");
    auto presetList = [&](const char* name, int mode, int slot) {
        canvas->scene = name;
        s_preset.mode = mode; s_preset.slot = slot;
        drawPresetList();
        canvas->save(name);
    };
    presetList("14-load", 1, 3);
    presetList("15-save", 2, 4);
    presetList("16-save-last", 2, 128);
    memset(s_preset.used, 0, sizeof(s_preset.used));
    presetList("17-load-empty", 1, 0);
    memset(s_preset.used, 1, sizeof(s_preset.used));
    presetList("18-load-full", 1, 128);
    s_preset.current = 0;
    wheel("19-presets-unassigned", "8.1");
    s_preset.sdReady = 0;
    presetList("20-sd-absent", 2, 128);
    s_preset.busy = 1;
    presetList("21-sd-transfer", 2, 128);
    s_preset.busy = 0;
    s_preset.error = 1;
    presetList("22-sd-error", 2, 128);
    wheel("23-sd-error-hub", "8.0");
    for (int root = 0; root < kRootMenuCount; ++root) {
        const auto& menu = kRootMenu[root];
        for (int item = 0; item < menu.childCount; ++item) {
            char path[24]; snprintf(path, sizeof(path), "%d.%d", root, item);
            canvas->scene = path;
            applyNavPath(String(path));
            drawCurrentLevel();
        }
    }
    assert(canvas->errors == 0);
    puts("All preview scenes are nonblank; text bounds and overlap checks passed.");
}
'''


def write_png(path, pixels):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    rows = b"".join(b"\x00" + pixels[row * 1398:(row + 1) * 1398] for row in range(466))
    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", 466, 466, 8, 2, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")
    )


def main():
    for bridge_name in ("combined_bridge.py", "usb_bridge.py"):
        tree = ast.parse((ROOT / bridge_name).read_text())
        relay_test = next(
            node.value for node in ast.walk(tree)
            if isinstance(node, ast.Assign)
            and any(isinstance(target, ast.Name) and target.id == "is_control_msg" for target in node.targets)
        )
        predicate = compile(ast.Expression(relay_test), bridge_name, "eval")
        frame = "OSC,W=4,A=100,R=32,F=-50,M=3"
        assert len((frame + "\r\n").encode("ascii")) < 128
        assert eval(predicate, {"__builtins__": {}}, {"text": frame})
        frame = "PRST,C=128,M=2,S=128,U=" + "F" * 32 + ",W=4,B=200,R=11,G=7,F=4,H=20000,N=32,E=1"
        assert len((frame + "\r\n").encode("ascii")) < 128
        assert eval(predicate, {"__builtins__": {}}, {"text": frame})
        assert not eval(predicate, {"__builtins__": {}}, {"text": "UNKNOWN,W=4"})
    print("Both USB bridges accept OSC and PRST frames; worst-case frames fit the logger.")
    source = (ROOT / "esp32/src/main.cpp").read_text()
    rendering = source[source.index("// Color definitions"):source.index("void initDisplay()")]
    daisy = (ROOT / "daisy/src/main.cpp").read_text()
    preset_test = PRESET_TEST_SUPPORT
    for signature in ("static int32_t NextUsedPatchSlot(", "static void ApplyPatchLoad(int32_t slot)",
                      "static void ApplyPatchSave(int32_t slot)",
                      "static void CompletePatchLoad(int32_t slot, const dco::PatchData& patch)"):
        start = daisy.index(signature)
        preset_test += daisy[start:daisy.index("\n}", start) + 2] + "\n"
    confirm = daisy.split("// Confirm the edited value and return to the submenu wheel\n", 1)[1]
    confirm = confirm.split("                last_menu_send = now;", 1)[0]
    preset_test += "void confirm() {\n" + confirm + "}\n" + PRESET_TEST_CASES
    status_body = daisy.split("static void SendPlayStatus()\n{", 1)[1].split("\n}", 1)[0]
    advance = daisy.split("s_status_frame_index = (s_status_frame_index + 1)", 1)[1].split(";", 1)[0]
    status_test = (
        "\nstatic uint8_t s_status_frame_index = 0;\nstd::vector<int> sentFrames;\n"
        "static void SendNextStatusFrame() { sentFrames.push_back(s_status_frame_index);\n"
        "s_status_frame_index = (s_status_frame_index + 1)" + advance + "; }\n"
        "static void SendPlayStatus() {" + status_body + "\n}\n"
    )
    libraries = ROOT / "esp32/.pio/libdeps/esp32-s3-amoled/Adafruit GFX Library"
    OUTPUT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="dco-preview-") as temporary:
        folder = Path(temporary)
        translation = folder / "preview.cpp"
        translation.write_text(SUPPORT + rendering + status_test + preset_test + SCENES)
        executable = folder / "preview"
        subprocess.run(["clang++", "-std=c++17", "-O1", "-fsanitize=address,undefined",
                        "-I", str(libraries), "-I", str(ROOT / "esp32/src"),
                        str(translation), "-o", str(executable)], check=True)
        result = subprocess.run([str(executable)], cwd=folder, check=False)
        for image in folder.glob("*.rgb"):
            write_png(OUTPUT / f"{image.stem}.png", image.read_bytes())
        result.check_returncode()
    print(f"Previews: {OUTPUT}")


if __name__ == "__main__":
    main()