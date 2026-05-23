"""
Galaxy — slowly rotating spiral arms in the XZ plane. The arms sit at
the cube's vertical mid-band but drift up and down with volume so the
disc breathes a little.
"""
from led_cube import Preset
import numpy as np


class Galaxy(Preset):
    name        = "Galaxy"
    description = "Rotating spiral arms in a disc through the cube's mid-band."
    author      = "Built-in"
    tags        = ["radial", "spiral", "ambient"]

    def on_load(self, cube):
        ang  = cube.angles()
        self.theta = ang[:, 0]                       # azimuth in XZ
        self.r     = np.hypot(
            cube.positions()[:, 0] - cube.centre[0],
            cube.positions()[:, 2] - cube.centre[2],
        )
        self.y     = cube.positions()[:, 1] - cube.centre[1]
        self.max_r = self.r.max() + 1e-3

    def on_frame(self, cube, audio):
        arms = 3
        twist = 0.9 + audio.bass * 0.3
        spin  = audio.time * (0.4 + audio.vol * 0.8)

        # Spiral arm field: cos(arms*theta - twist*r + spin). Higher near arms.
        arm = np.cos(arms * self.theta - twist * self.r + spin)
        # Disc thickness: vertical falloff around cube centre.
        disc_thick = 1.2 + audio.vol * 1.4
        vert = np.clip(1.0 - np.abs(self.y) / disc_thick, 0.0, 1.0)
        # Radial falloff: dim core, peak mid, dim at edge.
        radial = np.exp(-((self.r / self.max_r - 0.55) ** 2) * 10.0)

        val = np.clip((arm * 0.5 + 0.5) * vert * radial * (0.7 + audio.vol * 0.3),
                      0.0, 1.0)
        # Hue: outer arms cool blue, inner red core.
        hue = (220.0 - self.r / self.max_r * 220.0 + audio.time * 8.0) % 360.0
        cube.set_hsv(hue, 1.0, val)
