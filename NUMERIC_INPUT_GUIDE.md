# Guide : Saisie des Valeurs Numériques (Numeric Input)

## Vue d'ensemble

Le système de saisie numérique permet d'éditer des paramètres numériques (BPM, fréquence, etc.) via l'encodeur MENU et le bouton MENU_SW. Le code existant du BPM sert de modèle.

---

## Structure de Base

### 1. Définir la structure NumericParam

```cpp
// Déclaration du paramètre
static NumericParam kMyParam = {
    minValue:  10,           // valeur minimale
    maxValue:  500,          // valeur maximale
    step:      5,            // incrément par tour de l'encodeur
    value:     100,          // valeur initiale
    unit:      "Hz"          // unité affichée (peut être nullptr)
};
```

**Champs :**
- `minValue` : valeur minimale acceptée
- `maxValue` : valeur maximale acceptée
- `step` : pas d'incrémentation (1 = un par cran, 5 = cinq par cran)
- `value` : valeur courante
- `unit` : pointeur vers une chaîne (ex: "BPM", "Hz", "ms", nullptr)

---

## 2. Ajouter le paramètre au menu

```cpp
// Dans la structure du menu (kRootMenu)
MenuNode* children = new MenuNode[count];

// Pour chaque paramètre numérique :
{
    label:       "My Param",      // nom affiché
    color:       COLOR_DEFAULT,   // couleur
    children:    nullptr,         // pas de sous-menu
    childCount:  0,
    onSelect:    nullptr,         // callback (optionnel)
    selectedIndex: 0,
    numeric:     &kMyParam        // pointer vers NumericParam !
}
```

---

## 3. Utiliser la valeur en lecture

Dans la boucle principale ou dans les callbacks :

```cpp
// Lecture de la valeur
int32_t currentValue = kMyParam.value;

// Exemple : appliquer au synthétiseur
if (kMyParam.value != oldValue) {
    synth.SetFrequency(static_cast<float>(kMyParam.value));
    oldValue = kMyParam.value;
}
```

---

## Flux d'édition (automatique)

1. **Entrée** : l'utilisateur appuie sur MENU_SW sur le paramètre
   - `s_editing_numeric` = true
   - `s_editing_node` pointeur sur le paramètre
   - `s_editing_original_value` sauvegarde la valeur pré-édition

2. **Édition** : l'encodeur MENU ajuste la valeur
   - `newValue = p->value + encoder_delta * p->step`
   - Clamped entre minValue et maxValue
   - `SendEditState()` envoie au ESP32

3. **Confirmation** : appuyer sur MENU_SW à nouveau
   - `s_editing_numeric` = false
   - La valeur est conservée
   - Retour au menu

4. **Annulation** : appuyer sur HOME
   - La valeur revient à `s_editing_original_value`
   - Retour au menu

---

## Exemple Complet : Ajouter "Attaque" (Attack)

### Étape 1 : Déclarer le paramètre

```cpp
// Après kBpmParam (ligne ~258)
static NumericParam kAttackParam = { 1, 1000, 10, 50, "ms" };
```

### Étape 2 : Ajouter au menu

```cpp
// Dans kRootMenu ou un sous-menu
{
    "Attack",
    COLOR_DEFAULT,
    nullptr,
    0,
    nullptr,
    0,
    &kAttackParam
}
```

### Étape 3 : Utiliser la valeur

```cpp
// Dans audio.cpp ou main.cpp
void UpdateADSR() {
    float attackMs = static_cast<float>(kAttackParam.value);
    envelope.SetAttackTime(attackMs / 1000.0f);  // convertir en secondes
}
```

### Étape 4 : Ajouter à SendStatus() (optionnel)

```cpp
// Pour afficher dans le hub central du ESP32
hw.PrintLine("STAT,BPM=%ld,ATTACK=%ld,...",
             static_cast<long>(kBpmParam.value),
             static_cast<long>(kAttackParam.value),
             ...);
```

---

## Guide de Paramètres (Best Practices)

| Paramètre | Min | Max | Step | Unit | Exemple |
|-----------|-----|-----|------|------|---------|
| BPM | 1 | 200 | 1 | "BPM" | `{ 1, 200, 1, 120, "BPM" }` |
| Fréquence (Hz) | 20 | 20000 | 10 | "Hz" | `{ 20, 20000, 10, 440, "Hz" }` |
| Attaque (ms) | 1 | 1000 | 10 | "ms" | `{ 1, 1000, 10, 50, "ms" }` |
| Volume (%) | 0 | 100 | 1 | "%" | `{ 0, 100, 1, 80, "%" }` |
| Delay (ms) | 0 | 5000 | 50 | "ms" | `{ 0, 5000, 50, 100, "ms" }` |
| Cutoff (Hz) | 20 | 20000 | 50 | "Hz" | `{ 20, 20000, 50, 1000, "Hz" }` |

---

## Debugging

Pour vérifier l'édition numérique :

```cpp
// Dans la section édition (ligne ~714)
if (s_editing_numeric && s_editing_node != nullptr) {
    hw.PrintLine("EDIT: %s = %ld [%ld..%ld]",
                 s_editing_node->label,
                 s_editing_node->numeric->value,
                 s_editing_node->numeric->minValue,
                 s_editing_node->numeric->maxValue);
}
```

---

## Notes Importantes

1. **Thread-safety** : les paramètres sont lus dans le main loop (pas d'accès IRQ)
2. **Persistance** : les valeurs ne sont *pas* persistées en EEPROM (à implémenter si nécessaire)
3. **Envoi ESP32** : chaque changement déclenche `SendEditState()` tous les 100ms max
4. **Annulation** : `s_editing_original_value` permet de restaurer la valeur pré-édition
