# NxNow v0.2d

**Fixes for the problems reported by players after v0.2c.**

Thank you all for your valuable feedback and your precious help. Every report made this version better!

## What's fixed

- **Battle.net sign-in works with the on-screen keyboard.** In Mouse & Keyboard Mode, **ZR + R3** now opens a plain keyboard. It no longer submits the empty email or password form on its own.
- **No more "controller disconnected" messages when typing.** Opening or closing the keyboard no longer makes games think the controller was unplugged. This removes Diablo IV's *"Please reconnect your controller"* and World of Warcraft's *Xbox 360 Controller / Input controller #1 connected/disconnected* chat messages.
- **ZR + R3 also closes the keyboard.** Press the same shortcut again to close it. What you have already typed stays in the text field.
- **Sharp pictures for games added from the catalog.** Their library background is now loaded in high definition, and their vertical cover fills the whole card in the library. For a game you added before this update, remove it and add it again from the catalog to get its vertical cover.
- **Catalog and Library buttons** at the top of the screen no longer touch each other.

## Signing in to Battle.net

1. Press **L3 + R3** to switch to **Mouse & Keyboard Mode**.
2. Click in the field with the **left stick** and **ZR**, then press **ZR + R3** to open the keyboard and type.
3. Once signed in, press **L3 + R3** again to go back to **Controller Mode**. The keyboard is still opened with **ZR + R3**.

The full guide is in the [FAQ](https://github.com/kazar-root/NxNow/blob/main/FAQ.md).

Everything from v0.2c (more stable streaming in heavy games, much better 60 FPS) and v0.2b is included. A maximum bitrate of **20 Mbps** is still recommended.

**Have fun, everyone!**

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

The app and the Homebrew Menu show this version as **0.2.3**.

## Integrity

SHA-256 (`NxNow.nro`):

```
16fc6f47cadc02e3dd1ba86fa06097ddf1190d9d74be6f3e3add5f280327bf49
```
