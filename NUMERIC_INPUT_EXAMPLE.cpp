// ============================================================================
// EXEMPLE CONCRET : Ajouter un Paramètre "Gain" (Volume)
// ============================================================================
// Cet exemple montre comment intégrer complètement un nouveau paramètre
// numérique dans le système existant.

// ============================================================================
// ÉTAPE 1 : Déclarer le paramètre
// ============================================================================
// À ajouter après kBpmParam (ligne ~258 dans main.cpp)

static NumericParam kGainParam = {
    minValue:  0,
    maxValue:  100,
    step:      1,
    value:     80,           // volume initial à 80%
    unit:      "%"
};

// ============================================================================
// ÉTAPE 2 : Déclarer le callback d'application (live-apply)
// ============================================================================
// À ajouter après les autres callbacks ApplyXxx()

static void ApplyGain(int32_t index) {
    // 'index' n'est pas utilisé pour les paramètres numériques
    // Utiliser directement kGainParam.value
    
    float gainPercent = static_cast<float>(kGainParam.value);
    float gainLinear = gainPercent / 100.0f;  // convertir % en [0.0, 1.0]
    
    // Appliquer à l'oscillateur
    osc.SetGain(gainLinear);
    
    // Ou si on utilise une synthèse différente :
    // synth.SetAmplitude(gainLinear);
    // dco.SetVolume(gainLinear);
}

// ============================================================================
// ÉTAPE 3 : Ajouter au menu
// ============================================================================
// Exemple 1 : comme paramètre dans le menu racine

// Chercher cette section (environ ligne 200-280) :
// static MenuNode kRootChildren[] = { ... };
//
// Ajouter cette entrée au tableau :

{
    label:        "Gain",
    color:        COLOR_DEFAULT,
    children:     nullptr,           // pas de sous-menu
    childCount:   0,
    onSelect:     ApplyGain,         // applique en temps réel
    selectedIndex: 0,
    numeric:      &kGainParam        // pointer sur le paramètre
}

// Exemple 2 : dans un sous-menu "SYNTH" (si un tel menu existe)

// Chercher kSynthMenu ou équivalent, ajouter :

{
    label:        "Gain",
    color:        COLOR_YELLOW,
    children:     nullptr,
    childCount:   0,
    onSelect:     ApplyGain,
    selectedIndex: 0,
    numeric:      &kGainParam
}

// ============================================================================
// ÉTAPE 4 : Utiliser la valeur dans le code principal
// ============================================================================
// À ajouter dans la boucle principale (main loop) ou dans AudioCallback

// Option A : Appliquer continuellement dans la boucle principale
static int32_t s_prev_gain = -1;
if (s_prev_gain != kGainParam.value) {
    s_prev_gain = kGainParam.value;
    ApplyGain(0);  // forcer la mise à jour
}

// Option B : Si on utilise un AudioCallback (IRQ), mettre à jour directement :
static float s_current_gain = 0.8f;
void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    // Utiliser s_current_gain dans le traitement
    for (size_t i = 0; i < size; ++i) {
        out[0][i] = osc.Process() * s_current_gain;
    }
}

// Puis mettre à jour s_current_gain dans la main loop :
static void UpdateAudioGain() {
    s_current_gain = static_cast<float>(kGainParam.value) / 100.0f;
}

// ============================================================================
// ÉTAPE 5 : Envoyer au ESP32 (affichage dans le hub)
// ============================================================================
// Chercher SendStatus() et ajouter le Gain à l'affichage

// Avant (ligne ~529) :
// hw.PrintLine("STAT,BPM=%ld,ROOT=%ld,SCALE=%ld,PLAY=%d",
//              static_cast<long>(kBpmParam.value),
//              ...);

// Après (nouvelle version) :
// hw.PrintLine("STAT,BPM=%ld,GAIN=%ld,ROOT=%ld,SCALE=%ld,PLAY=%d",
//              static_cast<long>(kBpmParam.value),
//              static_cast<long>(kGainParam.value),  // <-- AJOUTÉ
//              ...);

// Dans esp32/src/main.cpp, parser le "GAIN=" et afficher dans le hub central

// ============================================================================
// ÉTAPE 6 : (Optionnel) Persistance EEPROM
// ============================================================================
// Si on veut sauvegarder le Gain au redémarrage

void SaveGainToEEPROM() {
    // Pseudo-code (dépend de la lib EEPROM)
    EEPROM.write(GAIN_EEPROM_ADDR, kGainParam.value);
    EEPROM.commit();
}

void LoadGainFromEEPROM() {
    int32_t saved = EEPROM.read(GAIN_EEPROM_ADDR);
    if (saved >= 0 && saved <= 100) {
        kGainParam.value = saved;
        ApplyGain(0);
    }
}

// Appeler dans main() :
// LoadGainFromEEPROM();
// ... et dans la boucle principale, sauvegarde périodique :
// if (now - last_eeprom_save > 5000) {
//     SaveGainToEEPROM();
//     last_eeprom_save = now;
// }

// ============================================================================
// ÉTAPE 7 : Tester
// ============================================================================
//
// 1. Compiler et flasher le Daisy Seed
// 2. Naviguer le menu jusqu'au "Gain"
// 3. Appuyer sur MENU_SW → entrée en mode d'édition
// 4. Tourner l'encodeur MENU → le gain augmente/diminue
// 5. Appuyer sur MENU_SW → confirmation et sortie
// 6. Appuyer sur HOME avant MENU_SW → annulation
//
// Vous devriez entendre le volume changer en temps réel.

// ============================================================================
// VARIANTE : Paramètre Attaque (Attack Time)
// ============================================================================
// Un autre exemple courant : temps d'attaque de l'enveloppe

static NumericParam kAttackParam = {
    minValue:  1,              // 1 ms min
    maxValue:  1000,           // 1 sec max
    step:      10,             // par 10 ms
    value:     50,             // 50 ms initial
    unit:      "ms"
};

static void ApplyAttack(int32_t index) {
    float attackMs = static_cast<float>(kAttackParam.value);
    float attackSec = attackMs / 1000.0f;
    
    // Si on utilise une enveloppe ADSRish
    envelope.SetAttackTime(attackSec);
    
    // Ou pour DaisySP :
    // adsr.SetTime(ADSR::SEGMENT_ATTACK, attackSec);
}

// Ajouter au menu :
// { "Attack", COLOR_BLUE, nullptr, 0, ApplyAttack, 0, &kAttackParam }

// ============================================================================
// VARIANTE : Paramètre BPM avec Tempo Sync
// ============================================================================
// (le BPM est déjà implémenté, mais voici comment l'utiliser)

static void ApplyBPM(int32_t index) {
    // Calculer la période de la note de base
    float bpm = static_cast<float>(kBpmParam.value);
    float beatMs = (60000.0f / bpm);           // beat en ms
    float quarterNoteMs = beatMs;              // croche = 1 beat
    float eighthNoteMs = beatMs / 2.0f;        // double croche = 1/2 beat
    float halfNoteMs = beatMs * 2.0f;          // blanche = 2 beats
    
    // Utiliser pour les LFOs, arpéggiateurs, etc.
    // lfo.SetFrequency(1000.0f / beatMs);
    // arpeggiator.SetBPM(bpm);
}

// ============================================================================
// CHECKLIST RÉSUMÉE
// ============================================================================
//
// ✓ Ajouter la structure static NumericParam kXxxParam
// ✓ Déclarer la fonction callback ApplyXxx()
// ✓ Ajouter l'entrée au MenuNode[] avec .numeric = &kXxxParam
// ✓ Utiliser kXxxParam.value dans le code
// ✓ (Optionnel) Ajouter dans SendStatus()
// ✓ Compiler et tester la saisie
// ✓ Vérifier que HOME annule correctement
//
// ============================================================================
