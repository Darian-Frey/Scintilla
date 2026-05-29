"""
Spiral — a single voxel walks a slowly-rising spiral. Demonstrates the
Animation model: builds a fixed sequence of frames and commits each one
via cube.frame(). No audio reactivity.

Edit guide
----------
  N (frame count)     — 120 by default; raise for a longer animation.
  fps                 — cube.play(24) sets playback rate.
  turns               — total revolutions over the run (3.0 = three spins).
  hue_step            — colour cycles as the spiral climbs.
"""
from led_cube import Animation
import numpy as np


class Spiral(Animation):
    name        = "Spiral"
    description = "Animation script: a voxel walks a rising 3D spiral."
    author      = "Built-in"
    tags        = ["animation", "geometric", "no-audio"]

    def run(self, cube):
        N      = 120
        turns  = 3.0
        s      = cube.size
        cx, cz = (s - 1) * 0.5, (s - 1) * 0.5
        r      = max(1.0, (s - 1) * 0.45)

        for t in range(N):
            cube.fade(0.78)                              # trail
            theta = turns * 2.0 * np.pi * t / N
            y     = int(round(t * (s - 1) / max(1, N - 1)))
            x     = int(round(cx + np.cos(theta) * r))
            z     = int(round(cz + np.sin(theta) * r))
            x = max(0, min(s - 1, x))
            z = max(0, min(s - 1, z))
            hue   = (t * 4.0) % 360
            # Convert HSV to a uint8 RGB for the single voxel write.
            h = hue / 60.0
            i = int(h) % 6
            f = h - int(h)
            v = 1.0
            p, q, tt = v * (1 - 1.0), v * (1 - f * 1.0), v * (1 - (1 - f) * 1.0)
            r1, g1, b1 = [(v, tt, p), (q, v, p), (p, v, tt),
                          (p, q, v), (tt, p, v), (v, p, q)][i]
            cube.set(x, y, z,
                     int(r1 * 255), int(g1 * 255), int(b1 * 255))
            cube.retain()                                # keep the trail
            cube.frame()
        cube.play(fps=24)
