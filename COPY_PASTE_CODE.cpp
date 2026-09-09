// ============================================================================
// CODE PRÊT À COPIER-COLLER (Copy & Paste Ready)
// ============================================================================
// Ce fichier contient les 3 blocs de code à intégrer dans main.cpp
// Chaque bloc est clairement délimité et prêt à copier dans son emplacement

// ============================================================================
// ############################################################################
// BLOC 1 : DÉCLARATION DE TOUS LES PARAMÈTRES NUMÉRIQUES
// ############################################################################
// À COPIER après la ligne 258 (après kBpmParam = { 1, 200, 1, 120, "BPM" };)
// À COLLER juste AVANT : static MenuNode kPlaySubmenu[] = {
// ============================================================================

// ---- PLAY ----
static NumericParam kBpmParam          = {   1,   200,   1,  120, "BPM" };
static NumericParam kPlayAutoParam     = {   0,    16,   1,    0, "steps" };
static NumericParam kPlayOctParam      = { -24,    24,   1,    0, "st" };
static NumericParam kPlayStepParam     = {   1,    32,   1,   16, "steps" };

// ---- OSC ----
static NumericParam kOscCoarseParam    = { -24,    24,   1,    0, "st" };
static NumericParam kOscFineParam      = { -100,   100,   1,    0, "cents" };
static NumericParam kOscPulseParam     = {   0,    100,   1,   50, "%" };
static NumericParam kOscSubParam       = {   0,    100,   1,   30, "%" };
static NumericParam kOscHardParam      = {   0,    100,   1,    0, "%" };

// ---- VCF ----
static NumericParam kVcfCutoffParam    = {  20, 20000,  50, 5000, "Hz" };
static NumericParam kVcfResonanceParam = {   0,   100,   1,   30, "%" };
static NumericParam kVcfKeyParam       = {   0,   100,   1,   50, "%" };
static NumericParam kVcfDriveParam     = {   0,   100,   5,    0, "%" };
static NumericParam kVcfEnvParam       = {-100,   100,   5,    0, "%" };

// ---- ENV1 ----
static NumericParam kEnv1AttackParam   = {   1,  5000,  10,   50, "ms" };
static NumericParam kEnv1DecayParam    = {   1,  5000,  10,  200, "ms" };
static NumericParam kEnv1SustainParam  = {   0,   100,   1,   80, "%" };
static NumericParam kEnv1ReleaseParam  = {   1,  5000,  10,  300, "ms" };
static NumericParam kEnv1VelocityParam = {   0,   100,   1,   80, "%" };

// ---- ENV2 ----
static NumericParam kEnv2AttackParam   = {   1,  5000,  10,   50, "ms" };
static NumericParam kEnv2DecayParam    = {   1,  5000,  10,  200, "ms" };
static NumericParam kEnv2SustainParam  = {   0,   100,   1,   50, "%" };
static NumericParam kEnv2ReleaseParam  = {   1,  5000,  10,  300, "ms" };

// ---- LFO ----
static NumericParam kLfo1RateParam     = {   1,   500,   1,   10, "Hz" };
static NumericParam kLfo1AmpParam      = {   0,   100,   1,   50, "%" };
static NumericParam kLfo1PhaseParam    = {   0,   360,   5,    0, "°" };
static NumericParam kLfo2RateParam     = {   1,   500,   1,    8, "Hz" };
static NumericParam kLfo2AmpParam      = {   0,   100,   1,   40, "%" };
static NumericParam kLfo2PhaseParam    = {   0,   360,   5,  180, "°" };

// ---- MATRIX ----
static NumericParam kMatSlot1AmtParam  = { -100,   100,   5,    0, "%" };
static NumericParam kMatSlot2AmtParam  = { -100,   100,   5,    0, "%" };

// ---- FX ----
static NumericParam kFxDryWetParam     = {   0,   100,   1,   50, "%" };
static NumericParam kFxTimeParam       = {  10,  5000,  50,  500, "ms" };
static NumericParam kFxFbParam         = {   0,   100,   1,   50, "%" };
static NumericParam kFxBitParam        = {   4,    16,   1,    8, "bits" };

// ---- MIDI ----
static NumericParam kMidiChannelParam  = {   1,    16,   1,    1, "ch" };
static NumericParam kMidiBendParam     = {   1,    24,   1,    2, "st" };

// ---- SYSTEM ----
static NumericParam kSysLuminositeParam = {  10,   100,   1,   80, "%" };

// ============================================================================
// ############################################################################
// BLOC 2 : TOUTES LES FONCTIONS D'APPLICATION (Apply callbacks)
// ############################################################################
// À COPIER après ApplyWaveform() (vers ligne 190)
// À COLLER juste AVANT les structures MenuNode[] (kPlaySubmenu, etc.)
// ============================================================================

static void ApplyPlayAuto(int32_t index) {
    // synth.SetArpeggiatorMode(kPlayAutoParam.value);
}

static void ApplyOscCoarse(int32_t index) {
    // synth.SetPitch(kOscCoarseParam.value + kOscFineParam.value / 100.0f);
}

static void ApplyOscFine(int32_t index) {
    // synth.SetPitch(kOscCoarseParam.value + kOscFineParam.value / 100.0f);
}

static void ApplyOscPulse(int32_t index) {
    float pw = static_cast<float>(kOscPulseParam.value) / 100.0f;
    // synth.SetPulseWidth(pw);
}

static void ApplyOscSub(int32_t index) {
    float vol = static_cast<float>(kOscSubParam.value) / 100.0f;
    // synth.SetSubOscVolume(vol);
}

static void ApplyOscHard(int32_t index) {
    float drive = static_cast<float>(kOscHardParam.value) / 100.0f;
    // synth.SetOscDrive(drive);
}

static void ApplyVcfCutoff(int32_t index) {
    float cutoff = static_cast<float>(kVcfCutoffParam.value);
    // vcf.SetCutoff(cutoff);
}

static void ApplyVcfResonance(int32_t index) {
    float res = static_cast<float>(kVcfResonanceParam.value) / 100.0f;
    // vcf.SetResonance(res);
}

static void ApplyVcfKey(int32_t index) {
    float kt = static_cast<float>(kVcfKeyParam.value) / 100.0f;
    // vcf.SetKeyboardTracking(kt);
}

static void ApplyVcfDrive(int32_t index) {
    float drive = static_cast<float>(kVcfDriveParam.value) / 100.0f;
    // vcf.SetDrive(drive);
}

static void ApplyVcfEnv(int32_t index) {
    float envAmt = static_cast<float>(kVcfEnvParam.value) / 100.0f;
    // vcf.SetEnvelopeAmount(envAmt);
}

static void ApplyEnv1Attack(int32_t index) {
    float ms = static_cast<float>(kEnv1AttackParam.value);
    // envelope1.SetAttackTime(ms / 1000.0f);
}

static void ApplyEnv1Decay(int32_t index) {
    float ms = static_cast<float>(kEnv1DecayParam.value);
    // envelope1.SetDecayTime(ms / 1000.0f);
}

static void ApplyEnv1Sustain(int32_t index) {
    float level = static_cast<float>(kEnv1SustainParam.value) / 100.0f;
    // envelope1.SetSustainLevel(level);
}

static void ApplyEnv1Release(int32_t index) {
    float ms = static_cast<float>(kEnv1ReleaseParam.value);
    // envelope1.SetReleaseTime(ms / 1000.0f);
}

static void ApplyEnv2Attack(int32_t index) {
    float ms = static_cast<float>(kEnv2AttackParam.value);
    // envelope2.SetAttackTime(ms / 1000.0f);
}

static void ApplyEnv2Decay(int32_t index) {
    float ms = static_cast<float>(kEnv2DecayParam.value);
    // envelope2.SetDecayTime(ms / 1000.0f);
}

static void ApplyEnv2Sustain(int32_t index) {
    float level = static_cast<float>(kEnv2SustainParam.value) / 100.0f;
    // envelope2.SetSustainLevel(level);
}

static void ApplyEnv2Release(int32_t index) {
    float ms = static_cast<float>(kEnv2ReleaseParam.value);
    // envelope2.SetReleaseTime(ms / 1000.0f);
}

static void ApplyLfo1Rate(int32_t index) {
    float hz = static_cast<float>(kLfo1RateParam.value);
    // lfo1.SetFrequency(hz);
}

static void ApplyLfo1Amp(int32_t index) {
    float amp = static_cast<float>(kLfo1AmpParam.value) / 100.0f;
    // lfo1.SetAmplitude(amp);
}

static void ApplyLfo1Phase(int32_t index) {
    float phase = static_cast<float>(kLfo1PhaseParam.value) / 360.0f;
    // lfo1.SetPhase(phase);
}

static void ApplyLfo2Rate(int32_t index) {
    float hz = static_cast<float>(kLfo2RateParam.value);
    // lfo2.SetFrequency(hz);
}

static void ApplyLfo2Amp(int32_t index) {
    float amp = static_cast<float>(kLfo2AmpParam.value) / 100.0f;
    // lfo2.SetAmplitude(amp);
}

static void ApplyLfo2Phase(int32_t index) {
    float phase = static_cast<float>(kLfo2PhaseParam.value) / 360.0f;
    // lfo2.SetPhase(phase);
}

static void ApplyMatSlot1Amt(int32_t index) {
    float amt = static_cast<float>(kMatSlot1AmtParam.value) / 100.0f;
    // matrix.SetSlotAmount(1, amt);
}

static void ApplyMatSlot2Amt(int32_t index) {
    float amt = static_cast<float>(kMatSlot2AmtParam.value) / 100.0f;
    // matrix.SetSlotAmount(2, amt);
}

static void ApplyFxDryWet(int32_t index) {
    float mix = static_cast<float>(kFxDryWetParam.value) / 100.0f;
    // fx.SetDryWet(mix);
}

static void ApplyFxTime(int32_t index) {
    float ms = static_cast<float>(kFxTimeParam.value);
    // fx.SetTime(ms / 1000.0f);
}

static void ApplyFxFb(int32_t index) {
    float fb = static_cast<float>(kFxFbParam.value) / 100.0f;
    // fx.SetFeedback(fb);
}

static void ApplyMidiChannel(int32_t index) {
    int32_t ch = kMidiChannelParam.value;
    // midi.SetChannel(ch - 1);
}

static void ApplyMidiBend(int32_t index) {
    int32_t range = kMidiBendParam.value;
    // midi.SetBendRange(range);
}

static void ApplySysLuminosite(int32_t index) {
    float brightness = static_cast<float>(kSysLuminositeParam.value) / 100.0f;
    // display.SetBrightness(brightness);
}

// ============================================================================
// ############################################################################
// BLOC 3 : STRUCTURES MENUNODE MISES À JOUR
// ############################################################################
// À COPIER/REMPLACER les 10 structures MenuNode[] existantes
// ============================================================================

// ---- kPlaySubmenu[] ----
// REMPLACER entièrement (lignes ~273-281) par :
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
static constexpr uint8_t kPlaySubmenuCount = sizeof(kPlaySubmenu) / sizeof(kPlaySubmenu[0]);

// ---- kOscSubmenu[] ----
// REMPLACER entièrement (lignes ~290-296) par :
static MenuNode kOscSubmenu[] = {
    { "Form",     COLOR_DEFAULT, kOscWaveOptions, kOscWaveOptionCount, ApplyWaveform, 0 },
    { "Coarse",   COLOR_DEFAULT, nullptr, 0, ApplyOscCoarse, 0, &kOscCoarseParam },
    { "Fine",     COLOR_DEFAULT, nullptr, 0, ApplyOscFine, 0, &kOscFineParam },
    { "Pulse",    COLOR_DEFAULT, nullptr, 0, ApplyOscPulse, 0, &kOscPulseParam },
    { "Sub-Osc",  COLOR_DEFAULT, nullptr, 0, ApplyOscSub, 0, &kOscSubParam },
    { "Hard",     COLOR_DEFAULT, nullptr, 0, ApplyOscHard, 0, &kOscHardParam },
};
static constexpr uint8_t kOscSubmenuCount = sizeof(kOscSubmenu) / sizeof(kOscSubmenu[0]);

// ---- kVcfSubmenu[] ----
// REMPLACER entièrement (lignes ~305-312) par :
static MenuNode kVcfSubmenu[] = {
    { "Filter",    COLOR_DEFAULT, kVcfTypeOptions, kVcfTypeOptionCount, nullptr, 0 },
    { "Cutoff",    COLOR_DEFAULT, nullptr, 0, ApplyVcfCutoff, 0, &kVcfCutoffParam },
    { "Resonance", COLOR_DEFAULT, nullptr, 0, ApplyVcfResonance, 0, &kVcfResonanceParam },
    { "Key",       COLOR_DEFAULT, nullptr, 0, ApplyVcfKey, 0, &kVcfKeyParam },
    { "Drive",     COLOR_DEFAULT, nullptr, 0, ApplyVcfDrive, 0, &kVcfDriveParam },
    { "Env",       COLOR_DEFAULT, nullptr, 0, ApplyVcfEnv, 0, &kVcfEnvParam },
};
static constexpr uint8_t kVcfSubmenuCount = sizeof(kVcfSubmenu) / sizeof(kVcfSubmenu[0]);

// ---- kEnv1Submenu[] ----
// REMPLACER entièrement (lignes ~320-325) par :
static MenuNode kEnv1Submenu[] = {
    { "Attack",    COLOR_DEFAULT, nullptr, 0, ApplyEnv1Attack, 0, &kEnv1AttackParam },
    { "Decay",     COLOR_DEFAULT, nullptr, 0, ApplyEnv1Decay, 0, &kEnv1DecayParam },
    { "Sustain",   COLOR_DEFAULT, nullptr, 0, ApplyEnv1Sustain, 0, &kEnv1SustainParam },
    { "Release",   COLOR_DEFAULT, nullptr, 0, ApplyEnv1Release, 0, &kEnv1ReleaseParam },
    { "Velocity",  COLOR_DEFAULT, nullptr, 0, nullptr, 0, &kEnv1VelocityParam },
};
static constexpr uint8_t kEnv1SubmenuCount = sizeof(kEnv1Submenu) / sizeof(kEnv1Submenu[0]);

// ---- kEnv2Submenu[] ----
// REMPLACER entièrement (lignes ~333-338) par :
static MenuNode kEnv2Submenu[] = {
    { "Attack",   COLOR_DEFAULT, nullptr, 0, ApplyEnv2Attack, 0, &kEnv2AttackParam },
    { "Decay",    COLOR_DEFAULT, nullptr, 0, ApplyEnv2Decay, 0, &kEnv2DecayParam },
    { "Sustain",  COLOR_DEFAULT, nullptr, 0, ApplyEnv2Sustain, 0, &kEnv2SustainParam },
    { "Release",  COLOR_DEFAULT, nullptr, 0, ApplyEnv2Release, 0, &kEnv2ReleaseParam },
    { "Loop",     COLOR_DEFAULT, kEnv2LoopOptions, kEnv2LoopOptionCount, nullptr, 0 },
};
static constexpr uint8_t kEnv2SubmenuCount = sizeof(kEnv2Submenu) / sizeof(kEnv2Submenu[0]);

// ---- kLfoSubmenu[] ----
// REMPLACER entièrement (lignes ~347-358) par :
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
static constexpr uint8_t kLfoSubmenuCount = sizeof(kLfoSubmenu) / sizeof(kLfoSubmenu[0]);

// ---- kMatrixSubmenu[] ----
// REMPLACER entièrement (lignes ~399-405) par :
static MenuNode kMatrixSubmenu[] = {
    { "Slot 1 Src", COLOR_DEFAULT, kModSrcOptions, kModSrcOptionCount, nullptr, 1 },
    { "Slot 1 Dst", COLOR_DEFAULT, kModDstOptions, kModDstOptionCount, nullptr, 1 },
    { "Slot 1 Amt", COLOR_DEFAULT, nullptr, 0, ApplyMatSlot1Amt, 0, &kMatSlot1AmtParam },
    { "Slot 2 Src", COLOR_DEFAULT, kModSrcOptions, kModSrcOptionCount, nullptr, 0 },
    { "Slot 2 Dst", COLOR_DEFAULT, kModDstOptions, kModDstOptionCount, nullptr, 0 },
    { "Slot 2 Amt", COLOR_DEFAULT, nullptr, 0, ApplyMatSlot2Amt, 0, &kMatSlot2AmtParam },
};
static constexpr uint8_t kMatrixSubmenuCount = sizeof(kMatrixSubmenu) / sizeof(kMatrixSubmenu[0]);

// ---- kFxSubmenu[] ----
// REMPLACER entièrement (lignes ~411-415) par :
static MenuNode kFxSubmenu[] = {
    { "Effect Type", COLOR_DEFAULT, kFxTypeOptions, kFxTypeOptionCount, nullptr, 1 },
    { "Dry/Wet",     COLOR_DEFAULT, nullptr, 0, ApplyFxDryWet, 0, &kFxDryWetParam },
    { "Time/Decay",  COLOR_DEFAULT, nullptr, 0, ApplyFxTime, 0, &kFxTimeParam },
    { "FB/Tone",     COLOR_DEFAULT, nullptr, 0, ApplyFxFb, 0, &kFxFbParam },
};
static constexpr uint8_t kFxSubmenuCount = sizeof(kFxSubmenu) / sizeof(kFxSubmenu[0]);

// ---- kMidiSubmenu[] ----
// REMPLACER entièrement (lignes ~432-435) par :
static MenuNode kMidiSubmenu[] = {
    { "MIDI Channel", COLOR_DEFAULT, nullptr, 0, ApplyMidiChannel, 0, &kMidiChannelParam },
    { "Clock Source", COLOR_DEFAULT, kClockSrcOptions, kClockSrcOptionCount, nullptr, 0 },
    { "Bend Range",   COLOR_DEFAULT, nullptr, 0, ApplyMidiBend, 0, &kMidiBendParam },
};
static constexpr uint8_t kMidiSubmenuCount = sizeof(kMidiSubmenu) / sizeof(kMidiSubmenu[0]);

// ---- kSystemSubmenu[] ----
// REMPLACER entièrement (lignes ~443-448) par :
static MenuNode kSystemSubmenu[] = {
    { "Luminosite",     COLOR_DEFAULT, nullptr, 0, ApplySysLuminosite, 0, &kSysLuminositeParam },
    { "Mise en veille", COLOR_DEFAULT, kSysSleepOptions, kSysSleepOptionCount, nullptr, 1 },
    { "Sensibilite",    COLOR_DEFAULT, kSysSensOptions, kSysSensOptionCount, nullptr, 1 },
    { "Sample Rate",    COLOR_DEFAULT, kSysRateOptions, kSysRateOptionCount, nullptr, 0 },
    { "Test Hardware",  COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
static constexpr uint8_t kSystemSubmenuCount = sizeof(kSystemSubmenu) / sizeof(kSystemSubmenu[0]);

// ============================================================================
// ############################################################################
// RÉSUMÉ
// ############################################################################
//
// Bloc 1 : ~38 lignes (déclarations NumericParam)
// Bloc 2 : ~160 lignes (fonctions Apply)
// Bloc 3 : ~110 lignes (structures MenuNode mises à jour)
//
// TOTAL : ~310 lignes de code
//
// Le reste du main.cpp (notamment la boucle de gestion de l'édition numérique
// aux lignes 513-820) reste INCHANGÉ.
//
// ============================================================================
