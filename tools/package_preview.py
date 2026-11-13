"""Verify and package the self-contained GBA preview for emulator playtesting."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile
from verify_preview import verify, ROOT, ROM, CORE


def command(arguments):
    subprocess.run(list(map(str, arguments)), cwd=ROOT, check=True)


def revision(folder):
    return subprocess.check_output(['git', '-C', str(folder), 'rev-parse', 'HEAD'], text=True).strip()


def package(build=False, core=CORE):
    if build:
        command([sys.executable, ROOT / 'tools/build_gba.py', '--scene', 'tonight'])
        command(['cmake', '--build', ROOT / 'build/host', '--config', 'Debug'])
    native = subprocess.run(['ctest', '--test-dir', str(ROOT / 'build/host'), '-C', 'Debug', '--output-on-failure'], cwd=ROOT, text=True, capture_output=True)
    print(native.stdout)
    if native.returncode:
        raise RuntimeError('native checks failed: ' + native.stderr)
    report = verify(core, ROM)
    date = datetime.now(timezone.utc).date().isoformat()
    output = ROOT / 'build/release' / f'deceptus-gba-preview-{date}'
    output.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(ROM, output / 'Deceptus-Preview.gba')
    for name in ('README.txt', 'WALKTHROUGH.txt'):
        shutil.copyfile(ROOT / 'assets/release' / name, output / name)
    (output / 'NATIVE-CHECKS.txt').write_text(native.stdout, encoding='utf-8')
    (output / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')
    original = ROOT.parent / 'deceptus_engine'
    notices = [(ROOT / 'LICENSE', 'notices/port/LICENSE'),
               (original / 'LICENSE.md', 'notices/deceptus_engine/LICENSE.md'),
               (original / 'data/LICENSE.md', 'notices/deceptus_engine/data/LICENSE.md')]
    for name in ('ENGINE-MIT.txt', 'GAME-CONTENT.txt', 'LEGACY-CC-BY-NC-4.0.md', 'README.md'):
        notices.append((original / 'LICENSES' / name, 'notices/deceptus_engine/LICENSES/' + name))
    for source, relative in notices:
        destination = output / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, destination)
    content = json.loads((ROOT / 'assets/campaigns/tonight.json').read_text())
    for index, area in enumerate(content['stages']):
        folder = ROOT / f'assets/source/campaign/area{index}/gba'
        area['enemies'] = json.loads((folder / 'enemies.json').read_text())
        area['mechanisms'] = json.loads((folder / 'mechanisms.json').read_text())
    (output / 'CONTENT.json').write_text(json.dumps(content, indent=2) + '\n')
    source_files = ['data/level-catacombs/catacombs.tmx', 'data/scripts/enemies/rat.lua',
                    'data/scripts/enemies/arrowtrap_2.lua', 'data/sprites/player.png',
                    'data/sprites/rat.png', 'data/sprites/enemy_arrowtrap_2.png', 'data/fonts/3x5px.ttf',
                    'data/sprites/animations.json', 'data/sprites/levers.png', 'data/sprites/on_off_block.png',
                    'data/sprites/enemy_spikeblock.png', 'data/sprites/collapsing_platform.png', 'data/weapons/arrow.png']
    source_hashes = {name: hashlib.sha256((original / name).read_bytes()).hexdigest() for name in source_files}
    manifest = dict(created_utc=datetime.now(timezone.utc).isoformat(), port_revision=revision(ROOT),
                    original_revision=revision(original), rom_sha256=report['rom_sha256'],
                    compiler=json.loads((ROOT / 'build/gba/build.json').read_text())['compiler'],
                    source_hashes=source_hashes, full_game=False, areas=4,
                    native_checks_passed=True, rom_playthrough_passed=True)
    (output / 'BUILD.json').write_text(json.dumps(manifest, indent=2) + '\n')
    hashes = []
    for path in sorted(output.rglob('*')):
        if path.is_file() and path.name != 'SHA256SUMS.txt':
            hashes.append(hashlib.sha256(path.read_bytes()).hexdigest() + '  ' + path.relative_to(output).as_posix())
    (output / 'SHA256SUMS.txt').write_text('\n'.join(hashes) + '\n')
    archive = output.with_suffix('.zip')
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as bundle:
        for path in sorted(output.rglob('*')):
            if path.is_file():
                bundle.write(path, path.relative_to(output.parent).as_posix())
    with zipfile.ZipFile(archive) as bundle:
        assert bundle.testzip() is None
        packaged = bundle.read(output.name + '/Deceptus-Preview.gba')
        assert hashlib.sha256(packaged).hexdigest() == report['rom_sha256']
    print(f'Package: {archive}')
    print(f'ROM: {output / "Deceptus-Preview.gba"}')
    return archive


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', action='store_true', help='rebuild the ROM and native tests first')
    parser.add_argument('--core', type=Path, default=CORE)
    args = parser.parse_args()
    package(args.build, args.core)
