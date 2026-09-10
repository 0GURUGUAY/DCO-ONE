// Auto-generated from menu.json by tools/generate_menu.py
// Do not edit manually: regenerate instead.

// =============================================================================
// Menu content mirrored from menu.json (repo root).
// =============================================================================

// ---- PLAY: Mode Play & Auto ----

static const MenuNode kPLAY_play_rootOptions[] = {
    { "C", 0, nullptr, 0 },
    { "C#", 0, nullptr, 0 },
    { "D", 0, nullptr, 0 },
    { "D#", 0, nullptr, 0 },
    { "E ", 0, nullptr, 0 },
    { "F ", 0, nullptr, 0 },
    { "F#", 0, nullptr, 0 },
    { "G ", 0, nullptr, 0 },
    { "G#", 0, nullptr, 0 },
    { "A", 0, nullptr, 0 },
    { "A#", 0, nullptr, 0 },
    { "B", 0, nullptr, 0 },
};
static const int kPLAY_play_rootOptionCount = sizeof(kPLAY_play_rootOptions) / sizeof(kPLAY_play_rootOptions[0]);


static const MenuNode kPLAY_play_scaleOptions[] = {
    { "M", 0, nullptr, 0 },
    { "m", 0, nullptr, 0 },
    { "m harm", 0, nullptr, 0 },
    { "m melo", 0, nullptr, 0 },
    { "Pent M", 0, nullptr, 0 },
    { "Pent m", 0, nullptr, 0 },
    { "Blues", 0, nullptr, 0 },
    { "Chrom", 0, nullptr, 0 },
};
static const int kPLAY_play_scaleOptionCount = sizeof(kPLAY_play_scaleOptions) / sizeof(kPLAY_play_scaleOptions[0]);


static const MenuNode kPLAY_play_chordsOptions[] = {
    { "Mono", 0, nullptr, 0 },
    { "Triad", 0, nullptr, 0 },
    { "7 dom", 0, nullptr, 0 },
    { "M7", 0, nullptr, 0 },
    { "m7", 0, nullptr, 0 },
    { "Sus2", 0, nullptr, 0 },
    { "Sus4", 0, nullptr, 0 },
    { "Dim", 0, nullptr, 0 },
    { "Augm", 0, nullptr, 0 },
    { "Power", 0, nullptr, 0 },
};
static const int kPLAY_play_chordsOptionCount = sizeof(kPLAY_play_chordsOptions) / sizeof(kPLAY_play_chordsOptions[0]);


static const MenuNode kPLAY_play_poly_divOptions[] = {
    { "1/1", 0, nullptr, 0 },
    { "1/2", 0, nullptr, 0 },
    { "1/4", 0, nullptr, 0 },
    { "1/4T", 0, nullptr, 0 },
    { "1/8", 0, nullptr, 0 },
    { "1/8T", 0, nullptr, 0 },
    { "1/16", 0, nullptr, 0 },
    { "1/16T", 0, nullptr, 0 },
    { "1/32", 0, nullptr, 0 },
};
static const int kPLAY_play_poly_divOptionCount = sizeof(kPLAY_play_poly_divOptions) / sizeof(kPLAY_play_poly_divOptions[0]);


static const MenuNode kPlaySubmenu[] = {
    { "BPM", 0, nullptr, 0 },
    { "Root", 0, kPLAY_play_rootOptions, kPLAY_play_rootOptionCount },
    { "Scale", 0, kPLAY_play_scaleOptions, kPLAY_play_scaleOptionCount },
    { "Chords", 0, kPLAY_play_chordsOptions, kPLAY_play_chordsOptionCount },
    { "Auto", 0, nullptr, 0 },
    { "Oct", 0, nullptr, 0 },
    { "Step", 0, nullptr, 0 },
    { "Div", 0, kPLAY_play_poly_divOptions, kPLAY_play_poly_divOptionCount },
};
static const int kPlaySubmenuCount = sizeof(kPlaySubmenu) / sizeof(kPlaySubmenu[0]);


// ---- OSC: Oscillateur ----

static const MenuNode kOSC_osc_waveOptions[] = {
    { "Saw", 0, nullptr, 0 },
    { "Pulse", 0, nullptr, 0 },
    { "Tri", 0, nullptr, 0 },
    { "Sin", 0, nullptr, 0 },
    { "FM", 0, nullptr, 0 },
};
static const int kOSC_osc_waveOptionCount = sizeof(kOSC_osc_waveOptions) / sizeof(kOSC_osc_waveOptions[0]);


static const MenuNode kOSC_osc_fm_modwaveOptions[] = {
    { "Sin", 0, nullptr, 0 },
    { "Tri", 0, nullptr, 0 },
    { "Saw", 0, nullptr, 0 },
    { "Square", 0, nullptr, 0 },
};
static const int kOSC_osc_fm_modwaveOptionCount = sizeof(kOSC_osc_fm_modwaveOptions) / sizeof(kOSC_osc_fm_modwaveOptions[0]);


static const MenuNode kOscFmSubmenu[] = {
    { "Amount", 0, nullptr, 0 },
    { "Ratio", 0, nullptr, 0 },
    { "Ratio Fine", 0, nullptr, 0 },
    { "Mod Wave", 0, kOSC_osc_fm_modwaveOptions, kOSC_osc_fm_modwaveOptionCount },
};
static const int kOscFmSubmenuCount = sizeof(kOscFmSubmenu) / sizeof(kOscFmSubmenu[0]);


static const MenuNode kOscSubmenu[] = {
    { "Form", 0, kOSC_osc_waveOptions, kOSC_osc_waveOptionCount },
    { "Coarse", 0, nullptr, 0 },
    { "Fine", 0, nullptr, 0 },
    { "Pulse", 0, nullptr, 0 },
    { "Sub-Osc", 0, nullptr, 0 },
    { "Hard", 0, nullptr, 0 },
    { "FM", 0, kOscFmSubmenu, kOscFmSubmenuCount },
};
static const int kOscSubmenuCount = sizeof(kOscSubmenu) / sizeof(kOscSubmenu[0]);


// ---- VCF: Filter ----

static const MenuNode kVCF_vcf_typeOptions[] = {
    { "LP24", 0, nullptr, 0 },
    { "LP12", 0, nullptr, 0 },
    { "BP12", 0, nullptr, 0 },
    { "HP12", 0, nullptr, 0 },
    { "NOTCH", 0, nullptr, 0 },
};
static const int kVCF_vcf_typeOptionCount = sizeof(kVCF_vcf_typeOptions) / sizeof(kVCF_vcf_typeOptions[0]);


static const MenuNode kVcfSubmenu[] = {
    { "Filter", 0, kVCF_vcf_typeOptions, kVCF_vcf_typeOptionCount },
    { "Cutoff", 0, nullptr, 0 },
    { "Resonance", 0, nullptr, 0 },
    { "Key", 0, nullptr, 0 },
    { "Drive", 0, nullptr, 0 },
    { "Env", 0, nullptr, 0 },
};
static const int kVcfSubmenuCount = sizeof(kVcfSubmenu) / sizeof(kVcfSubmenu[0]);


// ---- ENV1: Env Amp ----

static const MenuNode kEnv1Submenu[] = {
    { "Attack", 0, nullptr, 0 },
    { "Decay", 0, nullptr, 0 },
    { "Sustain", 0, nullptr, 0 },
    { "Release", 0, nullptr, 0 },
    { "Velocity", 0, nullptr, 0 },
};
static const int kEnv1SubmenuCount = sizeof(kEnv1Submenu) / sizeof(kEnv1Submenu[0]);


// ---- ENV2: Env Mod ----

static const MenuNode kENV2_env2_loopOptions[] = {
    { "OFF", 0, nullptr, 0 },
    { "FREE", 0, nullptr, 0 },
    { "SYNC", 0, nullptr, 0 },
};
static const int kENV2_env2_loopOptionCount = sizeof(kENV2_env2_loopOptions) / sizeof(kENV2_env2_loopOptions[0]);


static const MenuNode kEnv2Submenu[] = {
    { "Attack", 0, nullptr, 0 },
    { "Decay", 0, nullptr, 0 },
    { "Sustain", 0, nullptr, 0 },
    { "Release", 0, nullptr, 0 },
    { "Loop", 0, kENV2_env2_loopOptions, kENV2_env2_loopOptionCount },
};
static const int kEnv2SubmenuCount = sizeof(kEnv2Submenu) / sizeof(kEnv2Submenu[0]);


// ---- LFO: LFO ----

static const MenuNode kLFO_lfo1_shapeOptions[] = {
    { "Sin", 0, nullptr, 0 },
    { "Tri", 0, nullptr, 0 },
    { "Saw", 0, nullptr, 0 },
    { "Sq", 0, nullptr, 0 },
    { "S&H", 0, nullptr, 0 },
    { "Rnd", 0, nullptr, 0 },
};
static const int kLFO_lfo1_shapeOptionCount = sizeof(kLFO_lfo1_shapeOptions) / sizeof(kLFO_lfo1_shapeOptions[0]);


static const MenuNode kLFO_lfo1_syncOptions[] = {
    { "FREE", 0, nullptr, 0 },
    { "1/1", 0, nullptr, 0 },
    { "1/2", 0, nullptr, 0 },
    { "1/4", 0, nullptr, 0 },
    { "1/8", 0, nullptr, 0 },
    { "1/16", 0, nullptr, 0 },
    { "1/32", 0, nullptr, 0 },
};
static const int kLFO_lfo1_syncOptionCount = sizeof(kLFO_lfo1_syncOptions) / sizeof(kLFO_lfo1_syncOptions[0]);


static const MenuNode kLFO_lfo2_shapeOptions[] = {
    { "Sin", 0, nullptr, 0 },
    { "Tri", 0, nullptr, 0 },
    { "Saw", 0, nullptr, 0 },
    { "Sq", 0, nullptr, 0 },
    { "S&H", 0, nullptr, 0 },
    { "Rnd", 0, nullptr, 0 },
};
static const int kLFO_lfo2_shapeOptionCount = sizeof(kLFO_lfo2_shapeOptions) / sizeof(kLFO_lfo2_shapeOptions[0]);


static const MenuNode kLFO_lfo2_syncOptions[] = {
    { "FREE", 0, nullptr, 0 },
    { "1/1", 0, nullptr, 0 },
    { "1/2", 0, nullptr, 0 },
    { "1/4", 0, nullptr, 0 },
    { "1/8", 0, nullptr, 0 },
    { "1/16", 0, nullptr, 0 },
    { "1/32", 0, nullptr, 0 },
};
static const int kLFO_lfo2_syncOptionCount = sizeof(kLFO_lfo2_syncOptions) / sizeof(kLFO_lfo2_syncOptions[0]);


static const MenuNode kLfoSubmenu[] = {
    { "1 Shape", 0, kLFO_lfo1_shapeOptions, kLFO_lfo1_shapeOptionCount },
    { "1 Rate", 0, nullptr, 0 },
    { "1 Sync", 0, kLFO_lfo1_syncOptions, kLFO_lfo1_syncOptionCount },
    { "1 Amp", 0, nullptr, 0 },
    { "1 Phase", 0, nullptr, 0 },
    { "2 Shape", 0, kLFO_lfo2_shapeOptions, kLFO_lfo2_shapeOptionCount },
    { "2 Rate", 0, nullptr, 0 },
    { "2 Sync", 0, kLFO_lfo2_syncOptions, kLFO_lfo2_syncOptionCount },
    { "2 Amp", 0, nullptr, 0 },
    { "2 Phase", 0, nullptr, 0 },
};
static const int kLfoSubmenuCount = sizeof(kLfoSubmenu) / sizeof(kLfoSubmenu[0]);


// ---- MATRIX: Matrice ----

static const MenuNode kMATRIX_mod1_srcOptions[] = {
    { "NONE", 0, nullptr, 0 },
    { "LFO1", 0, nullptr, 0 },
    { "LFO2", 0, nullptr, 0 },
    { "ENV2", 0, nullptr, 0 },
    { "VEL", 0, nullptr, 0 },
    { "MODWHEEL", 0, nullptr, 0 },
    { "CV1", 0, nullptr, 0 },
};
static const int kMATRIX_mod1_srcOptionCount = sizeof(kMATRIX_mod1_srcOptions) / sizeof(kMATRIX_mod1_srcOptions[0]);


static const MenuNode kMATRIX_mod1_dstOptions[] = {
    { "NONE", 0, nullptr, 0 },
    { "CUTOFF", 0, nullptr, 0 },
    { "PITCH", 0, nullptr, 0 },
    { "PW", 0, nullptr, 0 },
    { "DRIVE", 0, nullptr, 0 },
    { "LFO_RATE", 0, nullptr, 0 },
};
static const int kMATRIX_mod1_dstOptionCount = sizeof(kMATRIX_mod1_dstOptions) / sizeof(kMATRIX_mod1_dstOptions[0]);


static const MenuNode kMATRIX_mod2_srcOptions[] = {
    { "NONE", 0, nullptr, 0 },
    { "LFO1", 0, nullptr, 0 },
    { "LFO2", 0, nullptr, 0 },
    { "ENV2", 0, nullptr, 0 },
    { "VEL", 0, nullptr, 0 },
    { "MODWHEEL", 0, nullptr, 0 },
    { "CV1", 0, nullptr, 0 },
};
static const int kMATRIX_mod2_srcOptionCount = sizeof(kMATRIX_mod2_srcOptions) / sizeof(kMATRIX_mod2_srcOptions[0]);


static const MenuNode kMATRIX_mod2_dstOptions[] = {
    { "NONE", 0, nullptr, 0 },
    { "CUTOFF", 0, nullptr, 0 },
    { "PITCH", 0, nullptr, 0 },
    { "PW", 0, nullptr, 0 },
    { "DRIVE", 0, nullptr, 0 },
    { "LFO_RATE", 0, nullptr, 0 },
};
static const int kMATRIX_mod2_dstOptionCount = sizeof(kMATRIX_mod2_dstOptions) / sizeof(kMATRIX_mod2_dstOptions[0]);


static const MenuNode kMatrixSubmenu[] = {
    { "Slot 1 Src", 0, kMATRIX_mod1_srcOptions, kMATRIX_mod1_srcOptionCount },
    { "Slot 1 Dst", 0, kMATRIX_mod1_dstOptions, kMATRIX_mod1_dstOptionCount },
    { "Slot 1 Amt", 0, nullptr, 0 },
    { "Slot 2 Src", 0, kMATRIX_mod2_srcOptions, kMATRIX_mod2_srcOptionCount },
    { "Slot 2 Dst", 0, kMATRIX_mod2_dstOptions, kMATRIX_mod2_dstOptionCount },
    { "Slot 2 Amt", 0, nullptr, 0 },
};
static const int kMatrixSubmenuCount = sizeof(kMatrixSubmenu) / sizeof(kMatrixSubmenu[0]);


// ---- FX: Effects ----

static const MenuNode kFX_fx_typeOptions[] = {
    { "Off", 0, nullptr, 0 },
    { "Reverb", 0, nullptr, 0 },
    { "Delay", 0, nullptr, 0 },
    { "Chorus", 0, nullptr, 0 },
    { "BitCrusher", 0, nullptr, 0 },
};
static const int kFX_fx_typeOptionCount = sizeof(kFX_fx_typeOptions) / sizeof(kFX_fx_typeOptions[0]);


static const MenuNode kFxSubmenu[] = {
    { "Effect Type", 0, kFX_fx_typeOptions, kFX_fx_typeOptionCount },
    { "Dry/Wet", 0, nullptr, 0 },
    { "Time/Decay", 0, nullptr, 0 },
    { "FB/Tone", 0, nullptr, 0 },
};
static const int kFxSubmenuCount = sizeof(kFxSubmenu) / sizeof(kFxSubmenu[0]);


// ---- PRESETS: Patches ----

static const MenuNode kPRESETS_patch_bankOptions[] = {
    { "BANK A", 0, nullptr, 0 },
    { "BANK B", 0, nullptr, 0 },
    { "BANK C", 0, nullptr, 0 },
    { "USER", 0, nullptr, 0 },
};
static const int kPRESETS_patch_bankOptionCount = sizeof(kPRESETS_patch_bankOptions) / sizeof(kPRESETS_patch_bankOptions[0]);


static const MenuNode kPresetsSubmenu[] = {
    { "Load Patch", 0, nullptr, 0 },
    { "Save Patch", 0, nullptr, 0 },
    { "Bank Select", 0, kPRESETS_patch_bankOptions, kPRESETS_patch_bankOptionCount },
    { "Init Sound", 0, nullptr, 0 },
};
static const int kPresetsSubmenuCount = sizeof(kPresetsSubmenu) / sizeof(kPresetsSubmenu[0]);


// ---- MIDI: MIDI ----

static const MenuNode kMIDI_clock_srcOptions[] = {
    { "INTERNAL", 0, nullptr, 0 },
    { "MIDI USB", 0, nullptr, 0 },
    { "MIDI DIN", 0, nullptr, 0 },
    { "CV CLOCK", 0, nullptr, 0 },
};
static const int kMIDI_clock_srcOptionCount = sizeof(kMIDI_clock_srcOptions) / sizeof(kMIDI_clock_srcOptions[0]);


static const MenuNode kMidiSubmenu[] = {
    { "MIDI Channel", 0, nullptr, 0 },
    { "Clock Source", 0, kMIDI_clock_srcOptions, kMIDI_clock_srcOptionCount },
    { "Bend Range", 0, nullptr, 0 },
};
static const int kMidiSubmenuCount = sizeof(kMidiSubmenu) / sizeof(kMidiSubmenu[0]);


// ---- SYSTEM: Sys ----

static const MenuNode kSYSTEM_sys_sleepOptions[] = {
    { "OFF", 0, nullptr, 0 },
    { "1 MIN", 0, nullptr, 0 },
    { "5 MIN", 0, nullptr, 0 },
    { "15 MIN", 0, nullptr, 0 },
};
static const int kSYSTEM_sys_sleepOptionCount = sizeof(kSYSTEM_sys_sleepOptions) / sizeof(kSYSTEM_sys_sleepOptions[0]);


static const MenuNode kSYSTEM_sys_sensOptions[] = {
    { "FAIBLE", 0, nullptr, 0 },
    { "MOYENNE", 0, nullptr, 0 },
    { "ELEVEE", 0, nullptr, 0 },
};
static const int kSYSTEM_sys_sensOptionCount = sizeof(kSYSTEM_sys_sensOptions) / sizeof(kSYSTEM_sys_sensOptions[0]);


static const MenuNode kSYSTEM_sys_rateOptions[] = {
    { "48 kHz", 0, nullptr, 0 },
    { "96 kHz", 0, nullptr, 0 },
};
static const int kSYSTEM_sys_rateOptionCount = sizeof(kSYSTEM_sys_rateOptions) / sizeof(kSYSTEM_sys_rateOptions[0]);


static const MenuNode kSystemSubmenu[] = {
    { "Luminosite", 0, nullptr, 0 },
    { "Mise en veille", 0, kSYSTEM_sys_sleepOptions, kSYSTEM_sys_sleepOptionCount },
    { "Sensibilite", 0, kSYSTEM_sys_sensOptions, kSYSTEM_sys_sensOptionCount },
    { "Sample Rate", 0, kSYSTEM_sys_rateOptions, kSYSTEM_sys_rateOptionCount },
    { "Volume", 0, nullptr, 0 },
    { "440Hz", 0, nullptr, 0 },
    { "Test Hardware", 0, nullptr, 0 },
};
static const int kSystemSubmenuCount = sizeof(kSystemSubmenu) / sizeof(kSystemSubmenu[0]);


// Root menu (main wheel), same order as menu.json's main_menus
static const MenuNode kRootMenu[] = {
    { "PLAY", C_PALE_PLAY, kPlaySubmenu, kPlaySubmenuCount },
    { "OSC", C_PALE_OSC, kOscSubmenu, kOscSubmenuCount },
    { "VCF", C_PALE_VCF, kVcfSubmenu, kVcfSubmenuCount },
    { "ENV1", C_PALE_ENV1, kEnv1Submenu, kEnv1SubmenuCount },
    { "ENV2", C_PALE_ENV2, kEnv2Submenu, kEnv2SubmenuCount },
    { "LFO", C_PALE_LFO, kLfoSubmenu, kLfoSubmenuCount },
    { "MATRIX", C_PALE_MATRIX, kMatrixSubmenu, kMatrixSubmenuCount },
    { "FX", C_PALE_FX, kFxSubmenu, kFxSubmenuCount },
    { "PRESETS", C_PALE_PRESETS, kPresetsSubmenu, kPresetsSubmenuCount },
    { "MIDI", C_PALE_MIDI, kMidiSubmenu, kMidiSubmenuCount },
    { "SYSTEM", C_PALE_SYSTEM, kSystemSubmenu, kSystemSubmenuCount },
};
static const int kRootMenuCount = sizeof(kRootMenu) / sizeof(kRootMenu[0]);

static const MenuNode kRootNode = { "MENU", 0, kRootMenu, kRootMenuCount };
