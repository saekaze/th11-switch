# Touhou 11: Subterranean Animism — Nintendo Switch Port
*(東方地霊殿　〜 Subterranean Animism)*

![Platform](https://img.shields.io/badge/Platform-Nintendo%20Switch-e60012?style=for-the-badge&logo=nintendoswitch&logoColor=white)
![Status](https://img.shields.io/badge/Status-Fully%20Playable-brightgreen?style=for-the-badge)
![License](https://img.shields.io/badge/Port%20Code-CC0%201.0-blue?style=for-the-badge)

A native homebrew port of ZUN's 2008 bullet hell **Touhou 11: Subterranean Animism** for the **Nintendo Switch** (Horizon OS).

This build runs the clean C++ reimplementation from [YomotsuHisami/th11](https://github.com/YomotsuHisami/th11), compiled **natively for ARM64** (no wasm, no emulation) and driven by an SDL2 host on an OpenGL ES 3 context — no Linux, Box64 or Wine involved.

Companion to the [Touhou 6](https://github.com/saekaze/th06-switch), [Touhou 7](https://github.com/saekaze/th07-switch), [Touhou 8](https://github.com/saekaze/th08-switch), [Touhou 9](https://github.com/saekaze/th09-switch) and [Touhou 10](https://github.com/saekaze/th10-switch) Switch ports, with the same `touhou11.nro` + `sd:/switch/touhou/touhou11/` layout.

---

## 🆕 What's New — first release (1.00a-r3)

* 🌸 The full game: all six shot types, every difficulty, all 6 stages and Extra, endings, staff roll, music room and replays.
* 🎮 Remappable controls through **Option → Key Config** (the D-Pad and sticks only move).
* ⏳ Loading bar for the one-time dialogue-font build on first launch (about 13 seconds).
* 📁 Recommended folder: `sd:/switch/touhou/touhou11/`, next to the other Touhou ports.

---

## ✨ Key Features

* ⬛ **OLED-Friendly Pillarboxing:** the original 640×480 picture is centred with pure black (`#000000`) bars and an aspect-correct upscale.
* 🔊 **Full Audio:** sound effects plus BGM streamed straight out of your `thbgm.dat` (original 16-bit PCM and loop points — no conversion step).
* 🎮 **Sane, Remappable Controls:** Joy-Con (handheld, grip, detached) and Pro Controller with the same default layout as every other Touhou Switch port (B shoot, A bomb, L/ZL focus, R/ZR skip, + pause); the in-game **Key Config** can rebind them.
* 👆 **Touch Screen:** in handheld mode, drag to move and tap menus — upstream's touch controller, mapped through the pillarbox.
* 🈂️ **Japanese Text Without Windows:** dialogue glyphs are baked with FreeType from `msgothic.ttc` / `msmincho.ttc` (or the Switch's built-in Japanese font) on first launch and cached.
* 🌏 **Language-Aware Title:** hbmenu shows `東方地霊殿　～ Subterranean Animism` on consoles set to 日本語 and the romanised title everywhere else.
* 💾 **Saves Next to the Data:** `th11.cfg`, `scoreth11.dat`, replays (`replay/th11_01.rpy` …) and snapshots are written into the SD folder the game loaded from.

---

## 📥 Installation Guide

> ⚠️ **Disclaimer:** In compliance with ZUN's guidelines and copyright law, this repository contains **ONLY the homebrew engine code**. No game assets are distributed. You must legally own a copy of *Touhou 11: Subterranean Animism v1.00a*.

### 1. SD Card File Structure

1. Ensure your Nintendo Switch is running custom firmware (Atmosphère CFW).
2. Download the latest `touhou11.nro` from the [Releases](../../releases) tab (or build from source).
3. Create the folder `sd:/switch/touhou/touhou11/` and copy the following into it:

```text
sd:/switch/touhou/touhou11/
    ├── touhou11.nro          # Nintendo Switch homebrew executable
    ├── th11.dat              # Main game archive (v1.00a)
    ├── thbgm.dat             # Background music archive
    ├── msgothic.ttc          # MS Gothic (C:\Windows\Fonts) - recommended
    └── msmincho.ttc          # MS Mincho (C:\Windows\Fonts) - optional
```

`thbgm.dat` is optional (the game runs silent without it). Without the MS fonts the console's built-in Japanese font is used.

**Recommended place: `sd:/switch/touhou/touhou11/`.** Keeping every Touhou port in one `sd:/switch/touhou/` folder (`touhou6`, `touhou7`, `touhou8` …) is much tidier than a separate folder per game. Other places still work: the port first looks in its own folder (wherever the NRO is), then for a `th11` / `touhou11` folder (any capitalisation) directly on the SD card, in `switch/`, `touhou/`, `switch/touhou/`, `games/` or `roms/`.

The **first launch** bakes the dialogue font into `fontcache/` (about 13 seconds, with a loading bar); later launches load it almost instantly. If you have the GDI-baked tables from an upstream web build (`th11_web/assets/sdl-native/fonts/`), copy that folder to `sd:/switch/th11/fonts/` for pixel-exact Windows text.

### 2. Launching

Run `touhou11.nro` from the **Homebrew Menu (hbmenu)**, **Sphaira launcher**, or a home screen forwarder. Title-menu **Quit** returns to hbmenu. If something is missing, the port shows what and where it looked, and `th11-switch.log` is written next to the saves.

---

## 🕹 Controls

| Nintendo Switch Button | Action |
| :--- | :--- |
| **Left Stick / D-Pad** | Character Movement |
| **B** | Shoot / Confirm |
| **A** | Bomb / Cancel |
| **L / ZL** | Focus (Precision Slow-Motion Movement) |
| **R / ZR** | Skip Dialogue (hold) |
| **+ (Plus)** | Pause / In-Game Menu |
| **X** | Enter (while not bound in Key Config) |
| **Y** | R key (Retry shortcut, while not bound in Key Config) |
| **− (Minus)** | Snapshot (saved to `snapshot/` next to the game data) |
| **Touch** (handheld) | Drag to move, tap menus |

**The same default layout in every Touhou Switch port:** B shoots, A bombs, L/ZL focuses, R/ZR skips dialogue, + pauses. These are only defaults — the Switch buttons act as the game's own gamepad, so the in-game **Key Config** can rebind them, and **Default** there brings this layout back. The D-Pad and sticks only move — they can never be picked as a button. Key Config numbers: 0 B, 1 A, 2 L/ZL, 3 R/ZR, 4 +, 5 X, 6 Y.

---

## 🛠 Building from Source

### Automated Build (GitHub Actions)

`.github/workflows/build-switch.yml` (a copy also lives in `scripts/`) compiles `touhou11.nro` inside the official `devkitpro/devkita64` container and uploads it as an artifact; tagging `v*` also publishes it as a release asset. A second job builds the same sources for Linux and runs the host tests.

### Local Build (Linux / macOS / WSL)

1. Install [devkitPro](https://devkitpro.org/wiki/Getting_Started) with `devkitA64` and `libnx`.
2. Install the required Switch portlibs:

   ```bash
   sudo dkp-pacman -Syu
   sudo dkp-pacman -S switch-dev switch-mesa switch-libdrm_nouveau switch-sdl2 switch-freetype switch-libpng switch-bzip2 switch-zlib switch-harfbuzz
   ```

3. Build:

   ```bash
   export DEVKITPRO=/opt/devkitpro
   ./scripts/build_switch.sh
   ```

   The result is `build-switch/touhou11.nro`.

Desktop Linux test build and host tests (same sources; needs `libsdl2-dev libfreetype-dev libgles-dev`, and `xvfb` + a CJK font for the full test set):

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build
xvfb-run -a ./build/th11_host_tests
./build/touhou11 /path/to/folder/with/th11.dat
```

---

## 📂 How It Works

The game logic is the architecture-independent C++ from upstream (`game/`), compiled natively for AArch64 with the upstream's own code-generation contract (`-ffp-contract=off -fno-strict-aliasing -fno-exceptions -fno-rtti`). The few places where the original stored pointers in 32-bit slots are patched for 64-bit — see [`upstream/UPSTREAM.md`](upstream/UPSTREAM.md) and the exact [`switch-port.patch`](upstream/switch-port.patch).

| File | Purpose |
| :--- | :--- |
| `game/` | upstream TH11 game logic (session, ECL/ANM VMs, player, bullets, lasers, menus, replays, …) |
| `portable/sdl/Renderer.*` | upstream semantic GLES renderer, ported SDL3/WebGL2 → SDL2/GLES 3 |
| `src/main.cpp` | upstream `Application` wiring + Switch main loop, controls, touch, snapshots |
| `src/GraphicsDevice.*` | ANM textures → renderer surfaces |
| `src/AudioDevice.*` / `src/BgmStream.*` | miniaudio mixer; BGM streamed from `thbgm.dat` with original loop points |
| `src/FontDevice.*` / `src/FontBaker.*` | dialogue glyph tables (upstream GDI tables, cached FreeType bake, or fresh bake) |
| `src/Platform.*` | SD paths, files, log |
| `platform/switch/icon.jpg` | 256×256 NRO icon (the original game cover) |
| `scripts/build_switch.sh` | one-shot Switch build |
| `scripts/nacp_lang.py` | Japanese NACP title slot |
| `scripts/gen_cp932.py` | CP932 → Unicode table for the glyph baker |
| `tests/test_host.cpp` | host tests for the port layer |

The game ticks once per vsync (60 Hz); after a real stall it catches up by at most four ticks, like upstream's frame cadence. The CPU is set to the same 1785 MHz boost the other native ports use.

---

## 🤝 Credits & Acknowledgments

* **ZUN / Team Shanghai Alice** — original creator and developer of the Touhou Project series.
* **[YomotsuHisami](https://github.com/YomotsuHisami/th11)** — the clean-room TH11 C++ reimplementation this port is built on.
* **miniaudio** (David Reid), **FreeType**, **SDL2** — audio mixing, font rasterisation, windowing.
* **Switchbrew & devkitPro Team** — the open-source `libnx` SDK and Switch toolchain.

**Licensing:** the Switch host code (`src/`, `scripts/`, `tests/`, build files) is CC0 1.0 (see `LICENSE`). `game/` and `portable/` are upstream code by YomotsuHisami, which does not state a licence — see [`upstream/THIRD-PARTY-NOTICES.txt`](upstream/THIRD-PARTY-NOTICES.txt). Bundled third-party headers keep their own licences (`portable/sdl/third_party/`).
