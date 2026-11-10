# Native startup regression tests

This target runs the production MainActivity, startup gate, MainTabsView,
LibraryTab, SettingsTab and animation views with actual Borealis, Yoga and
NanoVG. A headless platform replaces the OS window and GPU. Existing external
service fixtures provide synthetic account/catalog responses; no credentials or
network requests are needed. It creates no GUI window.

The tests cover the cold-start state where the library is still empty, a truly
empty account, guest mode, rendered shell geometry, and repeated carousel/menu
activity. The renderer records texture references until NanoVG flushes a frame
and rejects deletion while a texture is still referenced. The default build
uses AddressSanitizer and UndefinedBehaviorSanitizer.

Before the fix, guest/empty construction threw `Only focusable views can have a
custom navigation route`; background transitions also deleted two sampled
textures before the end of their frames. Both regressions are now checked.

Build using a host compiler and Jansson, independently of the Switch toolchain:

```sh
cmake -S tests/ui_startup -B build/ui-startup -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/ui-startup -j6
build/ui-startup/ui_startup guest
build/ui-startup/ui_startup empty
build/ui-startup/ui_startup cold
build/ui-startup/ui_startup cold-fr
build/ui-startup/ui_startup pages-fr
build/ui-startup/ui_startup auto-fr
build/ui-startup/ui_startup loading-fr
build/ui-startup/ui_startup account-fr
build/ui-startup/ui_startup qr-fr
build/ui-startup/ui_startup qr-zh
build/ui-startup/ui_startup detail-fr
build/ui-startup/ui_startup direct-fr
build/ui-startup/ui_startup stress
```

Set `JANSSON_LIBRARY` and `JANSSON_INCLUDE` if Jansson is outside the normal host
search paths. The stress case runs for at least 40 seconds. It uses the normal
launcher transitions and opens/closes Settings and the main menu repeatedly.
Every frame checks the ten-cover resident limit. The French cases check the
catalogue request language, loading a localized full description, and skipping
the extra description request when launching directly from the library.

This is a lifecycle/layout test, not a screenshot renderer or a Switch GPU
certification. Real NVIDIA authentication, GPU rendering, physical input,
playback and frame timing still require hardware. Production image decoding
bounds and cache cancellation use separate host cache tests.

The page case activates the real Next/Previous actions for a 21-game library,
checks page sizes 10/10/1, and checks that old artwork is destroyed. The loading
case holds image completion and verifies the French logo/dots page before
revealing the carousel. Account tests disconnect the active account and open
the NVIDIA QR dialog directly, with no provider discovery. On macOS the host
target keeps RTTI enabled for Yoga so libc++ exceptions remain catchable.

The QR cases supply a synthetic challenge through the existing authentication
callback, inspect the production NanoVG white scan-area geometry, check card
and action bounds, cancel with B, request a new code, and approve a synthetic
session. They cover French and Chinese layouts without contacting NVIDIA.
Real phone scanning and provider authorization still require hardware.

The page fixture selects the right arrow with native controller navigation,
activates it to advance, then selects the left arrow to return. It retains only
ten game textures and verifies that the last page has no next-page action. The automatic-language fixture uses a
French system locale, selects manual English in Settings, then restores the
console option through the real dropdown. Shell checks cover centered action
groups, profile placement and removal of the redundant information button.

The `input-fr` fixture uses the real advanced settings dropdown to persist mouse
speed and retain it across streaming-profile changes. It opens the update placeholder,
formats real/unknown battery states, and verifies launch-title bounds and neutral
cancel focus at 1280×720.

The `profile-fr` and `settings-login-fr` cases start signed out and inspect the
real profile-pictogram guidance. They complete a synthetic NVIDIA QR sign-in
from the header or Settings, wait for the first ten games in the library, and
then sign out directly through the profile without opening Settings.
