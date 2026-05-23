"""
Pulse Sphere — shell expands from centre on bass hit, fades to nothing.

Edit guide
----------
  velocity init       — shell launch speed in on_beat; raise for snappier expansion.
  damping (0.83)      — how quickly the shell decelerates.
  shell_width         — thickness of the glowing ring.
  hue formula         — uses centroid; replace with audio.bass*240 for warm
                        bass / cool treble swap.
"""
from led_cube import Preset
import numpy as np


class PulseSphere(Preset):
    name        = "Pulse Sphere"
    description = "Sphere expands from centre on bass hit, fades to nothing."
    author      = "Built-in"
    tags        = ["beat", "bass", "ambient", "sphere"]

    def on_load(self, cube):
        self.radius   = 0.0
        self.velocity = 0.0
        self.hue      = 0.0

    def on_beat(self, cube, audio):
        # Fresh impulse on each beat; stronger bass = faster expansion
        self.velocity = 0.12 + audio.bass * 0.28
        self.hue      = audio.centroid * 300.0    # bass=red, treble=purple

    def on_frame(self, cube, audio):
        self.radius    = max(0.0, self.radius + self.velocity)
        self.velocity *= 0.83 - audio.vol * 0.04  # damping, slightly faster when loud

        max_r = cube.distances_from_centre().max()
        if self.radius > max_r + 1.0:
            self.radius   = 0.0
            self.velocity = 0.0

        dist = cube.distances_from_centre()
        rim  = np.abs(dist - self.radius)

        # Soft shell: full brightness at rim, fades off either side
        shell_width = 0.7 + audio.bass * 0.4
        bright = np.where(
            rim < shell_width,
            (1.0 - rim / shell_width) * audio.vol,
            0.0
        ).astype(np.float32)

        sat = np.where(bright > 0, 1.0, 0.0)
        cube.set_hsv(self.hue, sat, bright)
