"""
Ripple — concentric waves expand from the cube centre on every beat.
Multiple ripples coexist; each fades as it expands beyond the cube radius.
"""
from led_cube import Preset
import numpy as np


class Ripple(Preset):
    name        = "Ripple"
    description = "Beat-triggered concentric waves expanding from the centre."
    author      = "Built-in"
    tags        = ["beat", "radial", "trails", "ambient"]

    def on_load(self, cube):
        # Each ripple is (radius, hue, energy). Empty array at start.
        self.ripples = np.zeros((0, 3), dtype=np.float32)
        self.max_r   = cube.distances_from_centre().max()

    def on_beat(self, cube, audio):
        # Spawn a fresh ripple; centroid picks the hue so high-pitched beats
        # ripple in a different colour than bass-heavy ones.
        new = np.array([[0.0, audio.centroid * 320.0, 0.8 + audio.bass * 0.2]],
                       dtype=np.float32)
        self.ripples = np.vstack([self.ripples, new])

    def on_frame(self, cube, audio):
        # Expand every ripple; speed nudged by volume.
        speed = 0.18 + audio.vol * 0.25
        self.ripples[:, 0] += speed

        # Cull ripples that have run off the outside of the cube.
        keep = self.ripples[:, 0] < (self.max_r + 1.5)
        self.ripples = self.ripples[keep]

        dist = cube.distances_from_centre()
        val  = np.zeros(cube.count, dtype=np.float32)
        hue  = np.zeros(cube.count, dtype=np.float32)

        # Each ripple contributes a thin glowing shell at its current radius.
        for r, h, e in self.ripples:
            shell_width = 0.6 + audio.bass * 0.4
            d_to_shell  = np.abs(dist - r)
            contrib     = np.where(d_to_shell < shell_width,
                                   (1.0 - d_to_shell / shell_width) * e,
                                   0.0)
            # Brightest contributor on each LED wins both hue and value.
            mask        = contrib > val
            val         = np.where(mask, contrib, val)
            hue         = np.where(mask, h, hue)

        cube.set_hsv(hue, 1.0, val)
