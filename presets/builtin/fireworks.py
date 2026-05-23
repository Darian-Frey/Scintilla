"""
Fireworks — every beat detonates a fresh shower of sparks at a random
spawn point. Sparks are tracked individually with simple gravity and
drag so the showers look like real ballistic debris.

Edit guide
----------
  n (sparks)          — 16 + bass*32 per detonation in on_beat.
  speed               — initial spark velocity; lower = tighter explosions.
  gravity (-0.04)     — pulls sparks down; raise magnitude for heavier debris.
  drag (0.93)         — air resistance per frame.
  life *= 0.90        — how long sparks burn before culling.
  self.val *= 0.7     — fade rate of the visual trail.
"""
from led_cube import Preset
import numpy as np


class Fireworks(Preset):
    name        = "Fireworks"
    description = "Each beat fires a shower of sparks with simple ballistic physics."
    author      = "Built-in"
    tags        = ["beat", "particles", "trails", "celebration"]

    def on_load(self, cube):
        # Spark state: (x, y, z, vx, vy, vz, hue, life).
        self.sparks = np.zeros((0, 8), dtype=np.float32)
        self.val    = np.zeros(cube.count, dtype=np.float32)
        self.hue    = np.zeros(cube.count, dtype=np.float32)
        self.rng    = np.random.default_rng(0xF1)
        self.size   = cube.size

    def on_beat(self, cube, audio):
        n = int(16 + audio.bass * 32)
        s = self.size
        cx = self.rng.uniform(s * 0.25, s * 0.75)
        cy = self.rng.uniform(s * 0.40, s * 0.85)
        cz = self.rng.uniform(s * 0.25, s * 0.75)
        dirs = self.rng.normal(0, 1, (n, 3)).astype(np.float32)
        dirs /= np.maximum(0.01, np.linalg.norm(dirs, axis=1, keepdims=True))
        speed = 0.35 + audio.bass * 0.7

        new = np.zeros((n, 8), dtype=np.float32)
        new[:, 0] = cx; new[:, 1] = cy; new[:, 2] = cz
        new[:, 3:6] = dirs * speed
        new[:, 6]   = (audio.centroid * 280.0 + self.rng.uniform(0, 60)) % 360
        new[:, 7]   = 1.0
        self.sparks = np.vstack([self.sparks, new])

    def on_frame(self, cube, audio):
        # Step the simulation.
        self.sparks[:, 0:3] += self.sparks[:, 3:6]
        self.sparks[:, 4]   -= 0.04                  # gravity
        self.sparks[:, 3:6] *= 0.93                  # air drag
        self.sparks[:, 7]   *= 0.90                  # life fades

        s = self.size
        live = (self.sparks[:, 7] > 0.05) \
             & (self.sparks[:, 0] > -1) & (self.sparks[:, 0] < s) \
             & (self.sparks[:, 1] > -1) & (self.sparks[:, 1] < s + 2) \
             & (self.sparks[:, 2] > -1) & (self.sparks[:, 2] < s)
        self.sparks = self.sparks[live]

        # Fade the trail buffer; hue persists until a brighter spark replaces it.
        self.val *= 0.7

        if len(self.sparks):
            gc = cube.grid_coords()
            for sp in self.sparks:
                ix, iy, iz = int(round(sp[0])), int(round(sp[1])), int(round(sp[2]))
                m = (gc[:, 0] == ix) & (gc[:, 1] == iy) & (gc[:, 2] == iz)
                brighter = m & (sp[7] > self.val)
                self.val = np.where(brighter, sp[7], self.val)
                self.hue = np.where(brighter, sp[6], self.hue)

        cube.set_hsv(self.hue, 1.0, self.val)
