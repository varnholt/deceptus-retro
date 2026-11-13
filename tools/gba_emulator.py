"""Verify the real GBA mechanism scene through mGBA."""

import argparse
import json
from pathlib import Path
import struct

from PIL import Image

from emulator import Emulator, ROOT


def verify(core, rom):
    emulator = Emulator(core, rom, {'mgba_use_bios': 'OFF', 'mgba_frameskip': '0'})
    state = lambda: emulator.debug(camera_y=True)
    def mechanisms():
        memory = emulator.memory()
        values = struct.unpack_from('<23H', memory, memory.find(b'\xce\xd3'))
        return dict(count=values[18], lever=values[19], enabled_mask=values[20], overflow=values[21])
    def check_background():
        current = state()
        x, y = current['camera_x'], current['camera_y']
        expected = Image.open(ROOT / 'assets/generated/gba/room-preview.png').convert('RGB')
        expected = expected.crop((x, y, x + 240, y + 16))
        actual = emulator.last_image.convert('RGB').crop((0, 0, 240, 16))
        assert all(tuple(channel >> 3 for channel in first) == tuple(channel >> 3 for channel in second)
                   for first, second in zip(expected.getdata(), actual.getdata())), 'streamed GBA background differs from converted artwork'
    try:
        emulator.run(120)
        initial = state()
        assert not initial['content_error'] and initial['grounded'], 'GBA scene failed to initialize'
        assert emulator.last_image.size == (240, 160)
        assert mechanisms() == dict(count=13, lever=1, enabled_mask=8191, overflow=0)
        check_background()
        emulator.screenshot(ROOT / 'artifacts/gba-lever-on.png')
        emulator.run(1, [4])
        assert mechanisms()['lever'] == 0 and mechanisms()['enabled_mask'] == 0, 'original lever targets did not switch off'
        emulator.run(20, [4])
        assert mechanisms()['lever'] == 0, 'held Up repeatedly toggled the lever'
        emulator.screenshot(ROOT / 'artifacts/gba-lever-off.png')
        emulator.run(1)
        emulator.run(1, [4])
        assert mechanisms()['lever'] == 1 and mechanisms()['enabled_mask'] == 8191, 'lever did not switch back on'
        emulator.run(1)
        emulator.run(1, [4])
        emulator.run(60, [7])
        walking = state()
        assert walking['x'] == 400 and walking['camera_x'] > 256, 'walk or background page boundary failed'
        check_background()
        emulator.run(45, [7, 0])
        emulator.run(10)
        safe = state()
        assert safe['x'] >= 480 and safe['y'] == 168 and safe['grounded'], 'retracted traps did not permit passage'
        assert not safe['content_error'] and not mechanisms()['overflow']
        check_background()
        emulator.screenshot(ROOT / 'artifacts/gba-traps-safe.png')
        emulator.run(1, [3])
        paused = state()
        emulator.run(15, [6])
        assert paused['paused'] and state()['ticks'] == paused['ticks']
        emulator.run(1, [3])
        emulator.run(1)
        assert not state()['paused']
        emulator.run(1, [2])
        emulator.run(5)
        assert state()['x'] == initial['x'] and mechanisms()['lever'] == 1 and mechanisms()['enabled_mask'] == 8191
        emulator.run(60, [7])
        emulator.run(45, [7, 0])
        dangerous = state()
        assert dangerous['x'] < 350 and dangerous['y'] == initial['y'], 'extended original traps did not damage and respawn the player'
        emulator.screenshot(ROOT / 'artifacts/gba-traps-respawn.png')
        report = {'core': emulator.version, 'boot': initial, 'walk': walking, 'safe_route': safe,
                  'dangerous_route': dangerous, 'mechanisms': mechanisms(), 'video_frames': emulator.video_frames,
                  'background_rgb555_verified': True, 'full_game': False}
        (ROOT / 'artifacts/gba-emulator-report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report, indent=2))
        print('GBA original lever and trap integration checks passed')
    finally:
        emulator.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core', type=Path, default=ROOT / '.tools/mgba/mgba_libretro.dll')
    parser.add_argument('--rom', type=Path, default=ROOT / 'build/gba/deceptus.gba')
    args = parser.parse_args()
    verify(args.core, args.rom)
