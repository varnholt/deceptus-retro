from pathlib import Path
import struct
import sys
import unittest

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from gba_assets import linear4
from palettes import palette_rgb555

ROOT = Path(__file__).resolve().parents[1]
GENERATED = ROOT / 'assets/generated/gba'


class GbaGraphicsTests(unittest.TestCase):
    def test_nibble_order(self):
        image = Image.new('P', (8, 8))
        image.putdata(list(range(16)) * 4)
        self.assertEqual(linear4(image)[:4], bytes([0x10, 0x32, 0x54, 0x76]))

    def test_generated_maps_reconstruct_the_gba_image(self):
        if not (GENERATED / 'room-preview.png').exists():
            self.skipTest('build the GBA assets to check their hardware maps')
        expected = Image.open(GENERATED / 'room-preview.png')
        maps = [struct.unpack('<2048H', (GENERATED / f'map{plane}.bin').read_bytes()) for plane in range(2)]
        patterns = [(GENERATED / f'background{plane}.bin').read_bytes() for plane in range(2)]
        reconstructed = Image.new('P', expected.size)
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
                            source_x = 7 - pixel_x if entry & 0x400 else pixel_x
                            source_y = 7 - pixel_y if entry & 0x800 else pixel_y
                            packed = tile[source_y * 4 + source_x // 2]
                            value = (packed >> ((source_x % 2) * 4)) & 15
                            if value:
                                reconstructed.putpixel((left + pixel_x, top + pixel_y), value + (entry >> 12) * 16)
        self.assertEqual(reconstructed.tobytes(), expected.tobytes())
        self.assertEqual(palette_rgb555(expected, 16), (GENERATED / 'background.pal').read_bytes())

    def test_actor_frames_preserve_all_original_rgb555_pixels(self):
        if not (GENERATED / 'actors.bin').exists():
            self.skipTest('build GBA assets to verify its actor frames')
        original = Image.open(ROOT / 'assets/source/actors.png').convert('RGBA')
        frames = (GENERATED / 'actors.bin').read_bytes()
        palette = struct.unpack('<256H', (GENERATED / 'actors.pal').read_bytes())
        for frame in range(19):
            for piece in range(6):
                for tile_y in range(2):
                    for tile_x in range(2):
                        offset = frame * 1536 + piece * 256 + (tile_y * 2 + tile_x) * 64
                        for pixel_y in range(8):
                            for pixel_x in range(8):
                                value = frames[offset + pixel_y * 8 + pixel_x]
                                left = (frame % 4) * 32 + (piece % 2) * 16 + tile_x * 8 + pixel_x
                                top = (frame // 4) * 48 + (piece // 2) * 16 + tile_y * 8 + pixel_y
                                red, green, blue, alpha = original.getpixel((left, top))
                                if alpha < 128:
                                    self.assertEqual(value, 0)
                                else:
                                    self.assertNotEqual(value, 0)
                                    self.assertEqual(palette[value], (red >> 3) | ((green >> 3) << 5) | ((blue >> 3) << 10))

    def test_gba_header_checksum_and_rom_size(self):
        rom = ROOT / 'build/gba/deceptus.gba'
        if not rom.exists():
            self.skipTest('build the ROM to check its header')
        payload = rom.read_bytes()
        self.assertEqual(payload[0xb2], 0x96)
        self.assertEqual((sum(payload[0xa0:0xbe]) + 0x19) & 255, 0)
        self.assertEqual(len(payload) & (len(payload) - 1), 0)
        self.assertEqual(payload[0xac:0xb0], b'DCPT')


if __name__ == '__main__':
    unittest.main()
