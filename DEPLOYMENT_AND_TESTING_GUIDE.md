# 🚀 PROCHAINES ÉTAPES - GUIDE DE TEST ET DÉPLOIEMENT

## ✅ État actuel

Le firmware Daisy a été **compilé avec succès** et contient :
- ✓ 38 paramètres numériques complètement intégrés
- ✓ 35 fonctions callback prêtes (actuellement vides)
- ✓ 10 menus mis à jour avec support complet de l'édition numérique
- ✓ Zéro erreur de compilation
- ✓ Fichier ELF généré : `daisy/build/dco_one_phase1` (273 KB)

---

## 📋 Étapes à suivre

### Phase 1 : Test du menu (AVANT de flasher)
*Durée estimée : 15-30 minutes*

#### Objectif
Vérifier que le menu fonctionne et que l'édition numérique est accessible.

#### Actions
1. **Connecter le Daisy Seed** via USB-C à l'ordinateur
2. **Ouvrir un terminal** de monitoring pour visualiser les messages USB
3. **Flasher le firmware** avec la commande appropriée
4. **Naviguer dans le menu** :
   - Appuyer sur MENU_SW pour entrer dans un menu
   - Tourner MENU encoder pour naviguer
   - Appuyer sur MENU_SW sur un paramètre numérique (ex: BPM)
   - Tourner MENU encoder pour modifier la valeur
   - Appuyer sur HOME pour annuler
   - Appuyer sur MENU_SW pour confirmer

#### Vérifications à faire

- [ ] Navigation dans tous les menus fonctionne
- [ ] Les paramètres numériques s'éditent avec l'encoder
- [ ] Les valeurs restent dans leur plage min/max
- [ ] Les pas d'incrémentation sont respectés (1, 5, 10, etc.)
- [ ] HOME annule l'édition et restaure la valeur
- [ ] Les unités s'affichent correctement au ESP32
- [ ] Les messages USB arrivent bien au ESP32

#### Diagnostic en cas de problème

**Le menu ne s'affiche pas du tout**
- Vérifier la connexion USB
- Vérifier que le code de l'écran ESP32 a le bon baudrate (115200)

**L'édition numérique ne fonctionne pas**
- Vérifier que le paramètre a bien un `.numeric` pointeur
- Vérifier la console USB pour les erreurs

**Les valeurs clignent ou oscillent**
- Problème possible dans SendEditState() ou SendMenuState()
- Ajouter des délais entre les envois USB

---

### Phase 2 : Implémenter les callbacks
*Durée estimée : 2-4 heures selon le nombre de callbacks*

#### Objectif
Faire fonctionner les paramètres numériques sur le synthétiseur.

#### Approche recommandée

**Étape 2.1 : Implémenter par groupe**
1. Commencer par **OSC** (oscillateur) - changements visuels immédiats
2. Puis **VCF** (filtre) - très important pour le son
3. Puis **ENV1/ENV2** (enveloppes) - essentiel pour la dynamique
4. Puis **LFO** - modulations
5. Puis les autres

**Étape 2.2 : Pour chaque callback**
1. Identifier l'API correspondante du synthétiseur
2. Écrire le code de conversion (valeur menu → paramètre synthé)
3. Remplacer le stub vide
4. Tester immédiatement

#### Exemple complet : ApplyVcfCutoff

**Avant (stub vide)**
```cpp
static void ApplyVcfCutoff(int32_t index) {
}
```

**Après (implémenté)**
```cpp
static void ApplyVcfCutoff(int32_t index) {
    float cutoff = static_cast<float>(kVcfCutoffParam.value);  // 20-20000 Hz
    
    // Vérifier que le filtre existe
    if (vcf_filter != nullptr) {
        // Appliquer la fréquence de coupure
        vcf_filter->SetCutoff(cutoff);
        
        // Optionnel : Log pour le débogage
        // USB_SERIAL << "VCF Cutoff: " << cutoff << " Hz\n";
    }
}
```

#### Où trouver l'API du synthétiseur

- **Fichiers DaisySP** : `/Users/maxpatissier/Downloads/DCO-ONE/daisy/DaisySP/Source/`
- **Fichiers libDaisy** : `/Users/maxpatissier/Downloads/DCO-ONE/daisy/libDaisy/src/`
- **Fichiers du projet** : `/Users/maxpatissier/Downloads/DCO-ONE/daisy/src/` (oscillator.cpp, etc.)

#### Tester chaque implémentation

```bash
# Après chaque implémentation de callback
cd /Users/maxpatissier/Downloads/DCO-ONE/daisy/build
make  # Compile uniquement les fichiers modifiés
# Vérifier absence d'erreur
# Flasher
# Tester le paramètre
```

---

### Phase 3 : Optimisation et polissage
*Durée estimée : 1-2 heures*

#### Checklist

- [ ] Tous les callbacks sont implémentés
- [ ] Pas d'erreurs à la compilation
- [ ] Tous les paramètres modifient réellement le son
- [ ] Pas de clics, pops ou artifacts lors de l'édition en temps réel
- [ ] Performance acceptable (pas de lag au menu)
- [ ] Transmission USB stable (pas de déconnexions)
- [ ] Interface ESP32 mise à jour en temps réel

#### Optimisations possibles

1. **Cache des valeurs** : Éviter les appels inutiles si la valeur n'a pas changé
2. **Ramping** : Lisser les changements de paramètres critiques (cutoff, etc.)
3. **Bypass temporaire** : Mueter l'audio brièvement si le paramètre cause des clics
4. **Rattrapage** : Ajouter des contrôles de saturation après les changements

---

## 🔧 Commandes pratiques

### Compiler le firmware
```bash
cd /Users/maxpatissier/Downloads/DCO-ONE/daisy/build
make
```

### Flasher le Daisy Seed
```bash
# Avec dfu-util (adapter selon votre configuration)
dfu-util -d 0483:df11 -a 0 -s 0x08000000 -D /Users/maxpatissier/Downloads/DCO-ONE/daisy/build/dco_one_phase1
```

### Monitorer les messages USB
```bash
# Sur macOS
cat /dev/cu.usbserial-* | hexdump -C

# Ou utiliser screen
screen /dev/cu.usbserial-* 115200
```

### Nettoyer et recompiler
```bash
cd /Users/maxpatissier/Downloads/DCO-ONE/daisy
rm -rf build
mkdir build
cd build
cmake ..
make
```

---

## 📊 Checklist de déploiement

### Avant de flasher
- [ ] Compilation réussie (0 erreurs)
- [ ] Pas de warnings importants
- [ ] Le fichier ELF existe et fait ~270-300 KB
- [ ] Backup du firmware précédent fait

### Après le flash
- [ ] Le Daisy démarre correctement
- [ ] Les LEDs fonctionnent normalement
- [ ] Le menu s'affiche
- [ ] L'encoder de menu répond
- [ ] Les messages USB arrivent à l'ESP32

### Phase de test du menu
- [ ] Navigation OSC fonctionne
- [ ] Edit numérique fonctionne
- [ ] Valeurs clampées correctement
- [ ] Pas de crash lors du changement rapide de valeurs

### Phase de test du son
- [ ] Les paramètres changent l'audio
- [ ] Pas de clics lors de l'édition
- [ ] Pas de lag notable
- [ ] Les enveloppes réagissent
- [ ] Le filtre fonctionne

---

## 🎯 Priorités d'implémentation

### Tier 1 : Critiques (faire en premier)
1. `ApplyOscCoarse` - Accordage base de l'oscillateur
2. `ApplyVcfCutoff` - Filtre essentiaire
3. `ApplyEnv1Attack` - Enveloppe principale

### Tier 2 : Importants (après Tier 1)
4. `ApplyOscPulse` - PWM de l'oscillateur
5. `ApplyVcfResonance` - Résonance du filtre
6. `ApplyEnv1Decay`, `Release` - Autres enveloppes

### Tier 3 : Fonctionalités bonus (quand on a le temps)
7-35. Tous les autres callbacks

---

## 📞 Troubleshooting rapide

### Problème : Compilation échoue
**Solution :**
1. Vérifier la syntaxe des callbacks implémentés
2. Vérifier que les API du synthétiseur existent
3. Nettoyer et recompiler : `rm -rf build && mkdir build && cd build && cmake .. && make`

### Problème : Le paramètre n'a aucun effet
**Solution :**
1. Vérifier que le callback est appelé (ajouter un log USB)
2. Vérifier que l'API synthé prend bien la valeur
3. Vérifier que la conversion de valeur est correcte
4. Vérifier que l'objet synthé n'est pas nullptr

### Problème : Clics/pops lors de l'édition
**Solution :**
1. Ajouter un ramping progressif : `target = new_value; progress += delta;`
2. Diminuer la fréquence d'appel du callback
3. Mueter brièvement lors du changement
4. Réduire la magnitude du changement de paramètre

### Problème : Lag au menu
**Solution :**
1. Réduire la fréquence des envois USB
2. Optimiser les callbacks pour utiliser moins de CPU
3. Vérifier qu'il n'y a pas de malloc/free dans les callbacks
4. Profiler avec Pylance (voir /memories/)

---

## 📚 Documentation de référence

Fichiers utiles dans le workspace :

- `IMPLEMENTATION_COMPLETE.md` - Résumé de tout ce qui a été fait
- `CALLBACKS_IMPLEMENTATION_GUIDE.md` - Guide détaillé pour chaque callback
- `daisy/src/main.cpp` - Code source principal
- `daisy/src/oscillator.cpp` - Oscillateur
- `daisy/src/input_manager.cpp` - Gestion des entrées

---

## 🎉 Résumé

**Le firmware est prêt.** Vous pouvez dès maintenant :
1. Tester le menu sur le Daisy Seed
2. Implémenter progressivement les callbacks
3. Écouter le synthétiseur prendre vie

C'est maintenant une question de branchement de l'API du synthétiseur aux callbacks vides. Chaque paramètre est déjà:
- ✅ Stocké dans sa variable NumericParam
- ✅ Éditable via le menu avec l'encoder
- ✅ Transmis au ESP32 en temps réel
- ✅ Attendant d'être connecté au synthétiseur via les callbacks

**Bon développement! 🚀**

