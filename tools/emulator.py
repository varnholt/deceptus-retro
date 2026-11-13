"""Minimal headless libretro frontend for ROM integration checks and screenshots.

The core is supplied by the user or tools/bootstrap.py, never committed here.
"""

from __future__ import annotations

import argparse
import ctypes as C
import json
from pathlib import Path
import struct
import sys

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


class GameInfo(C.Structure):
    _fields_ = [("path", C.c_char_p), ("data", C.c_void_p), ("size", C.c_size_t), ("meta", C.c_char_p)]


class SystemInfo(C.Structure):
    _fields_ = [("library_name", C.c_char_p), ("library_version", C.c_char_p),
                ("valid_extensions", C.c_char_p), ("need_fullpath", C.c_bool), ("block_extract", C.c_bool)]


class Variable(C.Structure):
    _fields_ = [("key", C.c_char_p), ("value", C.c_char_p)]


ENVIRONMENT = C.CFUNCTYPE(C.c_bool, C.c_uint, C.c_void_p)
VIDEO = C.CFUNCTYPE(None, C.c_void_p, C.c_uint, C.c_uint, C.c_size_t)
AUDIO = C.CFUNCTYPE(None, C.c_int16, C.c_int16)
AUDIO_BATCH = C.CFUNCTYPE(C.c_size_t, C.c_void_p, C.c_size_t)
INPUT_POLL = C.CFUNCTYPE(None)
INPUT_STATE = C.CFUNCTYPE(C.c_int16, C.c_uint, C.c_uint, C.c_uint, C.c_uint)


class Emulator:
    def __init__(self, core, rom):
        self.core = C.CDLL(str(Path(core).resolve()))
        self.buttons = set()
        self.pixel_format = 0
        self.last_image = None
        self.video_frames = 0
        self.audio_samples = 0
        self.directory = str((ROOT / "artifacts").resolve()).encode()
        Path(self.directory.decode()).mkdir(exist_ok=True)
        self.defaults = []
        self.environment_callback = ENVIRONMENT(self.environment)
        self.video_callback = VIDEO(self.video)
        self.audio_callback = AUDIO(lambda left, right: None)
        self.audio_batch_callback = AUDIO_BATCH(self.audio_batch)
        self.input_poll_callback = INPUT_POLL(lambda: None)
        self.input_state_callback = INPUT_STATE(lambda port, device, index, button: int(port == 0 and button in self.buttons))
        for name, callback in [("environment", self.environment_callback), ("video_refresh", self.video_callback),
                               ("audio_sample", self.audio_callback), ("audio_sample_batch", self.audio_batch_callback),
                               ("input_poll", self.input_poll_callback), ("input_state", self.input_state_callback)]:
            function = getattr(self.core, "retro_set_" + name)
            function.argtypes = [type(callback)]
            function(callback)
        self.core.retro_get_system_info.argtypes = [C.POINTER(SystemInfo)]
        self.core.retro_init()
        info = SystemInfo()
        self.core.retro_get_system_info(C.byref(info))
        self.version = info.library_name.decode() + " " + info.library_version.decode()
        payload = Path(rom).read_bytes()
        self.rom_buffer = C.create_string_buffer(payload)
        self.rom_path = str(Path(rom).resolve()).encode()
        game = GameInfo(self.rom_path, C.cast(self.rom_buffer, C.c_void_p), len(payload), None)
        self.core.retro_load_game.argtypes = [C.POINTER(GameInfo)]
        self.core.retro_load_game.restype = C.c_bool
        if not self.core.retro_load_game(C.byref(game)):
            self.core.retro_deinit()
            raise RuntimeError("emulator refused the ROM")
        self.core.retro_get_memory_data.argtypes = [C.c_uint]
        self.core.retro_get_memory_data.restype = C.c_void_p
        self.core.retro_get_memory_size.argtypes = [C.c_uint]
        self.core.retro_get_memory_size.restype = C.c_size_t

    def environment(self, command, data):
        command &= 0xFFFF
        if command == 10:  # set pixel format
            self.pixel_format = C.cast(data, C.POINTER(C.c_uint))[0]
            return self.pixel_format in (0, 1, 2)
        if command in (9, 30, 31):  # system, content, save directories
            C.cast(data, C.POINTER(C.c_char_p))[0] = self.directory
            return True
        if command == 15:  # variable: let the core use its own defaults
            C.cast(data, C.POINTER(Variable))[0].value = None
            return False
        if command == 17:
            C.cast(data, C.POINTER(C.c_bool))[0] = False
            return True
        if command == 3:  # can dupe
            C.cast(data, C.POINTER(C.c_bool))[0] = True
            return True
        if command == 39:  # language: English
            C.cast(data, C.POINTER(C.c_uint))[0] = 0
            return True
        if command == 47:  # enable both video and audio
            C.cast(data, C.POINTER(C.c_int))[0] = 3
            return True
        return False

    def video(self, data, width, height, pitch):
        if not data:
            return
        self.video_frames += 1
        raw = C.string_at(data, pitch * height)
        if self.pixel_format == 1:
            self.last_image = Image.frombytes("RGB", (width, height), raw, "raw", "BGRX", pitch)
        else:
            # Pillow's RGB;16 handles RGB565; 0RGB1555 needs explicit decoding.
            if self.pixel_format == 2:
                self.last_image = Image.frombytes("RGB", (width, height), raw, "raw", "BGR;16", pitch)
            else:
                pixels = []
                for row in range(height):
                    for value in struct.unpack_from(f"<{width}H", raw, row * pitch):
                        pixels.append((((value >> 10) & 31) * 255 // 31,
                                       ((value >> 5) & 31) * 255 // 31, (value & 31) * 255 // 31))
                self.last_image = Image.new("RGB", (width, height))
                self.last_image.putdata(pixels)

    def audio_batch(self, data, frames):
        self.audio_samples += frames
        return frames

    def run(self, frames, buttons=()):
        self.buttons = set(buttons)
        for _ in range(frames):
            self.core.retro_run()

    def memory(self):
        size = self.core.retro_get_memory_size(2)
        pointer = self.core.retro_get_memory_data(2)
        return C.string_at(pointer, size)

    def debug(self):
        memory = self.memory()
        marker = memory.find(b"\xce\xd3")
        if marker < 0:
            raise RuntimeError("game debug mailbox not found: game has not reached its loop")
        values = struct.unpack_from("<12H", memory, marker)
        return dict(zip(["magic", "x", "y", "camera_x", "ticks", "paused", "health", "grounded",
                         "door_entries", "events", "draws", "patrol_active"], values))

    def screenshot(self, path):
        if self.last_image is None:
            raise RuntimeError("emulator has produced no video frames")
        self.last_image.save(path)

    def close(self):
        self.core.retro_unload_game()
        self.core.retro_deinit()


def verify(core, rom):
    emulator = Emulator(core, rom)
    try:
        emulator.run(120)
        initial = emulator.debug()
        emulator.screenshot(ROOT / "artifacts/snes-boot.png")
        # verify actual PPU tile attributes and all palette banks, not just files.
        # with BG scroll Y=0, the SNES starts displaying map row 1.
        reference = Image.open(ROOT / "assets/generated/room-preview.png").convert("RGB").crop((0, 21, 240, 151))
        rendered = emulator.last_image.convert("RGB").crop((0, 20, 240, 150))
        assert all(tuple(channel >> 3 for channel in expected) == tuple(channel >> 3 for channel in actual)
                   for expected, actual in zip(reference.getdata(), rendered.getdata())), "PPU background differs from converted artwork"
        print("boot", initial, flush=True)
        emulator.run(30, [7])  # right
        moving = emulator.debug()
        emulator.screenshot(ROOT / "artifacts/snes-walk.png")
        print("walk", moving, flush=True)
        assert moving["x"] > initial["x"] + 10, "right input did not move the player"
        assert 28 <= moving["ticks"] - initial["ticks"] <= 32, "simulation clock fell behind display time"
        emulator.run(10, [0])  # B
        jumping = emulator.debug()
        emulator.screenshot(ROOT / "artifacts/snes-jump.png")
        print("jump", jumping, flush=True)
        assert jumping["y"] < moving["y"] - 8, "jump did not rise above the floor"
        emulator.run(60)
        landed = emulator.debug()
        assert landed["grounded"], "player did not land"
        emulator.run(8, [3])  # start; allow an input edge to reach a complete tick
        paused = emulator.debug()
        emulator.run(30, [7])
        frozen = emulator.debug()
        assert paused["paused"] and paused["ticks"] == frozen["ticks"], "pause did not freeze simulation"
        emulator.run(8, [3])
        emulator.run(10)
        assert emulator.debug()["ticks"] != frozen["ticks"], "resume did not advance simulation"
        emulator.run(8, [2])  # select
        emulator.run(5)
        reset = emulator.debug()
        assert reset["x"] == initial["x"], "restart did not restore spawn"
        # a reproducible real-ROM route: stomp the patrol, clear the ledge, enter.
        emulator.core.retro_reset()
        emulator.run(120)
        emulator.run(86, [7])
        emulator.run(50, [7, 0])
        stomp = emulator.debug()
        assert not stomp["patrol_active"], "jump route did not defeat the patrol"
        emulator.run(48, [7])
        emulator.run(30, [7, 0])
        emulator.screenshot(ROOT / "artifacts/snes-ledge.png")
        emulator.run(15, [7])
        ledge = emulator.debug()
        assert ledge["x"] > 440 and ledge["grounded"] and ledge["y"] == 168, "raised platform is unreachable"
        emulator.screenshot(ROOT / "artifacts/snes-portal.png")
        emulator.run(12, [4])
        door = emulator.debug()
        assert door["door_entries"] == 1 and door["x"] == initial["x"], "portal did not return to checkpoint"
        report = {"core": emulator.version, "boot": initial, "walk": moving, "jump": jumping,
                  "land": landed, "pause": paused, "restart": reset, "stomp": stomp,
                  "ledge": ledge, "door": door, "video_frames": emulator.video_frames}
        (ROOT / "artifacts/emulator-report.json").write_text(json.dumps(report, indent=2) + "\n")
        print("SNES emulator integration checks passed")
    finally:
        emulator.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core", type=Path, default=ROOT / ".tools/snes9x/snes9x_libretro.dll")
    parser.add_argument("--rom", type=Path, default=ROOT / "build/snes/deceptus.sfc")
    args = parser.parse_args()
    verify(args.core, args.rom)
