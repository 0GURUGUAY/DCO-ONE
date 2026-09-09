// ============================================================================
// COMPLETE NUMERIC INPUTS IMPLEMENTATION
// Tous les inputs numériques du synthétiseur DCO-ONE
// À copier-coller dans daisy/src/main.cpp
// ============================================================================

// ============================================================================
// 1. DÉCLARATIONS DE TOUS LES PARAMÈTRES NUMÉRIQUES
// ============================================================================
// À ajouter après la ligne ~258 (après kBpmParam = { ... })

// ---- PLAY ----
static NumericParam kBpmParam          = {   1,   200,   1,  120, "BPM" };
static NumericParam kPlayAutoParam     = {   0,    16,   1,    0, "steps" };  // arrépégiateur
static NumericParam kPlayOctParam      = { -24,    24,   1,    0, "st" };    // décalage octave
static NumericParam kPlayStepParam     = {   1,    32,   1,   16, "steps" }; // nombre de steps arpégiateur

// ---- OSC: Oscillateur ----
static NumericParam kOscCoarseParam    = { -24,    24,   1,    0, "st" };    // pitch coarse
static NumericParam kOscFineParam      = { -100,   100,   1,    0, "cents" }; // pitch fin
static NumericParam kOscPulseParam     = {   0,    100,   1,   50, "%" };     // pulse width
static NumericParam kOscSubParam       = {   0,    100,   1,   30, "%" };     // sub-oscillateur volume
static NumericParam kOscHardParam      = {   0,    100,   1,    0, "%" };     // saturation

// ---- VCF: Filtre Audio ----
static NumericParam kVcfCutoffParam    = {  20, 20000,  50, 5000, "Hz" };     // fréquence cutoff
static NumericParam kVcfResonanceParam = {   0,   100,   1,   30, "%" };      // résonance/Q
static NumericParam kVcfKeyParam       = {   0,   100,   1,   50, "%" };      // keyboard tracking
static NumericParam kVcfDriveParam     = {   0,   100,   5,    0, "%" };      // drive/saturation
static NumericParam kVcfEnvParam       = {-100,   100,   5,    0, "%" };      // envelope amount (bipolaire)

// ---- ENV1: Enveloppe Amplitude ----
static NumericParam kEnv1AttackParam   = {   1,  5000,  10,   50, "ms" };     // attaque
static NumericParam kEnv1DecayParam    = {   1,  5000,  10,  200, "ms" };     // déclin
static NumericParam kEnv1SustainParam  = {   0,   100,   1,   80, "%" };      // sustain
static NumericParam kEnv1ReleaseParam  = {   1,  5000,  10,  300, "ms" };     // release
static NumericParam kEnv1VelocityParam = {   0,   100,   1,   80, "%" };      // velocity sensitivity

// ---- ENV2: Enveloppe Modulation ----
static NumericParam kEnv2AttackParam   = {   1,  5000,  10,   50, "ms" };
static NumericParam kEnv2DecayParam    = {   1,  5000,  10,  200, "ms" };
static NumericParam kEnv2SustainParam  = {   0,   100,   1,   50, "%" };
static NumericParam kEnv2ReleaseParam  = {   1,  5000,  10,  300, "ms" };

// ---- LFO: Oscillateurs Basse Fréquence ----
static NumericParam kLfo1RateParam     = {   1,   500,   1,   10, "Hz" };     // LFO1 fréquence
static NumericParam kLfo1AmpParam      = {   0,   100,   1,   50, "%" };      // LFO1 amplitude
static NumericParam kLfo1PhaseParam    = {   0,   360,   5,    0, "°" };      // LFO1 phase offset
static NumericParam kLfo2RateParam     = {   1,   500,   1,    8, "Hz" };     // LFO2 fréquence
static NumericParam kLfo2AmpParam      = {   0,   100,   1,   40, "%" };      // LFO2 amplitude
static NumericParam kLfo2PhaseParam    = {   0,   360,   5,  180, "°" };      // LFO2 phase offset

// ---- MATRIX: Matrice de Modulation ----
static NumericParam kMatSlot1AmtParam  = { -100,   100,   5,    0, "%" };     // Slot 1 amount (bipolaire)
static NumericParam kMatSlot2AmtParam  = { -100,   100,   5,    0, "%" };     // Slot 2 amount (bipolaire)

// ---- FX: Effets ----
static NumericParam kFxDryWetParam     = {   0,   100,   1,   50, "%" };      // dry/wet mix
static NumericParam kFxTimeParam       = {  10,  5000,  50,  500, "ms" };     // time/decay
static NumericParam kFxFbParam         = {   0,   100,   1,   50, "%" };      // feedback/tone
static NumericParam kFxBitParam        = {   4,    16,   1,    8, "bits" };   // bit depth (BitCrusher)

// ---- MIDI ----
static NumericParam kMidiChannelParam  = {   1,    16,   1,    1, "ch" };     // canal MIDI
static NumericParam kMidiBendParam     = {   1,    24,   1,    2, "st" };     // pitch bend range

// ---- SYSTEM ----
static NumericParam kSysLuminositeParam = {  10,   100,   1,   80, "%" };    // luminosité écran

// ============================================================================
// 2. CALLBACKS D'APPLICATION (live-apply)
// ============================================================================
// À ajouter après ApplyWaveform() (vers ligne ~190)

static void ApplyPlayAuto(int32_t index) {
    // Appliquer l'arpégiateur
    // synth.SetArpeggiatorMode(kPlayAutoParam.value);
}

static void ApplyOscCoarse(int32_t index) {
    // Ajuster le pitch de base (en semitones)
    // synth.SetPitch(kOscCoarseParam.value + kOscFineParam.value / 100.0f);
}

static void ApplyOscFine(int32_t index) {
    // Ajuster le pitch fin
    // synth.SetPitch(kOscCoarseParam.value + kOscFineParam.value / 100.0f);
}

static void ApplyOscPulse(int32_t index) {
    // Régler la largeur d'impulsion (0-100%)
    float pw = static_cast<float>(kOscPulseParam.value) / 100.0f;
    // synth.SetPulseWidth(pw);
}

static void ApplyOscSub(int32_t index) {
    // Volume du sous-oscillateur
    float vol = static_cast<float>(kOscSubParam.value) / 100.0f;
    // synth.SetSubOscVolume(vol);
}

static void ApplyOscHard(int32_t index) {
    // Saturation/distortion de l'oscillateur
    float drive = static_cast<float>(kOscHardParam.value) / 100.0f;
    // synth.SetOscDrive(drive);
}

static void ApplyVcfCutoff(int32_t index) {
    // Fréquence de coupure du filtre
    float cutoff = static_cast<float>(kVcfCutoffParam.value);
    // vcf.SetCutoff(cutoff);
}

static void ApplyVcfResonance(int32_t index) {
    // Résonance/Q du filtre
    float res = static_cast<float>(kVcfResonanceParam.value) / 100.0f;
    // vcf.SetResonance(res);
}

static void ApplyVcfKey(int32_t index) {
    // Keyboard tracking
    float kt = static_cast<float>(kVcfKeyParam.value) / 100.0f;
    // vcf.SetKeyboardTracking(kt);
}

static void ApplyVcfDrive(int32_t index) {
    // Drive du filtre
    float drive = static_cast<float>(kVcfDriveParam.value) / 100.0f;
    // vcf.SetDrive(drive);
}

static void ApplyVcfEnv(int32_t index) {
    // Modulation par enveloppe
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
    // midi.SetChannel(ch - 1);  // 0-indexed
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
// 3. INTÉGRATION DANS LES MENUS (MenuNode arrays)
// ============================================================================
// Remplacer les sections existantes par celles-ci :

// ---- PLAY: Paramètres de jeu ----
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

// ---- OSC: Oscillateur ----
static MenuNode kOscSubmenu[] = {
    { "Form",     COLOR_DEFAULT, kOscWaveOptions, kOscWaveOptionCount, ApplyWaveform, 0 },
    { "Coarse",   COLOR_DEFAULT, nullptr, 0, ApplyOscCoarse, 0, &kOscCoarseParam },
    { "Fine",     COLOR_DEFAULT, nullptr, 0, ApplyOscFine, 0, &kOscFineParam },
    { "Pulse",    COLOR_DEFAULT, nullptr, 0, ApplyOscPulse, 0, &kOscPulseParam },
    { "Sub-Osc",  COLOR_DEFAULT, nullptr, 0, ApplyOscSub, 0, &kOscSubParam },
    { "Hard",     COLOR_DEFAULT, nullptr, 0, ApplyOscHard, 0, &kOscHardParam },
};

// ---- VCF: Filtre Audio ----
static MenuNode kVcfSubmenu[] = {
    { "Filter",    COLOR_DEFAULT, kVcfTypeOptions, kVcfTypeOptionCount, nullptr, 0 },
    { "Cutoff",    COLOR_DEFAULT, nullptr, 0, ApplyVcfCutoff, 0, &kVcfCutoffParam },
    { "Resonance", COLOR_DEFAULT, nullptr, 0, ApplyVcfResonance, 0, &kVcfResonanceParam },
    { "Key",       COLOR_DEFAULT, nullptr, 0, ApplyVcfKey, 0, &kVcfKeyParam },
    { "Drive",     COLOR_DEFAULT, nullptr, 0, ApplyVcfDrive, 0, &kVcfDriveParam },
    { "Env",       COLOR_DEFAULT, nullptr, 0, ApplyVcfEnv, 0, &kVcfEnvParam },
};

// ---- ENV1: Enveloppe Amplitude ----
static MenuNode kEnv1Submenu[] = {
    { "Attack",    COLOR_DEFAULT, nullptr, 0, ApplyEnv1Attack, 0, &kEnv1AttackParam },
    { "Decay",     COLOR_DEFAULT, nullptr, 0, ApplyEnv1Decay, 0, &kEnv1DecayParam },
    { "Sustain",   COLOR_DEFAULT, nullptr, 0, ApplyEnv1Sustain, 0, &kEnv1SustainParam },
    { "Release",   COLOR_DEFAULT, nullptr, 0, ApplyEnv1Release, 0, &kEnv1ReleaseParam },
    { "Velocity",  COLOR_DEFAULT, nullptr, 0, nullptr, 0, &kEnv1VelocityParam },
};

// ---- ENV2: Enveloppe Modulation ----
static MenuNode kEnv2Submenu[] = {
    { "Attack",   COLOR_DEFAULT, nullptr, 0, ApplyEnv2Attack, 0, &kEnv2AttackParam },
    { "Decay",    COLOR_DEFAULT, nullptr, 0, ApplyEnv2Decay, 0, &kEnv2DecayParam },
    { "Sustain",  COLOR_DEFAULT, nullptr, 0, ApplyEnv2Sustain, 0, &kEnv2SustainParam },
    { "Release",  COLOR_DEFAULT, nullptr, 0, ApplyEnv2Release, 0, &kEnv2ReleaseParam },
    { "Loop",     COLOR_DEFAULT, kEnv2LoopOptions, kEnv2LoopOptionCount, nullptr, 0 },
};

// ---- LFO: Oscillateurs Basse Fréquence ----
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

// ---- MATRIX: Matrice de Modulation ----
static MenuNode kMatrixSubmenu[] = {
    { "Slot 1 Src", COLOR_DEFAULT, kModSrcOptions, kModSrcOptionCount, nullptr, 1 },
    { "Slot 1 Dst", COLOR_DEFAULT, kModDstOptions, kModDstOptionCount, nullptr, 1 },
    { "Slot 1 Amt", COLOR_DEFAULT, nullptr, 0, ApplyMatSlot1Amt, 0, &kMatSlot1AmtParam },
    { "Slot 2 Src", COLOR_DEFAULT, kModSrcOptions, kModSrcOptionCount, nullptr, 0 },
    { "Slot 2 Dst", COLOR_DEFAULT, kModDstOptions, kModDstOptionCount, nullptr, 0 },
    { "Slot 2 Amt", COLOR_DEFAULT, nullptr, 0, ApplyMatSlot2Amt, 0, &kMatSlot2AmtParam },
};

// ---- FX: Effets ----
static MenuNode kFxSubmenu[] = {
    { "Effect Type", COLOR_DEFAULT, kFxTypeOptions, kFxTypeOptionCount, nullptr, 1 },
    { "Dry/Wet",     COLOR_DEFAULT, nullptr, 0, ApplyFxDryWet, 0, &kFxDryWetParam },
    { "Time/Decay",  COLOR_DEFAULT, nullptr, 0, ApplyFxTime, 0, &kFxTimeParam },
    { "FB/Tone",     COLOR_DEFAULT, nullptr, 0, ApplyFxFb, 0, &kFxFbParam },
};

// ---- MIDI ----
static MenuNode kMidiSubmenu[] = {
    { "MIDI Channel", COLOR_DEFAULT, nullptr, 0, ApplyMidiChannel, 0, &kMidiChannelParam },
    { "Clock Source", COLOR_DEFAULT, kClockSrcOptions, kClockSrcOptionCount, nullptr, 0 },
    { "Bend Range",   COLOR_DEFAULT, nullptr, 0, ApplyMidiBend, 0, &kMidiBendParam },
};

// ---- SYSTEM ----
static MenuNode kSystemSubmenu[] = {
    { "Luminosite",     COLOR_DEFAULT, nullptr, 0, ApplySysLuminosite, 0, &kSysLuminositeParam },
    { "Mise en veille", COLOR_DEFAULT, kSysSleepOptions, kSysSleepOptionCount, nullptr, 1 },
    { "Sensibilite",    COLOR_DEFAULT, kSysSensOptions, kSysSensOptionCount, nullptr, 1 },
    { "Sample Rate",    COLOR_DEFAULT, kSysRateOptions, kSysRateOptionCount, nullptr, 0 },
    { "Test Hardware",  COLOR_DEFAULT, nullptr, 0, nullptr, 0 },
};

// ============================================================================
// 4. EXEMPLE D'UTILISATION DANS LA BOUCLE PRINCIPALE
// ============================================================================
// À ajouter dans la section principale loop (void main())

    // Mettre à jour les paramètres live
    static uint32_t last_param_update = 0;
    if (now - last_param_update >= 50)  // mise à jour tous les 50ms
    {
        last_param_update = now;
        
        // Les callbacks ApplyXxx() ont déjà été appelés via le système de menu
        // Mais si on veut forcer une mise à jour :
        // ApplyOscCoarse(0);
        // ApplyVcfCutoff(0);
        // ApplyEnv1Attack(0);
        // etc.
    }

// ============================================================================
// 5. ENVOI AU ESP32 (OPTIONAL)
// ============================================================================
// À ajouter ou modifier dans SendStatus()

    // Au lieu de :
    // hw.PrintLine("STAT,BPM=%ld,ROOT=%ld,SCALE=%ld,PLAY=%d",
    //              static_cast<long>(kBpmParam.value), ...);
    
    // Faire :
    hw.PrintLine("STAT,BPM=%ld,CUTOFF=%ld,ATTACK=%ld,LFO1=%ld",
                 static_cast<long>(kBpmParam.value),
                 static_cast<long>(kVcfCutoffParam.value),
                 static_cast<long>(kEnv1AttackParam.value),
                 static_cast<long>(kLfo1RateParam.value));

// ============================================================================
// RÉSUMÉ DES PARAMÈTRES PAR CATÉGORIE
// ============================================================================
//
// PLAY (4 params) :
//   - kBpmParam : BPM du séquenceur
//   - kPlayAutoParam : mode arpégiateur
//   - kPlayOctParam : décalage octave
//   - kPlayStepParam : nombre de steps
//
// OSC (5 params) :
//   - kOscCoarseParam : pitch coarse en semitones
//   - kOscFineParam : pitch fin en cents
//   - kOscPulseParam : pulse width %
//   - kOscSubParam : sub-osc volume %
//   - kOscHardParam : saturation %
//
// VCF (5 params) :
//   - kVcfCutoffParam : cutoff frequency Hz
//   - kVcfResonanceParam : resonance %
//   - kVcfKeyParam : keyboard tracking %
//   - kVcfDriveParam : drive %
//   - kVcfEnvParam : envelope modulation amount % (bipolaire)
//
// ENV1 (5 params) :
//   - kEnv1AttackParam : attaque ms
//   - kEnv1DecayParam : déclin ms
//   - kEnv1SustainParam : sustain %
//   - kEnv1ReleaseParam : release ms
//   - kEnv1VelocityParam : velocity sensitivity %
//
// ENV2 (4 params) :
//   - kEnv2AttackParam : attaque ms
//   - kEnv2DecayParam : déclin ms
//   - kEnv2SustainParam : sustain %
//   - kEnv2ReleaseParam : release ms
//
// LFO (6 params) :
//   - kLfo1RateParam : LFO1 frequency Hz
//   - kLfo1AmpParam : LFO1 amplitude %
//   - kLfo1PhaseParam : LFO1 phase °
//   - kLfo2RateParam : LFO2 frequency Hz
//   - kLfo2AmpParam : LFO2 amplitude %
//   - kLfo2PhaseParam : LFO2 phase °
//
// MATRIX (2 params) :
//   - kMatSlot1AmtParam : slot 1 amount % (bipolaire)
//   - kMatSlot2AmtParam : slot 2 amount % (bipolaire)
//
// FX (4 params) :
//   - kFxDryWetParam : dry/wet mix %
//   - kFxTimeParam : time/decay ms
//   - kFxFbParam : feedback %
//   - kFxBitParam : bit depth (BitCrusher)
//
// MIDI (2 params) :
//   - kMidiChannelParam : canal MIDI 1-16
//   - kMidiBendParam : pitch bend range semitones
//
// SYSTEM (1 param) :
//   - kSysLuminositeParam : luminosité écran %
//
// TOTAL : 38 paramètres numériques
//
// ============================================================================
