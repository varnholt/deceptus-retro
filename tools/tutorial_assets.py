"""Build the prison tutorial from source rooms, dialogue, items and native bindings."""
import argparse
import json
from pathlib import Path
import re
import textwrap

from assets import ROOT
from content import map_objects
from campaign_assets import compile_campaign
from tutorial_ui import TutorialUI
from menu_assets import compile_menu


def compile_tutorial(data):
    _, objects = map_objects(data / 'level-catacombs/catacombs.tmx')
    by_id = {item['id']: item for item in objects}
    base = json.loads((ROOT / 'assets/rooms/gba-cell.json').read_text())
    physics = json.loads((data / 'config/physics.json').read_text())['PhysicsConfiguration']
    # native fixed-step tuning: approximately 78px held jump, with a shorter tap jump.
    base['player_gravity'], base['player_gravity_fraction'] = 3,0
    base['player_fall_gravity'] = 4
    base['import_water'] = True
    base['water_physics'] = [round(physics['player_speed_max_water']*48/60*16),round(-physics['player_in_water_linear_velocity_y_clamp_min']*48/60*16),round(physics['player_in_water_linear_velocity_y_clamp_max']*48/60*16),round(physics['player_in_water_time_to_allow_jump_button_ms']*60/1000),2,4]
    base['player_speed'] = round(physics['player_speed_max_run'] * 48/60*16)
    ids = [311,105,312,582,514,324,9,3,4,18,6,7]
    names = ['THE CELL','LOWER CELLS','CROSSROADS','UPPER CELLS','THE CELLAR',"GUARDS' OFFICE",'MAIN HALL','ALTAR OF THE BLIND OWL','CLOCKWORK ANTECHAMBER','THE LOOP','THE SEWERS',"FOOL'S LEDGE"]
    spawns = [[370,312],[100,336],[100,576],[720,288],[680,288],[100,288],[408,32],[128,408],[69,384],[46,264],[47,624],[46,360]]
    stages = []
    for index, source_id in enumerate(ids):
        room = by_id[source_id]
        recipe = dict(base)
        recipe.update(crop_pixels=[int(room[key]) for key in ('x','y','width','height')], spawn=spawns[index],
                      title=names[index],skills=1)
        if index == 1:
            recipe['scenery_variants'] = [dict(name='monk',actors=[573],forbidden_flags=8)]
        if index == 2:
            recipe['native_links'] = {'lever_spike_01': ';'.join(f'ct-crossroads-block{i}' for i in (9,10,11,12,13,14,1,2,3,4))}
        if index == 4:
            recipe['scenery_variants'] = [dict(name='locker_key',extras=['locker_key'],forbidden_flags=2)]
        if index == 5:
            recipe['scenery_variants'] = [dict(name='open_locker_with_handle',image_layers=['locker_open'],extras=['handle'],required_flags=1,forbidden_flags=4),
                                          dict(name='open_locker_empty',image_layers=['locker_open'],required_flags=5)]
        if index in (6,10):
            water_recipe = json.loads((ROOT / 'assets/rooms/gba-water.json').read_text())
            recipe['visual_layers'] = water_recipe['visual_layers']
            recipe['bake_water'] = True
        # these source layers carry the full artwork below the one-way floor lip.
        for name in ('level_solid_onesided','level_soild_onesided_dec','decoration-f2'):
            if name not in recipe['visual_layers']:
                recipe['visual_layers'] = list(recipe['visual_layers']) + [name]
        if index in (6,10):
            recipe['tile_animations']=[dict(name='water_foam',layers=['water-foam'],plane=1)]
        if index == 7:
            recipe['tile_animations']=[dict(name='shrine_column',layers=['bg4'],source_bounds=[3000,2952,24,96])]
        if index == 8:
            recipe['skills'] = 1
        import xml.etree.ElementTree as ET
        layers = ET.parse(data / recipe['map']).getroot().iter('layer')
        recipe['foreground_layers'] = [layer.get('name') for layer in layers if layer.get('name') in recipe['visual_layers']
            and layer.get('name') not in ('water-base-1',)
            and any(field.get('name')=='z' and int(field.get('value'))>20 for field in layer.findall('properties/property'))]
        stages.append(dict(room_recipe=recipe,title=names[index],instruction='',hint1='',hint2='',goal=[0,0,1,1]))
    compile_campaign(data,dict(stages=stages),scene='tutorial')
    output = ROOT / 'assets/generated/gba'
    header = (output/'gba_assets.h').read_text().replace('#endif', '#include "deceptus/adventure.h"\nextern const deceptus_adventure_def gba_tutorial_definition;\n#endif')
    (output/'gba_assets.h').write_text(header)
    lines = ['#include "gba_assets.h"']
    ui=TutorialUI(data)
    # data bindings reference source identities; no artwork or dialogue is maintained here.
    interactions = [
        [dict(id=645)],
        [dict(id=574, bounds_id=573, set_flags=16, forbidden_flags=16,automatic=1),
         dict(id=877,bounds_id=581,cost_item=2,set_flags=8,forbidden_flags=8,effect=1,mechanism_id=581, text=['The lever handle is fitted.']),
         dict(id=877,bounds_id=581,forbidden_flags=8),dict(id=581,required_flags=8)],
        [dict(id=909)],
        [dict(id=644),dict(id=643)],
        [dict(id=1719,bounds_id=1385,grant_item=1,set_flags=2,forbidden_flags=2),dict(id=885),dict(id=813,automatic=1,set_flags=32,forbidden_flags=32)],
        [dict(id=18258,bounds_id=580,grant_item=2,set_flags=4,required_flags=1,forbidden_flags=4),
         dict(id=1433,bounds_id=1432,cost_item=1,set_flags=1,forbidden_flags=1),
         dict(id=1357,bounds_id=1432,forbidden_flags=1),dict(id=1433,bounds_id=1432,required_flags=1)],
        [dict(id=1569,bounds_id=1569)],[],[],[dict(id=113)],[],[]
    ]
    report = []
    for room_index, actions in enumerate(interactions):
        serialized = []
        origin = stages[room_index]['room_recipe']['crop_pixels'][:2]
        for index, action in enumerate(actions):
            obj = by_id[action['id']]
            bounds_obj = by_id[action.get('bounds_id',action['id'])]
            texts = action.get('text') or [value for key,value in sorted(obj['properties'].items()) if re.fullmatch(r'\d\d',key)]
            pages = []
            for text in texts:
                text = re.sub(r'\[/?color[^\]]*\]', '', text).replace('<br>', ' ').replace('<player>','prisoner')
                pages += ui.wrap_pages(' '.join(text.split()))
            label = f'tutorial_pages_{room_index}_{index}'
            first_page=len(ui.resources)
            for page_index,page in enumerate(pages):
                ui.bubble(page,obj['properties'].get('01_background_color','#ff101ad4'),page_index+1<len(pages))
            if pages:
                lines.append('static const char *const ' + label + '[] = {' + ','.join(json.dumps(text,ensure_ascii=True) for text in pages) + '};')
            else: label='0'
            nearby=[item for item in objects if item['group']=='interaction_help' and
                    item['x'] < bounds_obj['x']+max(1,bounds_obj['width']) and item['x']+max(1,item['width'])>bounds_obj['x'] and
                    item['y'] < bounds_obj['y']+max(1,bounds_obj['height']) and item['y']+max(1,item['height'])>bounds_obj['y']]
            help_obj=nearby[0] if nearby else None
            help_props=help_obj['properties'] if help_obj else {}
            cue_label='Take' if action.get('grant_item') else 'Use' if action.get('cost_item') else 'Operate' if obj['group']=='levers' else help_props.get('text_0',help_props.get('text','Talk' if obj['id'] in (643,644) else 'Examine'))
            cue_animation='use' if action.get('cost_item') or obj['group']=='levers' else help_props.get('animation','examine')
            cue=ui.cue(cue_label,cue_animation)
            anchor=[round(help_obj['x']+help_props.get('offset_x_px',0)-origin[0]),round(help_obj['y']+help_props.get('offset_y_px',0)-origin[1])] if help_obj else [round(bounds_obj['x']+bounds_obj['width']/2-origin[0]),round(bounds_obj['y']-16-origin[1])]
            ui.presentation(f'tutorial_actions_{room_index}[{index}]',first_page,cue,anchor)
            bounds = [round(bounds_obj['x']-origin[0]),round(bounds_obj['y']-origin[1]),round(bounds_obj['width']),round(bounds_obj['height'])]
            # generous interaction reach around the authored object, without changing artwork.
            bounds[0] -= 12; bounds[1] -= 8; bounds[2] += 24; bounds[3] += 16
            serialized.append('    {'+str(obj['id'])+', {'+','.join(map(str,bounds))+'}, '+label+', '+','.join(str(action.get(key,0)) for key in ('required_flags','forbidden_flags','set_flags','mechanism_id'))+', '+str(len(pages))+', '+','.join(str(action.get(key,0)) for key in ('cost_item','grant_item','effect','automatic'))+'},')
            report.append(dict(room=room_index,source_id=obj['id'],bounds_source=bounds_obj['id'],pages=len(pages)))
        if serialized:
            lines += [f'static const deceptus_interaction tutorial_actions_{room_index}[] = {{',*serialized,'};']
    # portals are automatic at the real shared room edges. intervals are world coordinates.
    links = [(0,1,2,2592,2688),(1,0,1,2592,2688),(1,2,2,2496,2616),(2,1,1,2496,2616),
             (2,3,1,2136,2256),(3,2,2,2136,2256),(3,4,1,2136,2256),(4,3,2,2136,2256),
             (2,5,2,2064,2256),(5,2,1,2064,2256),(1,6,3,2016,2088),
             (6,7,2,3048,3168),(7,6,1,3048,3168),(7,8,2,3048,3168),(8,7,1,3048,3168),
             (8,9,2,2880,3000),(9,8,1,2880,3000),(9,10,2,2880,3000),(10,9,1,2880,3000),(2,11,2,2496,2616),(11,2,1,2496,2616)]
    arrival_ids = [528,527,521,522,515,516,588,587,517,518,None,525,526,531,532,533,534,535,536,520,519]
    counts = []
    for index, source_id in enumerate(ids):
        origin = stages[index]['room_recipe']['crop_pixels']
        portals = []
        for link_index,(source,destination,direction,minimum,maximum) in enumerate(links):
            if source != index: continue
            target = stages[destination]['room_recipe']['crop_pixels']
            if direction == 3:
                bounds = [minimum-origin[0],origin[3]-8,maximum-minimum,24]
            else:
                reverse_index = next(i for i, link in enumerate(links) if link[0] == destination and link[1] == source)
                entry = by_id[arrival_ids[reverse_index]]
                bounds = [round(entry['x']-origin[0]),minimum-origin[1],round(entry['width']),maximum-minimum]
            arrival = by_id[arrival_ids[link_index]]['properties'] if arrival_ids[link_index] else None
            spawn = [int(arrival['start_position_x_px'])-target[0], (int(arrival['start_position_y_px'])+9)//8*8-target[1],1] if arrival else [0,0,0]
            portals.append('    {{'+','.join(map(str,bounds))+'}, '+','.join(map(str,[origin[0]-target[0],origin[1]-target[1],destination,direction]+spawn))+'},')
        if portals:
            lines += [f'static const deceptus_portal tutorial_portals_{index}[] = {{',*portals,'};']
        counts.append(len(portals))
    lines += ['static const deceptus_adventure_room tutorial_rooms[] = {']
    for index,actions in enumerate(interactions):
        lines.append('    {'+(f'tutorial_portals_{index}' if counts[index] else '0')+', '+(f'tutorial_actions_{index}' if actions else '0')+f', {counts[index]}, {len(actions)}'+'},')
    lines += ['};','static const char *const tutorial_items[] = {"", "Locker Key", "Iron Lever"};',
              'const deceptus_adventure_def gba_tutorial_definition = {&gba_campaign_definition,tutorial_rooms,tutorial_items,2};']
    with (output/'room.c').open('a') as handle:
        handle.write('\n'.join(lines)+'\n'+ui.finish(output))
    compile_menu(data,output)
    (output/'tutorial.json').write_text(json.dumps(dict(source_rooms=ids,interactions=report,portals=links,
        limitations=['crossroads lever toggles groups immediately; camera cutscene not ported',
                     'monk uses the source idle pose; appear/kneel cutscene not ported',
                     'underwater swimming, unported enemy families and later game scripts remain unsupported']),indent=2)+'\n')


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data',type=Path,default=ROOT.parent/'deceptus_engine/data')
    args=parser.parse_args()
    compile_tutorial(args.data)
