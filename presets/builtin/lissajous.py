"""
Lissajous — 3D parametric Lissajous figure with fading trails.
Bass/mid/treb drive the three frequency ratios a:b:c.

Edit guide
----------
  a, b, c           — frequency ratios in on_frame(). Small integers give
                      closed curves; try (4, 3, 2) for a tighter figure.
  r = cube.size*0.44 — overall curve size; lower shrinks toward the centre.
  brush             — line thickness; lower = thinner stroke.
  decay (0.87)      — trail length; raise toward 1.0 for longer ghosts.
  pos_hue mult (8.0) — hue variation along the curve.
"""
from led_cube import Preset
import numpy as np


class Lissajous(Preset):
    name        = "Lissajous"
    description = "3D Lissajous figure. Bass/mid/treb drive the frequency ratios."
    author      = "Built-in"
    tags        = ["geometric", "trails", "frequency", "motion"]

    def on_load(self, cube):
        self.trail = np.zeros(cube.count, dtype=np.float32)
        self.hue   = 0.0

    def on_frame(self, cube, audio):
        # Frequency ratios: small integers give closed curves; audio warps them slightly
        a = 3.0 + audio.bass  * 1.2
        b = 2.0 + audio.mid   * 0.8
        c = 1.0 + audio.treb  * 0.5

        t   = audio.time * (0.8 + audio.vol * 0.6)
        cx, cy, cz = cube.centre
        r   = cube.size * 0.44

        # Lissajous point in world space
        px = cx + r * np.sin(a * t)
        py = cy + r * np.sin(b * t + np.pi / 4.0)
        pz = cz + r * np.sin(c * t + np.pi / 2.0)

        # Find the closest LED to the curve point
        pos  = cube.positions()
        dist = np.linalg.norm(pos - np.array([px, py, pz], dtype=np.float32), axis=1)

        # Soft brush: light up LEDs within a radius of the curve
        brush = 0.6 + audio.bass * 0.3
        hit   = dist < brush
        add   = np.where(hit, (1.0 - dist / brush) * (0.9 + audio.vol * 0.3), 0.0)

        # Decay trail, add new contribution
        decay = 0.87 - audio.vol * 0.04
        self.trail = np.clip(self.trail * decay + add, 0.0, 1.0).astype(np.float32)

        # Hue rotates slowly, beat causes a jump
        self.hue = (self.hue + 0.4 + audio.beat * 40.0) % 360.0

        # Colour: hue from position along the cube diagonal for variety
        pos_hue = (self.hue + dist * 8.0) % 360.0
        cube.set_hsv(pos_hue, np.ones(cube.count), self.trail)
        cube.retain()
