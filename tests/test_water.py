from pathlib import Path
import struct
import sys
import unittest

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from water import spring_step, bake_surface, periodic_surface
from gba_assets import compile_streamed_background


class WaterTests(unittest.TestCase):
    def surface(self):
        return dict(id=1, name='test', x=8, y=24, width=32, height=16,
                    properties=dict(clamp_segment_count=2, pixel_ratio=1, spread=2))

    def emitters(self):
        return [dict(id=2, x=16, width=8, properties=dict(count=2, width_px=4))]

    def test_spring_update_matches_source_scalar_order(self):
        heights, velocities = np.array([0., 1., -2., 3.]), np.array([0., .1, -.3, .4])
        expected_h, expected_v = heights.tolist(), velocities.tolist()
        for _ in range(10):
            for index in range(4):
                expected_v[index] += -.025 * expected_h[index] - .025 * expected_v[index]
                expected_h[index] += expected_v[index]
            for _ in range(8):
                left, right = [0.] * 4, [0.] * 4
                for index in range(4):
                    if index:
                        left[index] = 2 * (expected_h[index] - expected_h[index-1]) / 60 * 10
                        expected_v[index-1] += left[index]
                    if index < 3:
                        right[index] = 2 * (expected_h[index] - expected_h[index+1]) / 60 * 10
                        expected_v[index+1] += right[index]
                for index in range(4):
                    if index:
                        expected_h[index-1] += left[index]
                    if index < 3:
                        expected_h[index+1] += right[index]
            spring_step(heights, velocities, .025, .025, 2, 10)
        np.testing.assert_allclose(heights, expected_h, atol=1e-12)
        np.testing.assert_allclose(velocities, expected_v, atol=1e-12)

    def test_periodic_bake_is_deterministic_and_clamped(self):
        first, residual = periodic_surface(self.surface(), self.emitters())
        second, _ = periodic_surface(self.surface(), self.emitters())
        self.assertLess(residual, .0001)
        self.assertEqual(len(first), 12)
        for one, two in zip(first, second):
            np.testing.assert_array_equal(one, two)
            self.assertEqual(one[0], 0)
            self.assertEqual(one[-1], 0)
        self.assertGreater(np.ptp(np.stack(first)), .5)

    def test_frame_tiles_reconstruct_after_shared_palette_conversion(self):
        background = Image.new('RGB', (64, 64), (16, 24, 40))
        gradient = Image.new('RGBA', (48, 1), (64, 128, 96, 128))
        clip = bake_surface(background, self.surface(), self.emitters(), gradient, [0, 0, 64, 64])
        folder = Path(__file__).resolve().parents[1] / 'build/tests/water-conversion'
        folder.mkdir(parents=True, exist_ok=True)
        compile_streamed_background(background, folder, [clip])
        cells = struct.unpack('<' + 'H' * ((folder/'animation-cells.bin').stat().st_size // 2), (folder/'animation-cells.bin').read_bytes())
        patterns = (folder/'stream-patterns.bin').read_bytes()
        width, height = clip['bounds'][2:]
        for frame in range(12):
            expected = Image.open(folder/f'animation0/converted-{frame}.png')
            reconstructed = Image.new('P', expected.size)
            for y in range(height):
                for x in range(width):
                    cell = (frame * width * height // 64 + (y//8) * (width//8) + x//8) * 2
                    pattern, attributes = cells[cell:cell+2]
                    px = 7-x%8 if attributes & 0x400 else x%8
                    py = 7-y%8 if attributes & 0x800 else y%8
                    value = (patterns[pattern*32 + py*4 + px//2] >> ((px%2)*4)) & 15
                    reconstructed.putpixel((x,y), value + (attributes >> 12)*16)
            self.assertEqual(reconstructed.tobytes(), expected.tobytes())

    def test_unsupported_resolution_and_off_room_surface_fail(self):
        surface = self.surface()
        surface['properties']['pixel_ratio'] = 2
        with self.assertRaisesRegex(ValueError, 'pixel_ratio'):
            periodic_surface(surface, [])
        surface = self.surface()
        surface['x'] = 48
        with self.assertRaisesRegex(ValueError, 'fit completely'):
            bake_surface(Image.new('RGB', (64, 64)), surface, [], Image.new('RGBA', (48, 1)), [0, 0, 64, 64])


if __name__ == '__main__':
    unittest.main()
