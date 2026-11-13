"""Compile GBA graphics directly from the original-engine lossless import."""

import argparse
import json
from pathlib import Path
import struct

import numpy as np
from PIL import Image

from assets import ROOT, SOURCE, import_deceptus, ACTOR_FRAME_COUNT
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


def compile_streamed_background(image, output, animations=(), foreground=None):
    if image.width % 8 or image.height % 8 or image.width > 2040 or image.height > 2040:
        raise ValueError('streamed room dimensions must be multiples of 8 below 2048px')
    atlas = Image.new('RGB', (image.width, image.height * (2 if foreground is not None else 1) + sum(clip['bounds'][3] * clip['frame_count'] for clip in animations)))
    atlas.paste(image, (0, 0))
    cursor = image.height
    foreground_top = cursor
    if foreground is not None:
        atlas.paste(foreground.convert('RGB'), (0,cursor))
        cursor += image.height
    for clip in animations:
        clip['atlas_y'] = cursor
        for frame in clip['frames']:
            atlas.paste(frame, (0, cursor))
            cursor += frame.height
    converted, banks, metrics = banked_background(atlas, bank_count=15)
    preview = converted.crop((0, 0, image.width, image.height))
    preview.save(output / 'base-background-preview.png')
    if foreground is not None:
        pixels = np.asarray(converted).copy()
        mask = np.asarray(foreground.convert('RGBA'))[:,:,3] >= 128
        pixels[foreground_top:foreground_top+image.height][~mask] = 0
        updated = Image.fromarray(pixels, mode='P')
        updated.putpalette(converted.getpalette())
        converted = updated
        front = converted.crop((0,foreground_top,image.width,foreground_top+image.height))
        front.save(output / 'foreground-preview.png')
        composite = np.asarray(preview).copy()
        composite[mask] = np.asarray(front)[mask]
        preview = Image.fromarray(composite, mode='P')
        preview.putpalette(converted.getpalette())
    preview.save(output / 'room-preview.png')
    for index, clip in enumerate(animations):
        folder = output / f'animation{index}'
        folder.mkdir(exist_ok=True)
        for frame_index in range(clip['frame_count']):
            y = clip['atlas_y'] + frame_index * clip['bounds'][3]
            converted.crop((0, y, clip['bounds'][2], y + clip['bounds'][3])).save(folder / f'converted-{frame_index}.png')
    patterns, lookup, cells = [bytes(32)], {bytes(32): 0}, []
    animation_cells = []
    foreground_cells = []
    regions = [(0, image.width, image.height, cells, None)]
    for clip in animations:
        regions.append((clip['atlas_y'], clip['bounds'][2], clip['bounds'][3] * clip['frame_count'], animation_cells, clip))
    if foreground is not None:
        regions.append((foreground_top,image.width,image.height,foreground_cells,None))
    for region_top, region_width, region_height, destination, clip in regions:
        if destination is foreground_cells:
            # each plane has its own character block and residency pool.
            lookup = {bytes(32):0}
        if clip is not None:
            clip['cell_offset'] = len(animation_cells) * 2
        for top in range(region_top, region_top + region_height, 8):
            for left in range(0, region_width, 8):
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
                destination.extend((entry & 0x3fff, (bank << 12) | ((entry & 0xc000) >> 4)))
    if len(patterns) > 8192:
        raise ValueError('room exceeds the 8192-pattern streaming lookup budget')
    (output / 'stream-patterns.bin').write_bytes(b''.join(patterns))
    (output / 'stream-cells.bin').write_bytes(struct.pack(f'<{len(cells)}H', *cells))
    (output / 'animation-cells.bin').write_bytes(struct.pack(f'<{len(animation_cells)}H', *animation_cells) or bytes(4))
    (output / 'foreground-cells.bin').write_bytes(struct.pack(f'<{len(foreground_cells)}H', *foreground_cells) or bytes(4))
    durations = []
    for clip in animations:
        clip['duration_offset'] = len(durations) * 2
        durations += clip.get('frame_durations', [])
    (output/'animation-durations.bin').write_bytes(struct.pack(f'<{len(durations)}H',*durations) or bytes(4))
    (output / 'background.pal').write_bytes(palette_rgb555(converted, 16))
    # legacy resources stay valid for the common backend declarations.
    for filename, size in [('background0.bin', 32), ('background1.bin', 32), ('map0.bin', 4096), ('map1.bin', 4096)]:
        (output / filename).write_bytes(bytes(size))
    return len(patterns), metrics


def compile_actors(image, output):
    rgba = np.asarray(image.convert('RGBA'), dtype=np.uint8)
    opaque = rgba[:, :, 3] >= 128
    rgb5 = rgba[:, :, :3] >> 3
    colours = sorted(set(map(tuple, rgb5[opaque].tolist())))
    if len(colours) > 51:
        raise ValueError('actor palette exceeds its 51-colour reservation')
    lookup = {colour: index + 1 for index, colour in enumerate(colours)}
    indices = np.zeros(opaque.shape, dtype=np.uint8)
    for top, left in zip(*np.where(opaque)):
        indices[top, left] = lookup[tuple(rgb5[top, left])]
    palette = [0] * 256
    for index, (red, green, blue) in enumerate(colours, start=1):
        palette[index] = red | (green << 5) | (blue << 10)
    (output / 'actors.pal').write_bytes(struct.pack('<256H', *palette))
    frames = bytearray()
    for _variant in range(2):
        for frame in range(ACTOR_FRAME_COUNT):
            frame_left, frame_top = (frame % 4) * 32, (frame // 4) * 48
            frames.extend(pack_objects(indices[frame_top:frame_top + 48, frame_left:frame_left + 32]))
    (output / 'actors.bin').write_bytes(frames)
    return len(colours)


def pack_objects(indices):
    graphics = bytearray()
    for top in range(0, indices.shape[0], 16):
        for left in range(0, indices.shape[1], 16):
            for tile_y in range(2):
                for tile_x in range(2):
                    graphics.extend(indices[top + tile_y * 8:top + tile_y * 8 + 8,
                                            left + tile_x * 8:left + tile_x * 8 + 8].tobytes())
    return graphics


def compile_mechanisms(data, output):
    graphics, palettes, descriptors, manifest = bytearray(), bytearray(), bytearray(), []
    specifications = [('levers.png', 72, 72, 11, 33, 52, 136, False),
                      ('enemy_spikeblock.png', 24, 24, 8, 40, 188, 12, False),
                      ('on_off_block.png', 24, 24, 8, 32, 200, 12, True),
                      ('collapsing_platform.png', 24, 72, 38, 152, 212, 44, True),
                      ('rat.png', 24, 24, 7, 70, 200, 12, True),
                      ('trap-body', 72, 72, 8, 4, 52, 24, True),
                      ('trap-muzzles', 24, 24, 8, 40, 52, 24, True),
                      ('../weapons/arrow.png', 24, 24, 15, 16, 80, 16, True)]
    for filename, width, height, columns, count, palette_first, palette_count, shared in specifications:
        actual_filename = 'enemy_arrowtrap_2.png' if filename.startswith('trap-') else filename
        atlas = Image.open(data / 'sprites' / actual_filename).convert('RGBA')
        rects, rotations = [], []
        for index in range(count):
            rotation = None
            if filename == 'trap-body':
                x, y = [(0, 0), (0, 72), (360, 0), (360, 72)][index]
                rect = [x, y, 72, 72]
            elif filename == 'trap-muzzles':
                direction, step = index // 10, index % 10
                rect = [step * 24, (6 + direction) * 24, 24 if direction < 2 else 8, 8 if direction < 2 else 24]
            elif filename == '../weapons/arrow.png':
                rect = [(index % 4) * 24, 24, 24, 24]
                rotation = [180, 0, 270, 90][index // 4]
            else:
                rect = [(index % columns) * width, (index // columns) * height, width, height]
            rects.append(rect)
            rotations.append(rotation)
        images = []
        for rect, rotation in zip(rects, rotations):
            x, y, w, h = rect
            image = atlas.crop((x, y, x + w, y + h))
            if rotation:
                image = image.transpose({90: Image.Transpose.ROTATE_90, 180: Image.Transpose.ROTATE_180, 270: Image.Transpose.ROTATE_270}[rotation])
            images.append(image)
        common = set()
        if shared:
            for image in images:
                pixels = np.asarray(image)
                common.update(map(tuple, (pixels[:, :, :3][pixels[:, :, 3] >= 128] >> 3).tolist()))
        if actual_filename == 'enemy_arrowtrap_2.png':
            pixels = np.asarray(atlas)
            common = set(map(tuple, (pixels[:, :, :3][pixels[:, :, 3] >= 128] >> 3).tolist()))
        for index, image in enumerate(images):
            width, height = image.size
            padded = Image.new('RGBA', (((width + 15) // 16) * 16, ((height + 15) // 16) * 16))
            padded.alpha_composite(image)
            pixels = np.asarray(padded)
            opaque = pixels[:, :, 3] >= 128
            rgb5 = pixels[:, :, :3] >> 3
            colours = sorted(common if shared else set(map(tuple, rgb5[opaque].tolist())))
            if len(colours) > palette_count:
                raise ValueError(f'{filename} frame {index}: {len(colours)} colours exceed {palette_count}')
            lookup = {colour: palette_first + slot for slot, colour in enumerate(colours)}
            indices = np.zeros(opaque.shape, dtype=np.uint8)
            for top, left in zip(*np.where(opaque)):
                indices[top, left] = lookup[tuple(rgb5[top, left])]
            packed = pack_objects(indices)
            palette = [red | (green << 5) | (blue << 10) for red, green, blue in colours]
            palette += [0] * (palette_count - len(palette))
            descriptors.extend(struct.pack('<IIH4B2x', len(graphics), len(palettes), len(packed),
                                           padded.width // 16, padded.height // 16, palette_first, palette_count))
            manifest.append(dict(source=actual_filename, frame=index, source_rect=rects[index], rotation=rotations[index], size=list(padded.size),
                                 offset=len(graphics), palette_offset=len(palettes), bytes=len(packed),
                                 palette_first=palette_first, palette_count=palette_count))
            graphics.extend(packed)
            palettes.extend(struct.pack(f'<{palette_count}H', *palette))
    (output / 'mechanisms.bin').write_bytes(graphics)
    (output / 'mechanisms.pal').write_bytes(palettes)
    (output / 'mechanisms.tbl').write_bytes(descriptors)
    (output / 'mechanisms-frames.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return len(manifest)


def compile_assets(data, scene="lever", recipe_path=None):
    source = SOURCE / "gba"
    recipe_path = recipe_path or ROOT / f"assets/rooms/gba-{scene}.json"
    import_deceptus(data, recipe_path, source)
    OUTPUT.mkdir(parents=True, exist_ok=True)
    recipe = json.loads((recipe_path).read_text())
    room = Image.open(source / 'catacombs.png').convert('RGB')
    streamed = recipe.get('stream_background', False)
    animations = []
    if recipe.get('bake_water'):
        from water import bake_room
        if not streamed:
            raise ValueError('baked water requires a streamed room')
        animations = bake_room(data, recipe, room)
    if recipe.get('scenery_variants'):
        from scenery import variants
        if not streamed:
            raise ValueError('conditional scenery requires a streamed room')
        animations += variants(data, recipe, room)
    if len(animations) > 4:
        raise ValueError('room exceeds four background animation clips')
    foreground = Image.open(source/'foreground.png').convert('RGBA') if recipe.get('foreground_layers') else None
    patterns, metrics = compile_streamed_background(room, OUTPUT, animations, foreground) if streamed else compile_background(room, OUTPUT)
    for index, clip in enumerate(animations):
        folder = OUTPUT / f'animation{index}'
        folder.mkdir(exist_ok=True)
        for frame_index, image in enumerate(clip['frames']):
            image.save(folder / f'original-{frame_index}.png')
        clip.pop('frames')
    (OUTPUT / 'animations.json').write_text(json.dumps(animations, indent=2) + '\n')
    actor_colours = compile_actors(Image.open(source / 'actors.png'), OUTPUT)
    mechanism_frames = compile_mechanisms(data, OUTPUT)
    collision_bytes = export_world(source, OUTPUT, recipe)
    if animations:
        with (OUTPUT / 'room.c').open('a') as handle:
            handle.write('\n#include "gba_assets.h"\nconst gba_background_animation gba_background_animations[] = {\n')
            for clip in animations:
                x, y, width, height = clip['bounds']
                handle.write('    {' + ','.join(map(str, [x//8, y//8, width//8, height//8, clip['frame_count'], clip['frame_ticks']])) +
                             f', gba_animation_cells + {clip["cell_offset"]}, {clip.get("required_flags",0)}, {clip.get("forbidden_flags",0)}, ' +
                             (f'(const retro_u16 *)(gba_animation_durations + {clip["duration_offset"]})' if clip.get('frame_durations') else '0') + '},\n')
            handle.write('};\n')
    resources = [('gba_background0', 'background0.bin'), ('gba_background1', 'background1.bin'),
                 ('gba_map0', 'map0.bin'), ('gba_map1', 'map1.bin'),
                 ('gba_background_palette', 'background.pal'), ('gba_actor_palette', 'actors.pal'),
                 ('gba_actor_frames', 'actors.bin'), ('gba_mechanism_frames', 'mechanisms.bin'),
                 ('gba_mechanism_palettes', 'mechanisms.pal'), ('gba_mechanism_table', 'mechanisms.tbl')]
    if streamed:
        resources += [('gba_stream_patterns', 'stream-patterns.bin'), ('gba_stream_cells', 'stream-cells.bin'),
                      ('gba_animation_cells', 'animation-cells.bin'), ('gba_foreground_cells','foreground-cells.bin'), ('gba_animation_durations','animation-durations.bin')]
    assembly = ['.section .rodata.gba_assets,"a",%progbits']
    header = ['#ifndef GBA_ASSETS_H', '#define GBA_ASSETS_H', '#include "retro/types.h"']
    for label, filename in resources:
        assembly += ['.balign 4', f'.global {label}', f'{label}:', f'.incbin "{(OUTPUT / filename).as_posix()}"']
        header += [f'extern const retro_u8 {label}[];', f'#define {label.upper()}_BYTES {(OUTPUT / filename).stat().st_size}']
    header += ['#define GBA_CAMPAIGN 0', f'#define GBA_LAYERED_BACKGROUND {int(foreground is not None)}', f'#define GBA_STREAMED_BACKGROUND {int(streamed)}', f'#define GBA_ROOM_COLUMNS {room.width // 8}', f'#define GBA_ROOM_ROWS {room.height // 8}', f'#define GBA_PATTERN_COUNT {patterns}',
               f'#define GBA_BACKGROUND_ANIMATION_COUNT {len(animations)}', f'#define GBA_BACKGROUND_ANIMATION_CAPACITY {len(animations)}', '#define GBA_TUTORIAL 0',
               'typedef struct gba_background_animation { retro_u16 left, top, columns, rows, frames, frame_ticks; const retro_u8 *cells; retro_u16 required_flags, forbidden_flags; const retro_u16 *durations; } gba_background_animation;',
               'extern const gba_background_animation gba_background_animations[];']
    header += ['typedef struct gba_mechanism_resource { retro_u32 offset; retro_u32 palette_offset; retro_u16 bytes; retro_u8 columns, rows, palette_first, palette_count; retro_u16 reserved; } gba_mechanism_resource;',
               '#define GBA_LEVER_FIRST 0', '#define GBA_SPIKES_FIRST 33', '#define GBA_BLOCK_FIRST 73', '#define GBA_COLLAPSE_FIRST 105', '#define GBA_RAT_FIRST 257', '#define GBA_TRAP_FIRST 327', '#define GBA_MUZZLE_FIRST 331', '#define GBA_ARROW_FIRST 371',
               f'#define GBA_MECHANISM_FRAME_COUNT {mechanism_frames}',
               '#define GBA_ACTOR_FRAME_BYTES 1536', f'#define GBA_ACTOR_FRAME_COUNT {ACTOR_FRAME_COUNT}', '#endif', '']
    (OUTPUT / 'data.s').write_text('\n'.join(assembly) + '\n')
    (OUTPUT / 'gba_assets.h').write_text('\n'.join(header))
    report = {'source': 'lossless original-engine import', 'scene': scene, 'scale': '1:1', 'dithering': False,
              'background_patterns': patterns, 'background_streamed': streamed, 'background_palette_banks': 15 if streamed else 16, 'background_fidelity': metrics,
              'foreground_layered': foreground is not None,
              'baked_background_animations': animations,
              'actor_opaque_rgb555_colours': actor_colours, 'actor_palette_reduction': False,
              'mechanism_frame_rom_bytes': (OUTPUT / 'mechanisms.bin').stat().st_size,
              'mechanism_transitions': '60Hz C timers; streamed original source animation frames',
              'background_screen_chunks': room.width // 256,
              'actor_frame_rom_bytes': (OUTPUT / 'actors.bin').stat().st_size,
              'actor_vram_bytes_per_visible_actor': 1536, 'collision_rom_bytes': collision_bytes,
              'screen': [240, 160]}
    (OUTPUT / 'budget.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, default=ROOT.parent / 'deceptus_engine/data')
    parser.add_argument("--scene", choices=["lever", "platforms", "preview", "cell", "water"], default="lever")
    args = parser.parse_args()
    compile_assets(args.data, args.scene)
