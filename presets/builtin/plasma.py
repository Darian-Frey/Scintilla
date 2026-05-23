"""
Plasma — animated 3D plasma field. Volume modulates the colour cycle
speed and centroid biases the hue offset, but the visual is hypnotic
enough to be enjoyable in silence too.
"""
from led_cube import Preset
import numpy as np


class Plasma(Preset):
    name        = "Plasma"
    description = "Hypnotic 3D plasma field. Audio modulates speed and hue offset."
    author      = "Built-in"
    tags        = ["plasma", "ambient", "trippy", "no-beat"]

    def on_load(self, cube):
        # Pre-compute position offsets so we don't pay it every frame.
        pos = cube.positions().copy()
        self.px = pos[:, 0] - pos[:, 0].mean()
        self.py = pos[:, 1] - pos[:, 1].mean()
        self.pz = pos[:, 2] - pos[:, 2].mean()
        self.scale = 0.6 / max(1.0, cube.size * 0.5)   # frequency normalised by size

    def on_frame(self, cube, audio):
        t = audio.time * (0.8 + audio.vol * 1.4)
        s = self.scale

        # Classic three-component plasma: sum of sines along independent axes
        # plus a radial component for depth.
        f1 = np.sin(self.px * s * 1.3 + t * 1.1)
        f2 = np.sin(self.py * s * 1.7 + t * 0.7 + 1.4)
        f3 = np.sin(self.pz * s * 2.0 + t * 0.9 + 2.1)
        radial = np.sin(np.sqrt(self.px**2 + self.py**2 + self.pz**2) * s * 1.8 + t)

        field = (f1 + f2 + f3 + radial) * 0.25 + 0.5   # → [0, 1]

        # Map field strength to hue with a small offset from spectral centroid.
        hue = (field * 280.0 + audio.centroid * 90.0 + t * 18.0) % 360.0
        val = 0.4 + field * 0.6                         # darken the troughs
        cube.set_hsv(hue, 1.0, val)
