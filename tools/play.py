"""Play the GBA/SNES ROM or shared C desktop build in a small Tk window."""

import argparse
import ctypes as C
import os
from pathlib import Path
import time
import tkinter as tk

from PIL import Image, ImageEnhance, ImageTk

from emulator import Emulator, ROOT


class Host:
    def __init__(self, library):
        self.library = C.CDLL(str(library.resolve()))
        self.library.preview_init()
        self.room = Image.open(ROOT / "assets/generated/room-preview.png").convert("RGB")
        self.actors = Image.open(ROOT / "assets/generated/actors-preview.png")
        self.actors.info["transparency"] = 0
        self.actors = self.actors.convert("RGBA")
        self.sprites = []
        for frame in range(19):
            left, top = (frame % 4) * 32, (frame // 4) * 48
            self.sprites.append(self.actors.crop((left, top, left + 32, top + 48)))

    def run(self, buttons):
        self.library.preview_tick(buttons)
        camera = self.library.preview_state(2)
        camera_y = self.library.preview_state(11)
        canvas = self.room.crop((camera, camera_y, camera + 256, camera_y + 224)).convert("RGBA")
        for index in range(12):
            frame = self.library.preview_draw(index, 2)
            if frame < 0:
                break
            sprite = self.sprites[frame]
            if self.library.preview_draw(index, 4):
                sprite = sprite.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
            if self.library.preview_draw(index, 3):
                sprite = sprite.copy()
                red, green, blue, alpha = sprite.split()
                sprite = Image.merge("RGBA", (red.point(lambda value: value // 2),
                    green.point(lambda value: min(255, value + 45)), blue.point(lambda value: value // 2), alpha))
            canvas.alpha_composite(sprite, (self.library.preview_draw(index, 0), self.library.preview_draw(index, 1)))
        if self.library.preview_state(3):
            canvas = ImageEnhance.Brightness(canvas).enhance(0.5)
        return canvas


def play(backend, core, library, rom=None):
    timer_api = C.WinDLL('winmm') if os.name == 'nt' else None
    if timer_api:
        timer_api.timeBeginPeriod(1)
    window = tk.Tk()
    window.title(f"Deceptus — {rom.stem if rom else backend} | " +
                 ("arrows move / Z or X jump / Up interact / Enter menu / Tab inventory" if backend == 'gba'
                  else "arrows move / Z jump / Up interact / Enter pause / R restart"))
    canvas = tk.Label(window, borderwidth=0)
    canvas.pack()
    if backend == 'gba':
        emulator = Emulator(core, rom or ROOT / 'build/gba/deceptus.gba', {'mgba_use_bios': 'OFF', 'mgba_frameskip': '0'})
    elif backend == 'snes':
        emulator = Emulator(core, rom or ROOT / 'build/snes/deceptus.sfc')
    else:
        emulator = None
    host = Host(library) if backend == "host" else None
    keys = set()
    window.bind("<KeyPress>", lambda event: keys.add(event.keysym.lower()))
    window.bind("<KeyRelease>", lambda event: keys.discard(event.keysym.lower()))
    window.bind("<FocusOut>", lambda event: keys.clear())
    deadline = time.perf_counter()
    timer = None

    def refresh():
        nonlocal deadline, timer
        if emulator:
            mapping = {"left": 6, "right": 7, "z": 0, "space": 0, "up": 4, "return": 3, "r": 2}
            if backend == 'gba':
                mapping.update(z=8, space=8, x=0, down=5, tab=2)
            emulator.run(1, [value for key, value in mapping.items() if key in keys])
            image = emulator.last_image
        else:
            mapping = {"left": 1, "right": 2, "z": 4, "space": 4, "up": 8, "return": 16, "r": 32}
            buttons = 0
            for key, value in mapping.items():
                if key in keys:
                    buttons |= value
            image = host.run(buttons)
        if image:
            canvas.image = ImageTk.PhotoImage(image.resize((image.width * 3, image.height * 3), Image.Resampling.NEAREST))
            canvas.configure(image=canvas.image)
        deadline += 1 / 60
        now = time.perf_counter()
        if deadline < now - 0.1:
            deadline = now
        timer = window.after(max(1, round((deadline - now) * 1000)), refresh)

    def close():
        nonlocal timer
        if timer is not None:
            window.after_cancel(timer)
            timer = None
        if emulator:
            emulator.close()
        if timer_api:
            timer_api.timeEndPeriod(1)
        window.destroy()

    window.protocol("WM_DELETE_WINDOW", close)
    refresh()
    window.mainloop()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--backend", choices=["gba", "snes", "host"], default="gba")
    parser.add_argument("--core", type=Path)
    parser.add_argument("--library", type=Path, default=ROOT / "build/host/Debug/retro_preview.dll")
    parser.add_argument("--rom", type=Path, help="open a particular built ROM")
    args = parser.parse_args()
    core = args.core or ROOT / ('.tools/mgba/mgba_libretro.dll' if args.backend == 'gba' else '.tools/snes9x/snes9x_libretro.dll')
    play(args.backend, core, args.library, args.rom)
