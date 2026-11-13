# Deceptus SNES

A playable SNES prototype and the start of a portable C runtime for SNES, GBA
and Mega Drive. SNES is the only implemented console backend. The desktop
preview runs the same C simulation.

`deceptus_engine` remains the main repository and the source of truth for game
content. This repository contains code and conversion recipes. Every build
reads the original map, tilesets, collision vectors and animation definitions
from the neighbouring checkout; converted artwork and binary data are ignored.
There is no second maintained copy of the artwork.

## Play the prototype

The current workspace already has the SDK, emulator core and built ROM:

```powershell
cd D:\deceptus\deceptus-snes
python tools/play.py
```

This opens the actual ROM through a project-local Snes9x core. The small player
currently renders video without audio output. An ordinary SNES emulator can
also open `build/snes/deceptus.sfc`.

| Keyboard | SNES pad | Action |
| --- | --- | --- |
| Left / Right | D-pad | Move |
| Z or Space | B or A | Jump; release early for a shorter jump |
| Up | Up | Enter the gold portal |
| Enter | Start | Pause / resume |
| R | Select | Restart the room |

Walk right, jump onto the patrol actor to defeat it, then jump onto the raised
platform and enter the gold portal. The prototype portal returns to the room's
checkpoint. Contact damage also returns there. It does not load a second room.

## Build from a fresh checkout

Use Python 3.10+ and Pillow. Keep the repositories side by side:

```text
workspace/
  deceptus_engine/data/        original game content
  deceptus-snes/               this repository
```

```powershell
python -m pip install -r requirements.txt
python tools/bootstrap.py --emulator
python tools/build_snes.py
python tools/play.py
```

The bootstrap downloads PVSnesLib 4.7.0 into `.tools/pvsneslib` and, on Windows
x64, an optional Snes9x libretro core. No global SDK installation, Docker or
MSYS shell is required. Download URLs and hashes are recorded in the ignored
`.tools/downloads.json`. The emulator download follows a nightly channel;
PVSnesLib is pinned by version. Both tools can instead be supplied locally.

```powershell
python tools/build_snes.py --sdk C:/snesdev/pvsneslib --data D:/other/deceptus_engine/data
python tools/play.py --core C:/emulators/snes9x_libretro.dll
```

The build script also handles the SDK's native Linux/macOS executable names.
Those platforms have not been exercised in this workspace. Supply a native
libretro core explicitly for emulator checks outside Windows x64.

## Native development build

The host build needs CMake and a C compiler, but no SNES SDK:

```powershell
cmake -S . -B build/host
cmake --build build/host --config Debug
ctest --test-dir build/host -C Debug --output-on-failure
python tools/play.py --backend host
```

For another content checkout, configure with
`-DDECEPTUS_DATA_DIR=D:/other/deceptus_engine/data`. The preview's default library
path is the Windows multi-configuration build; pass `--library PATH` for other
layouts. The host preview is for simulation work, and does not emulate SNES
rendering budgets or reproduce exact SNES colour rounding.

Run actual ROM checks after changing its backend:

```powershell
python tools/emulator.py
```

The route boots the ROM, checks movement, jump/landing, simulation timing,
pause/resume and restart, then defeats the patrol, clears the raised ledge and
enters the portal. Screenshots and the state report are written to `artifacts/`.

## Data pipeline

`assets/rooms/catacombs.json` selects a 768x336 crop from the original catacombs
map and names the visual and collision layers. It converts artwork at two-thirds
scale into a 512x224 background: original 24px terrain tiles become 16px tiles,
and the visible player is approximately 24px tall. The screen shows 384x336
source pixels, 50% more in each direction than the initial full-size prototype.
Nearest-neighbour conversion keeps sharp pixels but discards some fine detail.
Player frames are looked up in
the original `sprites/animations.json`; the texture is read from that checkout.
The converter currently imports eight idle frames, eight run frames and two
airborne frames. The patrol reuses those frames with a different palette; its
appearance and the gold portal are prototype stand-ins.

`tools/tiled.py` reads external and embedded tilesets, CSV/base64 layers,
zlib/gzip compression, nested group offsets and Tiled flip flags. It rasterizes
tile/object collision rectangles, rotated rectangles, polygons, ellipses and
polylines. A pixel-centre fill rule avoids adding an extra row/column along
rectangle boundaries. Finite orthogonal maps and atlas tilesets are supported;
unsupported encodings and image-collection tilesets are rejected explicitly.

The collision grid uses 8px cells and an occupied-area threshold. Solid and
one-way cells are separate. **Slopes currently become steps**; continuous slope
collision is future work. Shader effects, lights and parallax are flattened or
omitted, and arbitrary mechanisms/enemy scripts are not automatically translated.
Collision, movement and object coordinates stay in original map units; the
renderer converts absolute actor and camera coordinates using the same scale.
This avoids camera-dependent rounding differences between actors and scenery.

The vector converter is usable independently of the SNES image conversion:

```powershell
python tools/vector_to_tiles.py ../deceptus_engine/data/level-catacombs/catacombs.tmx --layers level box_colliders --crop 3528 2832 768 336 --output artifacts/collision-review
```

It writes a pixel mask, grid preview, row-major collision bytes and a JSON
object table. `--cell` and `--threshold` control the approximation. Object
coordinates in the JSON are relative to the crop, including records outside
the crop so rotated geometry can be considered by later tools. Tile objects
without an explicit collision shape are rejected by the vector rasterizer.

SNES output includes 4bpp planar tiles, flip-deduplicated maps, 15-bit palettes,
object sheets, C collision tables and `assets/generated/budget.json`. A native
test decodes both hardware background maps and checks that they reconstruct
the converted image exactly.

## Runtime and future console backends

| Directory | Responsibility |
| --- | --- |
| `include/retro/` | Fixed-width types, engine state, input/events, logical draw records, collision API |
| `src/engine/` | Reusable grid collision, fixed-point math and bounded entity allocation |
| `src/game/` | Deceptus movement, patrol, damage, checkpoint, portal and animation rules |
| `src/platform/snes/` | PVSnesLib input, background setup, OAM packing, display timing |
| `src/platform/host/` | Shared-library bridge for the desktop preview |
| `tools/` | Original-content import, offline conversion, build and verification |

Shared code uses C90, caller-owned state, fixed-capacity queues/pools and no
runtime heap allocation or floating point. Room-local positions use signed
12.4 fixed point: a 1/16px step and a maximum positive coordinate below 2048px.
Large worlds will use separate room/chunk origins. The two collision movement
functions accept body dimensions and require velocity below one cell per tick.

Input bits and logical actor frames are independent of SNES registers. A new
backend must map input, pack those frames into its own hardware objects, manage
VRAM uploads and implement display timing. Offline SNES planar packing stays
in Python. GBA and Mega Drive converters can consume the same authoritative
content and collision geometry while supplying their own tile and palette
encoders. Reusing a game design across all three still needs target-specific
art, audio and performance budgets.

The current `retro_engine`/`retro_room` structs include prototype game state and
spawn fields. They are a starting interface, not a frozen general engine ABI.
There are no GBA/Mega Drive backend stubs and neither target is claimed working.

## Current SNES budget and limitations

- 256x224 NTSC output, a 768px-wide source room rendered at two-thirds scale
  into a 512px-wide background, and horizontal camera following.
- Two Mode 1 background planes preserve the converted image: 1,104 unique
  patterns, plus one overflow blank, using 35,360 VRAM bytes and 8,192 map bytes.
- The object sheet uses 15,360 VRAM bytes. Total allocated content is 58,912
  bytes out of 65,536, with unused reserved gaps in the address layout.
- Each logical actor uses up to six 16x16 objects. A conservative overlap
  budget drops excess draws before exceeding per-scanline object/sliver limits.
- The 60Hz simulation catches up across missed rendering deadlines, with a
  maximum of four catch-up ticks. **Rendering currently drops frames**, especially
  while moving or showing more actors. This is not a locked-60fps port.
- No sound/music, save storage, second-room loading, real slope handling, combat
  weapons, Lua translation or general mechanism importer yet.
- Verified in Snes9x and on the Windows native build; **not tested on hardware**.

The next useful engine work is room/resource streaming, improved slopes,
profiling the 65816 hot paths, a platform audio consumer for game events,
save-data serialization, and declarative mechanism/entity import. Add another
console backend after those interfaces have been exercised by more than one room.

## Ownership

Original code/tools here use the MIT license in `LICENSE`. Deceptus game
content retains the main checkout's content terms; artwork remains owned by
dstar. Refer to `deceptus_engine/LICENSE.md`, `data/LICENSE.md` and
`LICENSES/GAME-CONTENT.txt` before distributing any generated ROM or converted
art. A local import does not change those terms. PVSnesLib and Snes9x retain
their respective upstream licenses; the downloaded packages are not tracked.
