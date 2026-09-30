# Upstream

`game/` and `portable/` come from **[YomotsuHisami/th11](https://github.com/YomotsuHisami/th11)**
at commit `7a315bcdf739c5c67717ba56b0d612e97f70835c` (2026-09-28):

| Here | Upstream |
| :--- | :--- |
| `game/` | `th11_web/cpp/game/` |
| `portable/sdl/` | `portable/sdl/` (renderer, shaders, state, miniaudio / stb headers) |
| `portable/input/` | `portable/input/` (keyboard map, touch gestures) |
| `src/GraphicsDevice.*`, `src/AudioDevice.*`, `src/FontDevice.*`, `src/main.cpp` | adapted from `th11_web/cpp/sdl/` |

Every change to the upstream files is in [`switch-port.patch`](switch-port.patch)
(`patch -p1` from the repository root against a fresh upstream copy). In short:

* **64-bit (AArch64) safety** — the original kept a few pointers in 32-bit
  slots. `AnmVm::reserved_418/430` become pointer-sized; the ECL return
  address pushed on the script stack goes through `pointer_handle()`
  (`game/PointerHandles.cpp`). Byte-layout `static_assert`s that only hold for
  32-bit pointers are kept via `TH_LAYOUT_ASSERT` (active on 32-bit builds);
  nothing in the game reads those structs by original byte offset.
* **Renderer** — SDL3/WebGL2 → SDL2/GLES 3 (Switch Mesa): window creation,
  `EXT_clip_control` through the GLES loader, aspect-correct pillarboxed
  present, `packed` (a reserved GLSL ES word Mesa rejects) renamed.
* Include paths flattened.

Game logic, timing, RNG and the replay format are untouched.

When upstream updates: copy the new `th11_web/cpp/game` and `portable/`
over these folders and re-apply `switch-port.patch`.
