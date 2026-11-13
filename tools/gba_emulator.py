"""Verify the real GBA ROM through mGBA, including pixels and gameplay inputs."""

import argparse
import json
from pathlib import Path

from PIL import Image

from emulator import Emulator, ROOT


def verify(core, rom):
    emulator = Emulator(core, rom, {'mgba_use_bios': 'OFF', 'mgba_frameskip': '0'})
    state = lambda: emulator.debug(camera_y=True)
    try:
        emulator.run(120)
        initial = state()
        assert not initial['content_error'] and initial['grounded'], 'GBA game failed to initialize'
        assert emulator.last_image.size == (240, 160), 'unexpected GBA video dimensions'
        assert initial['camera_y'] == 64, 'vertical camera failed to show the floor'
        expected = Image.open(ROOT / 'assets/generated/gba/room-preview.png').convert('RGB')
        expected = expected.crop((0, 64, 240, 160))
        actual = emulator.last_image.convert('RGB').crop((0, 0, 240, 96))
        assert all(tuple(channel >> 3 for channel in first) == tuple(channel >> 3 for channel in second)
                   for first, second in zip(expected.getdata(), actual.getdata())), 'GBA pixels differ from converted artwork'
        emulator.screenshot(ROOT / 'artifacts/gba-boot.png')
        emulator.run(30, [7])
        walking = state()
        assert walking['x'] > initial['x'] + 40, 'GBA right input did not move the player'
        assert 29 <= walking['ticks'] - initial['ticks'] <= 31, 'GBA simulation missed display ticks'
        emulator.run(10, [0])
        jumping = state()
        assert jumping['y'] < walking['y'] - 8, 'GBA jump did not rise'
        emulator.screenshot(ROOT / 'artifacts/gba-jump.png')
        emulator.run(60)
        assert state()['grounded'], 'GBA player failed to land'
        emulator.run(8, [3])
        paused = state()
        emulator.run(30, [7])
        assert paused['paused'] and state()['ticks'] == paused['ticks'], 'GBA pause failed'
        emulator.run(8, [3])
        emulator.run(10)
        assert not state()['paused'] and state()['ticks'] > paused['ticks'], 'GBA resume failed'
        emulator.run(8, [2])
        emulator.run(5)
        assert state()['x'] == initial['x'], 'GBA restart failed'
        emulator.core.retro_reset()
        emulator.run(120)
        emulator.run(86, [7])
        emulator.run(50, [7, 0])
        assert not state()['patrol_active'], 'GBA route did not defeat the demo patrol'
        emulator.run(48, [7])
        emulator.run(30, [7, 0])
        emulator.run(15, [7])
        ledge = state()
        assert ledge['x'] > 440 and ledge['y'] == 168 and ledge['grounded'], 'GBA raised platform is unreachable'
        assert ledge['camera_x'] > 0 and ledge['camera_y'] == 48, 'GBA camera did not follow the ledge route'
        emulator.screenshot(ROOT / 'artifacts/gba-portal.png')
        emulator.run(12, [4])
        door = state()
        assert door['checkpoint_id'] == 1058, 'original checkpoint did not activate on GBA'
        assert door['door_entries'] == 1 and door['x'] == door['checkpoint_x'], 'GBA portal did not return to checkpoint'
        report = {'core': emulator.version, 'boot': initial, 'walk': walking, 'jump': jumping,
                  'ledge': ledge, 'door': door, 'video_frames': emulator.video_frames,
                  'background_rgb555_verified': True, 'full_game': False}
        (ROOT / 'artifacts/gba-emulator-report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report, indent=2))
        print('GBA emulator integration checks passed')
    finally:
        emulator.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core', type=Path, default=ROOT / '.tools/mgba/mgba_libretro.dll')
    parser.add_argument('--rom', type=Path, default=ROOT / 'build/gba/deceptus.gba')
    args = parser.parse_args()
    verify(args.core, args.rom)
