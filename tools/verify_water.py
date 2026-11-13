"""Verify baked surface playback and streaming against the actual GBA ROM."""
import argparse
import json
from pathlib import Path
import struct

import numpy as np
from PIL import Image

from emulator import Emulator, ROOT


def verify(core, rom):
    folder = ROOT / 'assets/generated/gba'
    clips = json.loads((folder / 'animations.json').read_text())
    assert len(clips) == 1 and clips[0]['source_id'] == 361, 'build the main hall water scene first'
    clip = clips[0]
    references = [np.asarray(Image.open(folder / f'animation0/converted-{index}.png').convert('RGB')) >> 3
                  for index in range(clip['frame_count'])]
    emulator = Emulator(core, rom, {'mgba_use_bios': 'OFF', 'mgba_frameskip': '0'})

    def debug():
        memory = emulator.memory()
        fields = struct.unpack_from('<36H', memory, memory.find(b'\xce\xd3'))
        return emulator.debug(camera_y=True), fields[34], fields[35]

    try:
        emulator.run(120)
        initial, _, _ = debug()
        assert initial['grounded'] and not initial['content_error']
        observed = set()
        for _ in range(144):
            before, phase, _ = debug()
            emulator.run(1)
            after, _, _ = debug()
            assert after['ticks'] - before['ticks'] == 1, 'water streaming missed a display tick'
            assert not after['content_error']
            x, y, width, height = clip['bounds']
            left, top = max(x, before['camera_x']), max(y, before['camera_y'])
            right, bottom = min(x+width, before['camera_x']+240), min(y+height, before['camera_y']+160)
            assert right > left and bottom > top
            actual = np.asarray(emulator.last_image.convert('RGB'))[top-before['camera_y']:bottom-before['camera_y'], left-before['camera_x']:right-before['camera_x']] >> 3
            expected = references[phase][top-y:bottom-y, left-x:right-x]
            assert np.array_equal(actual, expected), f'water pixels differ from baked frame {phase}'
            observed.add(actual.tobytes())
            if phase in (0, 3, 6, 9):
                emulator.screenshot(ROOT / f'artifacts/gba-water-frame-{phase}.png')
        assert len(observed) >= 6, 'visible water animation is frozen'
        emulator.run(1, [3])
        paused, phase, elapsed = debug()
        emulator.run(45)
        frozen, frozen_phase, frozen_elapsed = debug()
        assert paused['paused'] and frozen['ticks'] == paused['ticks']
        assert (frozen_phase, frozen_elapsed) == (phase, elapsed), 'water advanced while paused'
        emulator.run(1, [3])
        emulator.run(12)
        resumed, _, _ = debug()
        assert not resumed['paused'] and resumed['ticks'] > frozen['ticks']
        # exercise cache eviction and camera movement across the whole baked patch.
        emulator.run(140, [7])
        emulator.run(20)
        moved, _, _ = debug()
        assert not moved['content_error'] and moved['camera_x'] > initial['camera_x']
        emulator.run(1, [2])
        emulator.run(8)
        reset, _, _ = debug()
        assert reset['x'] == initial['x'] and not reset['content_error']
        report = dict(core=emulator.version, boot=initial, moved=moved, reset=reset,
                      unique_visible_frames=len(observed), rgb555_pixels_verified=True,
                      simulation_ticks_per_frame=1, paused_animation_verified=True,
                      scope='baked surface graphics; swimming, reactive splashes and animated water tiles are not ported')
        (ROOT / 'artifacts/gba-water-report.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report, indent=2))
    finally:
        emulator.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core', type=Path, default=ROOT / '.tools/mgba/mgba_libretro.dll')
    parser.add_argument('--rom', type=Path, default=ROOT / 'build/gba/deceptus-water.gba')
    args = parser.parse_args()
    verify(args.core, args.rom)
