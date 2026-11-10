# Dépendances et construction

NxNow utilise les sources et bibliothèques sous `extern/`, sans modifier leurs notices.
La chaîne Switch est devkitPro / devkitA64, libnx et les portlibs Switch. Préparer CMake,
Ninja, Python 3, Jinja2 et jsonschema avant `scripts/build-switch-msys2.sh`.
Le fichier `CMakeLists.txt` décrit les cibles et options effectivement utilisées.

## FFmpeg fourni

Les bibliothèques statiques de `extern/ffmpeg` annoncent la version
`N-117227-ge932809b8f`, avec `--enable-gpl` et le backend `nvtegra` pour Horizon.
Sources du fork : https://github.com/averne/FFmpeg/tree/e932809b8f
Une archive de ce commit accompagne la release v0.1.
Consulter `licenses/FFmpeg-LICENSE.md`, `licenses/FFmpeg-GPL-2.0.txt` et les notices du fork.

La configuration extraite de la bibliothèque est conservée dans `ffmpeg-configure.txt`.
Les bibliothèques sont statiques et ciblent aarch64 / Cortex-A57 / Horizon. Il faut les
reconstruire avec la même chaîne devkitPro et les portlibs correspondant aux fonctions
activées. Le chemin `/opt/devkitpro` est celui de l'environnement de construction.

## Autres composants

Borealis (Apache-2.0), libpeer (MIT), Mbed TLS (Apache-2.0), Jansson (MIT),
libnx (ISC), Deko3D (zlib), NanoVG (zlib), Yoga (MIT), Tweeny (MIT),
libcurl et les autres composants conservent leurs licences dans leurs sources et `licenses/`.
Les licences des polices, Material Icons et des composants du raccourci HOME figurent
également dans `resources/` et `tools/nxnow-forwarder-installer/`.

Le code de l'application reste sous MIT. Le NRO combiné avec ce FFmpeg est distribué
avec GPL-3.0 et les notices des composants ; la licence MIT d'origine est conservée.
Les sources de NxNow correspondant au NRO sont celles du tag `v0.1` de ce dépôt.
