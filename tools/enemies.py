"""Import the native enemy subset from original placements and Lua constants."""
import hashlib
from pathlib import Path
from lua_source import ast, nodes, literal, name_of
from content import map_objects


def constants(path):
    tree = ast.parse(Path(path).read_text(encoding='utf-8-sig'))
    result = {}
    for statement in tree.body.body:
        if isinstance(statement, nodes.Assign):
            for target, value in zip(statement.targets, statement.values):
                if isinstance(target, nodes.Name):
                    if isinstance(value, nodes.Table) and target.id == 'properties':
                        result[target.id] = {name_of(field.key): literal(field.value) for field in value.fields}
                    else:
                        try:
                            result[target.id] = literal(value)
                        except ValueError:
                            pass
        if isinstance(statement, nodes.Function) and name_of(statement.name) == 'initialize':
            for child in statement.body.body:
                if isinstance(child, nodes.Assign):
                    for target, value in zip(child.targets, child.values):
                        if isinstance(target, nodes.Name) and target.id == '_fire_delays':
                            result[target.id] = literal(value)
    return result


def crop_enemies(data, recipe):
    left, top, width, height = recipe['crop_pixels']
    _, objects = map_objects(Path(data) / recipe['map'])
    definitions, unsupported, paths, hashes = [], [], [], {}
    for obj in objects:
        if obj['group'] != 'enemies' or not (left <= obj['x'] < left + width and top <= obj['y'] < top + height):
            continue
        script = obj['properties'].get('script')
        if script not in ('rat.lua', 'arrowtrap_2.lua') or obj['rotation']:
            unsupported.append(dict(id=obj['id'], script=script, reason='enemy behavior not ported'))
            continue
        path = Path(data) / 'scripts/enemies' / script
        values = constants(path)
        hashes[script] = hashlib.sha256(path.read_bytes()).hexdigest()
        props = values['properties']
        first = len(paths)
        raw_path = [int(field.strip()) * 24 for field in obj['properties'].get('path', '').split(',') if field.strip()]
        if len(raw_path) % 2:
            raise ValueError(f'enemy {obj["id"]}: patrol path needs x/y pairs')
        if len(raw_path) == 2:
            raw_path = [int(obj['x']), int(obj['y'])] + raw_path
        if script == 'rat.lua' and not raw_path:
            raise ValueError(f'rat {obj["id"]}: no authored patrol path')
        points = [[x-left, y-top] for x, y in zip(raw_path[::2], raw_path[1::2])]
        if any(not (0 <= x < width and 0 <= y < height) for x, y in points):
            unsupported.append(dict(id=obj['id'], script=script, reason='patrol path leaves loaded room'))
            continue
        paths.extend(points)
        if len(paths) - first > 8:
            raise ValueError('enemy patrol exceeds eight points')
        alignment = {'left': 0, 'right': 1, 'down': 2, 'up': 3}.get(obj['properties'].get('alignment', 'right'))
        if alignment is None:
            raise ValueError('invalid arrow-trap alignment')
        rat = script == 'rat.lua'
        definitions.append(dict(source_id=obj['id'], kind='RETRO_RAT' if rat else 'RETRO_ARROW_TRAP',
            x=int(obj['x'])-left, y=int(obj['y'])-top, path_first=first, path_count=len(paths)-first,
            alignment=alignment, time_offset=round(float(obj['properties'].get('time_offset_s', 0))*60),
            speed=round((props['velocity_walk_max'] if rat else values['_speed'] * 5) * 48/60*16),
            acceleration=round(props.get('acceleration_ground', 0)*48/60*16),
            damage=int(props['damage'] if rat else 60),
            period=0 if rat else round(values['_fire_interval']*60),
            delays=[0,0,0] if rat else [round(value*60) for value in values['_fire_delays']],
            cycles=values.get('CYCLE_LENGTHS', [0]*5),
            animation_fps=int(values.get('ANIMATION_SPEED', 20)),
            idle_ticks=round(values.get('DELAY_BETWEEN_IDLE_FRAMES', 0)*60)))
    for definition in definitions:
        if not 0 < definition['speed'] < 128 or definition['animation_fps'] != 20:
            raise ValueError('enemy motion or animation exceeds the supported fixed-step contract')
        if any(not 0 <= definition[key] <= 65534 for key in ('source_id', 'time_offset', 'damage', 'period')):
            raise ValueError('enemy property exceeds the 16-bit runtime budget')
        if definition['kind'] == 'RETRO_RAT' and (not definition['acceleration'] or not 0 <= definition['idle_ticks'] <= 255):
            raise ValueError('unsupported rat movement or idle timing')
        if any(not 0 <= delay <= 65520 for delay in definition['delays']):
            raise ValueError('trap delay exceeds runtime budget')
    if len(definitions) > 8 or len(paths) > 64:
        raise ValueError('enemy room exceeds its fixed-capacity budget')
    return dict(definitions=definitions, paths=paths, unsupported=unsupported, source_hashes=hashes)
