"""
Snake — a single coloured snake winds through the cube, turning on
beats. The body is a fading tail of fixed length. Volume nudges the
snake's speed; treble bumps the turning probability between beats.

Edit guide
----------
  tail_len            — max(20, s*s) by default; lower for a stubbier snake.
  speed               — 0.35 + vol*0.7 advances per frame.
  turn chance         — 0.05 + treb*0.15 between beats.
  hue step on turn    — 30.0 + bass*60.0 per direction change.
"""
from led_cube import Preset
import numpy as np


class Snake(Preset):
    name        = "Snake"
    description = "A coloured snake winds through the cube, turning on beats."
    author      = "Built-in"
    tags        = ["motion", "playful", "trails"]

    def on_load(self, cube):
        s = cube.size
        self.size = s
        self.pos  = np.array([s // 2, s // 2, s // 2], dtype=np.int32)
        # Direction is one of the six axis-aligned unit vectors.
        self.dir  = np.array([1, 0, 0], dtype=np.int32)
        # Tail = list of (x, y, z, age). Age increments each frame, fade by age.
        self.tail = []
        self.tail_len = max(20, s * s)
        self.hue  = 120.0
        self.tick = 0.0
        self.rng  = np.random.default_rng(0x5E4)

    def _turn(self, audio):
        # Pick a new axis-aligned direction that isn't the inverse of current.
        axes = [np.array([1,0,0],dtype=np.int32),
                np.array([-1,0,0],dtype=np.int32),
                np.array([0,1,0],dtype=np.int32),
                np.array([0,-1,0],dtype=np.int32),
                np.array([0,0,1],dtype=np.int32),
                np.array([0,0,-1],dtype=np.int32)]
        valid = [d for d in axes if not np.array_equal(d, -self.dir)]
        self.dir = valid[self.rng.integers(0, len(valid))]
        self.hue = (self.hue + 30.0 + audio.bass * 60.0) % 360.0

    def on_beat(self, cube, audio):
        self._turn(audio)

    def on_frame(self, cube, audio):
        # Speed control — slowed via a tick accumulator so we advance roughly
        # once per N frames rather than every frame.
        self.tick += 0.35 + audio.vol * 0.7
        s = self.size
        while self.tick >= 1.0:
            self.tick -= 1.0
            # Occasional spontaneous turn between beats, more likely on treble.
            if self.rng.random() < 0.05 + audio.treb * 0.15:
                self._turn(audio)
            self.pos += self.dir
            # Bounce off walls (mirror direction).
            for ax in range(3):
                if self.pos[ax] < 0 or self.pos[ax] >= s:
                    self.dir[ax] = -self.dir[ax]
                    self.pos[ax] += 2 * self.dir[ax]
            self.tail.append((int(self.pos[0]), int(self.pos[1]), int(self.pos[2]),
                              self.hue))
            if len(self.tail) > self.tail_len:
                self.tail.pop(0)

        # Render — fade from head (brightest) to tail (dim).
        gc  = cube.grid_coords()
        val = np.zeros(cube.count, dtype=np.float32)
        hue = np.zeros(cube.count, dtype=np.float32)
        L = len(self.tail)
        for i, (x, y, z, h) in enumerate(self.tail):
            b = (i + 1) / L
            m = (gc[:, 0] == x) & (gc[:, 1] == y) & (gc[:, 2] == z)
            keep = m & (b > val)
            val = np.where(keep, b, val)
            hue = np.where(keep, h, hue)
        cube.set_hsv(hue, 1.0, val)
