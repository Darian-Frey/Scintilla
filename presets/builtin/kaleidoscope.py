"""
Kaleidoscope — mirrored radial pattern that rotates. The pattern is the
sum of a few rotating petals seen from the cube centre; audio rotates,
beats refresh the pattern's phase.

Edit guide
----------
  petals              — 6 + int(centroid*4); raise the base for more arms.
  rotation speed      — audio.time * (0.4 + vol*1.0).
  beat phase step     — 0.6 + bass*0.7 in on_beat.
  hue formula         — theta-based; drop the time*20 term for a fixed palette.
"""
from led_cube import Preset
import numpy as np


class Kaleidoscope(Preset):
    name        = "Kaleidoscope"
    description = "Rotating radial pattern. Audio drives rotation, beats refresh phase."
    author      = "Built-in"
    tags        = ["radial", "geometric", "trippy"]

    def on_load(self, cube):
        ang = cube.angles()
        self.theta = ang[:, 0]                  # azimuth in XZ plane
        self.r     = cube.distances_from_centre()
        self.phase = 0.0

    def on_beat(self, cube, audio):
        # Step the kaleidoscope phase on each beat so the pattern shifts in
        # time with the music rather than purely drifting.
        self.phase = (self.phase + 0.6 + audio.bass * 0.7) % (2.0 * np.pi)

    def on_frame(self, cube, audio):
        t      = audio.time * (0.4 + audio.vol * 1.0) + self.phase
        petals = 6 + int(audio.centroid * 4.0)   # 6–10 petals

        # Petal field: cos(petals * theta + t) modulated by a radial envelope.
        f = np.cos(petals * self.theta + t) * np.cos(self.r * 0.8 - t * 1.2)
        val = np.clip(0.5 + f * 0.5, 0.0, 1.0)

        hue = (self.theta * (180.0 / np.pi) + audio.time * 20.0 + audio.centroid * 60.0) % 360.0
        cube.set_hsv(hue, 1.0, val)
