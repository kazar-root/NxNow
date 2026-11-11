# NxNow v0.2

**This update fixes stream latency and greatly improves video smoothness.**

## What's new

- **Much smoother video.** NxNow now follows the rhythm of the frames sent by GeForce NOW. When a
  frame arrives just a few milliseconds late, it waits for it instead of repeating the previous
  picture, which removes most of the stutter and judder.
- **Lower latency.** The picture no longer waits for the sound before it is displayed. Each new
  frame is shown as soon as it is ready, so your actions appear on screen sooner.

## Fixes

- **No more random freezes.** Small Wi-Fi hiccups or a heavy scene could freeze the picture for a
  moment while the stream recovered. NxNow now absorbs these short bursts instead of restarting
  the video.
- **Steadier sound.** The audio reserve was raised slightly so sound no longer crackles when a
  heavy scene loads. Because the picture no longer waits for the sound, this does not add any
  video latency.
- **Faster software decoding.** When the hardware decoder is not used, decoding no longer holds
  extra frames back.

## A problem?

**If you notice a problem, don't hesitate to contact me** by opening an issue on the
[Issues](https://github.com/kazar-root/NxNow/issues) page.

— *KazaR-RooT*

## Installation / update

1. Download `NxNow.nro` below.
2. Copy it to `sdmc:/switch/NXNow/`, replacing the previous version. Your account and your
   settings are kept.
3. Launch the Homebrew Menu **in application mode** (hold **R** while launching an installed
   game), then open NxNow.

## Integrity

SHA-256 (`NxNow.nro`):

```
f447ec844e431a98916d9526dc8b7fa0a54c2b6d80ed148588aca5c56b434114
```
