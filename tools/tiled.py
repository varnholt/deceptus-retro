"""Read finite orthogonal Tiled maps and rasterize their vector collision shapes."""

from __future__ import annotations

import base64
import gzip
import math
import struct
import xml.etree.ElementTree as ET
import zlib
from pathlib import Path

from PIL import Image, ImageDraw

FLIP_X, FLIP_Y, FLIP_DIAGONAL = 0x80000000, 0x40000000, 0x20000000
GID_MASK = 0x0FFFFFFF


def properties(element):
    return {item.get("name"): item.get("value", item.text or "")
            for item in element.findall("properties/property")}


def layer_values(layer):
    data = layer.find("data")
    if data is None or data.find("chunk") is not None:
        raise ValueError("only finite tile layers are supported")
    encoding = data.get("encoding")
    if encoding == "csv":
        values = [int(value) for value in (data.text or "").replace("\n", "").split(",") if value.strip()]
    elif encoding == "base64":
        payload = base64.b64decode(data.text or "")
        compression = data.get("compression")
        if compression == "zlib":
            payload = zlib.decompress(payload)
        elif compression == "gzip":
            payload = gzip.decompress(payload)
        elif compression:
            raise ValueError(f"unsupported Tiled compression: {compression}")
        values = list(struct.unpack(f"<{len(payload) // 4}I", payload))
    elif encoding is None:
        values = [int(tile.get("gid", "0")) for tile in data.findall("tile")]
    else:
        raise ValueError(f"unsupported Tiled encoding: {encoding}")
    if len(values) != int(layer.get("width")) * int(layer.get("height")):
        raise ValueError("tile layer length does not match its dimensions")
    return values


def transform_tile(image, gid):
    # tiled's diagonal flag swaps axes first, then horizontal/vertical flips.
    if gid & FLIP_DIAGONAL:
        image = image.transpose(Image.Transpose.TRANSPOSE)
    if gid & FLIP_X:
        image = image.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    if gid & FLIP_Y:
        image = image.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    return image


def draw_object(mask, element, origin=(0, 0), stroke=1):
    """Rasterize rectangles, ellipses, polygons and polylines, including rotation.

    Pixel centres determine filled polygon coverage. The right and bottom edges
    of axis-aligned rectangles are exclusive, matching Tiled dimensions.
    """
    base_x = float(element.get("x", "0")) + origin[0]
    base_y = float(element.get("y", "0")) + origin[1]
    width, height = float(element.get("width", "0")), float(element.get("height", "0"))
    angle = math.radians(float(element.get("rotation", "0")))
    polygon, polyline = element.find("polygon"), element.find("polyline")
    if element.find("point") is not None or element.find("text") is not None:
        return
    if element.get("gid"):
        raise ValueError("tile objects need an explicit collision shape")
    if polygon is not None or polyline is not None:
        shape = polygon if polygon is not None else polyline
        points = [tuple(map(float, pair.split(","))) for pair in shape.get("points").split()]
    elif element.find("ellipse") is not None:
        points = [(width * (1 + math.cos(step * math.tau / 64)) / 2,
                   height * (1 + math.sin(step * math.tau / 64)) / 2) for step in range(64)]
    else:
        if width <= 0 or height <= 0:
            return
        points = [(0, 0), (width, 0), (width, height), (0, height)]
    points = [(base_x + x * math.cos(angle) - y * math.sin(angle),
               base_y + x * math.sin(angle) + y * math.cos(angle)) for x, y in points]
    draw = ImageDraw.Draw(mask)
    if polyline is not None:
        draw.line(points, fill=255, width=stroke)
        return
    # use pixel-centre scan conversion: Pillow's polygon includes both edges.
    for pixel_y in range(max(0, math.floor(min(y for _, y in points))),
                         min(mask.height, math.ceil(max(y for _, y in points)))):
        scan_y = pixel_y + 0.5
        crossings = []
        for first, second in zip(points, points[1:] + points[:1]):
            if (first[1] <= scan_y < second[1]) or (second[1] <= scan_y < first[1]):
                crossings.append(first[0] + (scan_y - first[1]) * (second[0] - first[0]) / (second[1] - first[1]))
        crossings.sort()
        for left, right in zip(crossings[::2], crossings[1::2]):
            start = max(0, math.ceil(left - 0.5))
            end = min(mask.width, math.ceil(right - 0.5))
            if end > start:
                draw.line((start, pixel_y, end - 1, pixel_y), fill=255)


def collision_grid(mask, cell=8, threshold=0.5):
    """Return row-major occupancy; threshold is the covered fraction per cell."""
    if cell <= 0 or mask.width % cell or mask.height % cell or not 0 < threshold <= 1:
        raise ValueError("mask dimensions must align to cells; threshold must be in (0, 1]")
    result = []
    for top in range(0, mask.height, cell):
        for left in range(0, mask.width, cell):
            pixels = mask.crop((left, top, left + cell, top + cell)).getdata()
            result.append(int(sum(value != 0 for value in pixels) / (cell * cell) >= threshold))
    return result


class TiledMap:
    def __init__(self, path):
        self.path = Path(path)
        self.root = ET.parse(path).getroot()
        if self.root.get("orientation") != "orthogonal" or self.root.get("infinite", "0") != "0":
            raise ValueError("only finite orthogonal Tiled maps are supported")
        self.tile_width = int(self.root.get("tilewidth"))
        self.tile_height = int(self.root.get("tileheight"))
        self.tilesets = []
        self.cache = {}
        for reference in self.root.findall("tileset"):
            source = reference.get("source")
            folder = self.path.parent
            tileset = reference
            if source:
                tileset_path = folder / source
                tileset = ET.parse(tileset_path).getroot()
                folder = tileset_path.parent
            image = tileset.find("image")
            if image is None:
                raise ValueError("image-collection tilesets are not supported")
            atlas = Image.open(folder / image.get("source")).convert("RGBA")
            self.tilesets.append((int(reference.get("firstgid")), tileset, atlas))
        self.tilesets.sort(key=lambda entry: entry[0])

    def layers(self, parent=None, offset=(0, 0), visible=True):
        for element in (self.root if parent is None else parent):
            position = (offset[0] + float(element.get("offsetx", "0")),
                        offset[1] + float(element.get("offsety", "0")))
            shown = visible and element.get("visible", "1") != "0"
            if element.tag == "group":
                yield from self.layers(element, position, shown)
            elif element.tag in ("layer", "objectgroup"):
                yield element, position, shown

    def tile(self, gid, collision=False):
        key = gid, collision
        if key in self.cache:
            return self.cache[key]
        raw = gid & GID_MASK
        reference = next((entry for entry in reversed(self.tilesets) if entry[0] <= raw), None)
        if reference is None:
            raise ValueError(f"unknown tile gid {gid}")
        first_gid, tileset, atlas = reference
        local = raw - first_gid
        width, height = int(tileset.get("tilewidth")), int(tileset.get("tileheight"))
        spacing, margin = int(tileset.get("spacing", "0")), int(tileset.get("margin", "0"))
        columns = int(tileset.get("columns"))
        if local >= int(tileset.get("tilecount")):
            raise ValueError(f"tile gid outside tileset: {gid}")
        if collision:
            image = Image.new("L", (width, height))
            tile = tileset.find(f"tile[@id='{local}']")
            if tile is not None:
                for element in tile.findall("objectgroup/object"):
                    draw_object(image, element)
        else:
            left = margin + (local % columns) * (width + spacing)
            top = margin + (local // columns) * (height + spacing)
            image = atlas.crop((left, top, left + width, top + height))
        image = transform_tile(image, gid)
        self.cache[key] = image
        return image

    def render(self, crop, names, collision=False):
        left, top, width, height = crop
        canvas = Image.new("L" if collision else "RGBA", (width, height))
        layers = [(element, offset) for element, offset, _ in self.layers()
                  if element.get("name") in names]
        if not collision:
            layers.sort(key=lambda entry: int(properties(entry[0]).get("z", 0)))
        for layer, offset in layers:
            if layer.tag == "objectgroup":
                if collision:
                    for element in layer.findall("object"):
                        draw_object(canvas, element, (offset[0] - left, offset[1] - top))
                continue
            values = layer_values(layer)
            columns, rows = int(layer.get("width")), int(layer.get("height"))
            for tile_y in range(max(0, math.floor((top - offset[1]) / self.tile_height)),
                                min(rows, math.ceil((top + height - offset[1]) / self.tile_height))):
                for tile_x in range(max(0, math.floor((left - offset[0]) / self.tile_width)),
                                    min(columns, math.ceil((left + width - offset[0]) / self.tile_width))):
                    gid = values[tile_y * columns + tile_x]
                    if not gid:
                        continue
                    tile = self.tile(gid, collision)
                    position = (round(tile_x * self.tile_width + offset[0] - left),
                                round(tile_y * self.tile_height + offset[1] - top))
                    if collision:
                        # max combines overlapping shapes instead of erasing prior ones.
                        from PIL import ImageChops
                        overlay = Image.new("L", canvas.size)
                        overlay.paste(tile, position)
                        canvas = ImageChops.lighter(canvas, overlay)
                    else:
                        canvas.alpha_composite(tile, position)
        return canvas
