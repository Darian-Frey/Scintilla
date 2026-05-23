"""
Matrix rain — vertical green streams falling through the cube. Each
column has its own head position and tail length; new columns spawn at
random Y on beats so the rain keeps feeling fresh.

Edit guide
----------
  n (spawn per beat)  — 2 + bass*6 columns per beat.
  spontaneous spawn   — 0.08 + vol*0.2 chance per frame even without a beat.
  fade (0.78)         — trail length; closer to 1.0 = longer tails.
  fall speed          — 0.45 + vol*0.5.
  hue 120.0           — green default; try 0 (red) for a hellfire variant.
"""
from led_cube import Preset
import numpy as np


class MatrixRain(Preset):
    name        = "Matrix rain"
    description = "Green vertical streams falling like the Matrix code rain."
    author      = "Built-in"
    tags        = ["columns", "ambient", "fall"]

    def on_load(self, cube):
        s = cube.size
        self.size = s
        # Per-(x,z) column: y-head position (negative means inactive).
        self.heads = np.full((s, s), -10.0, dtype=np.float32)
        # 3D field of brightness per cell, faded each frame.
        self.field = np.zeros((s, s, s), dtype=np.float32)
        self.rng   = np.random.default_rng(0x4A7)
        self.gc    = cube.grid_coords()

    def on_beat(self, cube, audio):
        # Spawn a handful of fresh columns; bass scales the count.
        s = self.size
        n = int(2 + audio.bass * 6)
        for _ in range(n):
            x = self.rng.integers(0, s)
            z = self.rng.integers(0, s)
            self.heads[x, z] = s - 0.5

    def on_frame(self, cube, audio):
        s = self.size
        # Spawn occasional columns even without a beat so silent moments aren't dark.
        if self.rng.random() < 0.08 + audio.vol * 0.2:
            x = self.rng.integers(0, s); z = self.rng.integers(0, s)
            self.heads[x, z] = s - 0.5

        # Fade the whole field — this is what creates the tail.
        self.field *= 0.78

        # Each active column lays a bright spot at its current head row.
        active = self.heads >= 0
        if active.any():
            xs, zs = np.where(active)
            ys     = np.clip(self.heads[xs, zs].astype(int), 0, s - 1)
            self.field[xs, ys, zs] = 1.0

        # Advance heads downward; speed nudged by audio.
        speed = 0.45 + audio.vol * 0.5
        self.heads[active] -= speed
        # Mark exhausted columns inactive.
        self.heads[self.heads < -2.0] = -10.0

        # Sample the field for active LEDs.
        vals = self.field[self.gc[:, 0], self.gc[:, 1], self.gc[:, 2]]
        # Green-with-bright-head: hue stays green, value carries the cell brightness.
        cube.set_hsv(120.0, 1.0, np.clip(vals, 0, 1))
