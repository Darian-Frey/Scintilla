"""
Spectrum waterfall — frequency-band history scrolling along Z. The
freshest spectrum sits at Z = size-1; each frame the slice shifts one
step toward Z = 0. Looks like a 3D EQ history display.

Edit guide
----------
  hue gradient        — 230.0 - x*(230.0/(s-1)); change 230 for the colour range.
  brightness floor    — 0.4 + vals*0.7; lower the floor to make quiet bands disappear.
  col_heights         — vals*(s-1); replace with a non-linear curve for taller
                        responses to small bands.
"""
from led_cube import Preset
import numpy as np


class SpectrumWaterfall(Preset):
    name        = "Spectrum waterfall"
    description = "Live spectrum scrolling toward Z=0; freshest at Z=size-1."
    author      = "Built-in"
    tags        = ["spectrum", "frequency", "scrolling"]

    def on_load(self, cube):
        s = cube.size
        self.size  = s
        # history[z][x] = band-x magnitude at age z. z = size-1 is the live row.
        self.history = np.zeros((s, s), dtype=np.float32)
        self.gc      = cube.grid_coords()
        # Cache (x, z) → grid index lookups via the integer coords.

    def on_frame(self, cube, audio):
        s = self.size
        bands = audio.spectrum
        if len(bands) != s:
            bands = np.resize(bands, s)

        # Shift history toward Z=0 then write the fresh row at Z=size-1.
        self.history[:-1] = self.history[1:]
        self.history[-1]  = bands

        # Each LED: value = history[z][x]; sample a band magnitude per LED.
        vals = self.history[self.gc[:, 2], self.gc[:, 0]]
        # Y axis acts as the band's column height (light cells up to band height).
        col_heights = vals * (s - 1)
        on          = self.gc[:, 1] <= col_heights
        v           = on.astype(np.float32) * np.clip(0.4 + vals * 0.7, 0.0, 1.0)

        # Hue: blue at low freq (X=0) to red at high freq (X=s-1).
        hue = 230.0 - self.gc[:, 0].astype(np.float32) * (230.0 / max(1, s - 1))
        cube.set_hsv(hue, 1.0, v)
