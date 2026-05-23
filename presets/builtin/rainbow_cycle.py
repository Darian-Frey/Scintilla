"""
Rainbow cycle — a hue wave sweeps along the cube diagonal. Volume
modulates wave speed; beats add momentum to the cycle so the wave
visibly accelerates with the music's drive.
"""
from led_cube import Preset
import numpy as np


class RainbowCycle(Preset):
    name        = "Rainbow cycle"
    description = "Hue wave along the cube diagonal. Volume drives speed."
    author      = "Built-in"
    tags        = ["rainbow", "wave", "ambient"]

    def on_load(self, cube):
        # Diagonal projection of every LED — a single scalar per cell.
        pos = cube.positions()
        d   = np.array([1.0, 1.0, 1.0], dtype=np.float32)
        d  /= np.linalg.norm(d)
        self.proj  = pos @ d
        self.phase = 0.0

    def on_beat(self, cube, audio):
        self.phase += 18.0 + audio.bass * 30.0

    def on_frame(self, cube, audio):
        self.phase += 1.5 + audio.vol * 4.0
        # Hue = position along diagonal + advancing phase.
        hue = (self.proj * 40.0 + self.phase) % 360.0
        # Volume nudges brightness so silent moments aren't blown out.
        bright = 0.45 + audio.vol * 0.55
        cube.set_hsv(hue, 1.0, bright)
