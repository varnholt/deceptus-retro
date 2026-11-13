"""Inventory all upstream levels and export original Tiled objects without losing properties."""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET

from lua_source import analyse, BooleanCompiler, CompileError

ROOT = Path(__file__).resolve().parents[1]


def typed_properties(element):
    result = {}
    for field in element.findall("properties/property"):
        value = field.get("value", field.text or "")
        kind = field.get("type", "string")
        if kind in ("int", "object"):
            value = int(value)
        elif kind == "float":
            value = float(value)
        elif kind == "bool":
            value = value == "true"
        result[field.get("name")] = value
    return result


def map_objects(path):
    root = ET.parse(path).getroot()
    objects = []

    def visit(parent, offset=(0, 0)):
        for child in parent:
            position = (offset[0] + float(child.get("offsetx", "0")), offset[1] + float(child.get("offsety", "0")))
            if child.tag == "group":
                visit(child, position)
            elif child.tag == "objectgroup":
                for item in child.findall("object"):
                    if item.get("template"):
                        raise ValueError(f"object templates need explicit resolution: {path}, id {item.get('id')}")
                    shape = "rectangle"
                    points = []
                    for kind in ("polygon", "polyline", "ellipse", "point"):
                        node = item.find(kind)
                        if node is not None:
                            shape = kind
                            points = [list(map(float, pair.split(","))) for pair in node.get("points", "").split()]
                    objects.append({"id": int(item.get("id")), "name": item.get("name", ""),
                        "group": child.get("name", ""), "class": item.get("class", item.get("type", "")),
                        "x": float(item.get("x", "0")) + position[0],
                        "y": float(item.get("y", "0")) + position[1],
                        "width": float(item.get("width", "0")), "height": float(item.get("height", "0")),
                        "rotation": float(item.get("rotation", "0")), "shape": shape, "points": points,
                        "gid": int(item.get("gid", "0")), "properties": typed_properties(item)})
    visit(root)
    return root, objects


def inventory(data, output):
    data, output = Path(data).resolve(), Path(output)
    output.mkdir(parents=True, exist_ok=True)
    # a failed/unsupported recompilation must never leave an old successful fixture.
    for filename in ("source_logic.c", "setSwordCageEnabled.bin", "releaseSwordRing.bin"):
        (output / filename).unlink(missing_ok=True)
    levels = []
    active_scripts = Counter()
    modules = {}
    callback_support = {}
    programs = {}
    callback_objects = []
    for reference in json.loads((data / "config/levels.json").read_text()):
        filename = reference["levelname"]
        config_path = data.parent / filename
        config = json.loads(config_path.read_text())
        map_path = data.parent / config["filename"]
        root, objects = map_objects(map_path)
        counts = Counter(obj["group"] for obj in objects)
        for obj in objects:
            script = obj["properties"].get("script")
            if script and obj["group"] == "enemies":
                active_scripts[script] += 1
        level = {"id": config_path.parent.name, "config": filename,
                 "map": config["filename"], "source_sha256": hashlib.sha256(map_path.read_bytes()).hexdigest(),
                 "tile_width": int(root.get("tilewidth")), "tile_height": int(root.get("tileheight")),
                 "columns": int(root.get("width")), "rows": int(root.get("height")),
                 "group_counts": dict(sorted(counts.items())), "objects": objects}
        levels.append(level)
        script_path = config_path.parent / "level.lua"
        if script_path.exists():
            tree, info = analyse(script_path)
            key = script_path.relative_to(data).as_posix()
            modules[key] = info
            if config_path.parent.name == "level-catacombs":
                for function in ("setSwordCageEnabled", "releaseSwordRing"):
                    compiler = BooleanCompiler(tree, objects)
                    try:
                        program = compiler.compile(function)
                        (output / (function + ".bin")).write_bytes(program)
                        programs[function] = program
                        if function == "releaseSwordRing":
                            callback_objects = [obj for obj in objects if obj["id"] in compiler.resolved_targets]
                        callback_support[function] = {"status": "compiled", "bytes": len(program),
                            "scope": "boolean mechanism actions only; not the complete level script"}
                    except CompileError as error:
                        callback_support[function] = {"status": "unsupported", "reason": str(error)}
    pending = [(data / "scripts/enemies" / name) for name in active_scripts]
    pending += [data / key for key in modules]
    seen = set()
    while pending:
        path = pending.pop()
        key = path.relative_to(data).as_posix()
        if key in seen:
            continue
        seen.add(key)
        tree, info = analyse(path)
        info["source_sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
        modules[key] = info
        for module in info["requires"]:
            dependency = data.parent / (module + ".lua")
            if not dependency.resolve().is_relative_to(data):
                raise ValueError(f"require escapes source data: {module}")
            if not dependency.is_file():
                raise ValueError(f"missing required module: {module}")
            pending.append(dependency)
    source = data.parent / "src/game/level"
    api = set()
    for name in ("luanode.cpp", "levelscript.cpp"):
        api.update(re.findall(r'lua_register\([^,]+,\s*"([^"]+)"', (source / name).read_text()))
    referenced_api = sorted({call for module in modules.values() for call in module["calls"] if call in api})
    native_groups = ('collapsing_platforms', 'on_off_blocks', 'spike_blocks', 'sensor_rects',
                     'blocking_rects', 'checkpoints', 'levers', 'damage_rects')
    mechanism_counts = {group: sum(level['group_counts'].get(group, 0) for level in levels) for group in native_groups}
    summary = {"levels": len(levels), "rooms": sum(level["group_counts"].get("rooms", 0) for level in levels),
        "enemy_placements": sum(active_scripts.values()), "active_enemy_scripts": dict(sorted(active_scripts.items())),
        "reachable_lua_modules": len(modules), "referenced_host_api": referenced_api,
        "compiled_callbacks": callback_support,
        "native_mechanism_family_placements": mechanism_counts,
        "native_enemy_family_placements": {name: active_scripts[name] for name in ("rat.lua", "arrowtrap_2.lua") if name in active_scripts},
        "unported_enemy_types": sorted(set(active_scripts) - {"rat.lua", "arrowtrap_2.lua"}),
        "native_mechanism_scope": "C components; import validates supported settings; not all placements are loaded or drawn",
        "full_game_supported": False,
        "remaining": ["room/resource streaming", "remaining mechanisms", "enemy behavior and animations",
                      "complete level scripts and cutscenes", "inventory and progression", "audio", "persistent save backend"]}
    if "releaseSwordRing" in programs:
        source = ['#include "retro/logic.h"', '', 'const retro_mechanism_def source_logic_definitions[] = {']
        for obj in callback_objects:
            kind = "RETRO_MECH_BLOCK" if obj["group"] == "blocking_rects" else "RETRO_MECH_FLAG"
            source.append(f'    {{{obj["id"]}, {{0, 0, 24, 24}}, 0, 0, 0, {kind}, 1, 0}},')
        source += ['};', f'const retro_u16 source_logic_definition_count = {len(callback_objects)};',
                   'const char *const source_logic_names[] = {' + ','.join(json.dumps(obj['name']) for obj in callback_objects) + '};',
                   'const char *const source_logic_groups[] = {' + ','.join(json.dumps(obj['group']) for obj in callback_objects) + '};',
                   'const retro_u8 source_logic_program[] = {' + ','.join(map(str, programs["releaseSwordRing"])) + '};',
                   f'const retro_u16 source_logic_program_length = {len(programs["releaseSwordRing"])};', '']
        (output / "source_logic.c").write_text("\n".join(source))
    (output / "catalog.json").write_text(json.dumps({"levels": levels, "lua": modules, "summary": summary}, indent=2) + "\n")
    (output / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps(summary, indent=2))
    return levels, summary


def crop_mechanisms(data, recipe):
    """Export the implemented rectangle semantics; report every remaining object."""
    root, objects = map_objects(Path(data) / recipe["map"])
    left, top, width, height = recipe["crop_pixels"]
    supported = {"checkpoints": "RETRO_MECH_CHECKPOINT", "sensor_rects": "RETRO_MECH_SENSOR",
                 "levers": "RETRO_MECH_LEVER", "blocking_rects": "RETRO_MECH_BLOCK",
                 "collapsing_platforms": "RETRO_MECH_COLLAPSING", "on_off_blocks": "RETRO_MECH_BLOCK", "spike_blocks": "RETRO_MECH_SPIKES", "damage_rects": "RETRO_MECH_DAMAGE"}
    selected, unsupported = [], []
    for obj in objects:
        if obj["x"] >= left + width or obj["y"] >= top + height or obj["x"] + max(1, obj["width"]) <= left or obj["y"] + max(1, obj["height"]) <= top:
            continue
        reason = None
        if obj["group"] not in supported:
            reason = "runtime or visual adapter not implemented"
        elif obj["rotation"] or obj["shape"] != "rectangle":
            reason = "dynamic mechanisms currently require unrotated rectangles"
        elif obj["properties"].get("mode", "lever") not in ("lever", "interval"):
            reason = "unknown mechanism mode"
        elif obj["group"] == "collapsing_platforms" and (obj["width"] % 24 or obj["height"] != 24):
            reason = "collapsing platform requires 24px segments and a 24px collision height"
        elif obj["group"] == "collapsing_platforms" and (float(obj["properties"].get("destruction_speed", 30)) != 30 or
                                                        float(obj["properties"].get("fall_speed", 6)) != 6):
            reason = "custom collapse visual speeds are not implemented"
        if reason:
            unsupported.append({"id": obj["id"], "name": obj["name"], "group": obj["group"], "reason": reason})
            continue
        selected.append(obj)
    if len(selected) > 32:
        raise ValueError("crop exceeds the 32-mechanism runtime budget")
    targets, definitions = [], []
    selected_ids = {obj["id"] for obj in selected}
    for obj in selected:
        if not 1 <= obj["id"] <= 65535:
            raise ValueError(f'object id {obj["id"]} exceeds the supported 16-bit identity range')
        props = obj["properties"]
        names = props.get("target_ids", props.get("target_id", ""))
        first = len(targets)
        for name in str(names).split(";"):
            if not name.strip():
                continue
            matches = [target for target in objects if target["name"] == name.strip()]
            if not matches or any(target["id"] not in selected_ids for target in matches):
                raise ValueError(f'mechanism {obj["id"]}: target {name!r} is missing or outside the supported crop')
            targets.extend(target["id"] for target in matches)
        bounds = [round(obj["x"] - left), round(obj["y"] - top), round(obj["width"]), round(obj["height"])]
        boolean = lambda value: str(value).lower() in ("true", "1")
        flags = int(boolean(props.get("inverted", False))) | (0 if boolean(props.get("handle_available", True)) else 2)
        value = int(props.get("damage", 100)) if obj["group"] in ("damage_rects", "spike_blocks") else int(props.get("index", 0))
        if not 0 <= value <= 65535:
            raise ValueError(f'mechanism {obj["id"]}: value {value} exceeds the supported unsigned 16-bit range')
        animated = obj['group'] in ('levers', 'on_off_blocks', 'spike_blocks')
        interval = props.get('mode', 'lever') == 'interval'
        if interval and obj['group'] not in ('on_off_blocks', 'spike_blocks'):
            raise ValueError(f'mechanism {obj["id"]}: interval mode is invalid for this kind')
        def ticks(value, scale=1):
            result = round(float(value) * scale * 60)
            if not 0 <= result <= 65534:
                raise ValueError(f'mechanism {obj["id"]}: timer exceeds the 16-bit tick budget')
            return result
        timing = [max(1, ticks(props.get('time_on_ms', 4000), .001)),
                  max(1, ticks(props.get('time_off_ms', 3000), .001)),
                  ticks(props.get('time_to_collapse_s', 1)), ticks(props.get('time_to_respawn_s', 4)),
                  ticks(props.get('fade_in_duration_s', 1)),
                  int(animated) | (2 if interval else 0) | (4 if boolean(props.get('collapse_once_touched', False)) else 0)]
        definitions.append({"source_id": obj["id"], "name": obj["name"], "kind": supported[obj["group"]],
                            "bounds": bounds, "timing": timing, "enabled": int(boolean(props.get("enabled", True))), "flags": flags,
                            "value": value, "target_first": first, "target_count": len(targets) - first,
                            "visual_status": "static background" if obj["group"] == "checkpoints" else "not implemented"})
    return definitions, targets, unsupported


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", type=Path, default=ROOT.parent / "deceptus_engine/data")
    parser.add_argument("--output", type=Path, default=ROOT / "build/content")
    parser.add_argument("--strict", action="store_true", help="fail while the complete game has unsupported features")
    arguments = parser.parse_args()
    levels, summary = inventory(arguments.data, arguments.output)
    if arguments.strict and not summary["full_game_supported"]:
        raise SystemExit("full-game build is incomplete; see " + str(arguments.output / "summary.json"))
