"""Export shared collision and mechanism definitions from the lossless import."""

import json
from pathlib import Path

from PIL import Image

from tiled import collision_grid


def export_world(source, output, recipe):
    source, output = Path(source), Path(output)
    output.mkdir(parents=True, exist_ok=True)
    solid_image = Image.open(source / "collision.png")
    solid = collision_grid(solid_image, recipe["collision_cell"], recipe["coverage_threshold"])
    one_way = collision_grid(Image.open(source / "one_way.png"), recipe["collision_cell"], threshold=0.125)
    collision = [1 if first else 2 if second else 0 for first, second in zip(solid, one_way)]
    columns, rows = solid_image.width // recipe["collision_cell"], solid_image.height // recipe["collision_cell"]
    (output / "collision.bin").write_bytes(bytes(collision))
    lines = ['#include "retro/engine.h"', '', 'static const retro_u8 room_collision[] = {']
    lines += ['    ' + ','.join(map(str, collision[offset:offset + columns])) + ','
              for offset in range(0, len(collision), columns)]
    lines += ['};', '']
    mechanisms = json.loads((source / "mechanisms.json").read_text())
    definitions, targets = mechanisms["definitions"], mechanisms["targets"]
    if definitions:
        lines += ['static const retro_mechanism_def room_mechanisms[] = {']
        for definition in definitions:
            bounds = ', '.join(map(str, definition['bounds']))
            lines.append(f"    {{{definition['source_id']}, {{{bounds}}}, {definition['target_first']}, "
                         f"{definition['target_count']}, {definition['value']}, {definition['kind']}, "
                         f"{definition['enabled']}, {definition['flags']}}},")
        lines += ['};', '']
    if targets:
        lines += ['static const retro_u16 room_targets[] = {' + ', '.join(map(str, targets)) + '};', '']
    coordinates = recipe["spawn"] + recipe["patrol"] + recipe["door"]
    lines += ['const retro_room deceptus_room = {', f'    room_collision, {columns}, {rows}, ' + ', '.join(map(str, coordinates)) + ',',
              f"    {'room_mechanisms' if definitions else '0'}, {len(definitions)}, {'room_targets' if targets else '0'}, {len(targets)}",
              '};', '']
    (output / "room.c").write_text('\n'.join(lines))
    return len(collision)
