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
    if image.width % 256 or image.height % 8 or image.height > 256:
        raise ValueError("GBA scene width must be a multiple of 256 and height a multiple of 8 up to 256")
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
    columns = image.width // 8
    map_size = (columns // 32) * 1024
    maps = [[0] * map_size, [0] * map_size]
    for row in range(image.height // 8):
        for column in range(columns):
            pattern, attributes = entries[row * columns + column]
            address = (column // 32) * 1024 + row * 32 + column % 32
            plane = int(pattern >= 1024)
            maps[plane][address] = attributes | (pattern - 1023 if plane else pattern)
    for plane in range(2):
        (output / f"map{plane}.bin").write_bytes(struct.pack(f'<{map_size}H', *maps[plane]))
    (output / "background.pal").write_bytes(palette_rgb555(converted, 16))
    return len(patterns), metrics


def compile_actors(image, output, mechanisms=()):
    rgba = np.asarray(image.convert('RGBA'), dtype=np.uint8)
    opaque = rgba[:, :, 3] >= 128
    rgb5 = rgba[:, :, :3] >> 3
    colours_set = set(map(tuple, rgb5[opaque].tolist()))
    for mechanism in mechanisms:
        pixels = np.asarray(mechanism)
        colours_set.update(map(tuple, (pixels[:, :, :3][pixels[:, :, 3] >= 128] >> 3).tolist()))
    colours = sorted(colours_set)
    if len(colours) > 255:
        raise ValueError("actor and mechanism colours exceed the 255-colour OBJ palette")
    lookup = {colour: index + 1 for index, colour in enumerate(colours)}
    indices = np.zeros(opaque.shape, dtype=np.uint8)
    for top in range(image.height):
        for left in range(image.width):
            if opaque[top, left]:
                indices[top, left] = lookup[tuple(rgb5[top, left])]
    palette = [0] * 256
    for index, (red, green, blue) in enumerate(colours, start=1):
        palette[index] = red | (green << 5) | (blue << 10)

    (output / 'actors.pal').write_bytes(struct.pack('<256H', *palette))
    frames = bytearray()
    # each frame is six 16x16 hardware objects, each containing four 8x8 tiles.
    for _variant in range(2):
        for frame in range(19):
            frame_left, frame_top = (frame % 4) * 32, (frame // 4) * 48
            for piece in range(6):
                piece_left, piece_top = frame_left + (piece % 2) * 16, frame_top + (piece // 2) * 16
                for tile_y in range(2):
                    for tile_x in range(2):
                        tile = indices[piece_top + tile_y * 8:piece_top + tile_y * 8 + 8,
                                       piece_left + tile_x * 8:piece_left + tile_x * 8 + 8].copy()
                        # the mechanism scene has no prototype tinted patrol.
                        frames.extend(tile.tobytes())
    (output / 'actors.bin').write_bytes(frames)
    graphics = bytearray()
    for mechanism in mechanisms:
        pixels = np.asarray(mechanism)
        # 16x16 pieces in row order, four hardware tiles per piece.
        for top in range(0, mechanism.height, 16):
            for left in range(0, mechanism.width, 16):
                for tile_y in range(2):
                    for tile_x in range(2):
                        for y in range(8):
                            for x in range(8):
                                pixel = pixels[top + tile_y * 8 + y, left + tile_x * 8 + x]
                                graphics.append(lookup[tuple(pixel[:3] >> 3)] if pixel[3] >= 128 else 0)
    if len(graphics) > 32768 - 12 * 1536:
        raise ValueError('mechanism graphics exceed remaining OBJ VRAM')
    (output / 'mechanisms.bin').write_bytes(graphics)
    return len(colours)


def compile_assets(data):
    source = SOURCE / "gba"
    recipe_path = ROOT / "assets/rooms/gba-lever.json"
    import_deceptus(data, recipe_path, source)
    OUTPUT.mkdir(parents=True, exist_ok=True)
    recipe = json.loads((recipe_path).read_text())
    room = Image.open(source / 'catacombs.png').convert('RGB')
    patterns, metrics = compile_background(room, OUTPUT)
    lever = Image.open(data / 'sprites/levers.png').convert('RGBA')
    block = Image.open(data / 'sprites/on_off_block.png').convert('RGBA')
    spikes = Image.open(data / 'sprites/enemy_spikeblock.png').convert('RGBA')
    mechanisms = []
    for image in (lever.crop((0, 0, 72, 72)), lever.crop((0, 144, 72, 216)),
                  block.crop((0, 24, 24, 48)), spikes.crop((168, 96, 192, 120)),
                  spikes.crop((0, 96, 24, 120))):
        padded = Image.new('RGBA', (80, 64) if image.width == 72 else (32, 32))
        padded.alpha_composite(image)
        mechanisms.append(padded)
    actor_colours = compile_actors(Image.open(source / 'actors.png'), OUTPUT, mechanisms)
    collision_bytes = export_world(source, OUTPUT, recipe)
    resources = [('gba_background0', 'background0.bin'), ('gba_background1', 'background1.bin'),
                 ('gba_map0', 'map0.bin'), ('gba_map1', 'map1.bin'),
                 ('gba_background_palette', 'background.pal'), ('gba_actor_palette', 'actors.pal'),
                 ('gba_actor_frames', 'actors.bin'), ('gba_mechanism_frames', 'mechanisms.bin')]
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
              'mechanism_frame_rom_bytes': (OUTPUT / 'mechanisms.bin').stat().st_size,
              'mechanism_transitions': 'instant endpoint states; original transition timing not ported',
              'background_screen_chunks': room.width // 256,
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
