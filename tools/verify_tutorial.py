"""Verify controlled-start GBA tutorial scenes; restore production outputs afterward.

These integration fixtures do not constitute an end-to-end tutorial playthrough.
Build --scene tutorial first. Fixtures start with normal game definitions at
specified scene positions; emulator memory is never patched.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import struct

import build_gba
from emulator import Emulator, ROOT


FIXTURES = [('floor', 2, 480, 336), ('exit', 2, 720, 576),
            ('dark', 4, 84, 288), ('water', 6, 448, 600),
            ('surface', 6, 420, 384), ('bed', 0, 270, 312),
            ('shrine', 7, 580, 336), ('blob',8,700,432), ('mines',6,40,360), ('gate',6,400,528),
            ('performance',6,420,384),
            ('title', -1, 0, 0)]


def state(emulator):
    memory = emulator.memory()
    marker = memory.find(b'\xce\xd3')
    if marker < 0:
        raise AssertionError('tutorial debug mailbox is not initialized')
    return struct.unpack_from('<44H', memory, marker)


def press(emulator, button):
    emulator.run(4, [button])
    emulator.run(4)


def verify(core, clang, linker, objcopy, fixture_name=None):
    folder = ROOT / 'build/gba'
    manifest = json.loads((folder / 'build.json').read_text())
    if manifest['scene'] != 'tutorial':
        raise ValueError('build --scene tutorial before running tutorial fixtures')
    source = ROOT / 'src/platform/gba/main.c'
    original_main = source.read_text()
    marker = 'deceptus_adventure_init(&adventure, &gba_tutorial_definition, &video);'
    if original_main.count(marker) != 1:
        raise ValueError('update the fixture entry hook for the current startup code')
    scratch = ROOT / 'build/tests/tutorial'
    output = ROOT / 'artifacts/tutorial-check'
    scratch.mkdir(parents=True, exist_ok=True)
    output.mkdir(parents=True, exist_ok=True)
    fixture = scratch / 'main.c'
    names = ('main.o', 'deceptus.elf', 'deceptus.gba', 'deceptus-tutorial.gba',
             'deceptus.map', 'build.json')
    backups = {folder / name: (folder / name).read_bytes() for name in names}
    original_run = build_gba.run

    def fixture_run(command, working_directory):
        # reuse the current generated data; every fixture changes only its start.
        if len(command) > 1 and str(command[1]).endswith('tutorial_assets.py'):
            return
        if '-c' in command and source in command:
            command = [fixture if item == source else item for item in command]
            command += ['-I' + str(source.parent)]
        original_run(command, working_directory)

    report = {'production_rom_sha256': hashlib.sha256(backups[folder / 'deceptus-tutorial.gba']).hexdigest(),
              'scope': 'controlled-start rendering, UI and selected connection fixtures', 'fixtures': []}
    build_gba.run = fixture_run
    try:
        for name, room, x, y in FIXTURES:
            if fixture_name and name!=fixture_name: continue
            code = original_main
            if room >= 0:
                setup = (f'\n campaign.room_index={room}; '
                         f'deceptus_init(&engine,gba_campaign_definition.rooms[{room}],&video); '
                         f'deceptus_place_player(&engine,{x},{y}); campaign.mode=DECEPTUS_CAMPAIGN_PLAYING;')
                code = code.replace(marker, marker + setup)
                code = code.replace('active_stage = &gba_stages[0];',
                                    'active_stage = &gba_stages[campaign.room_index];')
            fixture.write_text(code)
            build_gba.build(ROOT.parent / 'deceptus_engine/data', clang, linker, objcopy, scene='tutorial')
            emulator = Emulator(core, folder / 'deceptus-tutorial.gba',
                                {'mgba_use_bios': 'OFF', 'mgba_frameskip': '0'})
            try:
                emulator.run(40)
                if name == 'mines':
                    for _ in range(60):
                        emulator.run(1,[6])
                        if state(emulator)[28]==12:break
                    assert state(emulator)[28]==12,state(emulator)
                    emulator.run(16);emulator.screenshot(output/'mines-room.png')
                    for _ in range(100):
                        emulator.run(1,[7])
                        if state(emulator)[28]==6:break
                    assert state(emulator)[28]==6,state(emulator)
                elif name == 'gate':
                    emulator.run(70)
                    values=state(emulator)
                    assert values[36]&64 and values[29]==11,values
                    assert values[2]<540,values
                    ticks=values[4];emulator.run(20)
                    assert ((state(emulator)[4]-ticks)&65535)>=19,'warning froze gameplay'
                elif name == 'performance':
                    emulator.run(60)
                    missed=0;steps=240
                    for tick in range(steps):
                        before=state(emulator)
                        emulator.run(1,[6] if tick%40<10 or tick%40>=30 else [7])
                        after=state(emulator)
                        assert not after[15],after
                        assert before[28]==after[28]==6 and before[29]==after[29]==1,after
                        delta=(after[4]-before[4])&65535
                        if delta!=1:missed+=1
                    report['water_scrolling_timing']={'display_frames':steps,'non_unit_tick_steps':missed}
                    assert missed<=2,report['water_scrolling_timing']
                elif name == 'exit':
                    for _ in range(30):
                        emulator.run(1, [7])
                        if state(emulator)[28] == 11:
                            break
                    assert state(emulator)[28] == 11, state(emulator)
                    emulator.run(16)
                    emulator.screenshot(output / 'fools-ledge.png')
                    emulator.run(30, [6])
                    assert state(emulator)[28] == 2, state(emulator)
                elif name == 'dark':
                    emulator.run(20, [6])
                    assert state(emulator)[29] == 4, state(emulator)
                elif name == 'bed':
                    emulator.screenshot(output / 'room-title-and-cue.png')
                    press(emulator, 4)
                    assert state(emulator)[29] == 4, state(emulator)
                elif name == 'water':
                    values = state(emulator)
                    left, top = values[1] - values[3] - 16, values[2] - values[16] - 48
                    actor = emulator.last_image.crop((left, top, left + 32, top + 48))
                    assert any(r > 150 and g > 80 and b < 140 for r, g, b in actor.getdata()), 'underwater player hidden'
                elif name == 'surface':
                    emulator.screenshot(output / 'surface-early.png')
                    emulator.run(18)
                    press(emulator, 3)
                    emulator.run(4)
                    values = state(emulator)
                    area = ROOT / 'assets/generated/gba/area6'
                    clip = json.loads((area / 'animations.json').read_text())[0]
                    cells = (area / 'animation-cells.bin').read_bytes()
                    patterns = (area / 'stream-patterns.bin').read_bytes()
                    vram = C.string_at(emulator.core.retro_get_memory_data(3), 98304)
                    bx, by, width, height = [n // 8 for n in clip['bounds']]
                    checked = 0
                    for row in range(height):
                        for column in range(width):
                            wx, wy = bx + column, by + row
                            if not (values[3] // 8 <= wx < values[3] // 8 + 30 and values[16] // 8 <= wy < values[16] // 8 + 20):
                                continue
                            offset = clip['cell_offset'] + ((values[34] * height + row) * width + column) * 4
                            pattern = struct.unpack_from('<H', cells, offset)[0]
                            address = ((wx & 63) // 32) * 1024 + (wy & 31) * 32 + (wx & 31)
                            slot = struct.unpack_from('<H', vram, 0xe000 + address * 2)[0] & 1023
                            assert vram[slot*32:(slot+1)*32] == patterns[pattern*32:(pattern+1)*32], 'surface tile mismatch'
                            checked += 1
                    assert checked, 'surface fixture did not show the animated region'
                elif name in ('shrine','foam'):
                    assert not state(emulator)[15],state(emulator)
                    observed=set()
                    for _ in range(90):
                        emulator.run(1)
                        if name=='shrine':
                            observed.add(state(emulator)[34])
                        else:
                            vram=C.string_at(emulator.core.retro_get_memory_data(3),98304)
                            observed.add(hashlib.sha256(vram[0xf000:0x10000]).hexdigest())
                    assert len(observed)>5, (name,'source tile animation is not advancing',state(emulator))
                    report[name+'_distinct_states']=len(observed)
                elif name == 'blob':
                    for _ in range(30):emulator.run(1)
                    assert state(emulator)[11], 'original blob placement is missing'
                elif name == 'title':
                    frames = []
                    for _ in range(60):
                        emulator.run(1)
                        frames.append(emulator.last_image.tobytes())
                    changes = sum(a != b for a, b in zip(frames, frames[1:]))
                    assert changes > 50, changes
                    report['rotation_changed_frames'] = changes
                    emulator.screenshot(output / 'title.png')
                    press(emulator, 5); press(emulator, 3)
                    assert state(emulator)[29] == 9
                    press(emulator, 3)
                    assert state(emulator)[29] == 7
                    press(emulator, 0); press(emulator, 5); press(emulator, 3)
                    assert state(emulator)[29] == 0
                    press(emulator, 3); press(emulator, 3)
                    assert state(emulator)[29] == 5
                emulator.screenshot(output / ('pause.png' if name == 'title' else name + '.png'))
                values = state(emulator)
                assert not values[15], values
                report['fixtures'].append({'name': name, 'room': values[28], 'mode': values[29], 'content_error': values[15]})
            finally:
                emulator.close()
    finally:
        build_gba.run = original_run
        for path, data in backups.items():
            path.write_bytes(data)
    report['production_outputs_restored'] = all(path.read_bytes() == data for path, data in backups.items())
    assert report['production_outputs_restored']
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Tutorial scene checks passed; production outputs restored.')
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core', type=Path, default=ROOT / '.tools/mgba/mgba_libretro.dll')
    parser.add_argument('--clang', default='clang')
    parser.add_argument('--linker', default='ld.lld')
    parser.add_argument('--objcopy', default='llvm-objcopy')
    parser.add_argument('--fixture',choices=[entry[0] for entry in FIXTURES])
    args = parser.parse_args()
    verify(args.core, args.clang, args.linker, args.objcopy,args.fixture)
