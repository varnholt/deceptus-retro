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
    if (source/'water.png').exists():
        water=collision_grid(Image.open(source/'water.png'),recipe['collision_cell'],threshold=0.5)
        collision=[value|(8 if wet else 0) for value,wet in zip(collision,water)]
    columns, rows = solid_image.width // recipe["collision_cell"], solid_image.height // recipe["collision_cell"]
    (output / "collision.bin").write_bytes(bytes(collision))
    lines = ['#include "deceptus/engine.h"', '#include "deceptus/enemies.h"', '', 'static const deceptus_u8 room_collision[] = {']
    lines += ['    ' + ','.join(map(str, collision[offset:offset + columns])) + ','
              for offset in range(0, len(collision), columns)]
    lines += ['};', '']
    mechanisms = json.loads((source / "mechanisms.json").read_text())
    definitions, targets = mechanisms["definitions"], mechanisms["targets"]
    if definitions:
        lines += ['static const deceptus_mechanism_def room_mechanisms[] = {']
        for definition in definitions:
            bounds = ', '.join(map(str, definition['bounds']))
            lines.append(f"    {{{definition['source_id']}, {{{bounds}}}, {definition['target_first']}, "
                         f"{definition['target_count']}, {definition['value']}, {definition['kind']}, "
                         f"{definition['enabled']}, {definition['flags']}}},")
        lines += ['};', '']
    if definitions:
        lines += ['static const deceptus_mechanism_timing room_timings[] = {']
        lines += ['    {' + ', '.join(map(str, definition.get('timing', [0] * 6))) + '},' for definition in definitions]
        lines += ['};', '']
    if targets:
        lines += ['static const deceptus_u16 room_targets[] = {' + ', '.join(map(str, targets)) + '};', '']
    enemies = json.loads((source / 'enemies.json').read_text()) if (source / 'enemies.json').exists() else dict(definitions=[], paths=[])
    if enemies['definitions']:
        lines += ['static const deceptus_enemy_def room_enemies[] = {']
        for enemy in enemies['definitions']:
            values = [enemy[key] for key in ('source_id', 'x', 'y', 'path_first', 'path_count', 'time_offset', 'speed', 'acceleration', 'damage', 'period')]
            lines += ['    {' + ', '.join(map(str, values)) + ', {' + ', '.join(map(str, enemy['delays'])) + '}, ' + enemy['kind'] + ', ' + str(enemy['alignment']) + ', {' + ', '.join(map(str, enemy['cycles'])) + '}, ' + str(enemy['animation_fps']) + ', ' + str(enemy['idle_ticks']) + '},']
        lines += ['};', '']
    if enemies['paths']:
        lines += ['static const deceptus_path_point room_paths[] = {' + ', '.join('{' + ','.join(map(str, point)) + '}' for point in enemies['paths']) + '};', '']
    if recipe.get('water_physics'):
        lines += ['static const deceptus_water_configuration room_water = {'+', '.join(map(str,recipe['water_physics']))+'};']
    coordinates = recipe["spawn"] + recipe["patrol"] + recipe["door"]
    lines += ['const deceptus_room deceptus_scene = {', f'    room_collision, {columns}, {rows}, ' + ', '.join(map(str, coordinates)) + ',',
              f"    {'room_mechanisms' if definitions else '0'}, {len(definitions)}, {'room_targets' if targets else '0'}, {len(targets)}, {'room_timings' if definitions else '0'},",
              f"    {'room_enemies' if enemies['definitions'] else '0'}, {len(enemies['definitions'])}, {'room_paths' if enemies['paths'] else '0'}, {len(enemies['paths'])}, {recipe.get('max_health', 0)}, {recipe.get('skills', 0)}, {recipe.get('player_gravity',0)}, {recipe.get('player_gravity_fraction',0)}, {recipe.get('player_speed',0)}, {recipe.get('player_fall_gravity',0)}, {'&room_water' if recipe.get('water_physics') else '0'}",
              '};', '']
    (output / "room.c").write_text('\n'.join(lines))
    return len(collision)
