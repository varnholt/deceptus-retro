# GBA backend

GBA is the first complete-game target. The current ROM runs the existing
one-room prototype through the shared C engine. Room streaming, original enemy
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
The output is a 128KiB ROM with a valid header, checksum and ARM entry point.
The size can grow automatically up to the 32MiB cartridge address limit.

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
- The current background has 1,191 unique 4bpp patterns and 112 distinct RGB555
  colours. The fitter preserves about 92.8% of source pixels exactly on that
  colour grid; the remaining colour error is recorded in `budget.json`.
- Actors use an 8bpp object palette. All 48 opaque RGB555 actor colours in the
  current import are kept without palette reduction. The green patrol variant
  is a deliberate prototype tint, not an original enemy asset.
- Each 32x48 logical frame becomes six 16x16 objects. Visible frames are copied
  to object VRAM when their animation/palette changes. The 12-draw capacity
  uses at most 18,432 bytes of object VRAM.

Generated output stays in `assets/generated/gba/`, separate from SNES output.
`tools/world_export.py` supplies both targets with the same collision and
mechanism definitions. The underlying 512x224 source crop is unchanged.

## Hardware layout

| Region | Use |
| --- | --- |
| BG VRAM `0x06000000` | First 1,024 4bpp patterns in character block 0 |
| BG VRAM `0x06008000` | Overflow patterns in character block 2 |
| BG VRAM `0x0600e000` | Primary 64x32 map in screen blocks 28–29 |
| BG VRAM `0x0600f000` | Overflow 64x32 map in screen blocks 30–31 |
| OBJ VRAM `0x06010000` | Per-visible-actor 8bpp frame slots |
| Palette RAM `0x05000000` | 256 background entries |
| Palette RAM `0x05000200` | Original actor colours plus patrol tint |

The display uses mode 0, two background planes and 1D object mapping. Graphics
and OAM updates happen during VBlank through DMA3. Startup code sets system/IRQ
stacks and initializes data/BSS. Platform-local memory routines satisfy the
freestanding ARM ABI; no host C library, heap or floating point is used by the
runtime. Linker assertions check ROM and EWRAM limits.

The shared camera now follows both axes. GBA's 240x160 viewport is a view onto
the same source coordinates: it does not shrink the artwork to fit. The SNES
prototype retains its 256x224 view and its existing appearance.

## Verified scope and limits

The mGBA route checks background pixels against the generated RGB555 image,
movement, jump/landing, pause/resume, restart, both camera axes, the demo patrol,
the raised ledge, original checkpoint activation and portal return. It also
checks that simulation ticks keep pace with display frames on this scene.
Reports and screenshots are in `artifacts/gba-*`.

Native tests reconstruct the complete GBA background from packed tiles, flips,
palette attributes and both maps. They also decode all 19 actor frames and
compare their opaque RGB555 pixels against the lossless import. Shared engine
and source-Lua differential tests still run. The SNES ROM route passes after the
camera/exporter changes.

There is no hardware test result yet. Audio, save storage and interrupts are not
implemented; the current display loop polls VBlank. This backend establishes a
working GBA platform, not a completed port or a performance guarantee for the
full original game. `python tools/build_gba.py --full-game` fails explicitly
while content coverage is incomplete.
