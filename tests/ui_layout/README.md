# Production native UI verification

This target compiles actual LibraryTab, CatalogTab, SettingsTab and their actions,
GameDetailView, the shell, queue UI and launch worker, and the pure stream overlay
renderer with vendored Borealis SDL/OpenGL. It does not build an HTML mock or add
a demo path to the application. Production InitializeThemeAndFonts runs after
window creation and before any view constructor.

The application shell exposes only Library and Settings. The retained `store`
and `empty-store` scenarios mount the legacy catalog component separately; it
is no longer an application tab. Launch artwork uses FIT and its full-image
bounds, and the launch footer includes three sequential violet loading dots.
Controls/help is a separate scrolling view with paragraph rows and B to close.
Library search is exercised through the native Search icon; Plus opens the main menu. Queue cancellation is the only
launch action; simulated activity removal retains recovery coverage for the
existing background worker. Cover draw containment is separately exercised by
`cached_image_admission_test.cpp` with the real CachedImage implementation.
The essential settings start with advanced options collapsed. Expanding and
collapsing them retains focus on the toggle and preserves saved preferences.

Only external NVIDIA, image/avatar downloads, account/launcher-preference
persistence, shortcut installation, network information, play-history writes,
and the hardware stream handoff use test-link fixtures. Stream settings use the
real atomic implementation with isolated test AppHomePath storage. Queue tests
run real start/poll/cancel/adopt/minimize/restore code with scripted service
replies and a recording PresentCloudStream bridge. Notification instrumentation
records calls and forwards to the real Borealis notification implementation.
No fixture contains credentials or belongs to the NRO target.

## Build and run

On Ubuntu, build outside the repository:

```sh
sudo apt-get install cmake ninja-build libsdl2-dev libgl1-mesa-dev \
  libcurl4-openssl-dev libjansson-dev xvfb xauth imagemagick
cmake -S tests/ui_layout -B ~/.capy/work/nxnow-native-ui -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build ~/.capy/work/nxnow-native-ui -j4
python3 tests/ui_font_assets_test.py
python3 tests/ui_layout/font_chain_test.py
bash tests/ui_layout/run.sh ~/.capy/work/nxnow-native-ui/ui_layout \
  ~/.capy/work/nxnow-native-evidence
```

The full runner has 104 cases: six screens, all eight supported languages, and
1280×720/1920×1080 physical resolution, plus guest and empty Library/Store at both
sizes. Every case must retain 1280×720 logical geometry. Settings has one essential-list capture; queue has phase, position and restored-artwork captures.

The executable accepts `language account-name screenshot-path physical-width
scenario`. Use `guest` for the signed-out fixture. Scenarios are `library`,
`store`, `settings`, `detail`, `queue`, `overlay`, `empty`, and `empty-store`.
Pass scenario names after the runner's output directory for a narrow matrix.
For parallel isolated output directories, `--screens-only` before scenario names
omits only separately assigned guest/empty cases. Run one group without that
flag so all eight extra cases remain covered. Each attempted case gets a log;
failures do not stop subsequent cases, and any failure makes the runner nonzero.
Each case renders on Mesa's software rasterizer, so set `NXNOW_UI_JOBS` to run
that many cases at once on separate Xvfb displays. With `LP_NUM_THREADS=1`, one
case per CPU gives the best throughput. CI splits the matrix into six groups and
runs `NXNOW_UI_JOBS=$(nproc)` on each 8-vCPU Blacksmith runner.

## Assertions

- Child containment, flow sibling separation, one-line tabs, a single shell
  footer, exact Store 220×210 cards with 245-pixel origin spacing, and containment
  in 1200-pixel rows. Absolute art is not a flow sibling, and scrolling content
  may exceed its viewport. Two-pixel Yoga containment tolerance does not relax
  exact card dimensions.
- Actual NanoVG glyph bounds and font measurements for fixed Library captions
  in every 4-sort × 6-filter combination; row title/value containment inside
  content padding; focused Settings rows inside their scissor viewport; and no
  visible scrollbar over cards, labels or focus borders. Library/Store long
  game titles must use complete static ellipses, with identical cropped native
  framebuffer pixels before and after the native scroll timer. The test-link
  NanoVG observers forward every draw to the real implementation. Geometry-only
  passes do not establish readable text or visual acceptance.
  Bearings are measured at the recorded NanoVG transform and raster origin with
  the label's font quality. Fontstash rounds glyph quads to physical pixels, so
  only the label's own ink boundary allows one physical pixel (`1/windowScale`).
  Draw-anchor tolerance remains 0.5 logical pixels; full-caption fit, padded-row
  containment, no overlap, ancestor-scissor bounds and static-title pixel
  stability are unchanged.
- Public Borealis controller dispatch, real focus routing, hit-testing and
  START/END gestures, actual SDL IME text-input/submission, Library carousel selection and identity, native search, bounded ten-cover texture windows,
  L/R jumps without extra fetch, persisted favorites and toolbar navigation, Store order/query/cursor effects, and detail
  activity navigation.
- Settings quality selector, stream profile and statistics switches save immediately
  through actual persistence. Essential settings get geometry/font checks and a capture.
  Construction and help focus must not start implicit region probes.
- Detail next-stream profile selection, unselected cancellation, Play and shortcut argument recording, multi-variant selector and saved
  preference, actual selected-store launch, stale-account deferred-dismiss guard,
  and overflowing descriptions scrolled through native held SDL input.
- Real queue ownership, position 123/unknown, minimize/restore/cancel, adopted and
  after-cancel session cleanup, repeated polling, exactly one reminder, blocked
  second launch, patch/ad/error priority, and account-generation cleanup. Entire
  StartSession and PresentCloudStream settings must stay frozen when separately
  saved settings change while minimized. A covering real dialog must remain
  intact during handoff, then remove the stale queue after it closes.
- Named font IDs, stock-label font initialization, actual bundled glyph presence
  for rendered text, mixed Latin/Cyrillic/Chinese NanoVG measurement, complete UI
  translation codepoint coverage in every named face plus direct CJK fallback,
  static weights and licenses. Missing Chinese glyphs are failures.
- Production overlay formatting for unavailable RTT/no packets, sequence-gap
  loss estimate, fractional measured p95 latency, queue high-water and elapsed.
- Drawable/viewport coverage and bottom-right framebuffer coverage before native
  PPM capture and PNG conversion. Final captures wait for notification toasts.

Waits have finite deadlines; assertion failures are not skipped. Fixture covers
use the authentic NXNow logo and avatars are blank, so screenshots do not
prove real image download lifetime/cropping.

## Sanitizers

```sh
cmake -S tests/ui_layout -B ~/.capy/work/nxnow-native-asan -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DNXNOW_UI_SANITIZERS=ON
cmake --build ~/.capy/work/nxnow-native-asan -j4
xvfb-run -a -s '-screen 0 1920x1080x24' \
  ~/.capy/work/nxnow-native-asan/ui_layout en Player \
  ~/.capy/work/nxnow-native-asan/library.ppm 1280 library
```

Run detail and queue similarly. Keep sanitizer diagnostics. If SDL/Mesa reports
a dependency leak, distinguish that report from view/application lifetime
failures instead of broadly suppressing it. GNU linker notification observation
is intended for this Linux host target.

## Limits

Fixtures do not prove NVIDIA authentication/entitlement or remote allocation,
real cover workers, live WebRTC sampling, Switch/Deko3D rendering, physical input
coordinates, remote reports, inline-keyboard capture or gameplay latency. The
overlay uses production rendering with test samples; the handoff bridge records
a call instead of creating a hardware stream. Run the supported full Switch
build, inspect RomFS, sideload the real NxNow.nro, and verify login, playback,
dock/undock, queue cancellation and input release on real Switch hardware.

The simplified-menu changes have been cross-compiled for Switch and the host policy tests run on macOS. The SDL/Mesa visual matrix requires the Linux dependencies listed above and was not executed on this workstation.

The console carousel replaces the former two-row library. Header/footer geometry
is now 100/48 logical pixels; the carousel retains one focusable viewport while
its ten resident image slots change. The updated fixture exercises its native
controller dispatch and real per-account favorites persistence. The SDL/Mesa
visual matrix still requires the documented Linux environment.
