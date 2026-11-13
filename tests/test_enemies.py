from pathlib import Path
import json
import sys
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from enemies import constants, crop_enemies
ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT.parent / 'deceptus_engine/data'

class EnemyImportTests(unittest.TestCase):
    def test_rat_settings_are_read_from_the_original_lua(self):
        if not DATA.exists(): self.skipTest('original checkout is required')
        values = constants(DATA / 'scripts/enemies/rat.lua')
        self.assertEqual(values['properties']['damage'], 0)
        self.assertEqual(values['properties']['sprite'], 'data/sprites/rat.png')
        self.assertEqual(values['CYCLE_LENGTHS'], [7, 7, 5, 7, 3])
        result = crop_enemies(DATA, json.loads((ROOT / 'assets/rooms/gba-cell.json').read_text()))
        definition = result['definitions'][0]
        self.assertEqual((definition['source_id'], definition['kind']), (111, 'RETRO_RAT'))
        self.assertEqual((definition['speed'], definition['acceleration']), (10, 1))
        self.assertEqual(result['paths'], [[374, 302], [504, 288]])

    def test_arrow_timing_comes_from_source_initialization(self):
        if not DATA.exists(): self.skipTest('original checkout is required')
        result = crop_enemies(DATA, json.loads((ROOT / 'assets/rooms/gba-preview.json').read_text()))
        definition = result['definitions'][0]
        self.assertEqual(definition['source_id'], 1059)
        self.assertEqual(definition['delays'], [60, 75, 90])
        self.assertEqual((definition['period'], definition['speed'], definition['damage']), (180, 96, 60))
        self.assertTrue(any(item['id'] == 1049 and 'leaves loaded room' in item['reason'] for item in result['unsupported']))
        self.assertTrue(any(item['script'] == 'blob_2.lua' for item in result['unsupported']))

    def test_unsupported_enemies_are_reported(self):
        obj = dict(id=20, group='enemies', x=10, y=10, rotation=0, properties={'script': 'unported.lua'})
        with patch('enemies.map_objects', return_value=(None, [obj])):
            result = crop_enemies(DATA, dict(map='unused', crop_pixels=[0,0,256,224]))
        self.assertFalse(result['definitions'])
        self.assertEqual(result['unsupported'][0]['script'], 'unported.lua')

if __name__ == '__main__': unittest.main()
