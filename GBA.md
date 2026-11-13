# GBA backend

GBA is the first complete-game target. The current ROM runs the existing
mechanism scene through the shared C engine. Room streaming, original enemy
types, full level scripts, progression and audio remain in the full-game work
listed in `PORTING.md`.

## Build and play

Install the Python requirements and LLVM with `clang`, `ld.lld` and
`llvm-objcopy` available on PATH:

```powershell
python -m pip install -r requirements.txt
python tools/bootstrap.py --target gba --emulator
python tools/build_gba.py
python tools/gba_emulator.py
python tools/play.py
```

The current workspace already has these tools and a built
`build/gba/deceptus.gba`. `build/gba/deceptus.elf` retains symbols for debugging.
The ROM is padded to at least 512KiB with a valid header, checksum and ARM
entry point. It is currently 1MiB with the animation library. The size can grow
automatically up to the 32MiB cartridge address limit.

Use `--data PATH` for a different main-engine checkout. `--clang`, `--linker`
and `--objcopy` accept executable paths. After configuring the native CMake
build, `cmake --build build/host --target gba_rom` is an alternative entry point.
The GBA build does not require PVSnesLib or the SNES compiler.

The backend was tested with LLVM 18.1.8 on Windows x64 and mGBA
`0.11-212-7a12d6d`. The scripts use platform-independent paths but other host
platforms have not been exercised here; pass a native libretro core with
`--core PATH`. The small desktop player shows video without audio output.

Bootstrap keeps the unchanged public devkitPro `gbafix.c` reference in
`.tools/gbafix-reference.c`. Its mandatory logo/header data is used when preparing
the ROM header. The reference file keeps its upstream license notice. mGBA is
also downloaded into `.tools/`, preserving its own terms. Download URLs and
hashes are recorded locally. Neither third-party source/binary nor generated
game artwork is committed to this repository.

## Original content and graphics

`tools/gba_assets.py` invokes the shared lossless import directly on
`../deceptus_engine/data`. It never reads the SNES-converted image. Original
pixel positions and sprite dimensions stay at 1:1; no dithering is enabled.

- Backgrounds use 16 palettes of 15 opaque colours, selected per 8x8 tile.
- The current 768x224 background uses 1,621 unique 4bpp patterns. Its measured
  palette error is recorded in `budget.json`; about 96.7% of pixels retain their
  exact source RGB555 colour.
- The player keeps all 48 opaque RGB555 source colours. Mechanism frames use
  separate palette ranges, uploaded with their graphics. No mechanism colour
  reduction or sprite resampling is performed.
- The generated ROM library contains 257 original frames: 33 lever, 40 spike,
  32 on/off block and 152 collapsing-platform frames. Only visible resources
  are uploaded; repeated instances share their current frame in OBJ VRAM.
- Visible actor slots are followed by mechanism resources in the 32KiB OBJ
  area. Bounds checks reject excess graphics or OAM objects. Incompatible
  simultaneous animated palettes within one family fail explicitly; general
  per-instance palette allocation remains work for larger scenes.

Generated output stays in `assets/generated/gba/`, separate from SNES output.
`tools/world_export.py` supplies both targets with the same collision and
mechanism definitions. GBA uses `assets/rooms/gba-lever.json` or `gba-platforms.json` and imports into
`assets/source/gba/`; SNES retains its clock-room recipe.

## Hardware layout

| Region | Use |
| --- | --- |
| BG VRAM `0x06000000` | First 1,024 4bpp patterns in character block 0 |
| BG VRAM `0x06008000` | Overflow patterns in character block 2 |
| BG VRAM `0x0600e000` | Primary 64x32 map in screen blocks 28–29 |
| BG VRAM `0x0600f000` | Overflow 64x32 map in screen blocks 30–31 |
| OBJ VRAM `0x06010000` | Visible actor frames, then deduplicated mechanism resources; 32KiB total |
| Palette RAM `0x05000000` | 256 background entries |
| Palette RAM `0x05000200` | Original actor and mechanism colours |

The display uses mode 0, two background planes and 1D object mapping. Graphics
and OAM updates happen during VBlank through DMA3. Startup code sets system/IRQ
stacks and initializes data/BSS. Platform-local memory routines satisfy the
freestanding ARM ABI; no host C library, heap or floating point is used by the
runtime. Linker assertions check ROM and EWRAM limits.

The shared camera now follows both axes. GBA's 240x160 viewport is a view onto
the same source coordinates: it does not shrink the artwork to fit. The SNES
prototype retains its 256x224 view and its existing appearance.

## Verified scope and limits

The default scene imports original lever `lever_spike_06` (object 113) and its
12 named spike-block targets from the catacombs map. Start beside the lever.
Press **Up** to retract or extend the traps. Walk right, then jump onto the
raised floor; the same route is safe with traps retracted and lethal with them
extended. **R** restores the source states. Holding Up toggles only once.

The original links drive C mechanism states directly; this chain does not need
Lua. The full level Lua event dispatch is still not connected. Lever travel and
idle frames, the one-second interaction cooldown, 40fps trap animation, and the
deadly frame range are implemented in portable C. Rapid switch requests use a
bounded eight-entry queue; overflowing requests fail without a partial lever
chain or Lua callback write.

A second original area demonstrates collapsing platforms:

```powershell
python tools/build_gba.py --scene platforms
python tools/gba_emulator.py --scene platforms
python tools/play.py --rom build/gba/deceptus-platforms.gba
```

Stand on a platform for just over one second: it shakes, crumbles and stops
supporting the player. It returns after the original respawn and fade delay,
provided the player is clear. Restart restores source states. Both scene ROMs
are retained as `deceptus-lever.gba` and `deceptus-platforms.gba`; the last build
also becomes `deceptus.gba`. Build `--scene lever` to restore the default scene.

Collapse contact/touched rules, collision and timers are ported. Visual shake
is deterministic, segment destruction uses the base 30fps/6px-per-second speeds,
and the source's random per-segment multipliers are not reproduced. Returning
platforms remain hidden during the fade delay; gradual alpha fade is unfinished.
Custom destruction/fall speeds are rejected during import.

The background is paged in 256px screen blocks as the camera moves across this
768px area; it does not load neighbouring rooms. The mGBA route verifies source
links, all 13 enabled states, input edges, lethal versus safe traversal,
restart/pause, and background pixels before and after a page boundary.
Reports and screenshots are in `artifacts/gba-*`.

Native tests reconstruct the complete GBA background from packed tiles, flips,
palette attributes and both maps. They also decode all 19 actor frames and
compare their opaque RGB555 pixels against the lossless import. All 257
mechanism frames are decoded and checked against their source atlas pixels;
the ROM check also compares an actual middle lever frame and its palette. Shared engine
and source-Lua differential tests still run. The SNES ROM route passes after the
camera/exporter changes.

There is no hardware test result yet. Audio, save storage and interrupts are not
implemented; the current display loop polls VBlank. This backend establishes a
working GBA platform, not a completed port or a performance guarantee for the
full original game. `python tools/build_gba.py --full-game` fails explicitly
while content coverage is incomplete.
