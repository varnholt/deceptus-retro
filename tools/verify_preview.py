"""Play the shareable GBA preview from title to finish without altering game state."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from PIL import Image
from emulator import Emulator, ROOT

CORE = ROOT / '.tools/mgba/mgba_libretro.dll'
ROM = ROOT / 'build/gba/deceptus-tonight.gba'
CLOCK_ROUTE = [(80, [7]), (30, [7, 0]), (40, [7]), (40, [7, 0]), (180, [7]),
               (30, [7, 0]), (40, [7]), (30, [7, 0]), (40, [7]), (30, [7, 0]), (50, [7]),
               (12, [6]), (8, [6, 0]), (21, [7, 0]), (1, [7]), (30, [7, 0]),
               (1, [7]), (30, [7, 0]), (1, [7]), (35, [7, 0]), (60, [7])]


def verify(core=CORE, rom=ROM):
    emulator = Emulator(core, rom, {'mgba_use_bios': 'OFF', 'mgba_frameskip': '0'})
    snapshots, timing = {}, []
    def state():
        memory = emulator.memory()
        values = struct.unpack_from('<34H', memory, memory.find(b'\xce\xd3'))
        result = emulator.debug(camera_y=True)
        result.update(stage=values[28], mode=values[29], deaths=values[30], seconds=values[31],
                      arrows_fired=values[32], restarts=values[33], lever_enabled=values[19],
                      platform_phase=values[25], platform_solid=values[26])
        return result
    def run(frames, buttons=(), timed=False):
        previous = state() if emulator.video_frames else None
        emulator.run(frames, buttons)
        current = state()
        assert not current['content_error'], f'content failure: {current}'
        if timed:
            assert previous['stage'] == current['stage'] and current['ticks'] - previous['ticks'] == frames, 'simulation missed a display frame'
            timing.append(frames)
        return current
    def snapshot(name):
        snapshots[name] = state()
        emulator.screenshot(ROOT / f'artifacts/preview-{name}.png')
        return snapshots[name]
    def background():
        current = state()
        index = current['stage']
        folder = ROOT / f'assets/generated/gba/area{index}'
        image = Image.open(folder / 'room-preview.png').convert('RGB')
        recipe = json.loads((folder / 'recipe.json').read_text())
        source = ROOT / f'assets/source/campaign/area{index}/gba'
        mechanisms = json.loads((source / 'mechanisms.json').read_text())['definitions']
        enemies = json.loads((source / 'enemies.json').read_text())['definitions']
        masks = [(current['x'] - 16, current['y'] - 48, 32, 48)]
        for mechanism in mechanisms:
            x, y, width, height = mechanism['bounds']
            masks.append((x - 4, y - 30, width + 12, max(height, 96)))
        for enemy in enemies:
            if enemy['kind'] == 'RETRO_RAT':
                paths = json.loads((source / 'enemies.json').read_text())['paths']
                points = paths[enemy['path_first']:enemy['path_first'] + enemy['path_count']]
                xs = [point[0] for point in points]
                masks.append((min(xs) - 20, enemy['y'] - 32, max(xs) - min(xs) + 40, 64))
            else:
                masks.append((enemy['x'] - 32, 0, 96, image.height))
        checked = 0
        for screen_y in range(24, 80, 3):
            for screen_x in range(0, 240, 3):
                x, y = current['camera_x'] + screen_x, current['camera_y'] + screen_y
                if any(left <= x < left + width and top <= y < top + height for left, top, width, height in masks):
                    continue
                expected = image.getpixel((x, y))
                actual = emulator.last_image.getpixel((screen_x, screen_y))
                assert tuple(value >> 3 for value in expected) == tuple(value >> 3 for value in actual), f'streamed background mismatch in area {index}, {x},{y}'
                checked += 1
        assert checked > 250, f'not enough unobstructed background samples: {checked}'
        return checked
    try:
        run(120)
        assert state()['mode'] == 0 and state()['ticks'] == 1 and state()['seconds'] == 0
        snapshot('title')
        run(1, [3]); run(1)
        assert state()['mode'] == 1
        run(60, [7], timed=True); run(10)
        run(1, [2]); run(10)
        assert state()['x'] == 264 and state()['restarts'] == 1
        run(1, [3]); paused = state(); run(40, [7])
        assert state()['paused'] and state()['ticks'] == paused['ticks'] and state()['seconds'] == paused['seconds']
        run(1, [3]); run(1)
        run(170, [7], timed=True); run(10)
        assert state()['stage'] == 0 and state()['x'] > 620
        background(); snapshot('cell-exit')
        run(1, [4]); run(10)
        assert state()['stage'] == 1 and state()['mode'] == 3
        frozen = state()['ticks']; run(40); assert state()['ticks'] == frozen
        snapshot('clock-briefing')
        run(1, [3]); run(10)
        assert state()['mode'] == 1
        background(); snapshot('clock-start')
        for frames, buttons in CLOCK_ROUTE:
            run(frames, buttons, timed=True)
        run(10)
        clock = snapshot('clock-exit')
        assert clock['x'] >= 1160 and clock['y'] == 216 and clock['checkpoint_id'] == 1058 and clock['arrows_fired'] >= 12
        background()
        run(1, [4]); run(10)
        assert state()['stage'] == 2 and state()['mode'] == 3
        run(1, [3]); run(10)
        assert state()['mode'] == 1 and state()['lever_enabled'] == 1
        snapshot('lever-start'); background()
        # leaving the original traps active must actually kill and recover.
        run(60, [7]); run(45, [7, 0]); run(10)
        assert state()['deaths'] == 1 and state()['x'] < 360 and state()['health'] == 100
        snapshot('spike-respawn')
        run(1, [2]); run(10)
        run(1, [4]); run(60, [7], timed=True); run(45, [7, 0], timed=True); run(70, [7], timed=True); run(10)
        assert state()['stage'] == 2 and state()['x'] > 620 and state()['lever_enabled'] == 0
        snapshot('lever-exit'); background()
        run(1, [4]); run(10)
        assert state()['stage'] == 3 and state()['mode'] == 3
        snapshot('bridge-briefing')
        run(1, [3]); run(10)
        assert state()['mode'] == 1
        snapshot('bridge-start'); background()
        # wait on the original segment, observe collision loss and falling.
        run(58)
        assert state()['platform_phase'] == 1 and not state()['platform_solid'] and state()['y'] > 72
        snapshot('bridge-collapse')
        run(1, [2]); run(10)
        assert state()['platform_solid'] and state()['y'] == 72
        run(80, [7], timed=True); run(10)
        assert state()['stage'] == 3 and state()['x'] >= 528 and state()['y'] == 72
        snapshot('bridge-exit'); background()
        run(1, [4]); run(10)
        assert state()['mode'] == 2
        finished = snapshot('finished')
        run(60, [7])
        assert state()['ticks'] == finished['ticks'] and state()['seconds'] == finished['seconds']
        run(1, [3]); run(10)
        assert state()['stage'] == 0 and state()['mode'] == 1 and state()['deaths'] == 0 and state()['restarts'] == 0
        snapshot('replay')
        # second route: stand under the arrow stream and verify checkpoint recovery.
        run(170, [7]); run(10); run(1, [4]); run(10); run(1, [3]); run(10)
        for frames, buttons in CLOCK_ROUTE[:5]:
            run(frames, buttons)
        run(22, [7]); run(10)
        assert 910 <= state()['x'] <= 923 and state()['y'] == 432
        run(350)
        assert state()['deaths'] >= 1 and state()['x'] == state()['checkpoint_x'] == 276 and state()['health'] == 100
        snapshot('arrow-checkpoint-respawn')
        report = dict(rom_sha256=hashlib.sha256(Path(rom).read_bytes()).hexdigest(), core=emulator.version,
                      snapshots=snapshots, timed_display_frames=sum(timing), video_frames=emulator.video_frames,
                      title_to_finish_and_replay=True, damage_checkpoint_pause_restart_verified=True,
                      full_game=False)
        (ROOT / 'artifacts/preview-verification.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps({key: value for key, value in report.items() if key != 'snapshots'}, indent=2))
        print('Four-area GBA preview playthrough checks passed')
        return report
    finally:
        emulator.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core', type=Path, default=CORE)
    parser.add_argument('--rom', type=Path, default=ROM)
    args = parser.parse_args()
    verify(args.core, args.rom)
