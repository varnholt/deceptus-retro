# Deceptus Retro

A portable C runtime with working GBA and SNES prototypes. GBA is the first
complete-game target; SNES stays available as a secondary backend. The desktop
preview runs the same C simulation. The folder retains its original name,
`deceptus-snes`.

The full-game port has started; see [PORTING.md](PORTING.md) for the source
inventory, Lua compilation strategy, verified scope and remaining work. The
default GBA ROM is now a four-area playable preview; the whole game remains unfinished.

`deceptus_engine` remains the main repository and the source of truth for game
content. This repository contains code and conversion recipes. Every build
reads the original map, tilesets, collision vectors and animation definitions
from the neighbouring checkout; converted artwork and binary data are ignored.
There is no second maintained copy of the artwork.

Artwork keeps its original size and pixel grid at 1:1. The console screens
shows less of the world; future visibility improvements should use camera
framing and scrolling. Palette conversion for the SNES remains necessary.

## Play the preview

The current workspace already has the build tools, emulator cores and ROMs:

```powershell
cd D:\deceptus\deceptus-snes
python tools/play.py
```

This opens the actual GBA ROM through the project-local mGBA core. Use
`python tools/play.py --backend snes` for the SNES version. The small player
currently renders video without audio output. Ordinary emulators can also open
`build/gba/deceptus.gba` or `build/snes/deceptus.sfc`.

| Keyboard | Console pad | Action |
| --- | --- | --- |
| Left / Right | D-pad | Move |
| Z or Space | B or A | Jump; release early for a shorter jump |
| Up | Up | Interact / continue at an exit |
| Enter | Start | Begin / briefing / pause / replay |
| R | Select | Restart the room |

The GBA preview starts at a title screen. Play through the cell, the full
clockwork antechamber, the spike gallery and the crumbling bridge. Each area
has a briefing; press Start when ready. The clockwork route uses wall jumps:
release and press jump again against a wall. The route finishes with a score
and supports replay. All artwork and enemy/mechanism placements come from the
main engine checkout; the four-area preview progression is curated here.

The standalone SNES scene retains its clock checkpoint, stand-in patrol and
gold portal. State is held in RAM; audio and persistent saves are unfinished.

To create a verified, self-contained GBA playtest ZIP:

```powershell
python tools/package_preview.py --build
```

See [GBA.md](GBA.md) for resource layout and verified limits. Packaged controls
and route help come from `assets/release/`.

An additional development scene exercises the main hall's water surface:

```powershell
python tools/build_gba.py --scene water
python tools/gba_emulator.py --scene water
python tools/play.py --rom build/gba/deceptus-water.gba
```

The build reads the original map's surface and emitters, runs the source spring
equations offline, and composites the original gradient into twelve native-size
frames. The GBA streams visible tile changes at 10 fps using the shared C
animation clock. The emitter schedule repeats deterministically instead of using
the desktop game's random timings. This scene tests surface graphics only;
swimming, reactive player splashes, water tile animations and stencil effects
remain unfinished. It is separate from the four-area preview and tutorial work.

## Build from a fresh checkout

Use Python 3.10+ and the packages in `requirements.txt`. GBA compilation uses
LLVM's `clang`, `ld.lld` and `llvm-objcopy` on PATH. Keep the repositories side by side:

```text
workspace/
  deceptus_engine/data/        original game content
  deceptus-snes/               this repository
```

```powershell
python -m pip install -r requirements.txt
python tools/bootstrap.py --target gba --emulator
python tools/build_gba.py
python tools/play.py
```

The GBA bootstrap downloads the public header reference and, optionally, mGBA
for Windows x64 into `.tools/`. The build uses a small freestanding platform
layer and needs no global console SDK, Docker or MSYS shell. It was verified
with LLVM 18.1.8 on Windows. See [GBA.md](GBA.md) for layout, build options and
current limitations.

For the SNES toolchain and prototype:

```powershell
python tools/bootstrap.py --target snes --emulator
python tools/build_snes.py
python tools/play.py --backend snes
```

The SNES bootstrap installs project-local PVSnesLib 4.7.0 and optionally Snes9x.
Download URLs and hashes are recorded in `.tools/downloads.json`; emulator cores
follow nightly channels. Tools can instead be supplied locally.

```powershell
python tools/build_snes.py --sdk C:/snesdev/pvsneslib --data D:/other/deceptus_engine/data
python tools/play.py --backend snes --core C:/emulators/snes9x_libretro.dll
python tools/build_gba.py --data D:/other/deceptus_engine/data
python tools/play.py --core C:/emulators/mgba_libretro.dll
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
python tools/gba_emulator.py
```

The route boots the ROM, checks movement, jump/landing, simulation timing,
pause/resume and restart, then defeats the patrol, clears the raised ledge and
enters the portal. Screenshots and the state report are written to `artifacts/`.

## Data pipeline

SNES recipe `assets/rooms/catacombs.json` selects a 512x224 crop from the original catacombs
map and names the visual and collision layers. Player frames are looked up in
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

The vector converter is usable independently of the SNES image conversion:

```powershell
python tools/vector_to_tiles.py ../deceptus_engine/data/level-catacombs/catacombs.tmx --layers level box_colliders --crop 3528 2928 512 224 --output artifacts/collision-review
```

It writes a pixel mask, grid preview, row-major collision bytes and a JSON
object table. `--cell` and `--threshold` control the approximation. Object
coordinates in the JSON are relative to the crop, including records outside
the crop so rotated geometry can be considered by later tools. Tile objects
without an explicit collision shape are rejected by the vector rasterizer.

The imported PNGs remain lossless at their original pixel size. SNES conversion
then fits eight 15-colour background palettes to the room's 8x8 tiles. Each tile
selects one palette through its tilemap attributes. The fitter uses actual
source colours on the console's RGB555 grid; it does not dither, resize or blur.
This replaces the first prototype's single 15-colour palette, which discarded
much of the original shading and accent colours. It still cannot reproduce
every original 24-bit colour exactly. See `assets/generated/colour-report.json`
for measured colour error and exact RGB555 pixel coverage.

The default GBA build independently imports four original areas through
`assets/campaigns/tonight.json`. It uses 15 background palette banks plus a UI
bank, a visible-tile cache for whole-room scrolling, and streamed 8bpp object
frames/palettes. Original sprite RGB555 colours are preserved. It does not read
the reduced SNES PNG. Generated imports and resources stay under
`assets/source/campaign/` and `assets/generated/gba/`.

The content tools also export supported rectangle mechanisms from the original
map. GBA connects an original lever to 12 visible, damaging spike traps; SNES
activates the clock checkpoint. Native fixtures also test lever/block links,
sensor edges and dynamically enabled collision. Other crop objects are reported
as unsupported in the respective lossless import’s `mechanisms.json`.

SNES output includes 4bpp planar tiles, flip-deduplicated maps, 15-bit palettes,
object sheets, C collision tables and `assets/generated/budget.json`. A native
test decodes both hardware background maps and checks that they reconstruct
the converted image exactly. The emulator route also compares a background
region from the actual PPU output against the converted image on the RGB555 grid.

## Runtime and future console backends

| Directory | Responsibility |
| --- | --- |
| `include/retro/` | Fixed-width types, engine state, input/events, logical draw records, collision API |
| `src/engine/` | Reusable grid collision, fixed-point math and bounded entity allocation |
| `src/game/` | Deceptus movement, patrol, damage, checkpoint, portal and animation rules |
| `src/platform/snes/` | PVSnesLib input, background setup, OAM packing, display timing |
| `src/platform/gba/` | ARM startup, GBA registers, two background planes, object streaming and display timing |
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
GBA and SNES both build and pass real-ROM integration checks. Mega Drive is not
implemented. Neither existing console build is the complete game yet.

## Current SNES budget and limitations

- 256x224 NTSC output, a 512px-wide room and horizontal camera following.
- Two Mode 1 background planes preserve the converted image: 1,192 unique
  patterns, plus one overflow blank, using 38,176 VRAM bytes and 8,192 map bytes.
- All eight background palette banks are used, with 83 distinct output RGB555
  colours in the current room, compared with 15 in the initial prototype.
- The object sheet uses 15,360 VRAM bytes. Total allocated content is 61,728
  bytes out of 65,536, with unused reserved gaps in the address layout.
- Each logical actor uses up to six 16x16 objects. A conservative overlap
  budget drops excess draws before exceeding per-scanline object/sliver limits.
- The 60Hz simulation catches up across missed rendering deadlines, with a
  maximum of four catch-up ticks. **Rendering currently drops frames**, especially
  while moving or showing more actors. This is not a locked-60fps port.
- No sound/music, save storage, second-room loading, real slope handling or combat
  weapons yet. A limited Lua callback compiler and rectangle mechanism importer
  are implemented; complete enemy scripts, level logic and remaining mechanism visuals/timing remain.
- Verified in Snes9x and on the Windows native build; **not tested on hardware**.

The next useful engine work is the original room graph and persistent room state, improved slopes,
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
