"""
Fire — flames rising from Y=0. Cellular-automaton style: each cell's
heat is the average of the cells immediately below it, slightly cooled
each step. Bass kicks fan the flames; treble adds sparks at the top.

Edit guide
----------
  floor_intensity     — heat seeded at the bottom row; higher = taller flames.
  cooled (0.86)       — cooling per step; lower = shorter, snappier flames.
  treb sparks         — change the 0.4 threshold to make sparks more/less frequent.
  hue formula (60*v)  — colour gradient; 40*v gives a redder flame.
  sat formula         — 1.5 - v*1.4 controls how white the hottest cells go.
"""
from led_cube import Preset
import numpy as np


class Fire(Preset):
    name        = "Fire"
    description = "Flames rising from the bottom. Bass fans them taller, treble adds sparks."
    author      = "Built-in"
    tags        = ["fire", "rising", "ambient", "bass"]

    def on_load(self, cube):
        s = cube.size
        # Heat grid in (x, y, z); we shuffle this in 3D each frame.
        self.heat = np.zeros((s, s, s), dtype=np.float32)
        self.size = s
        self.rng  = np.random.default_rng(0x712E)
        # Cache the grid_coords → flat-index mapping for the active LEDs.
        gc        = cube.grid_coords()
        self.gc   = gc

    def on_frame(self, cube, audio):
        s   = self.size
        h   = self.heat

        # New heat seeded at the floor — louder mix = hotter floor.
        floor_intensity = 0.4 + audio.bass * 0.6 + audio.vol * 0.3
        h[:, 0, :] = self.rng.uniform(0.5, 1.0, (s, s)) * floor_intensity

        # Convection upward: each cell becomes a weighted average of the
        # cell directly below + small contributions from diagonals.
        cooled = h * (0.86 - audio.vol * 0.05)
        below  = np.zeros_like(cooled)
        below[:, 1:, :] = (cooled[:, :-1, :]
                          + np.roll(cooled[:, :-1, :], 1, axis=0) * 0.4
                          + np.roll(cooled[:, :-1, :], -1, axis=0) * 0.4
                          + np.roll(cooled[:, :-1, :], 1, axis=2) * 0.4
                          + np.roll(cooled[:, :-1, :], -1, axis=2) * 0.4) / 2.6
        h[:] = np.maximum(cooled, below)

        # Treble triggers occasional sparks near the top.
        if audio.treb > 0.4 and self.rng.random() < audio.treb:
            sx, sz = self.rng.integers(0, s), self.rng.integers(0, s)
            h[sx, s - 1, sz] = 1.0

        # Sample heat for each active LED.
        vals = h[self.gc[:, 0], self.gc[:, 1], self.gc[:, 2]]
        vals = np.clip(vals, 0.0, 1.0)

        # Black → red → orange → yellow → white as heat rises.
        hue = 60.0 * vals               # 0=red, ~60=yellow
        sat = np.clip(1.5 - vals * 1.4, 0.0, 1.0)   # de-saturate at peak heat
        cube.set_hsv(hue, sat, vals)
