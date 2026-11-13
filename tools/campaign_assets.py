"""Build the curated, four-area GBA preview from authoritative source content."""
import argparse
import json
from pathlib import Path
import re
import struct
from PIL import Image, ImageDraw, ImageFont
import gba_assets
from assets import ROOT


def font_tiles(data, output):
    font = ImageFont.truetype(str(data / 'fonts/3x5px.ttf'), 5)
    tiles = bytearray(32)
    for code in range(32, 128):
        image = Image.new('L', (8, 8))
        ImageDraw.Draw(image).text((2, 1), chr(code), font=font, fill=255)
        pixels = [1 if value >= 128 else 3 for value in image.getdata()]
        tiles.extend(bytes(pixels[index] | (pixels[index + 1] << 4) for index in range(0, 64, 2)))
    (output / 'font.bin').write_bytes(tiles)


def compile_campaign(data):
    config = json.loads((ROOT / 'assets/campaigns/tonight.json').read_text())
    output = ROOT / 'assets/generated/gba'
    output.mkdir(parents=True, exist_ok=True)
    common_source = ROOT / 'assets/source/campaign'
    assemblies, room_sources, stages, goals, reports = [], [], [], [], []
    header = None
    for index, stage in enumerate(config['stages']):
        folder = output / f'area{index}'
        folder.mkdir(parents=True, exist_ok=True)
        recipe = json.loads((ROOT / 'assets/rooms' / stage['recipe']).read_text())
        recipe.update(stream_background=True, max_health=100)
        recipe_path = folder / 'recipe.json'
        recipe_path.write_text(json.dumps(recipe, indent=2) + '\n')
        gba_assets.OUTPUT = folder
        gba_assets.SOURCE = common_source / f'area{index}'
        gba_assets.compile_assets(data, stage['recipe'], recipe_path)
        report = json.loads((folder / 'budget.json').read_text())
        reports.append(report)
        prefix = f'gba_area{index}'
        for suffix, filename in [('palette', 'background.pal'), ('patterns', 'stream-patterns.bin'), ('cells', 'stream-cells.bin')]:
            label = prefix + '_' + suffix
            assemblies += ['.balign 4', f'.global {label}', f'{label}:', f'.incbin "{(folder / filename).as_posix()}"']
        source = (folder / 'room.c').read_text()
        source = re.sub(r'\broom_', f'area{index}_room_', source)
        source = source.replace('deceptus_room', f'deceptus_area{index}')
        room_sources.append(source)
        stages.append(f'    {{&deceptus_area{index}, {prefix}_palette, {prefix}_patterns, {prefix}_cells, {report["background_patterns"]}, {json.dumps(stage["title"])}, {json.dumps(stage["instruction"])}, {json.dumps(stage["hint1"])}, {json.dumps(stage["hint2"])}}},')
        goals.append('    {{' + ','.join(map(str, stage['goal'])) + '}, ' + str(stage.get('disabled_mechanism', 0)) + '},')
        if index == 0:
            header = (folder / 'gba_assets.h').read_text()
            common_assembly = (folder / 'data.s').read_text()
            # room-specific background resources have their own labels above.
            common_assembly = common_assembly[:common_assembly.index('.balign 4\n.global gba_stream_patterns')]
    maximum = max(report['background_patterns'] for report in reports)
    header = header.replace('#define GBA_CAMPAIGN 0', '#define GBA_CAMPAIGN 1')
    header = re.sub(r'#define GBA_PATTERN_COUNT \d+', f'#define GBA_PATTERN_COUNT {maximum}', header)
    header = re.sub(r'#define GBA_ROOM_COLUMNS \d+', '#define GBA_ROOM_COLUMNS (active_stage->room->columns)', header)
    header = re.sub(r'#define GBA_ROOM_ROWS \d+', '#define GBA_ROOM_ROWS (active_stage->room->rows)', header)
    header = header.replace('extern const retro_u8 gba_stream_patterns[];', '').replace('extern const retro_u8 gba_stream_cells[];', '')
    additions = ['#include "retro/campaign.h"',
        'typedef struct gba_stage { const retro_room *room; const retro_u8 *palette, *patterns, *cells; retro_u16 pattern_count; const char *title, *instruction, *hint1, *hint2; } gba_stage;',
        f'extern const gba_stage gba_stages[{len(stages)}];', 'extern const retro_campaign_def gba_campaign_definition;',
        'extern const retro_u8 gba_font[];', '#define GBA_FONT_BYTES 3104',
        '#define gba_stream_patterns (active_stage->patterns)', '#define gba_stream_cells (active_stage->cells)']
    header = header.replace('#endif', '\n'.join(additions) + '\n#endif')
    (output / 'gba_assets.h').write_text(header)
    font_tiles(data, output)
    assemblies += ['.balign 4', '.global gba_font', 'gba_font:', f'.incbin "{(output / "font.bin").as_posix()}"']
    (output / 'data.s').write_text(common_assembly + '\n'.join(assemblies) + '\n')
    declarations = ['#include "gba_assets.h"']
    for index in range(len(stages)):
        declarations += [f'extern const retro_u8 gba_area{index}_{suffix}[];' for suffix in ('palette', 'patterns', 'cells')]
    declarations += ['const gba_stage gba_stages[] = {', *stages, '};',
                     'static const retro_room *const campaign_rooms[] = {' + ','.join(f'&deceptus_area{index}' for index in range(len(stages))) + '};',
                     'static const retro_campaign_goal campaign_goals[] = {', *goals, '};',
                     f'const retro_campaign_def gba_campaign_definition = {{campaign_rooms, campaign_goals, {len(stages)}}};']
    (output / 'room.c').write_text('\n'.join(room_sources + declarations) + '\n')
    (output / 'budget.json').write_text(json.dumps(dict(scene='tonight', stages=reports, full_game=False), indent=2) + '\n')
    print('Built four-area playable preview assets')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, default=ROOT.parent / 'deceptus_engine/data')
    args = parser.parse_args()
    compile_campaign(args.data)
