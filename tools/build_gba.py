"""Build the shared C engine as a freestanding ARM7TDMI GBA ROM with LLVM."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def run(command, folder):
    print(' '.join(map(str, command)), flush=True)
    subprocess.run(list(map(str, command)), cwd=folder, check=True)


def fix_header(rom, reference):
    """Fill the mandatory header data using the unchanged public tool reference."""
    source = reference.read_text()
    match = re.search(r'// logo\s*\{([^}]+)\}', source)
    if not match:
        raise ValueError('could not find GBA logo data in the header reference')
    logo = bytes(int(value, 16) for value in re.findall(r'0x([0-9a-fA-F]{2})', match.group(1)))
    if len(logo) != 156:
        raise ValueError('invalid GBA logo data length')
    payload = bytearray(rom.read_bytes())
    payload[4:160] = logo
    payload[0xa0:0xac] = b'DECEPTUS'.ljust(12, b'\0')
    payload[0xac:0xb0] = b'DCPT'
    payload[0xb0:0xb2] = b'00'
    payload[0xb2] = 0x96
    payload[0xb3:0xbd] = bytes(10)
    payload[0xbd] = (-sum(payload[0xa0:0xbd]) - 0x19) & 255
    payload[0xbe:0xc0] = b'\0\0'
    size = 1 << max(15, (len(payload) - 1).bit_length())
    payload.extend(b'\xff' * (size - len(payload)))
    rom.write_bytes(payload)


def build(data, clang='clang', linker='ld.lld', objcopy='llvm-objcopy', full_game=False):
    for executable in (clang, linker, objcopy):
        if not shutil.which(str(executable)):
            raise SystemExit(f'{executable} not found; install LLVM or supply its executable path')
    clang, linker, objcopy = (shutil.which(str(executable)) for executable in (clang, linker, objcopy))
    reference = ROOT / '.tools/gbafix-reference.c'
    if not reference.exists():
        raise SystemExit('run python tools/bootstrap.py --target gba first')
    if full_game:
        result = subprocess.run([sys.executable, str(ROOT / 'tools/content.py'), '--data', str(data), '--strict'], cwd=ROOT)
        if result.returncode:
            raise SystemExit('Full-game content support is incomplete; see build/content/summary.json')
    folder = ROOT / 'build/gba'
    folder.mkdir(parents=True, exist_ok=True)
    run([sys.executable, ROOT / 'tools/gba_assets.py', '--data', data], ROOT)
    platform = ROOT / 'src/platform/gba'
    flags = ['--target=arm-none-eabi', '-mcpu=arm7tdmi', '-mthumb', '-O2', '-ffreestanding', '-fno-builtin',
             '-fno-unwind-tables', '-fno-asynchronous-unwind-tables', '-ffunction-sections', '-fdata-sections',
             '-std=c90', '-Wall', '-Wextra', '-Werror', '-I' + str(ROOT / 'include'),
             '-I' + str(ROOT / 'assets/generated/gba'), '-I' + str(platform / 'freestanding')]
    sources = [ROOT / 'src/engine/world.c', ROOT / 'src/engine/entities.c', ROOT / 'src/engine/mechanisms.c',
               ROOT / 'src/engine/logic.c', ROOT / 'src/game/deceptus.c', platform / 'main.c',
               platform / 'runtime.c', ROOT / 'assets/generated/gba/room.c']
    objects = []
    for source in sources:
        target = folder / (source.stem + '.o')
        run([clang, *flags, '-c', source, '-o', target], folder)
        objects.append(target)
    for name, source in [('start', platform / 'start.s'), ('data', ROOT / 'assets/generated/gba/data.s')]:
        target = folder / (name + '.o')
        run([clang, '--target=arm-none-eabi', '-mcpu=arm7tdmi', '-c', source, '-o', target], folder)
        objects.append(target)
    elf = folder / 'deceptus.elf'
    run([linker, '-T', platform / 'linker.ld', '--gc-sections', '-Map=' + str(folder / 'deceptus.map'),
         *objects, '-o', elf], folder)
    rom = folder / 'deceptus.gba'
    run([objcopy, '-O', 'binary', elf, rom], folder)
    fix_header(rom, reference)
    (folder / 'build.json').write_text(json.dumps({'compiler': subprocess.check_output([clang, '--version'], text=True).splitlines()[0],
        'rom_bytes': rom.stat().st_size, 'rom_sha256': hashlib.sha256(rom.read_bytes()).hexdigest(),
        'header_reference_sha256': hashlib.sha256(reference.read_bytes()).hexdigest(),
        'full_game': False}, indent=2) + '\n')
    print(f'ROM: {rom}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, default=ROOT.parent / 'deceptus_engine/data')
    parser.add_argument('--clang', default='clang')
    parser.add_argument('--linker', default='ld.lld')
    parser.add_argument('--objcopy', default='llvm-objcopy')
    parser.add_argument('--full-game', action='store_true')
    args = parser.parse_args()
    build(args.data.resolve(), args.clang, args.linker, args.objcopy, args.full_game)
