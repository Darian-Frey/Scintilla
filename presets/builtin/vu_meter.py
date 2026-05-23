"""
VU meter — frequency bands drive vertical columns climbing from Y=0.
Classic EQ rig. Hue runs green → yellow → red the higher each column
reaches, mimicking the analog VU meter aesthetic.
"""
from led_cube import Preset
import numpy as np


class VuMeter(Preset):
    name        = "VU meter"
    description = "Classic vertical EQ columns. Green at low levels, red at peaks."
    author      = "Built-in"
    tags        = ["bands", "spectrum", "classic", "frequency"]

    def on_load(self, cube):
        s = cube.size
        # Per-column peak hold — drops slowly so the user can see the head.
        self.peaks = np.zeros(s, dtype=np.float32)

    def on_frame(self, cube, audio):
        s     = cube.size
        gc    = cube.grid_coords()                  # (N, 3) int
        bands = audio.spectrum                       # length = size
        if len(bands) != s:
            # Fall back to a tiled / truncated copy if the band count drifts.
            bands = np.resize(bands, s)

        # Peak hold — instantaneous max with slow decay so the head lingers.
        self.peaks = np.maximum(self.peaks * 0.94, bands)

        # For each LED, look up its X-column band height; light it if the LED's
        # Y is at or below the band's height (in LED units).
        col_heights = np.clip(bands * (s - 1), 0, s - 1)
        led_heights = col_heights[gc[:, 0]]
        on          = gc[:, 1] <= led_heights

        # Hue: green at the bottom, red at the top — relative to each LED's
        # own height fraction so columns ramp through the gradient evenly.
        h = (1.0 - gc[:, 1].astype(np.float32) / max(1, s - 1)) * 110.0  # 0=red, 110=green
        v = on.astype(np.float32) * (0.65 + bands[gc[:, 0]] * 0.35)

        # Peak head: bright white speck at the peak-hold row of each column.
        peak_rows = np.clip(self.peaks * (s - 1), 0, s - 1).astype(np.int32)
        is_peak   = (gc[:, 1] == peak_rows[gc[:, 0]]) & (self.peaks[gc[:, 0]] > 0.04)
        v         = np.where(is_peak, 1.0, v)
        sat       = np.where(is_peak, 0.0, 1.0)        # white at the peak head

        cube.set_hsv(h, sat, v)
