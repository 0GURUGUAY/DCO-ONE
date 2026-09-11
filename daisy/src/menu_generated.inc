// Auto-generated from menu.json by tools/generate_menu.py
// Do not edit manually: regenerate instead.

// =============================================================================
// Menu content mirrored from menu.json (repo root).
// =============================================================================

// ---- PLAY: Mode Play & Auto ----

static MenuNode kPLAY_play_rootOptions[] = {
    { "C", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "C#", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "D", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "D#", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "E ", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "F ", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "F#", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "G ", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "G#", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "A", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "A#", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "B", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
static constexpr uint8_t kPLAY_play_rootOptionCount = sizeof(kPLAY_play_rootOptions) / sizeof(kPLAY_play_rootOptions[0]);


static MenuNode kPLAY_play_scaleOptions[] = {
    { "M", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "m", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "m harm", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "m melo", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Pent M", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Pent m", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Blues", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Chrom", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
static constexpr uint8_t kPLAY_play_scaleOptionCount = sizeof(kPLAY_play_scaleOptions) / sizeof(kPLAY_play_scaleOptions[0]);


static MenuNode kPLAY_play_chordsOptions[] = {
    { "Mono", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Triad", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "7 dom", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "M7", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "m7", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Sus2", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Sus4", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Dim", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Augm", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Power", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
static constexpr uint8_t kPLAY_play_chordsOptionCount = sizeof(kPLAY_play_chordsOptions) / sizeof(kPLAY_play_chordsOptions[0]);


static MenuNode kPLAY_play_poly_divOptions[] = {
    { "1/1", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "1/2", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "1/4", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "1/4T", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "1/8", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "1/8T", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "1/16", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "1/16T", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "1/32", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
static constexpr uint8_t kPLAY_play_poly_divOptionCount = sizeof(kPLAY_play_poly_divOptions) / sizeof(kPLAY_play_poly_divOptions[0]);

static NumericParam kBpmParam = { 1, 200, 1, 120, "BPM" };
static NumericParam kPlayAutoParam = { 0, 16, 1, 0, "steps" };
static NumericParam kPlayOctParam = { -24, 24, 1, 0, "st" };
static NumericParam kPlayStepParam = { 1, 32, 1, 16, "steps" };

static MenuNode kPlaySubmenu[] = {
    { "BPM", COLOR_DEFAULT, nullptr, 0, nullptr, 0, &kBpmParam, nullptr },
    { "Root", COLOR_DEFAULT, kPLAY_play_rootOptions, kPLAY_play_rootOptionCount, nullptr, 9, nullptr, nullptr },
    { "Scale", COLOR_DEFAULT, kPLAY_play_scaleOptions, kPLAY_play_scaleOptionCount, nullptr, 0, nullptr, nullptr },
    { "Chords", COLOR_DEFAULT, kPLAY_play_chordsOptions, kPLAY_play_chordsOptionCount, nullptr, 0, nullptr, nullptr },
    { "Auto", COLOR_DEFAULT, nullptr, 0, ApplyPlayAuto, 0, &kPlayAutoParam, nullptr },
    { "Oct", COLOR_DEFAULT, nullptr, 0, nullptr, 0, &kPlayOctParam, nullptr },
    { "Step", COLOR_DEFAULT, nullptr, 0, nullptr, 0, &kPlayStepParam, nullptr },
    { "Div", COLOR_DEFAULT, kPLAY_play_poly_divOptions, kPLAY_play_poly_divOptionCount, nullptr, 6, nullptr, nullptr },
};
static constexpr uint8_t kPlaySubmenuCount = sizeof(kPlaySubmenu) / sizeof(kPlaySubmenu[0]);


// ---- OSC: Oscillateur ----

static MenuNode kOSC_osc_waveOptions[] = {
    { "Saw", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Pulse", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Tri", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Sin", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "FM", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
static constexpr uint8_t kOSC_osc_waveOptionCount = sizeof(kOSC_osc_waveOptions) / sizeof(kOSC_osc_waveOptions[0]);


static MenuNode kOSC_osc_fm_modwaveOptions[] = {
    { "Sin", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Tri", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Saw", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Square", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
static constexpr uint8_t kOSC_osc_fm_modwaveOptionCount = sizeof(kOSC_osc_fm_modwaveOptions) / sizeof(kOSC_osc_fm_modwaveOptions[0]);

static NumericParam kOscFmAmtParam = { 0, 100, 1, 0, "%" };
static NumericParam kOscFmRatioParam = { 1, 32, 1, 2, "x" };
static NumericParam kOscFmRatioFineParam = { -50, 50, 1, 0, "%" };

static MenuNode kOscFmSubmenu[] = {
    { "Amount", COLOR_DEFAULT, nullptr, 0, ApplyOscFmAmt, 0, &kOscFmAmtParam, nullptr },
    { "Ratio", COLOR_DEFAULT, nullptr, 0, ApplyOscFmRatio, 0, &kOscFmRatioParam, nullptr },
    { "Ratio Fine", COLOR_DEFAULT, nullptr, 0, ApplyOscFmRatioFine, 0, &kOscFmRatioFineParam, nullptr },
    { "Mod Wave", COLOR_DEFAULT, kOSC_osc_fm_modwaveOptions, kOSC_osc_fm_modwaveOptionCount, ApplyOscFmModWave, 0, nullptr, nullptr },
};
static constexpr uint8_t kOscFmSubmenuCount = sizeof(kOscFmSubmenu) / sizeof(kOscFmSubmenu[0]);

static NumericParam kOscCoarseParam = { -24, 24, 1, 0, "st" };
static NumericParam kOscFineParam = { -100, 100, 1, 0, "cents" };
static NumericParam kOscPulseParam = { 0, 100, 1, 50, "%" };
static NumericParam kOscSubParam = { 0, 100, 1, 30, "%" };
static NumericParam kOscHardParam = { 0, 100, 1, 0, "%" };

static MenuNode kOscSubmenu[] = {
    { "Form", COLOR_DEFAULT, kOSC_osc_waveOptions, kOSC_osc_waveOptionCount, ApplyWaveform, 0, nullptr, nullptr },
    { "Coarse", COLOR_DEFAULT, nullptr, 0, ApplyOscCoarse, 0, &kOscCoarseParam, nullptr },
    { "Fine", COLOR_DEFAULT, nullptr, 0, ApplyOscFine, 0, &kOscFineParam, nullptr },
    { "Pulse", COLOR_DEFAULT, nullptr, 0, ApplyOscPulse, 0, &kOscPulseParam, nullptr },
    { "Sub-Osc", COLOR_DEFAULT, nullptr, 0, ApplyOscSub, 0, &kOscSubParam, nullptr },
    { "Hard", COLOR_DEFAULT, nullptr, 0, ApplyOscHard, 0, &kOscHardParam, nullptr },
    { "FM", COLOR_DEFAULT, kOscFmSubmenu, kOscFmSubmenuCount, nullptr, 0, nullptr, nullptr },
};
static constexpr uint8_t kOscSubmenuCount = sizeof(kOscSubmenu) / sizeof(kOscSubmenu[0]);


// ---- VCF: Filter ----

static MenuNode kVCF_vcf_typeOptions[] = {
    { "LP24", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "LP12", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "BP12", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "HP12", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "NOTCH", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
static constexpr uint8_t kVCF_vcf_typeOptionCount = sizeof(kVCF_vcf_typeOptions) / sizeof(kVCF_vcf_typeOptions[0]);

static NumericParam kVcfCutoffParam = { 20, 20000, 50, 5000, "Hz" };
static NumericParam kVcfResonanceParam = { 0, 100, 1, 30, "%" };
static NumericParam kVcfKeyParam = { 0, 100, 1, 50, "%" };
static NumericParam kVcfDriveParam = { 0, 100, 5, 100, "%" };
static NumericParam kVcfEnvParam = { -100, 100, 5, 0, "%" };

static MenuNode kVcfSubmenu[] = {
    { "Filter", COLOR_DEFAULT, kVCF_vcf_typeOptions, kVCF_vcf_typeOptionCount, ApplyVcfType, 0, nullptr, nullptr },
    { "Cutoff", COLOR_DEFAULT, nullptr, 0, ApplyVcfCutoff, 0, &kVcfCutoffParam, nullptr },
    { "Resonance", COLOR_DEFAULT, nullptr, 0, ApplyVcfResonance, 0, &kVcfResonanceParam, nullptr },
    { "Key", COLOR_DEFAULT, nullptr, 0, ApplyVcfKey, 0, &kVcfKeyParam, nullptr },
    { "Drive", COLOR_DEFAULT, nullptr, 0, ApplyVcfDrive, 0, &kVcfDriveParam, nullptr },
    { "Env", COLOR_DEFAULT, nullptr, 0, ApplyVcfEnv, 0, &kVcfEnvParam, nullptr },
};
static constexpr uint8_t kVcfSubmenuCount = sizeof(kVcfSubmenu) / sizeof(kVcfSubmenu[0]);


// ---- ENV1: Env Amp ----
static NumericParam kEnv1AttackParam = { 1, 5000, 10, 50, "ms" };
static NumericParam kEnv1DecayParam = { 1, 5000, 10, 200, "ms" };
static NumericParam kEnv1SustainParam = { 0, 100, 1, 80, "%" };
static NumericParam kEnv1ReleaseParam = { 1, 5000, 10, 300, "ms" };
static NumericParam kEnv1VelocityParam = { 0, 100, 1, 80, "%" };

static MenuNode kEnv1Submenu[] = {
    { "Attack", COLOR_DEFAULT, nullptr, 0, ApplyEnv1Attack, 0, &kEnv1AttackParam, nullptr },
    { "Decay", COLOR_DEFAULT, nullptr, 0, ApplyEnv1Decay, 0, &kEnv1DecayParam, nullptr },
    { "Sustain", COLOR_DEFAULT, nullptr, 0, ApplyEnv1Sustain, 0, &kEnv1SustainParam, nullptr },
    { "Release", COLOR_DEFAULT, nullptr, 0, ApplyEnv1Release, 0, &kEnv1ReleaseParam, nullptr },
    { "Velocity", COLOR_DEFAULT, nullptr, 0, nullptr, 0, &kEnv1VelocityParam, nullptr },
};
static constexpr uint8_t kEnv1SubmenuCount = sizeof(kEnv1Submenu) / sizeof(kEnv1Submenu[0]);


// ---- ENV2: Env Mod ----

static MenuNode kENV2_env2_loopOptions[] = {
    { "OFF", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "FREE", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "SYNC", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
static constexpr uint8_t kENV2_env2_loopOptionCount = sizeof(kENV2_env2_loopOptions) / sizeof(kENV2_env2_loopOptions[0]);

static NumericParam kEnv2AttackParam = { 1, 5000, 10, 50, "ms" };
static NumericParam kEnv2DecayParam = { 1, 5000, 10, 200, "ms" };
static NumericParam kEnv2SustainParam = { 0, 100, 1, 50, "%" };
static NumericParam kEnv2ReleaseParam = { 1, 5000, 10, 300, "ms" };

static MenuNode kEnv2Submenu[] = {
    { "Attack", COLOR_DEFAULT, nullptr, 0, ApplyEnv2Attack, 0, &kEnv2AttackParam, nullptr },
    { "Decay", COLOR_DEFAULT, nullptr, 0, ApplyEnv2Decay, 0, &kEnv2DecayParam, nullptr },
    { "Sustain", COLOR_DEFAULT, nullptr, 0, ApplyEnv2Sustain, 0, &kEnv2SustainParam, nullptr },
    { "Release", COLOR_DEFAULT, nullptr, 0, ApplyEnv2Release, 0, &kEnv2ReleaseParam, nullptr },
    { "Loop", COLOR_DEFAULT, kENV2_env2_loopOptions, kENV2_env2_loopOptionCount, nullptr, 0, nullptr, nullptr },
};
static constexpr uint8_t kEnv2SubmenuCount = sizeof(kEnv2Submenu) / sizeof(kEnv2Submenu[0]);


// ---- LFO: LFO ----

static MenuNode kLFO_lfo1_shapeOptions[] = {
    { "Sin", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Tri", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Saw", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Sq", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "S&H", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Rnd", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
static constexpr uint8_t kLFO_lfo1_shapeOptionCount = sizeof(kLFO_lfo1_shapeOptions) / sizeof(kLFO_lfo1_shapeOptions[0]);


static MenuNode kLFO_lfo1_syncOptions[] = {
    { "FREE", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "1/1", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "1/2", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "1/4", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "1/8", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "1/16", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "1/32", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
static constexpr uint8_t kLFO_lfo1_syncOptionCount = sizeof(kLFO_lfo1_syncOptions) / sizeof(kLFO_lfo1_syncOptions[0]);


static MenuNode kLFO_lfo2_shapeOptions[] = {
    { "Sin", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Tri", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Saw", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Sq", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "S&H", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Rnd", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
static constexpr uint8_t kLFO_lfo2_shapeOptionCount = sizeof(kLFO_lfo2_shapeOptions) / sizeof(kLFO_lfo2_shapeOptions[0]);


static MenuNode kLFO_lfo2_syncOptions[] = {
    { "FREE", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "1/1", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "1/2", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "1/4", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "1/8", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "1/16", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "1/32", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
static constexpr uint8_t kLFO_lfo2_syncOptionCount = sizeof(kLFO_lfo2_syncOptions) / sizeof(kLFO_lfo2_syncOptions[0]);

static NumericParam kLfo1RateParam = { 1, 500, 1, 10, "Hz" };
static NumericParam kLfo1AmpParam = { 0, 100, 1, 50, "%" };
static NumericParam kLfo1PhaseParam = { 0, 360, 5, 0, "°" };
static NumericParam kLfo2RateParam = { 1, 500, 1, 8, "Hz" };
static NumericParam kLfo2AmpParam = { 0, 100, 1, 40, "%" };
static NumericParam kLfo2PhaseParam = { 0, 360, 5, 180, "°" };

static MenuNode kLfoSubmenu[] = {
    { "1 Shape", COLOR_DEFAULT, kLFO_lfo1_shapeOptions, kLFO_lfo1_shapeOptionCount, ApplyLfo1Shape, 0, nullptr, nullptr },
    { "1 Rate", COLOR_DEFAULT, nullptr, 0, ApplyLfo1Rate, 0, &kLfo1RateParam, nullptr },
    { "1 Sync", COLOR_DEFAULT, kLFO_lfo1_syncOptions, kLFO_lfo1_syncOptionCount, ApplyLfo1Sync, 0, nullptr, nullptr },
    { "1 Amp", COLOR_DEFAULT, nullptr, 0, ApplyLfo1Amp, 0, &kLfo1AmpParam, nullptr },
    { "1 Phase", COLOR_DEFAULT, nullptr, 0, ApplyLfo1Phase, 0, &kLfo1PhaseParam, nullptr },
    { "2 Shape", COLOR_DEFAULT, kLFO_lfo2_shapeOptions, kLFO_lfo2_shapeOptionCount, ApplyLfo2Shape, 1, nullptr, nullptr },
    { "2 Rate", COLOR_DEFAULT, nullptr, 0, ApplyLfo2Rate, 0, &kLfo2RateParam, nullptr },
    { "2 Sync", COLOR_DEFAULT, kLFO_lfo2_syncOptions, kLFO_lfo2_syncOptionCount, ApplyLfo2Sync, 0, nullptr, nullptr },
    { "2 Amp", COLOR_DEFAULT, nullptr, 0, ApplyLfo2Amp, 0, &kLfo2AmpParam, nullptr },
    { "2 Phase", COLOR_DEFAULT, nullptr, 0, ApplyLfo2Phase, 0, &kLfo2PhaseParam, nullptr },
};
static constexpr uint8_t kLfoSubmenuCount = sizeof(kLfoSubmenu) / sizeof(kLfoSubmenu[0]);


// ---- MATRIX: Matrice ----

static MenuNode kMATRIX_mod1_srcOptions[] = {
    { "NONE", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "LFO1", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "LFO2", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "ENV2", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "VEL", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "MODWHEEL", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "CV1", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
static constexpr uint8_t kMATRIX_mod1_srcOptionCount = sizeof(kMATRIX_mod1_srcOptions) / sizeof(kMATRIX_mod1_srcOptions[0]);


static MenuNode kMATRIX_mod1_dstOptions[] = {
    { "NONE", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "CUTOFF", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "PITCH", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "PW", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "DRIVE", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "LFO_RATE", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
static constexpr uint8_t kMATRIX_mod1_dstOptionCount = sizeof(kMATRIX_mod1_dstOptions) / sizeof(kMATRIX_mod1_dstOptions[0]);


static MenuNode kMATRIX_mod2_srcOptions[] = {
    { "NONE", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "LFO1", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "LFO2", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "ENV2", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "VEL", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "MODWHEEL", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "CV1", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
static constexpr uint8_t kMATRIX_mod2_srcOptionCount = sizeof(kMATRIX_mod2_srcOptions) / sizeof(kMATRIX_mod2_srcOptions[0]);


static MenuNode kMATRIX_mod2_dstOptions[] = {
    { "NONE", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "CUTOFF", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "PITCH", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "PW", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "DRIVE", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "LFO_RATE", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
static constexpr uint8_t kMATRIX_mod2_dstOptionCount = sizeof(kMATRIX_mod2_dstOptions) / sizeof(kMATRIX_mod2_dstOptions[0]);

static NumericParam kMatSlot1AmtParam = { -100, 100, 5, 0, "%" };
static NumericParam kMatSlot2AmtParam = { -100, 100, 5, 0, "%" };

static MenuNode kMatrixSubmenu[] = {
    { "Slot 1 Src", COLOR_DEFAULT, kMATRIX_mod1_srcOptions, kMATRIX_mod1_srcOptionCount, nullptr, 1, nullptr, nullptr },
    { "Slot 1 Dst", COLOR_DEFAULT, kMATRIX_mod1_dstOptions, kMATRIX_mod1_dstOptionCount, nullptr, 1, nullptr, nullptr },
    { "Slot 1 Amt", COLOR_DEFAULT, nullptr, 0, ApplyMatSlot1Amt, 0, &kMatSlot1AmtParam, nullptr },
    { "Slot 2 Src", COLOR_DEFAULT, kMATRIX_mod2_srcOptions, kMATRIX_mod2_srcOptionCount, nullptr, 0, nullptr, nullptr },
    { "Slot 2 Dst", COLOR_DEFAULT, kMATRIX_mod2_dstOptions, kMATRIX_mod2_dstOptionCount, nullptr, 0, nullptr, nullptr },
    { "Slot 2 Amt", COLOR_DEFAULT, nullptr, 0, ApplyMatSlot2Amt, 0, &kMatSlot2AmtParam, nullptr },
};
static constexpr uint8_t kMatrixSubmenuCount = sizeof(kMatrixSubmenu) / sizeof(kMatrixSubmenu[0]);


// ---- FX: Effects ----

static MenuNode kFX_fx_typeOptions[] = {
    { "Off", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Reverb", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Delay", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "Chorus", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "BitCrusher", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
static constexpr uint8_t kFX_fx_typeOptionCount = sizeof(kFX_fx_typeOptions) / sizeof(kFX_fx_typeOptions[0]);

static NumericParam kFxDryWetParam = { 0, 100, 1, 50, "%" };
static NumericParam kFxTimeParam = { 10, 5000, 50, 500, "ms" };
static NumericParam kFxFbParam = { 0, 100, 1, 50, "%" };

static MenuNode kFxSubmenu[] = {
    { "Effect Type", COLOR_DEFAULT, kFX_fx_typeOptions, kFX_fx_typeOptionCount, nullptr, 1, nullptr, nullptr },
    { "Dry/Wet", COLOR_DEFAULT, nullptr, 0, ApplyFxDryWet, 0, &kFxDryWetParam, nullptr },
    { "Time/Decay", COLOR_DEFAULT, nullptr, 0, ApplyFxTime, 0, &kFxTimeParam, nullptr },
    { "FB/Tone", COLOR_DEFAULT, nullptr, 0, ApplyFxFb, 0, &kFxFbParam, nullptr },
};
static constexpr uint8_t kFxSubmenuCount = sizeof(kFxSubmenu) / sizeof(kFxSubmenu[0]);


// ---- PRESETS: Patches ----
static NumericParam kPatchLoadParam = { 1, 128, 1, 1, "" };
static NumericParam kPatchSaveParam = { 1, 128, 1, 1, "" };

static MenuNode kPresetsSubmenu[] = {
    { "Load Patch", COLOR_DEFAULT, nullptr, 0, nullptr, 0, &kPatchLoadParam, ApplyPatchLoad },
    { "Save Patch", COLOR_DEFAULT, nullptr, 0, nullptr, 0, &kPatchSaveParam, ApplyPatchSave },
    { "Init Sound", COLOR_DEFAULT, nullptr, 0, ApplyInitPatch, 0, nullptr, nullptr },
};
static constexpr uint8_t kPresetsSubmenuCount = sizeof(kPresetsSubmenu) / sizeof(kPresetsSubmenu[0]);


// ---- MIDI: MIDI ----

static MenuNode kMIDI_clock_srcOptions[] = {
    { "INTERNAL", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "MIDI USB", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "MIDI DIN", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "CV CLOCK", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
static constexpr uint8_t kMIDI_clock_srcOptionCount = sizeof(kMIDI_clock_srcOptions) / sizeof(kMIDI_clock_srcOptions[0]);

static NumericParam kMidiChannelParam = { 1, 16, 1, 1, "" };
static NumericParam kMidiBendParam = { 1, 24, 1, 2, "st" };

static MenuNode kMidiSubmenu[] = {
    { "MIDI Channel", COLOR_DEFAULT, nullptr, 0, ApplyMidiChannel, 0, &kMidiChannelParam, nullptr },
    { "Clock Source", COLOR_DEFAULT, kMIDI_clock_srcOptions, kMIDI_clock_srcOptionCount, nullptr, 0, nullptr, nullptr },
    { "Bend Range", COLOR_DEFAULT, nullptr, 0, ApplyMidiBend, 0, &kMidiBendParam, nullptr },
};
static constexpr uint8_t kMidiSubmenuCount = sizeof(kMidiSubmenu) / sizeof(kMidiSubmenu[0]);


// ---- SYSTEM: Sys ----

static MenuNode kSYSTEM_sys_sleepOptions[] = {
    { "OFF", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "1 MIN", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "5 MIN", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "15 MIN", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
static constexpr uint8_t kSYSTEM_sys_sleepOptionCount = sizeof(kSYSTEM_sys_sleepOptions) / sizeof(kSYSTEM_sys_sleepOptions[0]);


static MenuNode kSYSTEM_sys_sensOptions[] = {
    { "FAIBLE", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "MOYENNE", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "ELEVEE", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
static constexpr uint8_t kSYSTEM_sys_sensOptionCount = sizeof(kSYSTEM_sys_sensOptions) / sizeof(kSYSTEM_sys_sensOptions[0]);


static MenuNode kSYSTEM_sys_rateOptions[] = {
    { "48 kHz", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
    { "96 kHz", COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};
static constexpr uint8_t kSYSTEM_sys_rateOptionCount = sizeof(kSYSTEM_sys_rateOptions) / sizeof(kSYSTEM_sys_rateOptions[0]);

static NumericParam kSysLuminositeParam = { 10, 100, 1, 80, "%" };
static NumericParam kSysVolumeParam = { 0, 100, 1, 100, "%" };

static MenuNode kSystemSubmenu[] = {
    { "Luminosite", COLOR_DEFAULT, nullptr, 0, ApplySysLuminosite, 0, &kSysLuminositeParam, nullptr },
    { "Mise en veille", COLOR_DEFAULT, kSYSTEM_sys_sleepOptions, kSYSTEM_sys_sleepOptionCount, nullptr, 1, nullptr, nullptr },
    { "Sensibilite", COLOR_DEFAULT, kSYSTEM_sys_sensOptions, kSYSTEM_sys_sensOptionCount, nullptr, 1, nullptr, nullptr },
    { "Sample Rate", COLOR_DEFAULT, kSYSTEM_sys_rateOptions, kSYSTEM_sys_rateOptionCount, nullptr, 0, nullptr, nullptr },
    { "Volume", COLOR_DEFAULT, nullptr, 0, ApplySysVolume, 0, &kSysVolumeParam, nullptr },
    { "440Hz", COLOR_DEFAULT, nullptr, 0, ApplySys440Hz, 0, nullptr, nullptr },
    { "Test Hardware", COLOR_DEFAULT, nullptr, 0, nullptr, 0, nullptr, nullptr },
};
static constexpr uint8_t kSystemSubmenuCount = sizeof(kSystemSubmenu) / sizeof(kSystemSubmenu[0]);


// Root menu (main wheel), same order as menu.json's main_menus
static MenuNode kRootMenu[] = {
    { "PLAY", COLOR_PLAY, kPlaySubmenu, kPlaySubmenuCount, nullptr, 0, nullptr, nullptr },
    { "OSC", COLOR_OSC, kOscSubmenu, kOscSubmenuCount, nullptr, 0, nullptr, nullptr },
    { "VCF", COLOR_VCF, kVcfSubmenu, kVcfSubmenuCount, nullptr, 0, nullptr, nullptr },
    { "ENV1", COLOR_ENV1, kEnv1Submenu, kEnv1SubmenuCount, nullptr, 0, nullptr, nullptr },
    { "ENV2", COLOR_ENV2, kEnv2Submenu, kEnv2SubmenuCount, nullptr, 0, nullptr, nullptr },
    { "LFO", COLOR_LFO, kLfoSubmenu, kLfoSubmenuCount, nullptr, 0, nullptr, nullptr },
    { "MATRIX", COLOR_MATRIX, kMatrixSubmenu, kMatrixSubmenuCount, nullptr, 0, nullptr, nullptr },
    { "FX", COLOR_FX, kFxSubmenu, kFxSubmenuCount, nullptr, 0, nullptr, nullptr },
    { "PRESETS", COLOR_PRESETS, kPresetsSubmenu, kPresetsSubmenuCount, nullptr, 0, nullptr, nullptr },
    { "MIDI", COLOR_MIDI, kMidiSubmenu, kMidiSubmenuCount, nullptr, 0, nullptr, nullptr },
    { "SYSTEM", COLOR_SYSTEM, kSystemSubmenu, kSystemSubmenuCount, nullptr, 0, nullptr, nullptr },
};
static constexpr uint8_t kRootMenuCount = sizeof(kRootMenu) / sizeof(kRootMenu[0]);

static MenuNode kRootNode = { "MENU", COLOR_DEFAULT, kRootMenu, kRootMenuCount, nullptr, 0, nullptr, nullptr };
