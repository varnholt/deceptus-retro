"""Collect a source-revision-recorded GBA build for CI artifact download."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def package():
    output = ROOT / 'build/ci'
    output.mkdir(parents=True, exist_ok=True)
    rom = ROOT / 'build/gba/deceptus-tutorial.gba'
    metadata = json.loads((ROOT / 'build/gba/build.json').read_text())
    if metadata.get('scene') != 'tutorial' or hashlib.sha256(rom.read_bytes()).hexdigest() != metadata['rom_sha256']:
        raise ValueError('tutorial ROM does not match its build manifest')
    metadata['code_revision'] = subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    source = ROOT.parent / 'deceptus_engine'
    metadata['content_revision'] = subprocess.check_output(['git','rev-parse','HEAD'],cwd=source,text=True).strip()
    shutil.copyfile(rom,output/'Deceptus.gba')
    (output/'BUILD.json').write_text(json.dumps(metadata,indent=2)+'\n')
    (output/'README.txt').write_text('Deceptus GBA development build\n\nOpen Deceptus.gba in a GBA emulator.\nD-pad: move, Up: interact, A/B: jump, Down: bend, Start: pause, Select: inventory.\nSwimming: hold A after entering water to rise.\n\nThis is an unfinished opening playtest. Audio and persistent saves are not implemented.\nBUILD.json identifies both source revisions and the cartridge hash.\n')
    (output/'LICENSES').mkdir(exist_ok=True)
    shutil.copyfile(ROOT/'LICENSE',output/'LICENSES/ENGINE-CODE.txt')
    shutil.copyfile(source/'data/LICENSE.md',output/'LICENSES/DECEPTUS-DATA.md')
    shutil.copyfile(source/'LICENSES/GAME-CONTENT.txt',output/'LICENSES/GAME-CONTENT.txt')
    print(output)


if __name__ == '__main__':
    package()
