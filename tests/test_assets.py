import base64
from pathlib import Path
import struct
import sys
import unittest
import xml.etree.ElementTree as ET
import zlib

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from assets import planar4
from palettes import banked_background, palette_rgb555
from tiled import collision_grid, draw_object, layer_values, transform_tile, FLIP_X, FLIP_DIAGONAL


class CollisionTests(unittest.TestCase):
    def test_rectangle_excludes_right_bottom_boundary(self):
        image = Image.new("L", (24, 24))
        draw_object(image, ET.fromstring('<object x="8" y="8" width="8" height="8"/>'))
        self.assertEqual(sum(bool(value) for value in image.getdata()), 64)
        self.assertEqual(collision_grid(image), [0, 0, 0, 0, 1, 0, 0, 0, 0])

    def test_polygon_slope_coverage(self):
        image = Image.new("L", (16, 16))
        draw_object(image, ET.fromstring('<object><polygon points="0,16 16,0 16,16"/></object>'))
        self.assertEqual(collision_grid(image), [0, 1, 1, 1])

    def test_rotation_and_group_offset(self):
        image = Image.new("L", (32, 32))
        draw_object(image, ET.fromstring('<object x="16" y="0" width="8" height="16" rotation="90"/>'), (8, 8))
        self.assertEqual(sum(bool(value) for value in image.getdata()), 128)
        self.assertEqual(image.getbbox(), (8, 8, 24, 16))

    def test_ellipse_and_polyline(self):
        image = Image.new("L", (16, 16))
        draw_object(image, ET.fromstring('<object width="16" height="16"><ellipse/></object>'))
        self.assertGreater(sum(bool(value) for value in image.getdata()), 180)
        self.assertEqual(image.getpixel((0, 0)), 0)
        image = Image.new("L", (16, 16))
        draw_object(image, ET.fromstring('<object><polyline points="0,0 15,15"/></object>'))
        self.assertEqual(sum(bool(value) for value in image.getdata()), 16)

    def test_bad_grid_rejected(self):
        with self.assertRaises(ValueError):
            collision_grid(Image.new("L", (9, 8)))
        with self.assertRaises(ValueError):
            collision_grid(Image.new("L", (8, 8)), threshold=0)


class TileTests(unittest.TestCase):
    def test_generated_snes_maps_reconstruct_the_converted_room(self):
        folder = Path(__file__).resolve().parents[1] / "assets/generated"
        if not (folder / "room.map").exists():
            self.skipTest("run tools/assets.py to verify generated hardware maps")
        expected = Image.open(folder / "room-preview.png")
        maps = [struct.unpack("<2048H", (folder / filename).read_bytes())
                for filename in ["room.map", "overflow.map"]]
        patterns = [(folder / filename).read_bytes() for filename in ["room.pic", "overflow.pic"]]
        reconstructed = Image.new("P", expected.size)
        for top in range(0, expected.height, 8):
            for left in range(0, expected.width, 8):
                column, row = left // 8, top // 8
                address = (column // 32) * 1024 + row * 32 + column % 32
                for plane in [1, 0]:
                    entry = maps[plane][address]
                    tile = patterns[plane][(entry & 1023) * 32:(entry & 1023) * 32 + 32]
                    self.assertEqual(len(tile), 32)
                    for pixel_y in range(8):
                        for pixel_x in range(8):
                            source_x = 7 - pixel_x if entry & 0x4000 else pixel_x
                            source_y = 7 - pixel_y if entry & 0x8000 else pixel_y
                            value = sum(((tile[(bit // 2) * 16 + source_y * 2 + bit % 2]
                                          >> (7 - source_x)) & 1) << bit for bit in range(4))
                            if value:
                                reconstructed.putpixel((left + pixel_x, top + pixel_y), value + ((entry >> 10) & 7) * 16)
        self.assertEqual(reconstructed.tobytes(), expected.tobytes())
        self.assertEqual(palette_rgb555(expected, 8), (folder / "room.pal").read_bytes())

    def test_palette_banks_keep_separate_tile_colours(self):
        image = Image.new("RGB", (16, 8), (248, 0, 0))
        for top in range(8):
            for left in range(8, 16):
                image.putpixel((left, top), (0, 248, 0))
        converted, banks, metrics = banked_background(image, bank_count=2, colours_per_bank=1)
        self.assertNotEqual(banks[0], banks[1])
        self.assertEqual(converted.size, image.size)
        self.assertEqual(metrics["rgb555_mean_squared_error"], 0)
        self.assertEqual(metrics["exact_rgb555_pixels_fraction"], 1)
        for top in range(8):
            for left in range(16):
                self.assertEqual(converted.getpixel((left, top)) // 16, banks[left // 8])

    def test_palette_assignment_is_deterministic(self):
        image = Image.new("RGB", (8, 8), (24, 32, 48))
        first, banks, _ = banked_background(image)
        second, other_banks, _ = banked_background(image)
        self.assertEqual(first.tobytes(), second.tobytes())
        self.assertEqual(banks, other_banks)

    def test_csv_and_base64_are_equivalent(self):
        values = [1, 0, 0x80000002, 3]
        csv = ET.fromstring('<layer width="2" height="2"><data encoding="csv">1,0,2147483650,3</data></layer>')
        binary = base64.b64encode(zlib.compress(struct.pack("<4I", *values))).decode()
        compressed = ET.fromstring(f'<layer width="2" height="2"><data encoding="base64" compression="zlib">{binary}</data></layer>')
        self.assertEqual(layer_values(csv), values)
        self.assertEqual(layer_values(compressed), values)

    def test_bad_layer_length_rejected(self):
        with self.assertRaises(ValueError):
            layer_values(ET.fromstring('<layer width="2" height="2"><data encoding="csv">1</data></layer>'))

    def test_tiled_flip_order(self):
        image = Image.new("L", (2, 2))
        image.putdata([1, 2, 3, 4])
        self.assertEqual(list(transform_tile(image, FLIP_DIAGONAL | FLIP_X).getdata()), [3, 1, 4, 2])

    def test_snes_bitplanes(self):
        image = Image.new("P", (8, 8))
        image.putpixel((0, 0), 15)
        image.putpixel((7, 7), 5)
        data = planar4(image)
        self.assertEqual(len(data), 32)
        self.assertEqual([data[index] for index in [0, 1, 16, 17]], [128] * 4)
        self.assertEqual(data[14], 1)
        self.assertEqual(data[30], 1)
        self.assertEqual(data[15], 0)


if __name__ == "__main__":
    unittest.main()
