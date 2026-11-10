# NXNow v0.0.12 — clavier Switch avec ZR + MINUS + Y

Cette variante est construite depuis le tag officiel **v0.0.12** du dépôt
OpenCloudGaming/NXNow-Switch, commit
`730704d7ad80d15a6ca6020121be5313559de87e`.

Elle remplace le raccourci d'ouverture du clavier par **ZR + MINUS + Y**.
Le clavier utilisé est le clavier natif de la Switch, superposé au streaming.
Le texte est transmis au champ sélectionné dans la session distante : nom
d'utilisateur, email, mot de passe ou autre champ d'un service affiché dans
le flux, notamment EA, NVIDIA ou Steam.

## Compilation

Avec devkitPro et les dépendances du projet installés :

```sh
NXNOW_VERSION=0.0.12 bash scripts/build-switch-msys2.sh
```

Le résultat est `build/switch/NXNow.nro`. Le numéro de version est injecté
explicitement, comme pour les builds officiels : le tag conserve une ancienne
valeur par défaut dans les sources. Le NRO de cette variante doit afficher
**0.0.12** dans ses métadonnées et dans l'application.

## Installation et utilisation

1. Copier le NRO dans **`sd:/switch/NXNow/NXNow.nro`**, en remplaçant le
   fichier utilisé précédemment. Avec le ZIP fourni, copier le dossier `switch`
   à la racine de la carte SD.
2. Ouvrir **NXNow** dans Homebrew Menu et vérifier le numéro **0.0.12**.
3. Démarrer une session de streaming, puis toucher le champ voulu dans l'image
   distante pour lui donner le focus.
4. Appuyer sur **ZR + MINUS + Y**, puis **relâcher les trois touches**.
   Le clavier s'ouvre après leur relâchement pour éviter qu'une touche du
   raccourci ne soit interprétée comme une saisie ou une commande du clavier.
5. Saisir le texte au clavier tactile ou avec la manette.
6. **Tab** dans la barre tactile, ou **MINUS + L**, passe au champ suivant
   sans envoyer Entrée ; le tampon local est vidé pour le nouveau champ.
7. **PLUS** ou **Enter** envoie Entrée et ferme le clavier. **B** ferme le
   clavier sans Entrée ; les caractères déjà transmis restent dans le champ.
   Utiliser **Bksp** dans la barre tactile pour retirer un caractère.

Le menu **MINUS + PLUS** rappelle le nouveau raccourci. Les commandes du jeu
sont neutralisées pendant le raccourci complet et la saisie, puis reprennent
après relâchement des touches et contacts tactiles.
Le raccourci est disponible pendant une session distante connectée.

## Texte et confidentialité

Les lettres sans accents, majuscules, chiffres, espaces et tous les symboles
ASCII usuels sont pris en charge, notamment `@`, `.`, `_`, `+` et les symboles
de mots de passe. Le mapping distant existant utilise un clavier américain.
Les caractères accentués et Unicode sont refusés avec un message.

Le clavier manuel ne sauvegarde pas les identifiants dans un fichier et
désactive les traces de frappes et de paquets clavier pendant la saisie,
même lorsque les diagnostics du streaming sont actifs. L'usage du dictionnaire
du clavier système est désactivé et le tampon est vidé à la fermeture.
Le texte reste visible dans le clavier Switch ; le masquage du champ distant
dépend du formulaire du service.

## Validation

Les tests ciblés vérifient les six ordres d'appui et les six ordres de
relâchement, l'absence d'ouverture avec une combinaison incomplète, le maintien
sans répétition, le blocage pendant les autres interfaces et la neutralisation
des commandes du jeu. Les caractères ASCII et les raccourcis clavier sont
également vérifiés. Le fichier livré est compilé avec devkitA64.

L'essai avec un compte réel dans une session EA/NVIDIA/GeForce NOW doit être
effectué sur console : ouverture, saisie de `@` et de majuscules, correction,
Tab, validation, fermeture et reprise du jeu.
