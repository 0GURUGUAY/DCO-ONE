// ============================================================================
// TEMPLATE : Ajouter un Paramètre Numérique (Ready-to-Copy)
// ============================================================================
// Copier les sections ci-dessous dans main.cpp et adapter les valeurs.
// Les commentaires indiquent où placer chaque bloc.

// ============================================================================
// 1. DÉCLARATION (placer après la ligne ~258, après kBpmParam)
// ============================================================================

static NumericParam kMyNumericParam = {
    minValue:     10,              // valeur min
    maxValue:     500,             // valeur max
    step:         5,               // pas d'incrément
    value:        100,             // valeur initiale
    unit:         "units"          // unité (nullptr si pas d'unité)
};

// ============================================================================
// 2. AJOUTER AU MENU (trouver le MenuNode[] contenant ce paramètre et ajouter)
// ============================================================================

// Exemple 1 : comme paramètre racine dans kRootMenu
{
    label:        "My Param",
    color:        COLOR_DEFAULT,
    children:     nullptr,
    childCount:   0,
    onSelect:     nullptr,
    selectedIndex: 0,
    numeric:      &kMyNumericParam   // <-- IMPORTANT
}

// Exemple 2 : comme sous-menu (dans un sous-menu existant)
{
    label:        "Param Sub",
    color:        COLOR_BLUE,
    children:     nullptr,
    childCount:   0,
    onSelect:     nullptr,
    selectedIndex: 0,
    numeric:      &kMyNumericParam   // <-- IMPORTANT
}

// ============================================================================
// 3. UTILISER LA VALEUR (dans la boucle principale ou callbacks)
// ============================================================================

// Lecture simple
int32_t currentVal = kMyNumericParam.value;

// Exemple : application avec changement détecté
static int32_t s_prev_my_param = -1;
if (s_prev_my_param != kMyNumericParam.value) {
    s_prev_my_param = kMyNumericParam.value;
    // Faire quelque chose avec la nouvelle valeur
    MyClass::SetParameter(kMyNumericParam.value);
}

// Conversion pour use-case spécifique
float paramAsFloat = static_cast<float>(kMyNumericParam.value);
float paramMs = paramAsFloat / 1000.0f;  // si unit est ms et on veut des secondes

// ============================================================================
// 4. AJOUTER À LA TRANSMISSION (optionnel - pour afficher dans ESP32)
// ============================================================================

// Trouver SendStatus() et ajouter à PrintLine
hw.PrintLine("STAT,BPM=%ld,MY_PARAM=%ld,...",
             static_cast<long>(kBpmParam.value),
             static_cast<long>(kMyNumericParam.value),
             ...);

// Ou créer une nouvelle fonction de transmission
static void SendMyParamUpdate() {
    if (s_editing_node == kMyNumericParam) {
        hw.PrintLine("MY_PARAM=%ld MIN=%ld MAX=%ld UNIT=%s",
                     static_cast<long>(kMyNumericParam.value),
                     static_cast<long>(kMyNumericParam.minValue),
                     static_cast<long>(kMyNumericParam.maxValue),
                     kMyNumericParam.unit ? kMyNumericParam.unit : "");
    }
}

// ============================================================================
// 5. CALLBACK LIVE-APPLY (optionnel - se déclenche pendant la navigation)
// ============================================================================

// Déclarer une fonction callback
static void ApplyMyParam(int32_t index) {
    // Le paramètre 'index' n'est pas utilisé pour numeric
    // Utiliser directement kMyNumericParam.value
    float val = static_cast<float>(kMyNumericParam.value);
    MyAudioSystem::SetLiveParameter(val);
}

// Puis ajouter en tant que onSelect dans le MenuNode
{
    label:        "My Param",
    color:        COLOR_DEFAULT,
    children:     nullptr,
    childCount:   0,
    onSelect:     ApplyMyParam,  // <-- Applique en temps réel
    selectedIndex: 0,
    numeric:      &kMyNumericParam
}

// ============================================================================
// QUICK REFERENCE : Paramètres Courants
// ============================================================================

// Tempo/BPM (déjà implémenté)
static NumericParam kBpmParam = { 1, 200, 1, 120, "BPM" };

// Fréquence (Hz)
static NumericParam kFreqParam = { 20, 20000, 10, 440, "Hz" };

// Temps en millisecondes (Attack, Decay, Delay, etc.)
static NumericParam kAttackParam = { 1, 1000, 10, 50, "ms" };
static NumericParam kDecayParam = { 1, 2000, 10, 100, "ms" };
static NumericParam kDelayParam = { 0, 5000, 50, 200, "ms" };

// Volume/Gain en pourcentage
static NumericParam kVolumeParam = { 0, 100, 1, 80, "%" };

// Cutoff/Résonance (0-1 représentant 0-100%)
static NumericParam kCutoffParam = { 20, 20000, 50, 1000, "Hz" };
static NumericParam kResonanceParam = { 0, 100, 5, 50, "%" };

// Pitch/Octave
static NumericParam kPitchParam = { -24, 24, 1, 0, "st" };  // semitones

// Saturation/Distortion
static NumericParam kDriveParam = { 0, 100, 5, 30, "%" };

// LFO Rate
static NumericParam kLfoRateParam = { 1, 500, 5, 50, "Hz" };

// ============================================================================
// CHECKLIST DE MISE EN PLACE
// ============================================================================
//
// ✓ 1. Déclarer static NumericParam kMyParam = {...}
// ✓ 2. Ajouter le paramètre dans la structure MenuNode[] appropriée
// ✓ 3. Utiliser kMyParam.value dans le code pour appliquer la valeur
// ✓ 4. (Optionnel) Ajouter dans SendStatus() ou SendEditState()
// ✓ 5. (Optionnel) Créer une fonction ApplyXxx() comme callback
// ✓ 6. Tester : naviguer au menu, appuyer sur MENU_SW, tourner l'encodeur
// ✓ 7. Vérifier que HOME restaure la valeur originale
//
// ============================================================================
