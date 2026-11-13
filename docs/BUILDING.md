# Building and testing

## Prerequisites

Use the neighboring `deceptus_engine` checkout as the source of truth. Install
the pinned packages in `requirements.txt`: Pillow, NumPy and luaparser. NumPy
2.3.4 requires Python 3.11 or newer; Python 3.13 is the verified interpreter.
The source engine itself does not need to compile for asset conversion.

GBA requires LLVM's ARM-capable Clang, LLD and llvm-objcopy. Native tests require
CMake and a desktop C compiler. SNES additionally requires PVSnesLib, installed
by the project bootstrap or selected with `--sdk`.

```powershell
python -m pip install -r requirements.txt
python tools/bootstrap.py --target gba --emulator
python tools/build_gba.py --scene tutorial
```

For the verified Windows tool locations:

```powershell
python tools/build_gba.py --scene tutorial --clang C:/build_utils/llvm/bin/clang.exe --linker C:/build_utils/llvm/bin/ld.lld.exe --objcopy C:/build_utils/llvm/bin/llvm-objcopy.exe
```

Pass `--data D:/other/deceptus_engine/data` to select another source checkout.
The tutorial build generates twelve rooms, UI resources, water clips and
native definitions before compiling the freestanding C engine and GBA backend.
It produces `build/gba/deceptus-tutorial.gba`, the latest-build alias
`deceptus.gba`, ELF symbols, a linker map and `build.json` with ROM size/hash.
A later scene build replaces the alias and generated resources; numbered
playtest copies remain unchanged.

## Play

```powershell
python tools/play.py --backend gba --rom build/gba/deceptus-tutorial.gba
```

The project player uses its downloaded mGBA libretro core. `--core PATH` selects
another native core. Standalone mGBA can open the same ROM. Use an integer window
scale and disable smoothing to preserve the artwork's pixel grid. No external
BIOS is distributed. Audio is currently silent in both ROM and project player.

## Native and conversion checks

```powershell
cmake -S . -B build/host
cmake --build build/host --config Debug
ctest --test-dir build/host -C Debug --output-on-failure
python -m unittest discover -s tests -p test_*.py
```

CMake's `-DDECEPTUS_DATA_DIR=PATH` changes the source data folder. The native
preview uses the small SNES-oriented scene; it does not display the full GBA
adventure. Tests cover engine collision, mechanisms, source callbacks, enemy
schedules, adventure interactions, animation, target graphics and pixel fonts.
Some generated-resource checks skip when their inputs have not been built.
Build the tutorial before claiming its conversion checks have run.

Where available, `LUA_REFERENCE_SOURCE_DIR` supplies the original Lua sources
for differential tests. Without them, expected-state checks run but do not
establish equivalence against actual Lua execution.

## Real GBA scene checks

After building `--scene tutorial`:

```powershell
python tools/verify_tutorial.py --clang C:/build_utils/llvm/bin/clang.exe --linker C:/build_utils/llvm/bin/ld.lld.exe --objcopy C:/build_utils/llvm/bin/llvm-objcopy.exe
```

This builds controlled-start integration fixtures from the same game and assets.
It checks dialogue, menu navigation, rotation cadence, underwater visibility and
the Crossroads/Fool's Ledge return connection. It restores production build
outputs afterward. Reports and screenshots go to `artifacts/tutorial-check/`.
It deliberately does not claim to play the entire tutorial with normal input.

The older preview has its own complete curated input route:

```powershell
python tools/build_gba.py --scene tonight
python tools/gba_emulator.py --scene tonight
python tools/package_preview.py --build
```

Standalone `lever`, `platforms` and `water` scenes have dedicated emulator checks.
Use `python tools/gba_emulator.py --scene water` after building that scene.
Generated resources are scene-specific, so rebuild before switching tests.

## SNES regression check

```powershell
python tools/bootstrap.py --target snes --emulator
python tools/build_snes.py
python tools/emulator.py
```

This verifies the retained prototype's movement, jump, pause, checkpoint and
portal route in Snes9x. It does not run the GBA tutorial. GBA, SNES and desktop
share C engine code, so shared simulation changes need both console checks.

## Packaging

`tools/package_preview.py` verifies and packages the curated four-area preview.
The current numbered tutorial builds are local intermediate snapshots containing
the ROM, controls, build hash and source content notices. They are not a retail
release process. Do not accidentally package a controlled-start test fixture
as the playable game. Build folders, downloaded SDKs, original imports and ROMs
are all excluded from version control.
