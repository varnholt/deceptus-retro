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
    def check_lever_pixels():
        memory = emulator.memory()
        values = struct.unpack_from('<28H', memory, memory.find(b'\xce\xd3'))
        animation_frame = values[23]
        current = state()
        # the next video callback displays the frame currently in the mailbox.
        emulator.run(1)
        atlas = Image.open(ROOT.parent / 'deceptus_engine/data/sprites/levers.png').convert('RGBA')
        left, top = (animation_frame % 11) * 72, (animation_frame // 11) * 72
        sprite = atlas.crop((left, top, left + 72, top + 72))
        origin_x, origin_y = 288 - current['camera_x'], 144 - current['camera_y']
        player_x, player_y = current['x'] - 16 - current['camera_x'], current['y'] - 48 - current['camera_y']
        checked = 0
        for y in range(72):
            for x in range(72):
                red, green, blue, alpha = sprite.getpixel((x, y))
                screen_x, screen_y = origin_x + x, origin_y + y
                if alpha < 128 or not (0 <= screen_x < 240 and 0 <= screen_y < 160):
                    continue
                if player_x <= screen_x < player_x + 32 and player_y <= screen_y < player_y + 48:
                    continue
                actual = emulator.last_image.getpixel((screen_x, screen_y))
                assert tuple(channel >> 3 for channel in actual) == (red >> 3, green >> 3, blue >> 3), 'streamed lever pixels or palette differ from original frame'
                checked += 1
        assert checked > 50
        return animation_frame
    try:
        emulator.run(120)
        initial = state()
        assert not initial['content_error'] and initial['grounded'], 'GBA scene failed to initialize'
        assert emulator.last_image.size == (240, 160)
        assert mechanisms() == dict(count=13, lever=1, enabled_mask=8191, overflow=0)
        check_background()
        check_lever_pixels()
        emulator.screenshot(ROOT / 'artifacts/gba-lever-on.png')
        emulator.run(1, [4])
        assert mechanisms()['lever'] == 0 and mechanisms()['enabled_mask'] == 0, 'original lever targets did not switch off'
        emulator.run(5)
        middle_frame = check_lever_pixels()
        assert 11 <= middle_frame <= 21
        emulator.run(20, [4])
        assert mechanisms()['lever'] == 0, 'held Up repeatedly toggled the lever'
        emulator.screenshot(ROOT / 'artifacts/gba-lever-off.png')
        emulator.run(45)
        emulator.run(1, [4])
        assert mechanisms()['lever'] == 1 and mechanisms()['enabled_mask'] == 8191, 'lever did not switch back on'
        emulator.run(60)
        emulator.run(1, [4])
        walk_tick = state()['ticks']
        emulator.run(60, [7])
        walking = state()
        assert walking['ticks'] - walk_tick == 60, 'animated scene missed display ticks'
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
                  'background_rgb555_verified': True, 'original_mid_animation_frame_verified': middle_frame, 'full_game': False}
        (ROOT / 'artifacts/gba-emulator-report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report, indent=2))
        print('GBA original lever and trap integration checks passed')
    finally:
        emulator.close()


def verify_platforms(core, rom):
    emulator = Emulator(core, rom, {'mgba_use_bios': 'OFF', 'mgba_frameskip': '0'})
    def debug():
        memory = emulator.memory()
        values = struct.unpack_from('<28H', memory, memory.find(b'\xce\xd3'))
        return emulator.debug(camera_y=True), dict(phase=values[25], solid=values[26], timer=values[27])
    try:
        emulator.run(20)
        initial, platform = debug()
        assert initial['grounded'] and initial['y'] == 72 and platform['solid'] == 1
        assert platform['timer'] == 19
        emulator.screenshot(ROOT / 'artifacts/gba-platform-standing.png')
        emulator.run(39)
        assert debug()[1] == dict(phase=0, solid=1, timer=58)
        emulator.run(6)
        falling, collapsed = debug()
        assert not falling['grounded'] and falling['y'] > 72 and collapsed['phase'] == 1 and not collapsed['solid']
        emulator.run(20)
        assert debug()[0]['y'] > 100
        emulator.screenshot(ROOT / 'artifacts/gba-platform-crumbling.png')
        emulator.run(280)
        state, restored = debug()
        assert restored['phase'] == 0 and restored['solid'] == 1, 'original platform did not return after respawn and fade timing'
        assert not state['content_error']
        emulator.screenshot(ROOT / 'artifacts/gba-platform-restored.png')
        emulator.run(1, [2])
        emulator.run(5)
        reset, reset_platform = debug()
        assert reset['y'] == 72 and reset['grounded'] and reset_platform['phase'] == 0
        report = {'core': emulator.version, 'standing': initial, 'falling': falling,
                  'restored': restored, 'reset': reset, 'full_game': False}
        (ROOT / 'artifacts/gba-platforms-report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report, indent=2))
        print('GBA original collapsing platform integration checks passed')
    finally:
        emulator.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core', type=Path, default=ROOT / '.tools/mgba/mgba_libretro.dll')
    parser.add_argument('--rom', type=Path, default=ROOT / 'build/gba/deceptus.gba')
    parser.add_argument("--scene", choices=["lever", "platforms"], default="lever")
    args = parser.parse_args()
    (verify if args.scene == "lever" else verify_platforms)(args.core, args.rom)
