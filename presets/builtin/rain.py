"""
Rain — falling particles. Bass spawns more drops; the spectral centroid
picks the hue, so a bright snare sounds pastel while a bass thump sounds
saturated and warm.

Edit guide
----------
  n (drops per beat)  — 4 + bass*14 in on_beat; raise for denser rain.
  fall speed          — 0.22 + audio.vol * 0.35 in on_frame.
  self.val *= 0.78    — trail length; closer to 1.0 = longer streaks.
  hue formula         — centroid*280 + 40; tweak for a different palette.
"""
from led_cube import Preset
import numpy as np


class Rain(Preset):
    name        = "Rain"
    description = "Falling particles. Bass spawns more drops; centroid picks the hue."
    author      = "Built-in"
    tags        = ["particles", "trails", "ambient", "bass"]

    def on_load(self, cube):
        # Each drop is (x, y, z, hue). All in float grid space.
        self.drops = np.zeros((0, 4), dtype=np.float32)
        self.val   = np.zeros(cube.count, dtype=np.float32)
        self.hue   = np.zeros(cube.count, dtype=np.float32)
        self.rng   = np.random.default_rng(0xA17)

    def on_beat(self, cube, audio):
        n = int(4 + audio.bass * 14)
        s = cube.size
        new = np.zeros((n, 4), dtype=np.float32)
        new[:, 0] = self.rng.uniform(0, s - 1, n)
        new[:, 1] = s - 0.5
        new[:, 2] = self.rng.uniform(0, s - 1, n)
        new[:, 3] = (audio.centroid * 280.0 + 40.0) % 360
        self.drops = np.vstack([self.drops, new])

    def on_frame(self, cube, audio):
        # Gravity — faster fall under loud mix.
        self.drops[:, 1] -= 0.22 + audio.vol * 0.35
        self.drops = self.drops[self.drops[:, 1] > -0.6]

        # Fade brightness for the trail; hue stays so the trail keeps its colour.
        self.val *= 0.78

        if len(self.drops):
            gc = cube.grid_coords()
            for d in self.drops:
                ix, iy, iz = int(round(d[0])), int(round(d[1])), int(round(d[2]))
                m = (gc[:, 0] == ix) & (gc[:, 1] == iy) & (gc[:, 2] == iz)
                self.val = np.where(m, 1.0,  self.val)
                self.hue = np.where(m, d[3], self.hue)

        cube.set_hsv(self.hue, 1.0, self.val)
