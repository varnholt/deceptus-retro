from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from lua_source import ast, BooleanCompiler, CompileError
from content import map_objects, crop_mechanisms

FIXTURES = Path(__file__).resolve().parent / "fixtures"


class LuaCompilerTests(unittest.TestCase):
    objects = [{"id": 10, "name": "first", "group": "blocking_rects"},
               {"id": 20, "name": "second", "group": "blocking_rects"}]

    def compile(self, text, name="activate"):
        return BooleanCompiler(ast.parse(text), self.objects).compile(name)

    def test_constant_loop_and_helper_are_compiled_from_source(self):
        program = self.compile('''
            local ids = {"first", "second"}
            function helper(value)
                for _, object in ipairs(ids) do setMechanismEnabled(object, value, "blocking_rects") end
            end
            function activate(enabled) helper(not enabled) end
        ''')
        self.assertEqual(program, bytes([3, 0, 4, 7, 10, 0, 3, 0, 4, 7, 20, 0, 0]))

    def test_boolean_branch_has_valid_forward_targets(self):
        program = self.compile('''
            function activate(enabled)
                if enabled then setMechanismEnabled("first", true)
                else setMechanismEnabled("first", false) end
            end
        ''')
        self.assertEqual(program, bytes([3, 0, 10, 12, 0, 2, 7, 10, 0, 11, 16, 0, 1, 7, 10, 0, 0]))

    def test_boolean_operators_visibility_and_toggle(self):
        program = self.compile('''
            function activate(first, second)
                setMechanismEnabled("second", first and not second)
                setMechanismVisible("second", first or second)
                toggle("first")
            end
        ''')
        self.assertEqual(program, bytes([3, 0, 3, 1, 4, 5, 7, 20, 0, 3, 0, 3, 1, 6, 8, 20, 0, 9, 10, 0, 0]))

    def test_missing_target_fails_instead_of_dropping_the_call(self):
        with self.assertRaisesRegex(CompileError, "unresolved"):
            self.compile('function activate(enabled) setMechanismEnabled("missing", enabled) end')

    def test_unsupported_gameplay_is_not_silently_ignored(self):
        with self.assertRaisesRegex(CompileError, "unsupported call"):
            self.compile('function activate(enabled) applyLinearImpulse(1, 2) end')
        with self.assertRaisesRegex(CompileError, "unsupported callback statement"):
            self.compile('function activate(enabled) while enabled do enabled = false end end')

    def test_dynamic_array_cannot_be_frozen_at_build_time(self):
        with self.assertRaisesRegex(CompileError, "modified at runtime"):
            self.compile('''
                ids = {"first"}
                function change() ids[1] = "second" end
                function activate(enabled)
                    for _, object in ipairs(ids) do setMechanismEnabled(object, enabled) end
                end
            ''')

    def test_recursion_has_a_clear_error(self):
        with self.assertRaisesRegex(CompileError, "recursive"):
            self.compile('function activate(enabled) activate(enabled) end')

    def test_global_runtime_state_is_not_frozen_into_a_boolean(self):
        with self.assertRaisesRegex(CompileError, "modified at runtime"):
            self.compile('''
                active = true
                function change() active = false end
                function activate(enabled) setMechanismEnabled("first", active) end
            ''')


class ContentTests(unittest.TestCase):
    def test_legacy_string_booleans_and_spikes(self):
        obj = dict(id=1, name='trap', group='spike_blocks', x=8, y=8, width=24, height=24,
                   rotation=0, shape='rectangle', properties={'enabled': 'false', 'inverted': 'false'})
        with patch('content.map_objects', return_value=(None, [obj])):
            definitions, targets, unsupported = crop_mechanisms(FIXTURES, {'map': 'unused', 'crop_pixels': [0, 0, 256, 224]})
        self.assertEqual(definitions[0]['kind'], 'RETRO_MECH_SPIKES')
        self.assertEqual(definitions[0]['enabled'], 0)
        self.assertEqual(definitions[0]['flags'], 0)
        self.assertEqual(definitions[0]['value'], 100)
        self.assertEqual((targets, unsupported), ([], []))

    def test_typed_properties_and_group_offsets_survive_import(self):
        _, objects = map_objects(FIXTURES / "typed.tmx")
        self.assertEqual((objects[0]["x"], objects[0]["y"]), (40, 40))
        self.assertIs(objects[0]["properties"]["enabled"], False)
        self.assertEqual(objects[0]["properties"]["index"], 3)

    def test_remote_lever_target_fails_explicitly(self):
        with self.assertRaisesRegex(ValueError, "outside the supported crop"):
            crop_mechanisms(FIXTURES, {"map": "remote.tmx", "crop_pixels": [0, 0, 512, 224]})


if __name__ == "__main__":
    unittest.main()
