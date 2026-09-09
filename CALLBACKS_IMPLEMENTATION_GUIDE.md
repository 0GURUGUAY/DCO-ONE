# 🔧 IMPLÉMENTATION DES CALLBACKS - GUIDE D'ÉTAPE 3

## Qui ? Quoi ? Comment ?

Le firmware a été compilé avec succès et tous les paramètres numériques sont maintenant éditables via le menu. Cependant, les callbacks sont actuellement des **stubs vides** (fonctions sans contenu).

Pour que chaque paramètre agisse vraiment sur le synthétiseur, il faut ajouter le code réel dans chaque callback.

---

## 📍 Où sont les callbacks ?

Fichier : `daisy/src/main.cpp`
Lignes : ~190-225 (après la fonction `ApplyWaveform()`)

### Exemple d'un callback actuel

```cpp
static void ApplyVcfCutoff(int32_t index) {
}  // ← VIDE - À IMPLÉMENTER
```

---

## 🎯 Comment implémenter un callback

### Format général

```cpp
static void ApplyXxxParameter(int32_t index) {
    // 1. Lire la valeur du paramètre
    int32_t raw_value = kXxxParam.value;
    
    // 2. Convertir en unité appropriée pour le synthétiseur
    float converted_value = ConvertValue(raw_value);
    
    // 3. Appliquer au synthétiseur
    synth.SetParameter(converted_value);
    
    // 4. Optionnel : Envoyer une mise à jour au ESP32
    SendParameterUpdate("Xxx", converted_value);
}
```

---

## 📋 Liste des 35 callbacks à implémenter

### PLAY (4 callbacks)

#### 1. ApplyPlayAuto
```cpp
static void ApplyPlayAuto(int32_t index) {
    int32_t steps = kPlayAutoParam.value;
    // sequencer.SetAutoplaySteps(steps);
}
```
- **Paramètre** : `kPlayAutoParam` (0-16 steps)
- **Action** : Configure le nombre d'étapes pour l'autoplay du séquenceur

#### 2. ApplyOscCoarse (dans le menu PLAY ? NON - c'est dans OSC)
*À placer dans OSC*

#### 3. ApplyPlayOct
```cpp
static void ApplyPlayOct(int32_t index) {
    int32_t octave = kPlayOctParam.value;  // -24 à 24 demi-tons
    // sequencer.SetTranspose(octave);
}
```
- **Paramètre** : `kPlayOctParam` (-24 à 24 demi-tons)
- **Action** : Transpose le séquenceur par octave

#### 4. ApplyPlayStep
```cpp
static void ApplyPlayStep(int32_t index) {
    int32_t steps = kPlayStepParam.value;  // 1-32
    // sequencer.SetLength(steps);
}
```
- **Paramètre** : `kPlayStepParam` (1-32 steps)
- **Action** : Définit la longueur du séquenceur

---

### OSC (5 callbacks)

#### 5. ApplyOscCoarse
```cpp
static void ApplyOscCoarse(int32_t index) {
    int32_t semitones = kOscCoarseParam.value;  // -24 à 24
    float pitch = static_cast<float>(semitones);
    // oscillator.SetCoarseTune(pitch);
}
```
- **Paramètre** : `kOscCoarseParam` (-24 à 24 demi-tons)
- **Action** : Accordage grossier de l'oscillateur principal

#### 6. ApplyOscFine
```cpp
static void ApplyOscFine(int32_t index) {
    int32_t cents = kOscFineParam.value;  // -100 à 100
    float pitch = static_cast<float>(cents) / 100.0f;  // Convertir en demi-tons
    // oscillator.SetFineTune(pitch);
}
```
- **Paramètre** : `kOscFineParam` (-100 à 100 cents)
- **Action** : Accordage fin de l'oscillateur

#### 7. ApplyOscPulse
```cpp
static void ApplyOscPulse(int32_t index) {
    int32_t percent = kOscPulseParam.value;  // 0-100%
    float duty = static_cast<float>(percent) / 100.0f;
    // oscillator.SetPulseWidth(duty);
}
```
- **Paramètre** : `kOscPulseParam` (0-100 %)
- **Action** : Largueur d'impulsion (PWM) de l'oscillateur

#### 8. ApplyOscSub
```cpp
static void ApplyOscSub(int32_t index) {
    int32_t percent = kOscSubParam.value;  // 0-100%
    float level = static_cast<float>(percent) / 100.0f;
    // subosc.SetLevel(level);
}
```
- **Paramètre** : `kOscSubParam` (0-100 %)
- **Action** : Niveau du sous-oscillateur

#### 9. ApplyOscHard
```cpp
static void ApplyOscHard(int32_t index) {
    int32_t percent = kOscHardParam.value;  // 0-100%
    float hardness = static_cast<float>(percent) / 100.0f;
    // oscillator.SetHardness(hardness);
}
```
- **Paramètre** : `kOscHardParam` (0-100 %)
- **Action** : Durcissement du timbre (waveshaper input)

---

### VCF (5 callbacks)

#### 10. ApplyVcfCutoff
```cpp
static void ApplyVcfCutoff(int32_t index) {
    float cutoff = static_cast<float>(kVcfCutoffParam.value);  // 20-20000 Hz
    // vcf.SetCutoff(cutoff);
}
```
- **Paramètre** : `kVcfCutoffParam` (20-20000 Hz)
- **Action** : Fréquence de coupure du filtre

#### 11. ApplyVcfResonance
```cpp
static void ApplyVcfResonance(int32_t index) {
    int32_t percent = kVcfResonanceParam.value;  // 0-100%
    float resonance = static_cast<float>(percent) / 100.0f;
    // vcf.SetResonance(resonance);
}
```
- **Paramètre** : `kVcfResonanceParam` (0-100 %)
- **Action** : Résonance (Q) du filtre

#### 12. ApplyVcfKey
```cpp
static void ApplyVcfKey(int32_t index) {
    int32_t percent = kVcfKeyParam.value;  // 0-100%
    float key_follow = static_cast<float>(percent) / 100.0f;
    // vcf.SetKeyFollow(key_follow);
}
```
- **Paramètre** : `kVcfKeyParam` (0-100 %)
- **Action** : Suivi du clavier du filtre

#### 13. ApplyVcfDrive
```cpp
static void ApplyVcfDrive(int32_t index) {
    int32_t percent = kVcfDriveParam.value;  // 0-100%
    float drive = static_cast<float>(percent) / 100.0f;
    // vcf.SetDrive(drive);
}
```
- **Paramètre** : `kVcfDriveParam` (0-100 %)
- **Action** : Saturation / distorsion du filtre

#### 14. ApplyVcfEnv
```cpp
static void ApplyVcfEnv(int32_t index) {
    int32_t percent = kVcfEnvParam.value;  // -100 à 100%
    float mod_amt = static_cast<float>(percent) / 100.0f;
    // vcf.SetEnvelopeAmount(mod_amt);
}
```
- **Paramètre** : `kVcfEnvParam` (-100 à 100 %)
- **Action** : Quantité de modulation de l'enveloppe 2 sur le filtre

---

### ENV1 (5 callbacks) - Enveloppe ASDSR principale

#### 15. ApplyEnv1Attack
```cpp
static void ApplyEnv1Attack(int32_t index) {
    float ms = static_cast<float>(kEnv1AttackParam.value);  // 1-5000 ms
    float sec = ms / 1000.0f;
    // env1.SetAttackTime(sec);
}
```
- **Paramètre** : `kEnv1AttackParam` (1-5000 ms)
- **Action** : Temps d'attaque de l'enveloppe

#### 16. ApplyEnv1Decay
```cpp
static void ApplyEnv1Decay(int32_t index) {
    float ms = static_cast<float>(kEnv1DecayParam.value);  // 1-5000 ms
    float sec = ms / 1000.0f;
    // env1.SetDecayTime(sec);
}
```
- **Paramètre** : `kEnv1DecayParam` (1-5000 ms)
- **Action** : Temps de descente de l'enveloppe

#### 17. ApplyEnv1Sustain
```cpp
static void ApplyEnv1Sustain(int32_t index) {
    int32_t percent = kEnv1SustainParam.value;  // 0-100%
    float level = static_cast<float>(percent) / 100.0f;
    // env1.SetSustainLevel(level);
}
```
- **Paramètre** : `kEnv1SustainParam` (0-100 %)
- **Action** : Niveau de sustain de l'enveloppe

#### 18. ApplyEnv1Release
```cpp
static void ApplyEnv1Release(int32_t index) {
    float ms = static_cast<float>(kEnv1ReleaseParam.value);  // 1-5000 ms
    float sec = ms / 1000.0f;
    // env1.SetReleaseTime(sec);
}
```
- **Paramètre** : `kEnv1ReleaseParam` (1-5000 ms)
- **Action** : Temps de relâchement de l'enveloppe

#### 19. ApplyEnv1Velocity (BONUS - n'existe pas dans original)
```cpp
static void ApplyEnv1Velocity(int32_t index) {
    int32_t percent = kEnv1VelocityParam.value;  // 0-100%
    float sensitivity = static_cast<float>(percent) / 100.0f;
    // env1.SetVelocitySensitivity(sensitivity);
}
```
- **Paramètre** : `kEnv1VelocityParam` (0-100 %)
- **Action** : Sensibilité de l'enveloppe à la vélocité MIDI

---

### ENV2 (4 callbacks) - Enveloppe de modulation

#### 20. ApplyEnv2Attack
```cpp
static void ApplyEnv2Attack(int32_t index) {
    float ms = static_cast<float>(kEnv2AttackParam.value);  // 1-5000 ms
    float sec = ms / 1000.0f;
    // env2.SetAttackTime(sec);
}
```

#### 21. ApplyEnv2Decay
```cpp
static void ApplyEnv2Decay(int32_t index) {
    float ms = static_cast<float>(kEnv2DecayParam.value);  // 1-5000 ms
    float sec = ms / 1000.0f;
    // env2.SetDecayTime(sec);
}
```

#### 22. ApplyEnv2Sustain
```cpp
static void ApplyEnv2Sustain(int32_t index) {
    int32_t percent = kEnv2SustainParam.value;  // 0-100%
    float level = static_cast<float>(percent) / 100.0f;
    // env2.SetSustainLevel(level);
}
```

#### 23. ApplyEnv2Release
```cpp
static void ApplyEnv2Release(int32_t index) {
    float ms = static_cast<float>(kEnv2ReleaseParam.value);  // 1-5000 ms
    float sec = ms / 1000.0f;
    // env2.SetReleaseTime(sec);
}
```

---

### LFO (6 callbacks)

#### 24. ApplyLfo1Rate
```cpp
static void ApplyLfo1Rate(int32_t index) {
    float hz = static_cast<float>(kLfo1RateParam.value);  // 1-500 Hz
    // lfo1.SetRate(hz);
}
```
- **Paramètre** : `kLfo1RateParam` (1-500 Hz)
- **Action** : Fréquence du LFO 1

#### 25. ApplyLfo1Amp
```cpp
static void ApplyLfo1Amp(int32_t index) {
    int32_t percent = kLfo1AmpParam.value;  // 0-100%
    float depth = static_cast<float>(percent) / 100.0f;
    // lfo1.SetDepth(depth);
}
```
- **Paramètre** : `kLfo1AmpParam` (0-100 %)
- **Action** : Profondeur du LFO 1

#### 26. ApplyLfo1Phase
```cpp
static void ApplyLfo1Phase(int32_t index) {
    float degrees = static_cast<float>(kLfo1PhaseParam.value);  // 0-360°
    float radians = degrees * 3.14159f / 180.0f;
    // lfo1.SetPhase(radians);
}
```
- **Paramètre** : `kLfo1PhaseParam` (0-360°)
- **Action** : Phase du LFO 1

#### 27-29. LFO2 (Rate, Amp, Phase) - Même pattern que LFO1

---

### MATRIX (2 callbacks)

#### 30. ApplyMatSlot1Amt
```cpp
static void ApplyMatSlot1Amt(int32_t index) {
    int32_t percent = kMatSlot1AmtParam.value;  // -100 à 100%
    float amount = static_cast<float>(percent) / 100.0f;
    // matrix.SetSlotAmount(1, amount);
}
```
- **Paramètre** : `kMatSlot1AmtParam` (-100 à 100 %)
- **Action** : Quantité de modulation du slot 1 de la matrice

#### 31. ApplyMatSlot2Amt
```cpp
static void ApplyMatSlot2Amt(int32_t index) {
    int32_t percent = kMatSlot2AmtParam.value;  // -100 à 100%
    float amount = static_cast<float>(percent) / 100.0f;
    // matrix.SetSlotAmount(2, amount);
}
```

---

### FX (3 callbacks - pas de callback pour l'effet type)

#### 32. ApplyFxDryWet
```cpp
static void ApplyFxDryWet(int32_t index) {
    int32_t percent = kFxDryWetParam.value;  // 0-100%
    float mix = static_cast<float>(percent) / 100.0f;
    // fx.SetMix(mix);
}
```
- **Paramètre** : `kFxDryWetParam` (0-100 %)
- **Action** : Mix sec/humide de l'effet

#### 33. ApplyFxTime
```cpp
static void ApplyFxTime(int32_t index) {
    float ms = static_cast<float>(kFxTimeParam.value);  // 10-5000 ms
    float sec = ms / 1000.0f;
    // fx.SetTime(sec);
}
```
- **Paramètre** : `kFxTimeParam` (10-5000 ms)
- **Action** : Temps de délai / decay de l'effet

#### 34. ApplyFxFb
```cpp
static void ApplyFxFb(int32_t index) {
    int32_t percent = kFxFbParam.value;  // 0-100%
    float feedback = static_cast<float>(percent) / 100.0f;
    // fx.SetFeedback(feedback);
}
```
- **Paramètre** : `kFxFbParam` (0-100 %)
- **Action** : Feedback de l'effet

---

### MIDI (2 callbacks)

#### 35. ApplyMidiChannel
```cpp
static void ApplyMidiChannel(int32_t index) {
    int32_t channel = kMidiChannelParam.value;  // 1-16
    // midi.SetChannel(channel - 1);  // MIDI channels sont 0-15 en interne
}
```
- **Paramètre** : `kMidiChannelParam` (1-16)
- **Action** : Canal MIDI d'écoute

#### 36. ApplyMidiBend
```cpp
static void ApplyMidiBend(int32_t index) {
    int32_t semitones = kMidiBendParam.value;  // 1-24
    // midi.SetBendRange(semitones);
}
```
- **Paramètre** : `kMidiBendParam` (1-24 demi-tons)
- **Action** : Plage de bend MIDI

---

### SYSTEM (1 callback)

#### 37. ApplySysLuminosite
```cpp
static void ApplySysLuminosite(int32_t index) {
    int32_t percent = kSysLuminositeParam.value;  // 10-100%
    // lcd.SetBrightness(percent);
    // SendToESP32(USB_SET_LUMINOSITY, percent);
}
```
- **Paramètre** : `kSysLuminositeParam` (10-100 %)
- **Action** : Luminosité de l'écran LCD/AMOLED

---

## 🔨 Comment injecter les implémentations

### Option 1 : Une par une (recommandée pour le débogage)

Remplacer chaque callback stub avec son implémentation réelle.

### Option 2 : Batch (si vous avez tous les API synthétiseur définis)

Fournir tous les callbacks d'un coup via un fichier de remplacement.

---

## 📞 Notes importantes

1. **Les callbacks ne sont PAS urgents** : Le système fonctionne sans eux
2. **Valeurs persistantes** : Les valeurs numériques sont conservées même sans callback
3. **Transmission USB** : Tous les changements sont toujours envoyés au ESP32
4. **Flexibilité** : Vous pouvez implémenter les callbacks graduellement

---

**À faire ensuite :**
1. Tester le menu avec le firmware actuel
2. Vérifier que l'édition numérique fonctionne
3. Implémenter les callbacks un par un selon les besoins

