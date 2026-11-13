"""Parse upstream Lua without executing it; compile a checked boolean-action subset."""

from __future__ import annotations

from pathlib import Path
import re
import sys

try:
    from luaparser import ast, astnodes as nodes
except ModuleNotFoundError:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / ".tools/python"))
    from luaparser import ast, astnodes as nodes


class CompileError(ValueError):
    pass


def name_of(node):
    if isinstance(node, nodes.Name):
        return node.id
    if isinstance(node, nodes.Index):
        return name_of(node.value) + "." + name_of(node.idx)
    if isinstance(node, nodes.String):
        return node.s.decode("utf-8") if isinstance(node.s, bytes) else node.s
    return "<dynamic>"


def literal(node):
    if isinstance(node, nodes.String):
        return name_of(node)
    if isinstance(node, nodes.Number):
        return node.n
    if isinstance(node, nodes.TrueExpr):
        return True
    if isinstance(node, nodes.FalseExpr):
        return False
    if isinstance(node, nodes.Nil):
        return None
    if isinstance(node, nodes.Table):
        # only constant, sequential arrays are accepted for loop unrolling.
        result = []
        for index, field in enumerate(node.fields, start=1):
            if field.key is not None and literal(field.key) != index:
                raise CompileError("only sequential constant arrays can be unrolled")
            result.append(literal(field.value))
        return result
    raise CompileError(f"expected a constant, got {type(node).__name__}")


def analyse(path):
    source = Path(path).read_text(encoding="utf-8-sig")
    tree = ast.parse(source)
    calls, requires, functions, features = set(), set(), set(), set()
    for node in ast.walk(tree):
        features.add(type(node).__name__)
        if isinstance(node, (nodes.Function, nodes.LocalFunction)):
            functions.add(name_of(node.name))
        if isinstance(node, nodes.Call):
            function = name_of(node.func)
            calls.add(function)
            if function == "require" and node.args:
                requires.add(literal(node.args[0]))
    return tree, {"functions": sorted(functions), "calls": sorted(calls),
                  "requires": sorted(requires), "syntax_features": sorted(features)}


# the bytecode format is deliberately small and shared by all console backends.
END, FALSE, TRUE, ARG, NOT, AND, OR, SET_ENABLED, SET_VISIBLE, TOGGLE, JUMP_FALSE, JUMP, READ_ENABLED = range(13)


class BooleanCompiler:
    """Compile named callbacks and their helper functions, rejecting all other semantics.

    Supported: boolean arguments/literals, and/or/not, if/else, constant arrays
    with ipairs, same-file helper calls, and resolved mechanism API calls.
    This is not a general Lua translator and does not execute initializers.
    """
    def __init__(self, tree, objects):
        self.objects = objects
        self.functions = {}
        self.constants = {}
        self.mutable_constants = set()
        self.code = []
        self.call_stack = []
        self.resolved_targets = set()
        for node in tree.body.body:
            if isinstance(node, (nodes.Function, nodes.LocalFunction)) and isinstance(node.name, nodes.Name):
                self.functions[node.name.id] = node
            elif isinstance(node, (nodes.Assign, nodes.LocalAssign)):
                for target, value in zip(node.targets, node.values):
                    if isinstance(target, nodes.Name):
                        try:
                            self.constants[target.id] = literal(value)
                        except CompileError:
                            pass
        for function in self.functions.values():
            for statement in ast.walk(function.body):
                if isinstance(statement, nodes.Assign):
                    for target in statement.targets:
                        if isinstance(target, nodes.Name):
                            self.mutable_constants.add(target.id)
                        elif isinstance(target, nodes.Index):
                            self.mutable_constants.add(name_of(target.value).split(".")[0])
                elif isinstance(statement, nodes.Call):
                    function_name = name_of(statement.func)
                    if function_name in ("table.insert", "table.remove", "table.sort", "table.move") and statement.args:
                        self.mutable_constants.add(name_of(statement.args[0]))

    def emit(self, *values):
        self.code.extend(values)
        if len(self.code) > 2048:
            raise CompileError("callback exceeds 2048-byte code budget")

    def word(self, value):
        if not 0 <= value <= 65535:
            raise CompileError("operand exceeds 16-bit budget")
        return value & 255, value >> 8

    def constant(self, node, bindings):
        if isinstance(node, nodes.Name):
            if node.id not in bindings and node.id in self.mutable_constants:
                raise CompileError(f"{node.id} is modified at runtime and cannot be a compile-time constant")
            value = bindings.get(node.id, self.constants.get(node.id, ("unknown", node.id)))
            if isinstance(value, tuple):
                raise CompileError(f"{node.id} is not a compile-time constant")
            return value
        return literal(node)

    def boolean(self, node, bindings):
        if isinstance(node, nodes.Name):
            if node.id not in bindings and node.id in self.mutable_constants:
                raise CompileError(f"{node.id} is modified at runtime and cannot be a compile-time boolean")
            value = bindings.get(node.id, self.constants.get(node.id))
            if isinstance(value, tuple) and value[0] == "arg":
                self.emit(ARG, value[1])
            elif isinstance(value, tuple) and value[0] == "expr":
                self.boolean(value[1], value[2])
            elif isinstance(value, bool):
                self.emit(TRUE if value else FALSE)
            else:
                raise CompileError(f"unknown boolean {node.id}")
        elif isinstance(node, (nodes.TrueExpr, nodes.FalseExpr)):
            self.emit(TRUE if isinstance(node, nodes.TrueExpr) else FALSE)
        elif isinstance(node, nodes.ULNotOp):
            self.boolean(node.operand, bindings)
            self.emit(NOT)
        elif isinstance(node, (nodes.AndLoOp, nodes.OrLoOp)):
            # boolean-only operands have no side effects, so eager evaluation is safe.
            self.boolean(node.left, bindings)
            self.boolean(node.right, bindings)
            self.emit(AND if isinstance(node, nodes.AndLoOp) else OR)
        else:
            raise CompileError(f"unsupported boolean expression: {type(node).__name__}")

    def resolve(self, pattern, group=None):
        if not isinstance(pattern, str) or (group is not None and not isinstance(group, str)):
            raise CompileError("mechanism selectors must be constant strings")
        try:
            regex = re.compile(pattern)
        except re.error as error:
            raise CompileError(f"invalid mechanism selector {pattern!r}: {error}") from error
        found = [obj["id"] for obj in self.objects
                 if regex.fullmatch(obj["name"]) and (group is None or obj["group"] == group)]
        if not found:
            raise CompileError(f"unresolved mechanism selector {pattern!r} in {group!r}")
        self.resolved_targets.update(found)
        return sorted(found)

    def call(self, name, arguments, bindings):
        if name in ("setMechanismEnabled", "setMechanismVisible", "toggle"):
            expected = (1, 2) if name == "toggle" else (2, 3)
            if len(arguments) not in expected:
                raise CompileError(f"invalid argument count for {name}")
            pattern = self.constant(arguments[0], bindings)
            group_index = 1 if name == "toggle" else 2
            group = self.constant(arguments[group_index], bindings) if len(arguments) > group_index else None
            for object_id in self.resolve(pattern, group):
                if name != "toggle":
                    self.boolean(arguments[1], bindings)
                opcode = TOGGLE if name == "toggle" else SET_ENABLED if name == "setMechanismEnabled" else SET_VISIBLE
                self.emit(opcode, *self.word(object_id))
            return
        if name not in self.functions:
            raise CompileError(f"unsupported call {name}")
        function = self.functions[name]
        if len(arguments) != len(function.args):
            raise CompileError(f"invalid argument count for {name}")
        helper_bindings = {}
        for parameter, argument in zip(function.args, arguments):
            helper_bindings[parameter.id] = ("expr", argument, dict(bindings))
        self.function(name, helper_bindings)

    def function(self, name, bindings):
        if name in self.call_stack or len(self.call_stack) >= 8:
            raise CompileError("recursive or too deeply nested callback")
        self.call_stack.append(name)
        try:
            self.block(self.functions[name].body, bindings)
        finally:
            self.call_stack.pop()

    def block(self, block, bindings):
        bindings = dict(bindings)
        for statement in block.body:
            if isinstance(statement, nodes.Call):
                self.call(name_of(statement.func), statement.args, bindings)
            elif isinstance(statement, nodes.Forin):
                if len(statement.iter) != 1 or len(statement.targets) != 2:
                    raise CompileError("only constant ipairs loops are supported")
                iterator = statement.iter[0]
                if not isinstance(iterator, nodes.Call) or name_of(iterator.func) != "ipairs" or len(iterator.args) != 1:
                    raise CompileError("only constant ipairs loops are supported")
                values = self.constant(iterator.args[0], bindings)
                if not isinstance(values, list) or len(values) > 64:
                    raise CompileError("loop needs a constant array of at most 64 values")
                for index, value in enumerate(values, start=1):
                    nested = dict(bindings)
                    nested[statement.targets[0].id] = index
                    nested[statement.targets[1].id] = value
                    self.block(statement.body, nested)
            elif isinstance(statement, nodes.If):
                self.conditional(statement, bindings)
            elif isinstance(statement, nodes.LocalAssign):
                if len(statement.targets) != len(statement.values):
                    raise CompileError("local assignment needs one value per target")
                for target, value in zip(statement.targets, statement.values):
                    if not isinstance(target, nodes.Name):
                        raise CompileError("unsupported local assignment target")
                    bindings[target.id] = ("expr", value, dict(bindings))
            else:
                raise CompileError(f"unsupported callback statement: {type(statement).__name__}")

    def conditional(self, statement, bindings):
        self.boolean(statement.test, bindings)
        self.emit(JUMP_FALSE, 0, 0)
        patch_false = len(self.code) - 2
        self.block(statement.body, bindings)
        other = statement.orelse
        if other is not None:
            self.emit(JUMP, 0, 0)
            patch_end = len(self.code) - 2
            self.code[patch_false:patch_false + 2] = self.word(len(self.code))
            if isinstance(other, nodes.ElseIf):
                self.conditional(other, bindings)
            else:
                self.block(other, bindings)
            self.code[patch_end:patch_end + 2] = self.word(len(self.code))
        else:
            self.code[patch_false:patch_false + 2] = self.word(len(self.code))

    def compile(self, name):
        if name not in self.functions:
            raise CompileError(f"function {name} not found")
        arguments = self.functions[name].args
        if len(arguments) > 8 or any(not isinstance(argument, nodes.Name) for argument in arguments):
            raise CompileError("callbacks have at most eight boolean arguments")
        self.code = []
        self.resolved_targets = set()
        self.function(name, {argument.id: ("arg", index) for index, argument in enumerate(arguments)})
        self.emit(END)
        return bytes(self.code)
