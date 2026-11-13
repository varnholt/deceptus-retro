"""Bake original Tiled tile animations with their source frame durations."""
import math
from PIL import Image
from tiled import TiledMap, layer_values, GID_MASK


def bake_tile_animations(data, recipe):
    tiled = TiledMap(data / recipe['map'])
    animations = {first + int(tile.get('id')): tile.find('animation')
                  for first, tileset, _ in tiled.tilesets for tile in tileset.findall('tile')
                  if tile.find('animation') is not None}
    crop = recipe['crop_pixels']
    result = []
    for spec in recipe.get('tile_animations', []):
        selected = []
        source_bounds = spec.get('source_bounds', crop)
        for layer, offset, _ in tiled.layers():
            if layer.tag != 'layer' or layer.get('name') not in spec['layers']:
                continue
            values = layer_values(layer)
            columns = int(layer.get('width'))
            for y in range(max(0,math.floor((source_bounds[1]-offset[1])/tiled.tile_height)),
                           min(int(layer.get('height')),math.ceil((source_bounds[1]+source_bounds[3]-offset[1])/tiled.tile_height))):
                for x in range(max(0,math.floor((source_bounds[0]-offset[0])/tiled.tile_width)),
                               min(columns,math.ceil((source_bounds[0]+source_bounds[2]-offset[0])/tiled.tile_width))):
                    gid = values[y*columns+x] & GID_MASK
                    if gid in animations:
                        selected.append((round(x*tiled.tile_width+offset[0]),round(y*tiled.tile_height+offset[1]),gid))
        if not selected:
            raise ValueError(f"no source animated tiles for {spec['name']}")
        schedules = {tuple(int(frame.get('duration')) for frame in animations[gid]) for _,_,gid in selected}
        if len(schedules) != 1:
            raise ValueError('selected tile animations require separate timing groups')
        durations_ms = next(iter(schedules))
        left = min(x for x,_,_ in selected); top = min(y for _,y,_ in selected)
        right = max(x+tiled.tile_width for x,_,_ in selected); bottom = max(y+tiled.tile_height for _,y,_ in selected)
        if not (crop[0]<=left<right<=crop[0]+crop[2] and crop[1]<=top<bottom<=crop[1]+crop[3]):
            raise ValueError('tile animation leaves the loaded room')
        plane = spec.get('plane',0)
        names = recipe.get('foreground_layers',[]) if plane else [n for n in recipe['visual_layers'] if n not in recipe.get('foreground_layers',[])]
        frames, durations, source_indices = [], [], []
        elapsed = ticks = 0
        for source_index,duration in enumerate(durations_ms):
            image = Image.new('RGBA',(right-left,bottom-top),(0,0,0,0) if plane else (10,10,20,255))
            image.alpha_composite(tiled.render([left,top,right-left,bottom-top],names,time_ms=elapsed))
            elapsed += duration
            next_tick = math.ceil(elapsed*60/1000)
            if next_tick>ticks:
                frames.append(image if plane else image.convert('RGB'))
                durations.append(next_tick-ticks)
                source_indices.append(source_index)
            ticks=next_tick
        result.append(dict(name=spec['name'],bounds=[left-crop[0],top-crop[1],right-left,bottom-top],
                           plane=plane,frames=frames,frame_count=len(frames),frame_ticks=1,
                           frame_durations=durations,source_durations_ms=list(durations_ms),source_frame_indices=source_indices,source_gids=sorted({gid for _,_,gid in selected})))
    return result
