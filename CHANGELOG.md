# Changelog — Touhou 11 Switch port

## Touhou 11: Subterranean Animism — Switch Port r3

## What's new in r3

* **Remappable controls:** the Switch buttons now act as TH11's gamepad, so
  Option → Key Config rebinds shot, bomb, focus, pause and skip (saved in
  `th11.cfg`). First-launch defaults are the same layout as before. Only
  B, A, L/ZL, R/ZR, +, X and Y can be picked; the D-pad and sticks only move.
  Left-stick click no longer doubles as focus.
* Display version `1.00a-r3`.

---

## Touhou 11: Subterranean Animism — Switch Port r2

## What's new in r2

* **Crash fix:** stages crashed as soon as an enemy script jumped backwards
  (a loop). ECL jump offsets are signed; upstream reads them as 32-bit
  unsigned, which only wraps correctly on 32-bit targets. On the Switch's
  64-bit CPU the jump landed 4 GiB away. Host test added.
* **Loading bar** while start-up reads `th11.dat` and, on the first launch
  only, bakes the dialogue font (about 13 s on hardware; later launches load
  it from `fontcache/`). Start-up timings are written to `th11-switch.log`.
* Display version `1.00a-r2`.

---

## Touhou 11: Subterranean Animism — Switch Port r1

Native Nintendo Switch homebrew port of TH11 v1.00a (`touhou11.nro`).

First release. The game logic is YomotsuHisami's TH11 C++ reimplementation,
compiled natively for AArch64; the Switch host (SDL2 + OpenGL ES 3 + libnx)
follows the TH10 port's layout and controls.

## What's in r1

* Full upstream game: title, all characters/partners, story, Extra,
  practice, spell cards, replays, music room, endings, results.
* 640×480 picture pillarboxed on pure black, aspect-correct upscale.
* BGM streamed directly from `thbgm.dat` with the original loop points.
* Dialogue font baked with FreeType from `msgothic.ttc` / `msmincho.ttc`
  (or the Switch's Japanese system font) on first launch, then cached.
* Fixed Switch controls (see README), handheld touch, − (Minus) snapshots.
* hbmenu title in Japanese on 日本語 consoles, romanised elsewhere.
  Display version is `1.00a-r1`.

## Install

Copy to `sd:/switch/th11/` next to your legally owned game data:

```text
sd:/switch/th11/
    ├── touhou11.nro
    ├── th11.dat
    ├── thbgm.dat     # optional (BGM)
    ├── msgothic.ttc  # recommended (system font is the fallback)
    └── msmincho.ttc  # optional
```

Rebuild with `./scripts/build_switch.sh` (devkitA64 + switch portlibs).

Game data is **not** included — you must own a copy of TH11 v1.00a.
