# DCO-ONE : telecommande Android par Wi-Fi

## Connexion

Les deux firmwares doivent etre mis a jour. L'ESP32 heberge la page web et la
carte SD ; la Daisy execute les commandes et produit le son. Le cable UART
bidirectionnel existant reste necessaire : Daisy D13 -> ESP32 GPIO44,
Daisy D14 <- ESP32 GPIO43, masse commune.

1. Alimenter les deux cartes et inserer la carte SD FAT32 avant le demarrage.
2. Sur Android, rejoindre le reseau ouvert **DCO-ONE**, sans mot de passe.
  Si Android conserve l'ancien reseau protege, l'oublier puis se reconnecter.
3. Accepter **Rester connecte** si Android signale l'absence d'Internet.
4. Ouvrir **http://192.168.4.1** dans Chrome. Utiliser HTTP, pas HTTPS.

Pas d'application Android, de compte, de routeur ou de connexion Internet requis.
Deux appareils maximum peuvent rejoindre le point d'acces. Utiliser un seul
editeur a la fois : le syntheseur est partage entre les telephones et les encodeurs.
Ne pas exposer ce serveur HTTP a Internet. Le Wi-Fi est sans authentification ni
chiffrement : toute personne a portee peut acceder aux commandes, y compris la
suppression des patterns.

## Commandes

- La bibliotheque liste les emplacements SD occupes, de 001 a 128.
- Le bouton de lecture d'un pattern le charge et lance sa lecture. Les
  modifications en memoire sont remplacees ; une confirmation apparait si la
  page a des modifications non enregistrees.
- **Jouer** lance la sequence en memoire depuis le premier pas, sans relire la SD.
- **Arreter** coupe la lecture et les notes ; ce n'est pas une pause.
- Selectionner une pastille puis choisir **Off / Note / Arpege / Accord / Fixe**.
  Le degre va de -14 a +14. La transposition fixe va de -24 a +24, selon les memes
  regles que l'encodeur du syntheseur. Les pas au-dela de la longueur active sont
  desactives. La longueur reste reglee par les commandes physiques dans cette V1.
- **Enregistrer** ecrit la sequence et le son courants dans l'emplacement choisi.
  Un emplacement occupe demande confirmation. Les editions live ne remplacent
  pas automatiquement un pattern SD ; l'autosauvegarde QSPI de la session demeure.
- Le crayon renomme un pattern (1 a 64 octets UTF-8, accents acceptes).
  Les noms sont affiches sur le telephone ; l'ecran AMOLED garde ses libelles actuels.
- La corbeille supprime le pattern SD et ses fichiers de secours apres confirmation.
  Une sequence deja chargee reste jouable en memoire, mais n'a plus cet emplacement.
- Actualiser la bibliotheque apres une sauvegarde depuis les commandes physiques.

La tete de lecture est un indicateur rafraichi environ toutes les 400 ms,
pas un affichage de chaque evenement audio aux tempos rapides.
Un succes de commande est affiche seulement apres confirmation Daisy (ou SD
pour le renommage). Une erreur/expiration n'est jamais affichee comme un succes.
Apres une expiration, verifier l'etat avant de recommencer : ne pas relancer
automatiquement une sauvegarde ou une suppression.

## Synthese et matrice

Les onglets **Sequence**, **Synthese** et **Matrice** partagent la meme session.
Synthese expose le VCO (y compris FM), le VCF et les deux LFO : forme, frequence,
synchro au tempo, amplitude et phase. Les curseurs transmettent au relachement ;
les champs numeriques transmettent a la validation ou a la sortie du champ.
Les valeurs confirmees reviennent de la Daisy ; les encodeurs physiques et le Web
modifient les memes parametres. Une edition numerique physique en cours bloque
temporairement les commandes de parametres Web.

Les blocs **ENV1 : Amplitude** et **ENV2 : Modulation** exposent Attack, Decay,
Sustain et Release, avec curseurs, saisie numerique et trace ADSR indicatif.
Attack/Decay/Release vont de 1 a 5000 ms ; Sustain de 0 a 100 %.
ENV1 pilote le volume. ENV2 suit les memes declenchements de notes et portes
que ENV1, avec ses propres temps et niveau, et agit lorsqu'elle est assignee
dans la matrice (par exemple ENV2 vers cutoff). Le parametre Env direct du VCF
continue d'utiliser ENV1 pour conserver le comportement des presets existants.
Velocity d'ENV1 et Loop d'ENV2 ne sont pas implementes et ne sont pas proposes
sur le Web. Les valeurs ADSR utilisent les emplacements de sauvegarde existants.

La matrice offre huit routages, chacun avec source, destination et intensite
signee de -100 a +100 %. Sources actives : LFO1, LFO2, ENV1, ENV2.
Destinations : cutoff, hauteur (+/-12 demi-tons par routage a pleine intensite),
largeur d'impulsion, drive, vitesse de LFO1, resonance du VCF et volume VCA.
La largeur d'impulsion agit sur la forme Pulse, pas sur les voix FM.
La destination LFO1 vitesse module aussi la frequence derivee du tempo.
Les contributions se cumulent, sont lissees sur environ 5 ms et bornees avant
application au moteur. Le gain de modulation VCA est borne entre 0 et 2 ; reduire
le volume general si une modulation positive combinee aux effets sature la sortie.
Les anciennes sources non implementees restent desactivees dans la liste Web.
Les deux premiers routages restent accessibles sur le Waveshare ;
les six suivants se reglent sur le Web.

Exemple : choisir **LFO2**, **VCF : resonance**, puis **+35 %**. La case a gauche
desactive un routage en mettant sa source sur Aucune, sans effacer destination
ni intensite. Pendant la session Web, une reactivation retrouve la derniere source ;
apres rechargement de la page, une source desactivee est reactivee sur LFO1 par defaut.
Enregistrer pour conserver le son et la matrice dans le pattern SD.

Le format SD reste version 3, 640 octets. Les six routages supplementaires utilisent
les selections reservees 58..63, avec une signature par valeur ; la session QSPI
utilise 42..47. Aucun indice du menu existant n'est deplace. Les anciens presets
chargent les six routages supplementaires desactives. LFO2 et les destinations
de matrice autrefois inactives deviennent audibles si un ancien preset les assigne.
Cela vaut aussi pour les anciennes affectations ENV2, desormais actives.

## Stockage et protocole

Les fichiers `.dco` restent au format version 3 (640 octets, CRC32), sans migration.
Les noms sont des fichiers annexes `/DCO-ONE/PATTERNS/NNN.name`, avec ecriture
temporaire et verification avant renommage, et secours `.nbak`. Comme pour les
patterns existants, FAT ne garantit pas une transaction atomique en cas de coupure
d'alimentation. Eviter de retirer la carte ou de couper l'alimentation pendant
une ecriture. Redemarrer l'ESP32 apres un changement de carte.

L'API embarquee fournit `GET /api/state`, `GET /api/patterns` et
`POST /api/command` (JSON, en-tete `X-DCO-Token` fourni par l'etat).
Les commandes passent par `RMC`, les etats par `RMS`, les resultats par `RMR`.
`RMC,id,PARAM,slot,groupe,parametre,valeur` edite un parametre borne par le menu.
Groupes : 0=VCO (6 parametres puis 4 FM), 1=VCF, 2=LFO1/LFO2, 3=matrice
(8 triplets source/destination/intensite), 4=enveloppes (ADSR ENV1 puis ADSR ENV2).
Cinq trames `RMP,slot,groupe,hex`
transportent chacune 24 int32 little-endian, avant `RMS`. L'ESP32 ne publie
les parametres qu'apres reception d'un ensemble complet pour le meme slot.
Les transferts SD gardent `SDC`/`SDR` avec une operation `DELETE` supplementaire.
Les requetes sont bornees et les editions verifient l'identite du pattern courant.
Le traitement reste dans les boucles principales, hors interruption audio.

## Construction et verification

Apres une modification de la page :

```sh
npm --prefix esp32/web ci
npm --prefix esp32/web run build
pio run -d esp32
```

La generation embarque HTML, JavaScript et les icones Lucide dans
`esp32/src/remote_page.h`. Aucun CDN n'est utilise au runtime. Le fichier genere
est versionne pour que PlatformIO puisse compiler sans Node.js.

Tests locaux :

```sh
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined tools/test_sd_patterns.cpp -o /tmp/dco_test_sd_patterns
/tmp/dco_test_sd_patterns
sh tools/test_remote_control.sh
sh tools/test_display_uart.sh
sh tools/test_sequencer_audio.sh
sh tools/test_usb_midi.sh
node tools/test_remote_web.mjs
```

Pour les tests navigateur, installer Chromium une fois avec
`npm exec --prefix esp32/web -- playwright install chromium`.
Les captures desktop/mobile sont produites dans `build/display-preview/`.
Pour ouvrir la page embarquee sur le Mac : `node tools/preview_remote_web.mjs`,
puis `http://127.0.0.1:4173`. Le serveur relaie les requetes vers `192.168.4.1` ;
le Mac doit rejoindre le Wi-Fi DCO-ONE pour commander le vrai syntheseur.

Les anciens bridges USB Daisy/ESP32 et leur test ont ete retires. La liaison
ecran, les commandes Web et la SD passent par UART ; le MIDI USB natif est conserve.
Les sauvegardes historiques dans `backup/` ne sont pas supprimees.

Avant utilisation definitive : verifier sur Android la connexion sans Internet,
le chargement, l'edition audible d'une pastille, puis sauvegarder dans un
emplacement libre, renommer, redemarrer et recharger. Supprimer uniquement ce
pattern de test et verifier qu'il ne revient pas apres redemarrage.