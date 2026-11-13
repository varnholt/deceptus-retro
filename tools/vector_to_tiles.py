"""Convert Tiled collision vectors to a pixel mask, cell grid and object table."""

import argparse
import json
from pathlib import Path

from PIL import Image

from tiled import TiledMap, collision_grid, properties


def convert(source, layers, crop, output, cell=8, threshold=0.5):
    tiled = TiledMap(source)
    mask = tiled.render(crop, layers, collision=True)
    cells = collision_grid(mask, cell, threshold)
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    mask.save(output.with_suffix(".pixels.png"))
    output.with_suffix(".bin").write_bytes(bytes(cells))
    columns, rows = mask.width // cell, mask.height // cell
    grid = Image.new("L", (columns, rows))
    grid.putdata([value * 255 for value in cells])
    grid.resize(mask.size, Image.Resampling.NEAREST).save(output.with_suffix(".tiles.png"))
    objects = []
    for group, offset, _ in tiled.layers():
        if group.tag != "objectgroup" or group.get("name") not in layers:
            continue
        for element in group.findall("object"):
            x = float(element.get("x", "0")) + offset[0] - crop[0]
            y = float(element.get("y", "0")) + offset[1] - crop[1]
            width = float(element.get("width", "0"))
            height = float(element.get("height", "0"))
            # retain all objects: rotated/vector bounds may cross into the crop.
            objects.append({"id": int(element.get("id", "0")), "name": element.get("name", ""),
                            "class": element.get("class", element.get("type", "")),
                            "x": x, "y": y, "width": width, "height": height,
                            "rotation": float(element.get("rotation", "0")),
                            "properties": properties(element)})
    output.with_suffix(".json").write_text(json.dumps({"source": str(source), "crop": crop,
        "layers": layers, "cell_size": cell, "threshold": threshold, "columns": columns,
        "rows": rows, "layout": "row-major bytes: 0 empty, 1 occupied", "objects": objects}, indent=2) + "\n")
    print(f"wrote {columns}x{rows} collision cells and {len(objects)} object records to {output}.*")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--layers", nargs="+", required=True)
    parser.add_argument("--crop", type=int, nargs=4, required=True, metavar=("X", "Y", "WIDTH", "HEIGHT"))
    parser.add_argument("--cell", type=int, default=8)
    parser.add_argument("--threshold", type=float, default=0.5)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    convert(args.source, args.layers, args.crop, args.output, args.cell, args.threshold)
