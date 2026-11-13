import base64
from pathlib import Path
import struct
import sys
import unittest
import xml.etree.ElementTree as ET
import zlib

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from assets import planar4, scaled_actor, viewport_recipe
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
    def test_viewport_keeps_source_coordinates_and_expands_the_visible_area(self):
        recipe = {"crop_pixels": [0, 0, 768, 336], "render_scale": [2, 3],
                  "output_pixels": [512, 224], "collision_cell": 8}
        viewport = viewport_recipe(recipe)
        self.assertEqual((viewport["world_width"], viewport["world_height"]), (384, 336))
        recipe["render_scale"] = [200, 300]
        self.assertEqual(viewport_recipe(recipe), viewport)
        recipe["render_scale"] = [1, 2]
        with self.assertRaises(ValueError):
            viewport_recipe(recipe)

    def test_scaled_actor_keeps_its_feet_at_the_frame_origin(self):
        image = Image.new("RGBA", (32, 48), (255, 0, 0, 255))
        actor = scaled_actor(image, {"numerator": 2, "denominator": 3})
        self.assertEqual(actor.size, (32, 48))
        self.assertEqual(actor.getbbox(), (5, 16, 26, 48))
        unchanged = scaled_actor(image, {"numerator": 1, "denominator": 1})
        self.assertEqual(unchanged.tobytes(), image.tobytes())

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
                                reconstructed.putpixel((left + pixel_x, top + pixel_y), value)
        self.assertEqual(reconstructed.tobytes(), expected.tobytes())

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
