"""Import Deceptus content and compile indexed images for the SNES backend."""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

from tiled import TiledMap, collision_grid, layer_values, GID_MASK
from palettes import banked_background, palette_rgb555
from content import crop_mechanisms
from world_export import export_world

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "source"
GENERATED = ROOT / "assets" / "generated"
ACTOR_FRAME_COUNT = 26


def import_deceptus(data, recipe_path=None, source=SOURCE):
    data = Path(data)
    source.mkdir(parents=True, exist_ok=True)
    recipe = json.loads((recipe_path or ROOT / "assets/rooms/catacombs.json").read_text())
    tiled = TiledMap(data / recipe["map"])
    crop = recipe["crop_pixels"]
    names = recipe["visual_layers"]
    room = Image.new("RGBA", crop[2:], (10, 10, 20, 255))
    foreground_names = recipe.get('foreground_layers', [])
    room.alpha_composite(tiled.render(crop, [name for name in names if name not in foreground_names]))
    if foreground_names:
        tiled.render(crop, foreground_names).save(source / 'foreground.png')
    room.convert("RGB").save(source / "catacombs.png")
    # tile collision vectors, not artwork alpha; dynamic blockers stay dynamic.
    collision = tiled.render(crop, recipe["collision_layers"], collision=True)
    one_way = tiled.render(crop, recipe["one_way_layers"], collision=True)
    collision.save(source / "collision.png")
    one_way.save(source / "one_way.png")
    water = Image.new('L', tuple(crop[2:]))
    if recipe.get('import_water'):
        for layer, offset, _ in tiled.layers():
            if layer.get('name') != 'atmosphere' or layer.tag != 'layer':
                continue
            values = layer_values(layer); columns = int(layer.get('width'))
            for y in range(0,crop[3],8):
                for x in range(0,crop[2],8):
                    tx=int((crop[0]+x+4-offset[0])//tiled.tile_width); ty=int((crop[1]+y+4-offset[1])//tiled.tile_height)
                    if not (0<=tx<columns and 0<=ty<int(layer.get('height'))): continue
                    gid=values[ty*columns+tx]&GID_MASK
                    if not gid: continue
                    first,tileset,_=next(item for item in reversed(tiled.tilesets) if item[0]<=gid)
                    if tileset.get('name')=='physics_tiles' and 48<=gid-first<=56:
                        ImageDraw.Draw(water).rectangle((x,y,x+7,y+7),fill=255)
    water.save(source/'water.png')
    animations = json.loads((data / "sprites" / "animations.json").read_text())
    original = Image.open(data / "sprites" / "player.png").convert("RGBA")
    sheet = Image.new("RGBA", (128, 336))
    selections = [("player_idle_r", index) for index in range(8)]
    selections += [("player_run_r", index) for index in range(8)]
    selections += [("player_jump_up_r", 0), ("player_jump_down_r", 0)]
    selections += [('prototype_portal',0)] + [('player_bend_down_r',index) for index in range(7)]
    for frame, (name, index) in enumerate(selections):
        if name == 'prototype_portal': continue
        animation = animations[name]
        offset_x, offset_y = animation["frame_offset"]
        width, height = animation["frame_size"]
        origin_x, origin_y = animation["origin"]
        left = int(offset_x + index * width + origin_x - 16)
        top = int(offset_y + origin_y - 48)
        image = original.crop((left, top, left + 32, top + 48))
        sheet.alpha_composite(image, ((frame % 4) * 32, (frame // 4) * 48))
    # a simple prototype portal; original painted doors remain visible in the BG.
    portal = Image.new("RGBA", (32, 48))
    draw = ImageDraw.Draw(portal)
    draw.rounded_rectangle((3, 2, 28, 47), radius=10, fill=(22, 22, 44), outline=(221, 169, 97), width=2)
    draw.line((8, 45, 8, 16, 12, 8, 20, 8, 24, 16, 24, 45), fill=(120, 62, 100), width=2)
    draw.rectangle((20, 28, 22, 30), fill=(235, 190, 120))
    sheet.alpha_composite(portal, (64, 192))
    sheet.save(source / "actors.png")
    definitions, targets, unsupported = crop_mechanisms(data, recipe)
    (source / "mechanisms.json").write_text(json.dumps({"definitions": definitions, "targets": targets,
        "unsupported_objects": unsupported, "source_map": recipe["map"]}, indent=2) + "\n")
    if recipe.get('import_enemies'):
        from enemies import crop_enemies
        (source / 'enemies.json').write_text(json.dumps(crop_enemies(data, recipe), indent=2) + "\n")
    else:
        (source / 'enemies.json').write_text('{"definitions": [], "paths": [], "unsupported": []}\n')
    manifest = {
        "source_map": "deceptus_engine/data/level-catacombs/catacombs.tmx",
        "crop_pixels": crop,
        "visual_layers": names,
        "collision_layers": ["level", "box_colliders"],
        "one_way_layers": ["level_solid_onesided"],
        "collision_cell": 8,
        "coverage_threshold": 0.5,
        "limitations": ["slopes become 8px steps", "layer effects and parallax are flattened",
                        "only the selected crop is imported", "mechanism visuals and behavior depend on the target adapter"],
        "frames": [{"animation": name, "source_frame": index} for name, index in selections],
        "artwork_owner": "dstar",
    }
    (source / "import.json").write_text(json.dumps(manifest, indent=2) + "\n")


def indexed(image, transparent=False):
    rgba = image.convert("RGBA")
    # quantize only opaque pixels so transparent RGB does not waste colours.
    opaque = [colour[:3] for colour in rgba.getdata() if colour[3] >= 128]
    sample = Image.new("RGB", (max(1, len(opaque)), 1))
    if opaque:
        sample.putdata(opaque)
    quantized = sample.quantize(colors=15, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    palette = (quantized.getpalette() or [0] * 45)[:45]
    palette += [0] * (45-len(palette))
    colours = [tuple(palette[index:index + 3]) for index in range(0, 45, 3)]
    lookup = {}
    pixels = []
    for red, green, blue, alpha in rgba.getdata():
        if transparent and alpha < 128:
            pixels.append(0)
            continue
        colour = (red, green, blue)
        if colour not in lookup:
            lookup[colour] = 1 + min(range(15), key=lambda index: sum((colour[channel] - colours[index][channel]) ** 2 for channel in range(3)))
        pixels.append(lookup[colour])
    result = Image.new("P", rgba.size)
    result.putpalette([0, 0, 0] + palette + [0] * (768 - 48))
    result.putdata(pixels)
    return result


def planar4(tile):
    pixels = list(tile.getdata())
    payload = bytearray(32)
    for row in range(8):
        for column in range(8):
            value = pixels[row * 8 + column]
            for plane in range(4):
                offset = (plane // 2) * 16 + row * 2 + plane % 2
                payload[offset] |= ((value >> plane) & 1) << (7 - column)
    return bytes(payload)


def palette_bytes(image, green=False):
    palette = image.getpalette()[:48]
    values = []
    for offset in range(0, 48, 3):
        red, green_channel, blue = palette[offset:offset + 3]
        if green:
            red, green_channel, blue = red // 2, min(255, green_channel + 45), blue // 2
        values.append((red >> 3) | ((green_channel >> 3) << 5) | ((blue >> 3) << 10))
    return struct.pack("<16H", *values)


def compile_assets():
    GENERATED.mkdir(parents=True, exist_ok=True)
    room = Image.open(SOURCE / "catacombs.png").convert("RGB")
    # keep the label in the world so it consumes no extra background layer.
    draw = ImageDraw.Draw(room)
    draw.rectangle((0, 0, room.width, 15), fill=(10, 10, 20))
    font = ImageFont.load_default(size=10)
    draw.text((8, 1), "DECEPTUS  /  CATACOMBS", font=font, fill=(198, 175, 180))
    draw.text((280, 1), "B: JUMP   UP: ENTER   START: PAUSE", font=font, fill=(198, 175, 180))
    room, tile_banks, colour_metrics = banked_background(room)
    room.save(GENERATED / "room-preview.png")
    (GENERATED / "colour-report.json").write_text(json.dumps(colour_metrics, indent=2) + "\n")
    tiles = [bytes(32)]
    lookup = {tiles[0]: 0}
    entries = []
    for top in range(0, room.height, 8):
        for left in range(0, room.width, 8):
            image = room.crop((left, top, left + 8, top + 8))
            tile = planar4(image)
            if tile not in lookup:
                tile_index = len(tiles)
                lookup[tile] = tile_index
                tiles.append(tile)
                lookup.setdefault(planar4(image.transpose(Image.Transpose.FLIP_LEFT_RIGHT)), tile_index | 0x4000)
                lookup.setdefault(planar4(image.transpose(Image.Transpose.FLIP_TOP_BOTTOM)), tile_index | 0x8000)
                lookup.setdefault(planar4(image.transpose(Image.Transpose.ROTATE_180)), tile_index | 0xC000)
            entry = lookup[tile]
            bank = tile_banks[(top // 8) * (room.width // 8) + left // 8]
            entries.append((entry & 0x3FFF, (entry & 0xC000) | (bank << 10)))
    # use a second Mode 1 plane for overflow: preserve every converted tile.
    # BG0: word $1000-$4fff; BG1: $5000-$5fff; OBJ: $6000-$7fff.
    if len(tiles) > 1279:
        raise ValueError(f"room needs {len(tiles)} background tiles; combined VRAM budget is 1279")
    (GENERATED / "room.pic").write_bytes(b"".join(tiles[:1024]))
    overflow = [bytes(32)] + tiles[1024:]
    (GENERATED / "overflow.pic").write_bytes(b"".join(overflow))
    (GENERATED / "room.pal").write_bytes(palette_rgb555(room, 8))
    # hardware 64x32 tilemaps are two consecutive 32x32 screens.
    hardware_map = [0] * 2048
    overflow_map = [0] * 2048
    for row in range(room.height // 8):
        for column in range(64):
            address = (column // 32) * 1024 + row * 32 + column % 32
            pattern, attributes = entries[row * 64 + column]
            if pattern < 1024:
                hardware_map[address] = attributes | pattern
            else:
                overflow_map[address] = attributes | (pattern - 1023)
    (GENERATED / "room.map").write_bytes(struct.pack("<2048H", *hardware_map))
    (GENERATED / "overflow.map").write_bytes(struct.pack("<2048H", *overflow_map))
    actors = indexed(Image.open(SOURCE / "actors.png").crop((0,0,128,240)), transparent=True)
    actors.save(GENERATED / "actors-preview.png")
    # object tile rows must have exactly 16 tiles; frames use 2x3 16px objects.
    actor_tiles = [planar4(actors.crop((left, top, left + 8, top + 8)))
                   for top in range(0, actors.height, 8) for left in range(0, 128, 8)]
    (GENERATED / "actors.pic").write_bytes(b"".join(actor_tiles))
    (GENERATED / "actors.pal").write_bytes(palette_bytes(actors) + palette_bytes(actors, green=True))
    (GENERATED / "asset_sizes.h").write_text(
        f"#define ROOM_PATTERN_BYTES {min(len(tiles), 1024) * 32}\n"
        f"#define OVERFLOW_PATTERN_BYTES {len(overflow) * 32}\n"
        "#define ROOM_PALETTE_BYTES 256\n"
        f"#define ACTOR_PATTERN_BYTES {len(actor_tiles) * 32}\n")
    recipe = json.loads((ROOT / "assets/rooms/catacombs.json").read_text())
    collision_bytes = export_world(SOURCE, GENERATED, recipe)
    sections = []
    for label, filename in [("room_patterns", "room.pic"), ("room_palette", "room.pal"),
                            ("room_map", "room.map"), ("overflow_patterns", "overflow.pic"),
                            ("overflow_map", "overflow.map"), ("actor_patterns", "actors.pic"), ("actor_palette", "actors.pal")]:
        sections += [f'.section "{label}_section" superfree', f"{label}:",
                     f'.incbin "../../assets/generated/{filename}"', f"{label}_end:", ".ends", ""]
    (GENERATED / "data.asm").write_text('.include "hdr.asm"\n\n' + "\n".join(sections))
    report = {"background_tiles": len(tiles), "background_tile_limit": 1279,
              "background_vram_bytes": (len(tiles) + 1) * 32, "tilemap_vram_bytes": 8192,
              "object_vram_bytes": len(actor_tiles) * 32, "collision_rom_bytes": collision_bytes,
              "background_palettes": 8, "colour_fidelity": colour_metrics,
              "object_palettes": 2, "max_logical_draws": 12,
              "objects_per_actor": 6, "worst_objects_on_scanline": 24,
              "sprite_scanline_limit": 32, "sprite_8px_slivers_per_scanline": 48,
              "sprite_8px_sliver_limit": 34,
              "note": "backend enforces both scanline budgets and drops excess metasprites",
              "source_sha256": {path.name: hashlib.sha256(path.read_bytes()).hexdigest()
                                for path in sorted(SOURCE.glob("*.png"))}}
    (GENERATED / "budget.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--import-data", type=Path, default=ROOT.parent / "deceptus_engine/data", help="original deceptus_engine/data directory")
    arguments = parser.parse_args()
    import_deceptus(arguments.import_data)
    compile_assets()
