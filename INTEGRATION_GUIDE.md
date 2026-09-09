// ============================================================================
// GUIDE D'INTÉGRATION : Ajouter les inputs numériques au main.cpp
// ============================================================================
// Ce guide montre exactement où placer chaque bloc de code dans main.cpp

// ============================================================================
// ÉTAPE 1 : DÉCLARER LES PARAMÈTRES NUMÉRIQUES
// ============================================================================
// Emplacement : après la ligne 258 (après kBpmParam = { ... })
// Avant : static MenuNode kPlaySubmenu[] = { ... };

// REMPLACER CETTE SECTION (lignes 258-272) :
/*
static NumericParam kBpmParam = { 1, 200, 1, 120, "BPM" };

static MenuNode kPlaySubmenu[] = {
    { "BPM",           COLOR_DEFAULT, nullptr,             0,                       nullptr, 0, &kBpmParam },
    { "Root",   COLOR_DEFAULT, kPlayRootOptions,    kPlayRootOptionCount,     nullptr, 9 },
    ...
*/

// PAR CECI :
/*
// ---- DÉCLARATIONS DE TOUS LES PARAMÈTRES NUMÉRIQUES ----

// PLAY
static NumericParam kBpmParam          = {   1,   200,   1,  120, "BPM" };
static NumericParam kPlayAutoParam     = {   0,    16,   1,    0, "steps" };
static NumericParam kPlayOctParam      = { -24,    24,   1,    0, "st" };
static NumericParam kPlayStepParam     = {   1,    32,   1,   16, "steps" };

// OSC
static NumericParam kOscCoarseParam    = { -24,    24,   1,    0, "st" };
static NumericParam kOscFineParam      = { -100,   100,   1,    0, "cents" };
static NumericParam kOscPulseParam     = {   0,    100,   1,   50, "%" };
static NumericParam kOscSubParam       = {   0,    100,   1,   30, "%" };
static NumericParam kOscHardParam      = {   0,    100,   1,    0, "%" };

// VCF
static NumericParam kVcfCutoffParam    = {  20, 20000,  50, 5000, "Hz" };
static NumericParam kVcfResonanceParam = {   0,   100,   1,   30, "%" };
static NumericParam kVcfKeyParam       = {   0,   100,   1,   50, "%" };
static NumericParam kVcfDriveParam     = {   0,   100,   5,    0, "%" };
static NumericParam kVcfEnvParam       = {-100,   100,   5,    0, "%" };

// ENV1
static NumericParam kEnv1AttackParam   = {   1,  5000,  10,   50, "ms" };
static NumericParam kEnv1DecayParam    = {   1,  5000,  10,  200, "ms" };
static NumericParam kEnv1SustainParam  = {   0,   100,   1,   80, "%" };
static NumericParam kEnv1ReleaseParam  = {   1,  5000,  10,  300, "ms" };
static NumericParam kEnv1VelocityParam = {   0,   100,   1,   80, "%" };

// ENV2
static NumericParam kEnv2AttackParam   = {   1,  5000,  10,   50, "ms" };
static NumericParam kEnv2DecayParam    = {   1,  5000,  10,  200, "ms" };
static NumericParam kEnv2SustainParam  = {   0,   100,   1,   50, "%" };
static NumericParam kEnv2ReleaseParam  = {   1,  5000,  10,  300, "ms" };

// LFO
static NumericParam kLfo1RateParam     = {   1,   500,   1,   10, "Hz" };
static NumericParam kLfo1AmpParam      = {   0,   100,   1,   50, "%" };
static NumericParam kLfo1PhaseParam    = {   0,   360,   5,    0, "°" };
static NumericParam kLfo2RateParam     = {   1,   500,   1,    8, "Hz" };
static NumericParam kLfo2AmpParam      = {   0,   100,   1,   40, "%" };
static NumericParam kLfo2PhaseParam    = {   0,   360,   5,  180, "°" };

// MATRIX
static NumericParam kMatSlot1AmtParam  = { -100,   100,   5,    0, "%" };
static NumericParam kMatSlot2AmtParam  = { -100,   100,   5,    0, "%" };

// FX
static NumericParam kFxDryWetParam     = {   0,   100,   1,   50, "%" };
static NumericParam kFxTimeParam       = {  10,  5000,  50,  500, "ms" };
static NumericParam kFxFbParam         = {   0,   100,   1,   50, "%" };
static NumericParam kFxBitParam        = {   4,    16,   1,    8, "bits" };

// MIDI
static NumericParam kMidiChannelParam  = {   1,    16,   1,    1, "ch" };
static NumericParam kMidiBendParam     = {   1,    24,   1,    2, "st" };

// SYSTEM
static NumericParam kSysLuminositeParam = {  10,   100,   1,   80, "%" };
*/

// ============================================================================
// ÉTAPE 2 : DÉCLARER LES CALLBACKS D'APPLICATION
// ============================================================================
// Emplacement : après ApplyWaveform() (vers ligne 190)

// AJOUTER TOUTES CES FONCTIONS (voir NUMERIC_INPUTS_COMPLETE.cpp section 2)

// Exemple pour la première :
/*
static void ApplyPlayAuto(int32_t index) {
    // synth.SetArpeggiatorMode(kPlayAutoParam.value);
}

static void ApplyOscCoarse(int32_t index) {
    // synth.SetPitch(kOscCoarseParam.value + kOscFineParam.value / 100.0f);
}

... (voir fichier complet pour toutes les autres)
*/

// ============================================================================
// ÉTAPE 3 : REMPLACER LES STRUCTURES MENUNODE
// ============================================================================
// Emplacement : à partir de la ligne ~273 (kPlaySubmenu[])

// 3.1 REMPLACER kPlaySubmenu[] (lignes 273-281)
/*
AVANT :
static MenuNode kPlaySubmenu[] = {
    { "BPM",           COLOR_DEFAULT, nullptr,             0,                       nullptr, 0, &kBpmParam },
    { "Root",   COLOR_DEFAULT, kPlayRootOptions,    kPlayRootOptionCount,     nullptr, 9 },
    { "Scale",         COLOR_DEFAULT, kPlayScaleOptions,   kPlayScaleOptionCount,    nullptr, 0 },
    { "Chords",     COLOR_DEFAULT, kPlayChordsOptions,  kPlayChordsOptionCount,   nullptr, 0 },
    { "Auto",    COLOR_DEFAULT, nullptr,             0,                       nullptr, 0 },
    { "Oct",    COLOR_DEFAULT, nullptr,             0,                       nullptr, 0 },
    { "Step",      COLOR_DEFAULT, nullptr,             0,                       nullptr, 0 },
    { "Div", COLOR_DEFAULT, kPlayPolyDivOptions, kPlayPolyDivOptionCount,  nullptr, 6 },
};

APRÈS :
static MenuNode kPlaySubmenu[] = {
    { "BPM",      COLOR_DEFAULT, nullptr,             0,                       nullptr, 0, &kBpmParam },
    { "Root",     COLOR_DEFAULT, kPlayRootOptions,    kPlayRootOptionCount,     nullptr, 9 },
    { "Scale",    COLOR_DEFAULT, kPlayScaleOptions,   kPlayScaleOptionCount,    nullptr, 0 },
    { "Chords",   COLOR_DEFAULT, kPlayChordsOptions,  kPlayChordsOptionCount,   nullptr, 0 },
    { "Auto",     COLOR_DEFAULT, nullptr,             0,                       ApplyPlayAuto, 0, &kPlayAutoParam },
    { "Oct",      COLOR_DEFAULT, nullptr,             0,                       nullptr, 0, &kPlayOctParam },
    { "Step",     COLOR_DEFAULT, nullptr,             0,                       nullptr, 0, &kPlayStepParam },
    { "Div",      COLOR_DEFAULT, kPlayPolyDivOptions, kPlayPolyDivOptionCount,  nullptr, 6 },
};
*/

// 3.2 REMPLACER kOscSubmenu[] (lignes 290-296)
/*
AVANT :
static MenuNode kOscSubmenu[] = {
    { "Form",   COLOR_DEFAULT, kOscWaveOptions, kOscWaveOptionCount, ApplyWaveform, 0 },
    { "Coarse",COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Fine",  COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Pulse",COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Sub-Osc", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Hard",  COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};

APRÈS :
static MenuNode kOscSubmenu[] = {
    { "Form",     COLOR_DEFAULT, kOscWaveOptions, kOscWaveOptionCount, ApplyWaveform, 0 },
    { "Coarse",   COLOR_DEFAULT, nullptr, 0, ApplyOscCoarse, 0, &kOscCoarseParam },
    { "Fine",     COLOR_DEFAULT, nullptr, 0, ApplyOscFine, 0, &kOscFineParam },
    { "Pulse",    COLOR_DEFAULT, nullptr, 0, ApplyOscPulse, 0, &kOscPulseParam },
    { "Sub-Osc",  COLOR_DEFAULT, nullptr, 0, ApplyOscSub, 0, &kOscSubParam },
    { "Hard",     COLOR_DEFAULT, nullptr, 0, ApplyOscHard, 0, &kOscHardParam },
};
*/

// 3.3 REMPLACER kVcfSubmenu[] (lignes 305-312)
/*
static MenuNode kVcfSubmenu[] = {
    { "Filter",    COLOR_DEFAULT, kVcfTypeOptions, kVcfTypeOptionCount, nullptr, 0 },
    { "Cutoff",    COLOR_DEFAULT, nullptr, 0, ApplyVcfCutoff, 0, &kVcfCutoffParam },
    { "Resonance", COLOR_DEFAULT, nullptr, 0, ApplyVcfResonance, 0, &kVcfResonanceParam },
    { "Key",       COLOR_DEFAULT, nullptr, 0, ApplyVcfKey, 0, &kVcfKeyParam },
    { "Drive",     COLOR_DEFAULT, nullptr, 0, ApplyVcfDrive, 0, &kVcfDriveParam },
    { "Env",       COLOR_DEFAULT, nullptr, 0, ApplyVcfEnv, 0, &kVcfEnvParam },
};
*/

// 3.4 REMPLACER kEnv1Submenu[] (lignes 320-325)
/*
static MenuNode kEnv1Submenu[] = {
    { "Attack",    COLOR_DEFAULT, nullptr, 0, ApplyEnv1Attack, 0, &kEnv1AttackParam },
    { "Decay",     COLOR_DEFAULT, nullptr, 0, ApplyEnv1Decay, 0, &kEnv1DecayParam },
    { "Sustain",   COLOR_DEFAULT, nullptr, 0, ApplyEnv1Sustain, 0, &kEnv1SustainParam },
    { "Release",   COLOR_DEFAULT, nullptr, 0, ApplyEnv1Release, 0, &kEnv1ReleaseParam },
    { "Velocity",  COLOR_DEFAULT, nullptr, 0, nullptr, 0, &kEnv1VelocityParam },
};
*/

// 3.5 REMPLACER kEnv2Submenu[] (lignes 333-338)
/*
static MenuNode kEnv2Submenu[] = {
    { "Attack",   COLOR_DEFAULT, nullptr, 0, ApplyEnv2Attack, 0, &kEnv2AttackParam },
    { "Decay",    COLOR_DEFAULT, nullptr, 0, ApplyEnv2Decay, 0, &kEnv2DecayParam },
    { "Sustain",  COLOR_DEFAULT, nullptr, 0, ApplyEnv2Sustain, 0, &kEnv2SustainParam },
    { "Release",  COLOR_DEFAULT, nullptr, 0, ApplyEnv2Release, 0, &kEnv2ReleaseParam },
    { "Loop",     COLOR_DEFAULT, kEnv2LoopOptions, kEnv2LoopOptionCount, nullptr, 0 },
};
*/

// 3.6 REMPLACER kLfoSubmenu[] (lignes 347-358)
/*
static MenuNode kLfoSubmenu[] = {
    { "1 Shape",  COLOR_DEFAULT, kLfoShapeOptions, kLfoShapeOptionCount, nullptr, 0 },
    { "1 Rate",   COLOR_DEFAULT, nullptr, 0, ApplyLfo1Rate, 0, &kLfo1RateParam },
    { "1 Sync",   COLOR_DEFAULT, kLfoSyncOptions, kLfoSyncOptionCount, nullptr, 0 },
    { "1 Amp",    COLOR_DEFAULT, nullptr, 0, ApplyLfo1Amp, 0, &kLfo1AmpParam },
    { "1 Phase",  COLOR_DEFAULT, nullptr, 0, ApplyLfo1Phase, 0, &kLfo1PhaseParam },
    { "2 Shape",  COLOR_DEFAULT, kLfoShapeOptions, kLfoShapeOptionCount, nullptr, 1 },
    { "2 Rate",   COLOR_DEFAULT, nullptr, 0, ApplyLfo2Rate, 0, &kLfo2RateParam },
    { "2 Sync",   COLOR_DEFAULT, kLfoSyncOptions, kLfoSyncOptionCount, nullptr, 0 },
    { "2 Amp",    COLOR_DEFAULT, nullptr, 0, ApplyLfo2Amp, 0, &kLfo2AmpParam },
    { "2 Phase",  COLOR_DEFAULT, nullptr, 0, ApplyLfo2Phase, 0, &kLfo2PhaseParam },
};
*/

// 3.7 REMPLACER kMatrixSubmenu[] (lignes 399-405)
/*
static MenuNode kMatrixSubmenu[] = {
    { "Slot 1 Src", COLOR_DEFAULT, kModSrcOptions, kModSrcOptionCount, nullptr, 1 },
    { "Slot 1 Dst", COLOR_DEFAULT, kModDstOptions, kModDstOptionCount, nullptr, 1 },
    { "Slot 1 Amt", COLOR_DEFAULT, nullptr, 0, ApplyMatSlot1Amt, 0, &kMatSlot1AmtParam },
    { "Slot 2 Src", COLOR_DEFAULT, kModSrcOptions, kModSrcOptionCount, nullptr, 0 },
    { "Slot 2 Dst", COLOR_DEFAULT, kModDstOptions, kModDstOptionCount, nullptr, 0 },
    { "Slot 2 Amt", COLOR_DEFAULT, nullptr, 0, ApplyMatSlot2Amt, 0, &kMatSlot2AmtParam },
};
*/

// 3.8 REMPLACER kFxSubmenu[] (lignes 411-415)
/*
static MenuNode kFxSubmenu[] = {
    { "Effect Type", COLOR_DEFAULT, kFxTypeOptions, kFxTypeOptionCount, nullptr, 1 },
    { "Dry/Wet",     COLOR_DEFAULT, nullptr, 0, ApplyFxDryWet, 0, &kFxDryWetParam },
    { "Time/Decay",  COLOR_DEFAULT, nullptr, 0, ApplyFxTime, 0, &kFxTimeParam },
    { "FB/Tone",     COLOR_DEFAULT, nullptr, 0, ApplyFxFb, 0, &kFxFbParam },
};
*/

// 3.9 REMPLACER kMidiSubmenu[] (lignes 432-435)
/*
static MenuNode kMidiSubmenu[] = {
    { "MIDI Channel", COLOR_DEFAULT, nullptr, 0, ApplyMidiChannel, 0, &kMidiChannelParam },
    { "Clock Source", COLOR_DEFAULT, kClockSrcOptions, kClockSrcOptionCount, nullptr, 0 },
    { "Bend Range",   COLOR_DEFAULT, nullptr, 0, ApplyMidiBend, 0, &kMidiBendParam },
};
*/

// 3.10 REMPLACER kSystemSubmenu[] (lignes 443-448)
/*
static MenuNode kSystemSubmenu[] = {
    { "Luminosite",     COLOR_DEFAULT, nullptr, 0, ApplySysLuminosite, 0, &kSysLuminositeParam },
    { "Mise en veille", COLOR_DEFAULT, kSysSleepOptions, kSysSleepOptionCount, nullptr, 1 },
    { "Sensibilite",    COLOR_DEFAULT, kSysSensOptions, kSysSensOptionCount, nullptr, 1 },
    { "Sample Rate",    COLOR_DEFAULT, kSysRateOptions, kSysRateOptionCount, nullptr, 0 },
    { "Test Hardware",  COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
*/

// ============================================================================
// ÉTAPE 4 : LE SYSTÈME D'ÉDITION NUMÉRIQUE (déjà présent)
// ============================================================================
// Les lignes 513-820 du main.cpp gèrent déjà l'édition numérique :
//   - s_editing_numeric
//   - s_editing_node
//   - s_editing_original_value
//   - la logique d'encodeur et de boutons
//
// AUCUN CHANGEMENT N'EST NÉCESSAIRE DANS CES SECTIONS !

// ============================================================================
// RÉSUMÉ DES MODIFICATIONS
// ============================================================================
//
// 1. Ajouter ~38 déclarations static NumericParam (après ligne 258)
// 2. Ajouter ~35 fonctions ApplyXxx() (après ApplyWaveform vers ligne 190)
// 3. Remplacer 10 structures MenuNode[] pour ajouter les .numeric pointers
//    et les callbacks
// 4. AUCUN changement au système d'édition numérique (déjà complet)
//
// Total : ~500 lignes de code à ajouter/modifier
//
// ============================================================================
