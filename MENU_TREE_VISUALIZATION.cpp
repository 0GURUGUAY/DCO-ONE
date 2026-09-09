// ============================================================================
// VISUALISATION DE L'ARBORESCENCE COMPLÈTE DU MENU
// ============================================================================
// Cet arbre montre tous les 38 paramètres numériques et leur localisation

DCO-ONE SYNTHESIZER MENU TREE
├── PLAY (4 params)
│   ├── BPM                    ✎ [1..200] step:1, "BPM"
│   ├── Root
│   ├── Scale
│   ├── Chords
│   ├── Auto                   ✎ [0..16] step:1, "steps"        (NEW)
│   ├── Oct                    ✎ [-24..24] step:1, "st"          (NEW)
│   ├── Step                   ✎ [1..32] step:1, "steps"         (NEW)
│   └── Div
│
├── OSC (5 params)
│   ├── Form
│   ├── Coarse                 ✎ [-24..24] step:1, "st"          (NEW)
│   ├── Fine                   ✎ [-100..100] step:1, "cents"     (NEW)
│   ├── Pulse                  ✎ [0..100] step:1, "%"            (NEW)
│   ├── Sub-Osc                ✎ [0..100] step:1, "%"            (NEW)
│   └── Hard                   ✎ [0..100] step:1, "%"            (NEW)
│
├── VCF (5 params)
│   ├── Filter
│   ├── Cutoff                 ✎ [20..20000] step:50, "Hz"       (NEW)
│   ├── Resonance              ✎ [0..100] step:1, "%"            (NEW)
│   ├── Key                    ✎ [0..100] step:1, "%"            (NEW)
│   ├── Drive                  ✎ [0..100] step:5, "%"            (NEW)
│   └── Env                    ✎ [-100..100] step:5, "%"         (NEW) [BIPOLAIRE]
│
├── ENV1 (5 params)
│   ├── Attack                 ✎ [1..5000] step:10, "ms"         (NEW)
│   ├── Decay                  ✎ [1..5000] step:10, "ms"         (NEW)
│   ├── Sustain                ✎ [0..100] step:1, "%"            (NEW)
│   ├── Release                ✎ [1..5000] step:10, "ms"         (NEW)
│   └── Velocity               ✎ [0..100] step:1, "%"            (NEW)
│
├── ENV2 (4 params)
│   ├── Attack                 ✎ [1..5000] step:10, "ms"         (NEW)
│   ├── Decay                  ✎ [1..5000] step:10, "ms"         (NEW)
│   ├── Sustain                ✎ [0..100] step:1, "%"            (NEW)
│   ├── Release                ✎ [1..5000] step:10, "ms"         (NEW)
│   └── Loop
│
├── LFO (6 params)
│   ├── 1 Shape
│   ├── 1 Rate                 ✎ [1..500] step:1, "Hz"           (NEW)
│   ├── 1 Sync
│   ├── 1 Amp                  ✎ [0..100] step:1, "%"            (NEW)
│   ├── 1 Phase                ✎ [0..360] step:5, "°"            (NEW)
│   ├── 2 Shape
│   ├── 2 Rate                 ✎ [1..500] step:1, "Hz"           (NEW)
│   ├── 2 Sync
│   ├── 2 Amp                  ✎ [0..100] step:1, "%"            (NEW)
│   └── 2 Phase                ✎ [0..360] step:5, "°"            (NEW)
│
├── MATRIX (2 params)
│   ├── Slot 1 Src
│   ├── Slot 1 Dst
│   ├── Slot 1 Amt             ✎ [-100..100] step:5, "%"         (NEW) [BIPOLAIRE]
│   ├── Slot 2 Src
│   ├── Slot 2 Dst
│   └── Slot 2 Amt             ✎ [-100..100] step:5, "%"         (NEW) [BIPOLAIRE]
│
├── FX (4 params)
│   ├── Effect Type
│   ├── Dry/Wet                ✎ [0..100] step:1, "%"            (NEW)
│   ├── Time/Decay             ✎ [10..5000] step:50, "ms"        (NEW)
│   └── FB/Tone                ✎ [0..100] step:1, "%"            (NEW)
│
├── PRESETS
│   ├── Load Patch
│   ├── Save Patch
│   ├── Bank Select
│   └── Init Sound
│
├── MIDI (2 params)
│   ├── MIDI Channel           ✎ [1..16] step:1, "ch"            (NEW)
│   ├── Clock Source
│   └── Bend Range             ✎ [1..24] step:1, "st"            (NEW)
│
└── SYSTEM (1 param)
    ├── Luminosite             ✎ [10..100] step:1, "%"           (NEW)
    ├── Mise en veille
    ├── Sensibilite
    ├── Sample Rate
    └── Test Hardware

Legend :
  ✎ = éditable (NumericParam)
  [.....] = plage [min..max]
  step:N = incrément par cran d'encodeur
  NEW = ajouté dans cette implémentation

// ============================================================================
// ORGANISATION DES PARAMÈTRES PAR TYPE
// ============================================================================

Paramètres de TEMPS (en millisecondes)
├── kEnv1AttackParam      [1..5000] ms, pas 10
├── kEnv1DecayParam       [1..5000] ms, pas 10
├── kEnv1ReleaseParam     [1..5000] ms, pas 10
├── kEnv2AttackParam      [1..5000] ms, pas 10
├── kEnv2DecayParam       [1..5000] ms, pas 10
├── kEnv2ReleaseParam     [1..5000] ms, pas 10
├── kFxTimeParam          [10..5000] ms, pas 50
└── (Note: Convertir en secondes avec / 1000.0f)

Paramètres de FRÉQUENCE (Hz)
├── kVcfCutoffParam       [20..20000] Hz, pas 50
├── kLfo1RateParam        [1..500] Hz, pas 1
├── kLfo2RateParam        [1..500] Hz, pas 1
└── (Note: Multiplicateur du BPM possible)

Paramètres de PITCH (semitones)
├── kPlayOctParam         [-24..24] st (octaves)
├── kOscCoarseParam       [-24..24] st (coarse pitch)
├── kMidiBendParam        [1..24] st (pitch bend range)
└── (Note: Convertir en Hertz ou ratio si nécessaire)

Paramètres de POURCENTAGE (0-100%)
├── kOscPulseParam        [0..100] %
├── kOscSubParam          [0..100] %
├── kOscHardParam         [0..100] %
├── kVcfResonanceParam    [0..100] %
├── kVcfKeyParam          [0..100] %
├── kVcfDriveParam        [0..100] %
├── kEnv1SustainParam     [0..100] %
├── kEnv1VelocityParam    [0..100] %
├── kEnv2SustainParam     [0..100] %
├── kLfo1AmpParam         [0..100] %
├── kLfo2AmpParam         [0..100] %
├── kFxDryWetParam        [0..100] %
├── kFxFbParam            [0..100] %
├── kSysLuminositeParam   [10..100] %
└── (Note: Convertir en [0.0..1.0] avec / 100.0f)

Paramètres BIPOLAIRES (-100%..+100%)
├── kVcfEnvParam          [-100..100] % (envelope modulation amount)
├── kMatSlot1AmtParam     [-100..100] % (modulation amount slot 1)
└── kMatSlot2AmtParam     [-100..100] % (modulation amount slot 2)
     (Note: Convertir en [-1.0..1.0] avec / 100.0f)

Paramètres FINS (cents ou degrés)
├── kOscFineParam         [-100..100] cents (pitch fin)
└── kLfo1PhaseParam       [0..360] ° (phase offset LFO1)
└── kLfo2PhaseParam       [0..360] ° (phase offset LFO2)
     (Note: Convertir cents en ratio ou phase en [0..1])

Paramètres ÉNUMÉRATIFS (values discrètes)
├── kPlayAutoParam        [0..16] steps (arpégiateur)
├── kPlayStepParam        [1..32] steps (nombre de steps)
├── kMidiChannelParam     [1..16] ch (numéro de canal MIDI)
├── kFxBitParam           [4..16] bits (résolution BitCrusher)
└── kBpmParam             [1..200] BPM (tempo)

// ============================================================================
// CORRESPONDANCE CALLBACKS ↔ PARAMÈTRES
// ============================================================================

ApplyPlayAuto()       →  kPlayAutoParam
ApplyOscCoarse()      →  kOscCoarseParam
ApplyOscFine()        →  kOscFineParam
ApplyOscPulse()       →  kOscPulseParam
ApplyOscSub()         →  kOscSubParam
ApplyOscHard()        →  kOscHardParam
ApplyVcfCutoff()      →  kVcfCutoffParam
ApplyVcfResonance()   →  kVcfResonanceParam
ApplyVcfKey()         →  kVcfKeyParam
ApplyVcfDrive()       →  kVcfDriveParam
ApplyVcfEnv()         →  kVcfEnvParam
ApplyEnv1Attack()     →  kEnv1AttackParam
ApplyEnv1Decay()      →  kEnv1DecayParam
ApplyEnv1Sustain()    →  kEnv1SustainParam
ApplyEnv1Release()    →  kEnv1ReleaseParam
ApplyEnv2Attack()     →  kEnv2AttackParam
ApplyEnv2Decay()      →  kEnv2DecayParam
ApplyEnv2Sustain()    →  kEnv2SustainParam
ApplyEnv2Release()    →  kEnv2ReleaseParam
ApplyLfo1Rate()       →  kLfo1RateParam
ApplyLfo1Amp()        →  kLfo1AmpParam
ApplyLfo1Phase()      →  kLfo1PhaseParam
ApplyLfo2Rate()       →  kLfo2RateParam
ApplyLfo2Amp()        →  kLfo2AmpParam
ApplyLfo2Phase()      →  kLfo2PhaseParam
ApplyMatSlot1Amt()    →  kMatSlot1AmtParam
ApplyMatSlot2Amt()    →  kMatSlot2AmtParam
ApplyFxDryWet()       →  kFxDryWetParam
ApplyFxTime()         →  kFxTimeParam
ApplyFxFb()           →  kFxFbParam
ApplyMidiChannel()    →  kMidiChannelParam
ApplyMidiBend()       →  kMidiBendParam
ApplySysLuminosite()  →  kSysLuminositeParam

// ============================================================================
// EXEMPLE DE FLUX D'ÉDITION UTILISATEUR
// ============================================================================

Utilisateur navigue au menu VCF → Cutoff :

  1. ESP32 affiche : "VCF [>Cutoff< | Resonance | Key | Drive | Env]"
  
  2. Utilisateur appuie sur MENU_SW (bouton de l'encodeur MENU)
     → Le système détecte que "Cutoff" est un NumericParam
     → s_editing_numeric = true
     → s_editing_node = &kVcfSubmenu[1]  // pointeur sur "Cutoff"
     → s_editing_original_value = 5000   // sauvegarde la valeur courante
     → SendEditState() envoie "EDIT,VAL=5000,MIN=20,MAX=20000,UNIT=Hz" au ESP32
  
  3. ESP32 affiche : "CUTOFF [5000 Hz] ▼ ▲" (mode édition)
  
  4. Utilisateur tourne l'encodeur MENU dans le sens +
     → encoder_menu_accumulated_delta += 1
     → kVcfCutoffParam.value += 1 * 50 = 5050
     → SendEditState() envoie "EDIT,VAL=5050,MIN=20,MAX=20000,UNIT=Hz"
     → Callback ApplyVcfCutoff(0) est appelé
     → Le filtre effectue : vcf.SetCutoff(5050.0f)
  
  5. Utilisateur continue à tourner l'encodeur (10 crans)
     → kVcfCutoffParam.value = 5500
     → Mis à jour en temps réel toutes les 100ms
  
  6. Utilisateur appuie sur MENU_SW pour confirmer
     → s_editing_numeric = false
     → La valeur 5500 est conservée
     → SendMenuPath() envoie "NAV,P=2.1" au ESP32
     → Menu revient à l'affichage normal
  
  OU
  
  6. Utilisateur appuie sur HOME pour annuler
     → kVcfCutoffParam.value = 5000  // restaure s_editing_original_value
     → ApplyVcfCutoff(0) est appelé avec la valeur restaurée
     → s_editing_numeric = false
     → Menu revient à l'affichage normal

// ============================================================================
// TRANSMISSION VERS ESP32 (USB Serial)
// ============================================================================

Format pendant l'édition :
  EDIT,VAL=<value>,MIN=<min>,MAX=<max>,UNIT=<unit>
  
Exemple :
  EDIT,VAL=5000,MIN=20,MAX=20000,UNIT=Hz
  EDIT,VAL=50,MIN=1,MAX=5000,UNIT=ms
  EDIT,VAL=120,MIN=1,MAX=200,UNIT=BPM

Format après la navigation :
  NAV,P=<path>
  
Exemple :
  NAV,P=2              (à la racine, sélectionnée : VCF)
  NAV,P=2.1            (dans VCF, sélectionnée : Cutoff)
  NAV,P=2.1            (en édition Cutoff)

Format du status périodique (500ms) :
  STAT,BPM=120,ROOT=9,SCALE=0,PLAY=1
  (Peut être étendu avec d'autres paramètres)

// ============================================================================
// COMPARAISON AVANT / APRÈS
// ============================================================================

AVANT (phase 1) :
- Seulement le BPM était éditable
- Le reste du menu était des placeholders
- Aucune valeur numérique n'était saisie

APRÈS (avec ce code) :
- 38 paramètres numériques éditables
- Chaque paramètre a des limites min/max et un pas d'incrémentation
- Édition temps réel avec feedback immédiat
- Annulation via HOME
- Confirmation via MENU_SW
- Transmission au ESP32 pour affichage
- Toute la structure du synthétiseur est interactive

// ============================================================================
