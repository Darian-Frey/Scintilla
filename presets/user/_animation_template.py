"""
Animation template — rename this file and the class, then edit run().

Animations run once when loaded via File → Run animation script… The
script builds each frame and commits it with cube.frame(); the final
cube.play(fps) tells Scintilla what playback rate to use.

Unlike presets, animations do not receive audio. They produce a fixed
sequence of frames that can then be played, saved, or exported.

Quick reference for what's available on `cube` — see
presets/INSTRUCTIONS.md for the full preset/animation API.

  cube.size                     int — grid size (3..32)
  cube.count                    int — number of LEDs
  cube.positions()              np.float32 (N,3)
  cube.grid_coords()            np.int32   (N,3)
  cube.distances_from_centre()  np.float32 (N,)
  cube.set(x, y, z, r, g, b)
  cube.set_hsv(h, s, v)          scalar or (N,) per channel
  cube.fill(r, g, b) / cube.clear() / cube.fade(factor)
  cube.frame()                  commit current state as one frame
  cube.play(fps=12)             finish; play back at this fps
"""
from led_cube import Animation
import numpy as np


class MyAnimation(Animation):
    name        = "My animation"
    description = "Describe what your animation does here."
    author      = ""
    tags        = []

    def run(self, cube):
        # Example: hue wave sweeping across the cube along the X axis.
        n = cube.size
        gc = cube.grid_coords()
        for t in range(60):
            phase = t * 12.0
            hue   = (gc[:, 0] * (360.0 / max(1, n - 1)) + phase) % 360
            cube.set_hsv(hue, 1.0, 1.0)
            cube.frame()
        cube.play(fps=24)
