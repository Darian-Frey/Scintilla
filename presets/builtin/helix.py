"""
Helix — twin DNA-style strands spiralling up the cube. Rotation rate
follows audio volume; treble bumps the spread between the two strands.
"""
from led_cube import Preset
import numpy as np


class Helix(Preset):
    name        = "Helix"
    description = "Twin spiralling strands. Volume drives rotation, treble drives spread."
    author      = "Built-in"
    tags        = ["geometric", "motion", "ambient"]

    def on_load(self, cube):
        self.size = cube.size

    def on_frame(self, cube, audio):
        s   = self.size
        gc  = cube.grid_coords()
        cx  = (s - 1) * 0.5
        cz  = (s - 1) * 0.5

        # Per-Y rotation, plus a global rotation that scrolls the helix.
        twist     = 0.6
        global_t  = audio.time * (1.2 + audio.vol * 2.2)
        spread    = 0.32 * s * (0.6 + audio.treb * 0.7)

        # For each LED, what is its expected angle to be on either strand?
        y_angles  = gc[:, 1] * twist + global_t
        sa, ca    = np.sin(y_angles), np.cos(y_angles)
        s1_x      = cx + ca * spread
        s1_z      = cz + sa * spread
        s2_x      = cx - ca * spread
        s2_z      = cz - sa * spread

        d1 = np.hypot(gc[:, 0] - s1_x, gc[:, 2] - s1_z)
        d2 = np.hypot(gc[:, 0] - s2_x, gc[:, 2] - s2_z)
        d  = np.minimum(d1, d2)

        brush  = 0.7 + audio.bass * 0.5
        val    = np.clip(1.0 - d / brush, 0.0, 1.0) * (0.7 + audio.vol * 0.3)
        # Hue advances along Y so the helix has banded colour like a candy cane.
        hue    = (gc[:, 1].astype(np.float32) * (360.0 / s) + audio.time * 30.0) % 360.0
        cube.set_hsv(hue, 1.0, val)
