"""Compile conditional scenery from source image layers and extra animations."""
import json
import math
from pathlib import Path
import xml.etree.ElementTree as ET
from PIL import Image
from content import map_objects


def variants(data, recipe, background):
    data = Path(data)
    map_path = data / recipe['map']
    root, objects = map_objects(map_path)
    animations = json.loads((data / 'sprites/extra_animations.json').read_text())
    clips = []
    for variant in recipe.get('scenery_variants', []):
        overlays = []
        animated = None
        for name in variant.get('image_layers', []):
            layer = next(item for item in root.iter('imagelayer') if item.get('name') == name)
            image = Image.open(map_path.parent / layer.find('image').get('source')).convert('RGBA')
            overlays.append((round(float(layer.get('offsetx', 0))), round(float(layer.get('offsety', 0))), image))
        for name in variant.get('extras', []):
            obj = next(item for item in objects if item['group'] == 'extras' and item['name'] == name)
            animation = animations[obj['properties']['animation_main_0']]
            x, y = animation['frame_offset']
            width, height = animation['frame_size']
            image = Image.open(data.parent / animation['texture']).convert('RGBA').crop((x, y, x+width, y+height))
            position = (int(obj['x'] + obj['width']/2 - animation['origin'][0]), int(obj['y'] + obj['height']/2 - animation['origin'][1]))
            overlays.append((*position,image))
            if obj['properties'].get('animation_main_1'):
                if animated: raise ValueError('scenery clip supports one animated extra')
                animated = (len(overlays)-1, position, [animation,animations[obj['properties']['animation_main_1']]])
        for source_id in variant.get('actors', []):
            from enemies import constants
            obj = next(item for item in objects if item['id'] == source_id)
            values = constants(data / 'scripts/enemies' / obj['properties']['script'])
            width, height = values['SPRITE_WIDTH_PX'], values['SPRITE_HEIGHT_PX']
            image = Image.open(data.parent / values['properties']['sprite']).convert('RGBA').crop((0,0,width,height))
            overlays.append((int(obj['x']), int(obj['y']), image))
        if not overlays:
            raise ValueError('empty scenery variant')
        crop = recipe['crop_pixels']
        left = math.floor((min(item[0] for item in overlays) - crop[0])/8)*8
        top = math.floor((min(item[1] for item in overlays) - crop[1])/8)*8
        right = math.ceil((max(x+image.width for x,y,image in overlays) - crop[0])/8)*8
        bottom = math.ceil((max(y+image.height for x,y,image in overlays) - crop[1])/8)*8
        if not (0 <= left < right <= background.width and 0 <= top < bottom <= background.height):
            raise ValueError('scenery variant leaves its room')
        phases = [(None,0,1)]
        if animated:
            phases = [(animation,index,duration) for animation in animated[2]
                      for index,duration in enumerate(animation['frame_durations'])]
        frames, durations = [], []
        elapsed_ms, elapsed_ticks = 0,0
        for animation,index,duration in phases:
            if animation:
                x,y = animation['frame_offset']; w,h = animation['frame_size']
                frame = Image.open(data.parent / animation['texture']).convert('RGBA').crop((x+index*w,y,x+(index+1)*w,y+h))
                overlays[animated[0]] = (*animated[1],frame)
                elapsed_ms += duration
                next_tick = math.ceil(elapsed_ms*60/1000)
                durations.append(max(1,next_tick-elapsed_ticks)); elapsed_ticks=next_tick
            image = background.crop((left,top,right,bottom)).convert('RGBA')
            for x,y,overlay in overlays:
                image.alpha_composite(overlay, (x-crop[0]-left,y-crop[1]-top))
            frames.append(image.convert('RGB'))
        clips.append(dict(name=variant['name'], bounds=[left,top,right-left,bottom-top],
                          required_flags=variant.get('required_flags',0), forbidden_flags=variant.get('forbidden_flags',0),
                          frames=frames, frame_count=len(frames), frame_ticks=1, frame_durations=durations))
    return clips
