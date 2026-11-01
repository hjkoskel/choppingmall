# CHOPPING MALL — C port

A native C11 port of the single-file HTML5 game `index.html` (top-down shooter: a killer robot
loose in a hexagonal mall, three levels, pistol / shotgun / molotov, shops, chiptune music).

* **Window / input / audio output:** [raylib](https://www.raylib.com) 5.5 (vendored, `third_party/raylib`)
* **All drawing:** [PlutoVG](https://github.com/sammycage/plutovg) v1.3.3 (vendored, `third_party/plutovg`) through `src/c2d`, a small
  Canvas-2D-style layer (paths, gradients, clip, dash, shadow blur, text, `'lighter'` blending)
* **Sound:** `src/wa`, a small Web-Audio-style synthesis engine (oscillators, biquads, noise, automation,
  Blink-style dynamics compressor), plus `src/sound` (the game's sfx, music sequencer and stingers)

**Other platforms:** `../chop64/` — Commodore 64 (cc65) · `../nes/` — NES (cc65, NROM-256), sharing the integer game core in `../chop64/src/core.c`. See their READMEs.

## Build and run

```
make            # build/chopping-mall  (builds vendored raylib + PlutoVG on first run)
./build/chopping-mall
```
Linux needs the X11/GL development packages raylib needs (`libx11-dev libxrandr-dev libxi-dev libxcursor-dev
libxinerama-dev libgl1-mesa-dev`, and `libasound2-dev` for sound). The Makefile has link flags for macOS and
Windows (MinGW) but **only Linux was built and run while porting**.

**Controls** — WASD / arrows move · hold SPACE to stop, aim and fire · 1 2 3 weapons · M mute · R restart ·
ENTER/SPACE start · F11 borderless fullscreen · ESC quit. The window is resizable and letterboxed.
The best score is kept in `~/.config/chopping-mall/best.txt` (macOS: `~/Library/Application Support/...`,
Windows: `%APPDATA%\chopping-mall`; override with `CHOPPING_MALL_BEST=path`). Without an audio device the
game runs silently.

Developer flags: `--seed N`, `--silent`, `--frames N`, `--screenshot out.png`, `--replay inputs.txt`
(recorded per-frame inputs at a fixed 1/60 s step), `--window WxH`.

## Tests

```
make test    # 20 recorded traces (real JS vs C), 26 drawing checks, 40 audio checks  (needs python3)
make asan    # same suite under AddressSanitizer + UBSan
make lint    # -Wconversion on the game logic
```
Everything builds with `-std=c11 -Wall -Wextra -Wpedantic -Wshadow -fno-fast-math` without warnings.

### How it was verified (and against what)
| Area | Method | Result |
|---|---|---|
| Game logic | `tools/golden/trace_js.js` runs the **real code from index.html** under node (seeded mulberry32, fixed dt, bot-driven) and records inputs + state snapshots; `tests/trace_replay.c` replays the inputs in C | 20 traces / ~20 000 snapshots identical (worst relative difference 4e-12). Line coverage by the traces: `robot.c` 100 %, `weapons.c` 100 %, `game.c` 99.6 %. A deliberate mutation of a constant is caught. Two inputs are test-only hooks (refill ammo; robot hp = 0.5). |
| Drawing | Real index.html drawing code on a Skia canvas (`@napi-rs/canvas`, `tools/golden/ref_frames.js`) vs `tests/frame_dump.c` | Background mean difference 1.1/255; gameplay frames 1.5/255 in the fire region with matching brightness; the remainder is text (different font) and edge anti-aliasing. Numeric canvas checks: non-premultiplied gradients, `arcTo`, even-odd clip, additive saturation, shadow blur vs the Gaussian edge response. |
| Presentation | Real binary under Xvfb + software GL, GPU screenshot vs CPU compositor | max difference 2/255 on frames with fire, additive blending and screen shake; letterboxed and 2× backing-scale paths checked |
| Audio | Real index.html audio code on an independent Web Audio implementation (`node-web-audio-api`, `tools/golden/ref_audio.js`) vs `tests/audio_render.c` | sfx, clank and engine hum: waveform correlation 1.0000 (0.00 dB). Win/lose/music: RMS within 0.5 %, see compressor note below. Unit checks: automation maths, waveform conventions, filters, compressor closed form, GC, pool exhaustion. |
| Memory | ASan/UBSan over traces, 2000 rendered frames and audio scenarios | no errors, no leaks |

Performance on the (slow, single-core) sandbox: `draw_frame` averages 7.5 ms at 1× and 21 ms at 2× backing
scale (2× is only selected when the on-screen scale is ≥ 1.5).

## Known differences from the HTML version
1. **Fonts.** DejaVu Sans / Sans Bold (subset, embedded) replace "Trebuchet MS / Verdana / sans-serif". Glyphs are wider, so e.g.
   the TIME value nearly touches the ROBOTS column. Everything else about text placement follows the canvas rules (centre / middle baseline).
2. **Carpet dashes at three mitred corners.** PlutoVG butts a dash that straddles a corner; Chrome joins it. A few pixels.
3. **Screen shake** reveals clamped edge pixels instead of the previous frame's pixels (≤ 3 px strip).
4. **Layering.** Normal particles are drawn under the additive layer, and red bolts additively over bullets (the HTML interleaves them).
5. **Audio.** Oscillators use 2-point PolyBLEP/PolyBLAMP (Chrome uses band-limited wavetables): identical waveform on the game's
   sounds, ≥ 9 dB less aliasing than naive at a 2.3 kHz fundamental. The music sequencer schedules per audio block instead of one second ahead.
6. **Compressor.** Follows Blink's `DynamicsCompressorKernel` (make-up gain `(1/sat(1))^0.6`, 6 ms pre-delay); its steady-state gain matches the
   closed form to 0.1 dB. The independent reference implementation deviates from that rule (~1.3 dB hotter), so the stingers' peaks differ by that much from it.
   I could not compare against Chrome itself.
7. **Not ported:** touch controls and the touch HUD strings (desktop only, as specified). Added: ESC quit, F11 fullscreen.

### Plan vs. HTML: weapon unlocks
`nativeC (5).md` (section 6) describes weapon unlocks (pistol level 1, shotgun level 2, molotov level 3), "LOCKED" messages and
shops that stay closed ("OPENS LEVEL n"). The supplied `index.html` contains **none of that** (all weapons are selectable from the
start and every shop is always open). The plan says the HTML is authoritative, so the port follows the HTML. If the unlock feature
is wanted it has to be specified/added to the HTML first.

## Commodore 64 version
`../chop64/` contains a separate, tuned-down, integer-only version of the game that compiles to a real C64 program with cc65 (`cd ../chop64 && make`). See `../chop64/README.md`.

## Layout
```
src/game.c weapons.c robot.c world.c storage.c   game logic (one C function per JS function, same statement order)
src/render.c render_bg.c hud.c                    drawing (js: draw, buildBG, hud) via c2d
src/c2d/                                          canvas-style layer over PlutoVG (+ glow cache, layers, embedded font)
src/wa/ src/sound/                                audio engine, sfx/music/stingers, lock-free command ring
src/main.c                                        raylib window, input, loop, GPU compositing
tests/  tools/golden/                             tests, reference harnesses (node), recorded traces
```
Regenerating references needs node: `cd tools/golden && npm i @napi-rs/canvas node-web-audio-api`, then e.g.
`node trace_js.js ../../../index.html traces/t_x chaos 31 20000` (scenarios: fighter fighter2 molotov random afk medic chaos firekill)
and `node tools/gen_music_data.js ../index.html > src/sound/music_data.c` (`../index.html` is the original game).

## Licences
Game code: as the original. raylib: zlib. PlutoVG: MIT. DejaVu fonts: Bitstream Vera / DejaVu licence (free to embed).
