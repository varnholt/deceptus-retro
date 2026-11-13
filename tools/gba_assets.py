"""Compile GBA graphics directly from the original-engine lossless import."""

import argparse
import json
from pathlib import Path
import struct

import numpy as np
from PIL import Image

from assets import ROOT, SOURCE, import_deceptus
from palettes import banked_background, palette_rgb555
from world_export import export_world

OUTPUT = ROOT / "assets/generated/gba"


def linear4(tile):
    pixels = list(tile.getdata())
    return bytes((pixels[index] & 15) | ((pixels[index + 1] & 15) << 4) for index in range(0, 64, 2))


def compile_background(image, output):
    converted, banks, metrics = banked_background(image, bank_count=16)
    converted.save(output / "room-preview.png")
    patterns = [bytes(32)]
    lookup = {patterns[0]: 0}
    entries = []
    for top in range(0, image.height, 8):
        for left in range(0, image.width, 8):
            tile = converted.crop((left, top, left + 8, top + 8))
            packed = linear4(tile)
            if packed not in lookup:
                index = len(patterns)
                patterns.append(packed)
                lookup[packed] = index
                lookup.setdefault(linear4(tile.transpose(Image.Transpose.FLIP_LEFT_RIGHT)), index | 0x4000)
                lookup.setdefault(linear4(tile.transpose(Image.Transpose.FLIP_TOP_BOTTOM)), index | 0x8000)
                lookup.setdefault(linear4(tile.transpose(Image.Transpose.ROTATE_180)), index | 0xC000)
            entry = lookup[packed]
            bank = banks[(top // 8) * (image.width // 8) + left // 8]
            attributes = (bank << 12) | ((entry & 0xC000) >> 4)
            entries.append((entry & 0x3FFF, attributes))
    # CBB0 owns 32KiB, CBB2 the remaining 24KiB; SBB28-31 contain the maps.
    if len(patterns) > 1791:
        raise ValueError(f"GBA background needs {len(patterns)} patterns; budget is 1791")
    (output / "background0.bin").write_bytes(b''.join(patterns[:1024]))
    overflow = [bytes(32)] + patterns[1024:]
    (output / "background1.bin").write_bytes(b''.join(overflow))
    maps = [[0] * 2048, [0] * 2048]
    for row in range(image.height // 8):
        for column in range(64):
            pattern, attributes = entries[row * 64 + column]
            address = (column // 32) * 1024 + row * 32 + column % 32
            plane = int(pattern >= 1024)
            maps[plane][address] = attributes | (pattern - 1023 if plane else pattern)
    for plane in range(2):
        (output / f"map{plane}.bin").write_bytes(struct.pack('<2048H', *maps[plane]))
    (output / "background.pal").write_bytes(palette_rgb555(converted, 16))
    return len(patterns), metrics


def compile_actors(image, output):
    rgba = np.asarray(image.convert('RGBA'), dtype=np.uint8)
    opaque = rgba[:, :, 3] >= 128
    rgb5 = rgba[:, :, :3] >> 3
    colours = sorted(set(map(tuple, rgb5[opaque].tolist())))
    if len(colours) > 127:
        raise ValueError("actor colours exceed 127 plus the patrol tint; add per-resource palette management")
    lookup = {colour: index + 1 for index, colour in enumerate(colours)}
    indices = np.zeros(opaque.shape, dtype=np.uint8)
    for top in range(image.height):
        for left in range(image.width):
            if opaque[top, left]:
                indices[top, left] = lookup[tuple(rgb5[top, left])]
    palette = [0] * 256
    for index, (red, green, blue) in enumerate(colours, start=1):
        palette[index] = red | (green << 5) | (blue << 10)
        palette[index + 128] = (red // 2) | (min(31, green + 6) << 5) | ((blue // 2) << 10)
    (output / 'actors.pal').write_bytes(struct.pack('<256H', *palette))
    frames = bytearray()
    # each frame is six 16x16 hardware objects, each containing four 8x8 tiles.
    for variant in range(2):
        for frame in range(19):
            frame_left, frame_top = (frame % 4) * 32, (frame // 4) * 48
            for piece in range(6):
                piece_left, piece_top = frame_left + (piece % 2) * 16, frame_top + (piece // 2) * 16
                for tile_y in range(2):
                    for tile_x in range(2):
                        tile = indices[piece_top + tile_y * 8:piece_top + tile_y * 8 + 8,
                                       piece_left + tile_x * 8:piece_left + tile_x * 8 + 8].copy()
                        if variant:
                            tile[tile != 0] += 128
                        frames.extend(tile.tobytes())
    (output / 'actors.bin').write_bytes(frames)
    return len(colours)


def compile_assets(data):
    import_deceptus(data)
    OUTPUT.mkdir(parents=True, exist_ok=True)
    recipe = json.loads((ROOT / 'assets/rooms/catacombs.json').read_text())
    room = Image.open(SOURCE / 'catacombs.png').convert('RGB')
    patterns, metrics = compile_background(room, OUTPUT)
    actor_colours = compile_actors(Image.open(SOURCE / 'actors.png'), OUTPUT)
    collision_bytes = export_world(SOURCE, OUTPUT, recipe)
    resources = [('gba_background0', 'background0.bin'), ('gba_background1', 'background1.bin'),
                 ('gba_map0', 'map0.bin'), ('gba_map1', 'map1.bin'),
                 ('gba_background_palette', 'background.pal'), ('gba_actor_palette', 'actors.pal'),
                 ('gba_actor_frames', 'actors.bin')]
    assembly = ['.section .rodata.gba_assets,"a",%progbits']
    header = ['#ifndef GBA_ASSETS_H', '#define GBA_ASSETS_H', '#include "retro/types.h"']
    for label, filename in resources:
        assembly += ['.balign 4', f'.global {label}', f'{label}:', f'.incbin "{(OUTPUT / filename).as_posix()}"']
        header += [f'extern const retro_u8 {label}[];', f'#define {label.upper()}_BYTES {(OUTPUT / filename).stat().st_size}']
    header += ['#define GBA_ACTOR_FRAME_BYTES 1536', '#define GBA_ACTOR_FRAME_COUNT 19', '#endif', '']
    (OUTPUT / 'data.s').write_text('\n'.join(assembly) + '\n')
    (OUTPUT / 'gba_assets.h').write_text('\n'.join(header))
    report = {'source': 'lossless original-engine import', 'scale': '1:1', 'dithering': False,
              'background_patterns': patterns, 'background_palette_banks': 16, 'background_fidelity': metrics,
              'actor_opaque_rgb555_colours': actor_colours, 'actor_palette_reduction': False,
              'actor_frame_rom_bytes': (OUTPUT / 'actors.bin').stat().st_size,
              'actor_vram_bytes_per_visible_actor': 1536, 'collision_rom_bytes': collision_bytes,
              'screen': [240, 160]}
    (OUTPUT / 'budget.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, default=ROOT.parent / 'deceptus_engine/data')
    args = parser.parse_args()
    compile_assets(args.data)
