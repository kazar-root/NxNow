# NxNow v0.2c

**More stability for the whole streaming system, especially in heavy games such as Counter-Strike 2, and a much better 60 FPS mode.**

## What's new

- **Even more stable streaming, especially in demanding games such as Counter-Strike 2.** NxNow now absorbs the large bursts of data that heavy scenes send at once, instead of losing part of them over Wi-Fi.
- **Much better 60 FPS.** Pictures are no longer skipped in pairs when they arrive slightly bunched up, which removes the judder and the apparent FPS drops at 60 FPS.
- **Smarter recovery from lost packets.** When a packet goes missing, NxNow waits just long enough for GeForce NOW to resend it (based on your measured connection delay) instead of freezing the picture. There is no extra delay when nothing is lost.
- **New *Diagnostics* option** in Settings › Controls & audio, to save a detailed stream log when reporting a problem.

## Recommended settings

- **Maximum bitrate: 20 Mbps** — the best balance of image quality and stability, at 30 or 60 FPS.
- On Wi-Fi, stay close to your router and prefer 5 GHz.

Everything from v0.2b (most popular games, library management, fully working on-screen keyboard, faster catalog) is included.

## A problem?

**If you notice a problem, don't hesitate to contact me** by opening an issue on the
[Issues](https://github.com/kazar-root/NxNow/issues) page. Turning on *Diagnostics* and attaching
`sdmc:/switch/NXNow/stream_trace.log` helps a lot.

— *KazaR-RooT*

## Installation / update

1. Download `NxNow.nro` below.
2. Copy it to `sdmc:/switch/NXNow/`, replacing the previous version. Your account and your
   settings are kept.
3. Launch the Homebrew Menu **in application mode** (hold **R** while launching an installed
   game), then open NxNow.

The app and the Homebrew Menu show this version as **0.2.2**.

## Integrity

SHA-256 (`NxNow.nro`):

```
1819abb34593cd5dbede31e753beda0b235e08011b84b364474223b70ddddb8c
```
