"""Read native San Andreas mission instructions and relocate local jumps."""

from dataclasses import dataclass
import json
from struct import pack, pack_into, unpack_from


SCALAR = {1: "<i", 2: "<H", 3: "<H", 4: "<b", 5: "<h", 6: "<f",
          10: "<H", 11: "<H", 16: "<H", 17: "<H"}
WIDTH = (0, 4, 2, 2, 1, 2, 4, 6, 6, 8, 2, 2, 6, 6, 0, 16, 2, 2, 6, 6)
MISSION_META = 0xCFA4
MISSION_BUFFER_SIZE = 69_000


@dataclass(frozen=True)
class Operand:
    start: int
    end: int
    tag: int
    value: int | float | bytes
    kind: str = ""

    def encode(self, value):
        if self.tag not in SCALAR:
            raise ValueError("Expected a numeric SCM operand")
        return pack(SCALAR[self.tag], value)


@dataclass(frozen=True)
class Instruction:
    start: int
    end: int
    opcode: int
    name: str
    operands: tuple[Operand, ...]

    @property
    def args(self):
        return tuple(operand.value for operand in self.operands)


class OpcodeTable:
    def __init__(self, path):
        definition = json.loads(path.read_text(encoding="utf-8-sig"))
        commands = next(ext["commands"] for ext in definition["extensions"] if ext["name"] == "default")
        self.by_id = {int(cmd["id"], 16): cmd for cmd in commands}
        self.by_name = {cmd["name"]: int(cmd["id"], 16) for cmd in commands}

    def decode(self, data, start, end):
        opcode, = unpack_from("<H", data, start)
        command = self.by_id[opcode & 0x7FFF]
        name = command["name"]
        kinds = [p["type"] for p in command.get("input", []) + command.get("output", [])]
        kinds += [""] * (command["num_params"] - len(kinds))
        operands = []
        offset = start + 2
        for kind in kinds:
            if kind == "arguments":
                while offset < end and data[offset]:
                    argument = read_operand(data, offset, end)
                    operands.append(argument)
                    offset = argument.end
                if offset == end:
                    raise ValueError("Unterminated argument list")
                offset += 1
            else:
                argument = read_operand(data, offset, end, kind)
                operands.append(argument)
                offset = argument.end
        return Instruction(start, offset, opcode, name, tuple(operands))

    def encode(self, name, *operands, negate=False):
        opcode = self.by_name[name]
        command = self.by_id[opcode]
        if len(operands) != command["num_params"]:
            raise ValueError(f"Wrong number of arguments for {name}")
        return pack("<H", opcode | (0x8000 if negate else 0)) + b"".join(operands)


def read_operand(data, offset, end, kind=""):
    if offset >= end or not 1 <= data[offset] < len(WIDTH):
        raise ValueError(f"Invalid operand at 0x{offset:X}")
    tag = data[offset]
    start = offset + 1
    stop = start + WIDTH[tag]
    if tag == 14:
        stop = start + 1 + data[start]
        start += 1
    if stop > end:
        raise ValueError("Operand crosses mission boundary")
    value = unpack_from(SCALAR[tag], data, start)[0] if tag in SCALAR else bytes(data[start:stop])
    return Operand(offset, stop, tag, value, kind)


def integer(value):
    if -128 <= value <= 127:
        return pack("<Bb", 4, value)
    if -32768 <= value <= 32767:
        return pack("<Bh", 5, value)
    return pack("<Bi", 1, value)


def floating(value):
    return pack("<Bf", 6, value)


def local(slot):
    return pack("<BH", 3, slot)


def global_var(byte_offset):
    return pack("<BH", 2, byte_offset)


def text_key(name):
    key = name.encode("ascii")
    if not 1 <= len(key) <= 7:
        raise ValueError("A GXT text key must be one to seven characters")
    return pack("<B8s", 9, key)


class Snippet:
    def __init__(self, table):
        self.table = table
        self.code = bytearray()
        self.labels = {}
        self.jumps = []

    def emit(self, name, *args, negate=False):
        self.code.extend(self.table.encode(name, *args, negate=negate))

    def mark(self, name):
        if name in self.labels:
            raise ValueError(f"Duplicate label: {name}")
        self.labels[name] = len(self.code)

    def jump(self, name, target):
        self.jumps.append((len(self.code) + 3, target))
        self.emit(name, pack("<Bi", 1, 0))

    def linked(self, offset):
        code = bytearray(self.code)
        for address, label in self.jumps:
            pack_into("<i", code, address, -(offset + self.labels[label]))
        return code


class Mission:
    def __init__(self, data, table, difficulty="zor"):
        self.original = data
        self.data = bytearray(data)
        self.table = table
        self.difficulty = difficulty
        self.instructions = {}
        self.snippets = {}
        self.detours = {}
        self.decode_error = None
        offset = 0
        while offset < len(data):
            try:
                instruction = table.decode(data, offset, len(data))
            except (KeyError, ValueError) as error:
                self.decode_error = (offset, error)
                break
            self.instructions[offset] = instruction
            offset = instruction.end

    def instruction(self, offset, name, *, negate=False):
        cmd = self.instructions.get(offset)
        if not cmd or cmd.name != name or bool(cmd.opcode & 0x8000) != negate:
            raise ValueError(f"Expected {name} at mission offset 0x{offset:X}")
        return cmd

    def replace(self, offset, name, arg, old, new, *, negate=False):
        operand = self.instruction(offset, name, negate=negate).operands[arg]
        if operand.tag not in (1, 4, 5, 6):
            raise ValueError(f"Unexpected {name} operand at 0x{offset:X}")
        start = operand.start + 1
        if self.data[start:operand.end] != operand.encode(old):
            raise ValueError(f"Unexpected {name} operand at 0x{offset:X}")
        # Only scale a previously reviewed increase in hostile gunfire.
        # Story timers, navigation, friendly nerfs and retries stay authored.
        if self.difficulty == "acimasiz" and new > old:
            if (name, arg) in (("SET_CHAR_ACCURACY", 1), ("SET_CHAR_SHOOT_RATE", 1), ("TASK_DRIVE_BY", 9)):
                new = min(100, new + max(2, (new - old + 1) // 2))
        self.data[start:operand.end] = operand.encode(new)

    def insert_before(self, offset, name, snippet):
        self.instruction(offset, name)
        if offset in self.snippets:
            raise ValueError(f"Duplicate insertion at 0x{offset:X}")
        self.snippets[offset] = snippet

    def reserve_locals(self, *slots):
        for cmd in self.instructions.values():
            for operand in cmd.operands:
                if operand.tag in (3, 11, 17) and operand.value in slots:
                    raise ValueError(f"Mission already uses local {operand.value}")
                if operand.tag in (7, 8, 12, 13, 18, 19):
                    base, index, count, flags = unpack_from("<HHBB", operand.value)
                    stride = {8: 1, 13: 2, 19: 4}.get(operand.tag, 0)
                    if any(
                        (stride and base <= slot < base + count * stride)
                        or (not flags & 0x80 and slot == index)
                        for slot in slots
                    ):
                        raise ValueError("Local overlaps an existing mission array")

    def redirect_before(self, offset, name, snippet):
        # Keep the stock offsets in missions that contain opaque debug data.
        # The snippet can use a direct local GOTO to bypass the stock command.
        cmd = self.instruction(offset, name)
        if cmd.end - cmd.start < 7 or cmd.end not in self.instructions:
            raise ValueError("A detour requires a complete instruction and successor")
        self.detours.setdefault(offset, []).append(snippet)

    def build_detours(self):
        if self.snippets:
            raise ValueError("Cannot combine relocating insertions with fixed-offset detours")
        linked = self.data.copy()
        for start, steps in self.detours.items():
            cmd = self.instructions[start]
            tail = len(linked)
            linked[start:start + 7] = self.table.encode("GOTO", pack("<Bi", 1, -tail))
            for step in steps:
                linked.extend(step.linked(len(linked)))
            copied_at = len(linked)
            stock_bytes = self.data[start:cmd.end]
            unchanged = stock_bytes == self.original[start:cmd.end]
            linked.extend(stock_bytes)
            linked.extend(self.table.encode("GOTO", pack("<Bi", 1, -cmd.end)))
            known = set(self.instructions)
            offset = tail
            branches = []
            while offset < len(linked):
                ins = self.table.decode(linked, offset, len(linked))
                known.add(offset)
                targets = [-x.value for x in ins.operands if x.kind == "label" and x.value < 0]
                if offset == copied_at and unchanged and self.decode_error:
                    # The unchanged stock instruction can jump past the
                    # opaque section. Only its original targets may do so.
                    opaque_at = self.decode_error[0]
                    branches.extend(target for target in targets if not opaque_at <= target < len(self.original))
                else:
                    branches.extend(targets)
                offset = ins.end
            if any(target not in known for target in branches):
                raise ValueError("Invalid local branch in detour")
        if len(linked) > MISSION_BUFFER_SIZE:
            raise ValueError("Mission detours exceed the native mission buffer")
        return bytes(linked)

    def build(self):
        if self.detours:
            return self.build_detours()
        if not self.snippets:
            return bytes(self.data)
        if self.decode_error:
            at, error = self.decode_error
            raise ValueError(f"Cannot relocate mission with unparsed code at 0x{at:X}") from error

        starts = {}
        added = 0
        for start in self.instructions:
            starts[start] = start + added
            if start in self.snippets:
                added += len(self.snippets[start].code)
        if len(self.data) + added > MISSION_BUFFER_SIZE:
            raise ValueError("Mission exceeds the native mission buffer")

        output = bytearray()
        for start, cmd in self.instructions.items():
            if start in self.snippets:
                output.extend(self.snippets[start].linked(len(output)))
            instruction_bytes = self.data[start:cmd.end]
            for operand in cmd.operands:
                if operand.kind != "label" or operand.value >= 0:
                    continue
                target = -operand.value
                if target not in starts:
                    raise ValueError(f"Invalid local target 0x{target:X} from 0x{start:X}")
                new_target = operand.encode(-starts[target])
                instruction_bytes[operand.start + 1 - start:operand.end - start] = new_target
            output.extend(instruction_bytes)
        # Decode the newly linked script and verify all local branches land on instructions.
        verification = Mission(bytes(output), self.table)
        if verification.decode_error:
            raise ValueError("Rebuilt mission could not be decoded")
        for cmd in verification.instructions.values():
            for operand in cmd.operands:
                if operand.kind == "label" and operand.value < 0 and -operand.value not in verification.instructions:
                    raise ValueError(f"Invalid relocated branch at 0x{cmd.start:X}")
        return bytes(output)


class Campaign:
    def __init__(self, data):
        self.data = data
        self.main_end, self.largest, count = unpack_from("<IIH", data, MISSION_META + 8)
        self.offsets = unpack_from(f"<{count}I", data, MISSION_META + 24) + (len(data),)
        if not self.main_end <= self.offsets[0] or tuple(sorted(set(self.offsets))) != self.offsets:
            raise ValueError("Invalid mission table")

    def mission(self, index):
        start, end = self.offsets[index:index + 2]
        return self.data[start:end]

    def build(self, missions):
        output = bytearray(self.data[:self.offsets[0]])
        largest = self.largest
        for index in range(len(self.offsets) - 1):
            data = missions.get(index, self.mission(index))
            if len(data) > MISSION_BUFFER_SIZE:
                raise ValueError(f"Mission {index} exceeds the native mission buffer")
            largest = max(largest, len(data))
            pack_into("<I", output, MISSION_META + 24 + index * 4, len(output))
            output.extend(data)
        pack_into("<I", output, MISSION_META + 12, largest)
        return bytes(output)
