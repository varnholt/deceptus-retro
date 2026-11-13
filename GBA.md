# GBA backend

For the current twelve-room opening, build `--scene tutorial`. See
[building](docs/BUILDING.md), [architecture](docs/ARCHITECTURE.md) and
[status](docs/STATUS.md). The four-area preview described below is a separate,
retained verification route and remains the build script default.

The default build is a short, four-area playable preview. It has a title,
briefings, original rat and arrow-trap placements, checkpoints, wall jumps,
linked spike traps, collapsing platforms, room transitions, and finish/replay.
It is not the complete original game. Full progression, narrative, combat,
audio and persistent saves remain in `PORTING.md`.

## Build, verify and package

Install the Python requirements and LLVM (`clang`, `ld.lld`, `llvm-objcopy`):

```powershell
python tools/bootstrap.py --target gba --emulator
python tools/build_gba.py
python tools/gba_emulator.py
python tools/play.py
python tools/package_preview.py --build
```

`--scene tonight` is the default. The self-contained cartridge is
`build/gba/deceptus-tonight.gba`; the latest build is also `deceptus.gba`.
The package script runs native tests and the real-ROM playthrough, checks the
packaged ROM hash, and creates a dated ZIP under `build/release/`. No emulator,
BIOS, development tools or source artwork folders are bundled.

The preview is currently 2MiB. The builder pads to at least 512KiB, fixes the
header/checksum and retains ELF symbols. The current mGBA core misidentifies
256KiB images as multiboot; the minimum padding avoids that path. ROMs can grow
up to the 32MiB cartridge limit.

`--data PATH` selects another authoritative engine checkout. Tool executable
paths can be supplied with `--clang`, `--linker` and `--objcopy`. The CMake
`gba_rom` target invokes the same default build. PVSnesLib is not needed.

Standalone development scenes remain available:

```powershell
python tools/build_gba.py --scene lever
python tools/gba_emulator.py --scene lever
python tools/build_gba.py --scene platforms
python tools/gba_emulator.py --scene platforms
python tools/play.py --rom build/gba/deceptus-platforms.gba
```

`--scene cell` and `--scene preview` build the individual enemy/whole-room areas.
The SNES prototype remains separate and retains its clock/patrol/portal scene.

## Source content

`assets/campaigns/tonight.json` describes the curated route and goals. Its four
recipes point into the original catacombs map: the cell (720x408), the clockwork
antechamber (1224x480), the spike gallery crop (768x224), and the crumbling-bridge
crop (768x224). These are preview links, not the original full-game room graph.

All artwork stays at 1:1. Import reads original Tiled layers, collision vectors,
player animation definitions, sprites, Lua constants, and the original 3x5 font.
Generated imports and target resources are ignored. No maintained artwork copy
is introduced. SNES conversion is never an input to GBA conversion.

Rat paths use the same tile-to-pixel conversion as the original `TmxEnemy`.
Rat speed, acceleration, cycle lengths and idle delay are extracted from Lua.
Arrow delay/period settings come from the original initializer; the C projectile
service implements the bow launch multiplier. Unsupported enemies and paths
outside the loaded area remain explicit in the content report. The clockwork
room's blob and its rat's cross-room patrol are not included in this preview.

The rat port uses deterministic random choices and arrival waits. Physics uses
8px collision cells rather than Box2D circles/joints. Wall jumping is enabled
for the clockwork traversal; its tuning approximates the original movement.
Arrow collision uses sub-cell point steps and cardinal sprite rotations.
These are bounded native components, not a general Lua translator.

## Background and object streaming

Each campaign area fits 15 background palette banks; bank 15 is reserved for
readable UI. Colours are selected per 8x8 tile with no dithering or resampling.
Exact source RGB555 pixel coverage is about 92.7%, 94.6%, 96.2% and 99.5% for
the four areas. Detailed palette metrics stay in each area's `budget.json`.

Room pattern libraries live in ROM. A visible-tile cache maps global pattern ids
to 1,534 usable VRAM slots and updates a wrapping 64x32 map on both axes. The
clockwork room has 3,366 unique patterns, so it is not preloaded into VRAM.
Room loads clear cache mappings before switching the palette and source tables.

Actor and mechanism sprites use 8bpp OBJ graphics with disjoint palette ranges
where required by the loaded scene. All opaque source RGB555 sprite colours are
preserved. The shared ROM library contains 387 source frames: lever, spike,
on/off block, collapsing-platform, rat, trap-body/muzzle and rotated arrow
frames. Visible resources share uploads by id, within 32KiB OBJ VRAM and 128 OAM
entries. Incompatible overlapping animated palettes or excess resources fail
explicitly; general palette allocation for arbitrary scenes remains unfinished.

| Region | Use |
| --- | --- |
| `0x06000000` | Primary 4bpp pattern slots |
| `0x06008000` | Secondary pattern slots; campaign slots stop before `0x0600c000` |
| `0x0600c000` | Source-font UI glyphs |
| `0x0600d000` | UI map |
| `0x0600e000` | Primary 64x32 map |
| `0x0600f000` | Secondary 64x32 map |
| `0x06010000` | Actor slots followed by cached mechanism/enemy resources |

The playfield uses mode 0 and DMA3 during VBlank, with explicit DMA-start delay.
Hot renderer and simulation code is copied into IWRAM; renderer code uses ARM
instructions and portable engine code uses Thumb. Linker limits protect ROM,
EWRAM and the 28KiB code region below the reserved IWRAM stacks. The runtime has
no heap, floating point, host C library, interrupts, audio or save backend.

## Tutorial renderer and UI

The tutorial playfield uses mode 0: BG0 behind the player (priority 3), BG1 in
front (priority 1), OBJ at priority 2 and BG2 UI at priority 0. Source layer z
values select occluding artwork. Water body/foam are currently baked behind
the actor; original translucent/stencil water effects are not complete.

Pattern residency has separate 1,023-slot background and 511-slot foreground
pools. Reference counts keep still-visible tiles pinned while incoming/outgoing
rows and columns stream at 8px boundaries. Dirty animation rectangles update
separately. Room loads or larger camera changes rebuild the visible window.
The working window is 32x22 cells, including scroll margins.

UI resources use `0x0600c000..0x0600cfff` plus
`0x0600d800..0x0600dfff`, skipping the UI tilemap at `0x0600d000`. Corner cue
labels are shifted into their own scratch patterns so they stay on screen
while the world indicator moves. Transient room names reserve separate patterns
when a cue replaces the ordinary font resource, avoiding glyph corruption.

The title changes to mode 1 with an affine BG2 starmap texture and BG0 text.
A 4,096-entry sine table updates the matrix every rendered frame. It uses a
native 128x128 crop from the original starmap texture, without a runtime 3D mesh.
Returning to play restores the normal video configuration and tile cache.

Tutorial dialogue/menu text comes directly from the original Deceptum bitmap
atlas at 13px. Headings use exact integer 2x pixels. Source artwork and native
font pixels are never horizontally fitted or smoothed to fit the display.
Player graphics contain 26 logical frames: idle/run/airborne, the retained
prototype portal, and seven source bend frames. Crouching is shared simulation;
full crawl progression is not implemented by merely pressing Down.

The title/controls/inventory/pause menus and original dialogue windows are baked
offline into fixed-budget resources. No runtime font rasterizer or PSD parser
is included in the cartridge. `tools/verify_tutorial.py` runs controlled-start
scene checks and restores the production ROM afterward. It is separate from
the older preview's complete curated input route.

## Verification and limits

`tools/verify_preview.py` plays from title through all exits, wall jumps, lever
links and finish/replay without altering emulator memory. It also checks pause,
restart, briefings, spike death, collapsing collision, arrow damage, checkpoint
recovery, streaming pixels in each area and 1,274 display frames of simulation
at one tick per display frame. The ZIP includes its ROM-hash-matched report.

Native tests reconstruct full backgrounds, decode all 387 source sprite frames,
exercise campaign transitions and mechanism queues, and compare C arrow firing
against execution of the actual Lua script when the original Lua sources are
available. Existing SNES and standalone GBA ROM routes still run.

Tested with LLVM 18.1.8 and mGBA `0.11-212-7a12d6d` on Windows. No physical-console
result is claimed. Collapse visuals use deterministic shake/base destruction
speeds and remain hidden during the return fade delay. Full original-physics
fidelity, arbitrary room graphs, other enemy types, complete Lua event dispatch,
progression, combat, audio and saves are outside the verified preview scope.
