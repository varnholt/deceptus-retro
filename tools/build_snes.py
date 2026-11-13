"""Build a SNES ROM using native PVSnesLib tools; make/MSYS are not required."""

import argparse
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def run(command, folder):
    print(" ".join(map(str, command)), flush=True)
    subprocess.run(list(map(str, command)), cwd=folder, check=True)


def build(sdk, data, full_game=False):
    folder = ROOT / "build/snes"
    folder.mkdir(parents=True, exist_ok=True)
    if full_game:
        result = subprocess.run([sys.executable, str(ROOT / "tools/content.py"), "--data", str(data), "--strict"], cwd=ROOT)
        if result.returncode:
            raise SystemExit("Full-game build is incomplete. See build/content/summary.json; the existing ROM is still the prototype.")
    run([sys.executable, ROOT / "tools/assets.py", "--import-data", data], ROOT)
    suffix = ".exe" if os.name == "nt" else ""
    compiler = sdk / "devkitsnes/bin" / ("816-tcc" + suffix)
    assembler = sdk / "devkitsnes/bin" / ("wla-65816" + suffix)
    linker = sdk / "devkitsnes/bin" / ("wlalink" + suffix)
    optimizer = sdk / "devkitsnes/tools" / ("816-opt" + suffix)
    if not compiler.is_file():
        raise SystemExit("PVSnesLib not found. Run python tools/bootstrap.py, or pass --sdk PATH.")
    header = (sdk / "devkitsnes/include/hdr.asm.in").read_text()
    replacements = {"HIROMDEF": "", "FASTROMDEF": ".DEFINE FASTROM 1", "ROMTITLE": "DECEPTUS CATACOMBS",
                    "CARTRIDGETYPE": "00", "ROMSIZE": "08", "SRAMSIZE": "00", "COUNTRY": "01",
                    "LICENSEECODE": "00", "VERSION": "00", "ROMBANKS": "8",
                    "ROMBANKSIZE": "8000", "ROMMODE": "LOROM", "ROMSPEED": "FASTROM"}
    for key, value in replacements.items():
        header = header.replace(f"@{key}@", value)
    (folder / "hdr.asm").write_text(header)
    objects = []
    sources = [ROOT / "src/engine/world.c", ROOT / "src/engine/entities.c", ROOT / "src/engine/mechanisms.c",
               ROOT / "src/engine/logic.c", ROOT / "src/game/deceptus.c",
               ROOT / "src/platform/snes/main.c", ROOT / "assets/generated/room.c"]
    for source in sources:
        stem = source.stem
        run([compiler, "-I" + str(ROOT / "include"), "-I" + str(ROOT / "assets/generated"), "-I" + str(sdk / "pvsneslib/include"),
             "-I" + str(sdk / "devkitsnes/include"), "-I" + str(folder), "-Wall", "-c", source,
             "-F", "-o", stem + ".ps"], folder)
        run([optimizer, "-i", stem + ".ps", "-o", stem + ".asm"], folder)
        run([assembler, "-d", "-s", "-x", "-o", stem + ".obj", stem + ".asm"], folder)
        objects.append(stem + ".obj")
    (folder / "data.asm").write_text((ROOT / "assets/generated/data.asm").read_text())
    run([assembler, "-d", "-s", "-x", "-o", "data.obj", "data.asm"], folder)
    objects.append("data.obj")
    library = sdk / "pvsneslib/lib/LoROM_FastROM"
    objects += [path.as_posix() for path in sorted(library.glob("*.obj"))]
    (folder / "linkfile").write_text("[objects]\n" + "\n".join(objects) + "\n")
    run([linker, "-d", "-s", "-A", "-c", "-L", library, "linkfile", "deceptus.sfc"], folder)
    print(f"ROM: {folder / 'deceptus.sfc'}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, default=Path(os.environ.get("PVSNESLIB_HOME", ROOT / ".tools/pvsneslib")))
    parser.add_argument("--data", type=Path, default=ROOT.parent / "deceptus_engine/data")
    parser.add_argument("--full-game", action="store_true", help="require complete content support instead of building the prototype")
    arguments = parser.parse_args()
    build(arguments.sdk.resolve(), arguments.data.resolve(), arguments.full_game)
