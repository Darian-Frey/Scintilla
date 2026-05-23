"""
Starfield — drifting stars across the cube. The viewer's apparent motion
warps with audio: volume accelerates forward drift, treble jitters the
sideways course slightly, beats refresh stars at the far plane.

Edit guide
----------
  n (star count)      — max(32, size*size); raise for a denser sky.
  speed               — 0.10 + audio.vol * 0.45 forward drift rate.
  jitter              — audio.treb * 0.05 sideways noise.
  hue 200.0           — base palette; try 280 for a violet sky.
  sat 0.4             — lower for pure white stars, higher for saturated colour.
"""
from led_cube import Preset
import numpy as np


class Starfield(Preset):
    name        = "Starfield"
    description = "Drifting stars; volume drives speed, treble drives sideways drift."
    author      = "Built-in"
    tags        = ["particles", "motion", "ambient", "no-beat"]

    def on_load(self, cube):
        n = max(32, cube.size * cube.size)
        s = float(cube.size)
        rng = np.random.default_rng(0x573A8)
        # Each star: (x, y, z, brightness).
        self.stars = np.zeros((n, 4), dtype=np.float32)
        self.stars[:, 0] = rng.uniform(0, s, n)
        self.stars[:, 1] = rng.uniform(0, s, n)
        self.stars[:, 2] = rng.uniform(0, s, n)
        self.stars[:, 3] = rng.uniform(0.3, 1.0, n)
        self.rng         = rng
        self.size        = s

    def _respawn(self, mask):
        s = self.size
        n = int(mask.sum())
        if n == 0: return
        self.stars[mask, 0] = self.rng.uniform(0, s, n)
        self.stars[mask, 1] = self.rng.uniform(0, s, n)
        self.stars[mask, 2] = s - 0.5
        self.stars[mask, 3] = self.rng.uniform(0.3, 1.0, n)

    def on_beat(self, cube, audio):
        # On a strong beat, refresh the dimmest 20 % of stars to keep the
        # sky from sliding into a static pattern.
        cutoff = np.percentile(self.stars[:, 3], 20)
        self._respawn(self.stars[:, 3] <= cutoff)

    def on_frame(self, cube, audio):
        speed   = 0.10 + audio.vol  * 0.45
        jitter  = audio.treb * 0.05
        self.stars[:, 2] -= speed
        self.stars[:, 0] += self.rng.uniform(-jitter, jitter, len(self.stars))
        self.stars[:, 1] += self.rng.uniform(-jitter, jitter, len(self.stars))

        # Wrap any star that drifted off the near plane back to the far plane.
        out = self.stars[:, 2] < -0.5
        self._respawn(out)

        gc  = cube.grid_coords()
        val = np.zeros(cube.count, dtype=np.float32)

        # Star brightness gets a slight depth cue — distant stars dimmer.
        depths = self.stars[:, 2] / self.size
        brights = self.stars[:, 3] * (0.4 + depths * 0.6)
        for (x, y, z, _), b in zip(self.stars, brights):
            ix, iy, iz = int(round(x)), int(round(y)), int(round(z))
            m = (gc[:, 0] == ix) & (gc[:, 1] == iy) & (gc[:, 2] == iz)
            val = np.where(m & (b > val), b, val)

        # Hue: cool blue/white with a centroid-driven warm tint.
        hue = 200.0 + audio.centroid * 40.0
        sat = 0.4 - audio.centroid * 0.3                # bright voices fade to white
        cube.set_hsv(hue, sat, val)
