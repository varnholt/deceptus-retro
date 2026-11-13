"""Bake source water springs and gradient into deterministic, native-size scenery clips.

Only the build tool simulates springs or composites transparency. The console plays
tile frames. Authored random emitters become periodic, seeded emitter schedules;
player-driven splashes and underwater physics are deliberately separate features.
"""
import hashlib
import math
from pathlib import Path
import random

import numpy as np
from PIL import Image

from content import map_objects


def spring_step(heights, velocities, tension, dampening, spread, animation_speed):
    """WaterSurface::Segment::update and its eight neighbour integrations, at 60Hz."""
    velocities += -tension * heights - velocities * dampening
    heights += velocities
    for _ in range(8):
        differences = (heights[1:] - heights[:-1]) * (spread * animation_speed / 60)
        velocities[:-1] += differences
        velocities[1:] -= differences
        heights[:-1] += differences
        heights[1:] -= differences


def periodic_surface(surface, emitters, frame_count=12, frame_ticks=6):
    if not (1 <= frame_count <= 64 and 1 <= frame_ticks <= 60):
        raise ValueError('water clip exceeds the offline frame/time budget')
    props = surface['properties']
    ratio = float(props.get('pixel_ratio', 1))
    if ratio != 1:
        raise ValueError('water bake currently requires native pixel_ratio 1')
    width = int(surface['width'])
    segments = int(props.get('segment_count', width // 2))
    if segments < 2 or width % segments:
        raise ValueError('water segment count must divide its native width')
    period = frame_count * frame_ticks
    events = [[] for _ in range(period)]
    for emitter in emitters:
        fields = emitter['properties']
        minimum = float(fields.get('interval_min_s', .3)) * 60
        maximum = float(fields.get('interval_max_s', .6)) * 60
        intervals = [value for value in range(1, period + 1)
                     if period % value == 0 and minimum <= value <= maximum]
        if not intervals:
            raise ValueError('water emitter has no interval compatible with the bake period')
        interval = intervals[len(intervals) // 2]
        rng = random.Random(emitter['id'])
        x_from = float(fields.get('x_from_px', 0))
        x_to = float(fields.get('x_to_px', emitter['width']))
        splash_width = float(fields.get('width_px', emitter['width']))
        if splash_width <= 0 or x_to - x_from < splash_width:
            raise ValueError('invalid water emitter range')
        offset = emitter['x'] - surface['x']
        for tick in range(emitter['id'] % interval, period, interval):
            for _ in range(int(fields.get('count', 1))):
                left = offset + rng.uniform(x_from, x_to - splash_width)
                first = int(left * segments / width)
                last = int((left + splash_width) * segments / width)
                # the source splash intentionally skips the final segment.
                for index in range(max(0, first), min(segments - 1, last)):
                    events[tick].append((index, float(fields.get('velocity', 2))))
    heights, velocities = np.zeros(segments), np.zeros(segments)
    samples = []
    for cycle in range(65):
        previous = heights.copy(), velocities.copy()
        for tick in range(period):
            for index, velocity in events[tick]:
                velocities[index] = velocity
            spring_step(heights, velocities, float(props.get('tension', .025)),
                        float(props.get('dampening', .025)), float(props.get('spread', .25)),
                        float(props.get('animation_speed', 10)))
            if not np.isfinite(heights).all() or np.max(np.abs(heights)) > surface['height']:
                raise ValueError('water bake is unstable or leaves its authored height budget')
            if cycle == 64 and tick % frame_ticks == 0:
                samples.append(heights.copy())
        residual = max(np.max(np.abs(heights - previous[0])), np.max(np.abs(velocities - previous[1])))
    if residual > .0001:
        raise ValueError('water did not converge to a repeating clip')
    clamp = int(props.get('clamp_segment_count', 0))
    if clamp and (clamp < 0 or clamp * 2 >= segments):
        raise ValueError('invalid water corner clamp')
    scales = np.ones(segments)
    if clamp:
        scales[:clamp] = np.arange(clamp) / clamp
        scales[-clamp:] = scales[:clamp][::-1]
    return [sample * scales for sample in samples], float(residual)


def bake_surface(background, surface, emitters, gradient, crop, frame_count=12, frame_ticks=6):
    samples, residual = periodic_surface(surface, emitters, frame_count, frame_ticks)
    x, y = int(surface['x']) - crop[0], int(surface['y']) - crop[1]
    width, height = int(surface['width']), int(surface['height'])
    left, top = x // 8 * 8, math.floor((y + min(0, min(map(np.min, samples)))) / 8) * 8
    right, bottom = math.ceil((x + width) / 8) * 8, math.ceil((y + height) / 8) * 8
    if not (0 <= left < right <= background.width and 0 <= top < bottom <= background.height):
        raise ValueError('water surface must fit completely inside its loaded room')
    gradient_pixels = np.asarray(gradient.convert('RGBA'))[0]
    if gradient.height != 1 or gradient.width != 48:
        raise ValueError('water gradient must match the source 48x1 strip')
    opacity = int(surface['properties'].get('opacity', 200))
    frames = []
    for sample in samples:
        overlay = np.zeros((bottom - top, right - left, 4), dtype=np.uint8)
        wave = np.interp(np.arange(width) + .5, np.linspace(0, width, len(sample)), sample)
        for column, displacement in enumerate(wave):
            start = max(top, math.ceil(y + displacement - .5))
            for row in range(start, y + height):
                distance = (row + .5 - y - displacement) / (height - displacement)
                colour = gradient_pixels[min(len(gradient_pixels) - 1, int(distance * 48))].copy()
                colour[3] = int(colour[3]) * opacity // 255
                overlay[row - top, x + column - left] = colour
        image = background.crop((left, top, right, bottom)).convert('RGBA')
        image.alpha_composite(Image.fromarray(overlay))
        frames.append(image.convert('RGB'))
    return dict(source_id=surface['id'], name=surface['name'], bounds=[left, top, right-left, bottom-top],
                frame_count=frame_count, frame_ticks=frame_ticks, frames=frames,
                loop_residual=residual, emitter_ids=[item['id'] for item in emitters])


def bake_room(data, recipe, background):
    data = Path(data)
    _, objects = map_objects(data / recipe['map'])
    left, top, width, height = recipe['crop_pixels']
    clips = []
    for surface in objects:
        if surface['group'] != 'water_surface':
            continue
        if surface['x'] + surface['width'] <= left or surface['x'] >= left + width or surface['y'] + surface['height'] <= top or surface['y'] >= top + height:
            continue
        if surface['rotation'] or surface['shape'] != 'rectangle':
            raise ValueError('rotated or nonrectangular water is unsupported')
        emitters = [item for item in objects if item['group'] == 'water_surface_emitter'
                    and item['properties'].get('reference') == surface['name']]
        gradient_path = surface['properties'].get('gradient_texture', 'data/sprites/water_surface_gradient.png')
        gradient = Image.open(data.parent / gradient_path)
        clip = bake_surface(background, surface, emitters, gradient, recipe['crop_pixels'])
        sources = [data / recipe['map'], data.parent / gradient_path,
                   data.parent / 'src/game/mechanisms/watersurface.cpp',
                   data.parent / 'src/game/mechanisms/watersurface.h']
        clip['source_hashes'] = {path.relative_to(data.parent).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
                                 for path in sources}
        clips.append(clip)
    return clips
