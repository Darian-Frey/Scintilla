"""
Bouncing ball — a single ball with simple physics caroms around the
cube. Beats kick it sideways; volume bumps the ball's brightness. Walls
are perfectly elastic so it never stops.

Edit guide
----------
  initial vel         — np.array([0.35, 0.45, 0.30]) sets opening trajectory.
  kick magnitude      — 0.25 + bass*0.5 in on_beat.
  damping (0.995)     — global drag; lower = ball slows faster between kicks.
  glow                — 1.0 + bass*0.6 controls the visual ball size.
  self.val *= 0.7     — tail fade rate.
"""
from led_cube import Preset
import numpy as np


class BouncingBall(Preset):
    name        = "Bouncing ball"
    description = "A single ball ricocheting around the cube. Beats kick it."
    author      = "Built-in"
    tags        = ["physics", "particles", "playful"]

    def on_load(self, cube):
        s = cube.size
        self.size = float(s)
        self.pos = np.array([s * 0.5, s * 0.5, s * 0.5], dtype=np.float32)
        self.vel = np.array([0.35, 0.45, 0.30], dtype=np.float32)
        self.hue = 0.0
        self.val = np.zeros(cube.count, dtype=np.float32)
        self.cur_hue = np.zeros(cube.count, dtype=np.float32)

    def on_beat(self, cube, audio):
        # Kick — small random impulse on each beat; bass scales it.
        rng = np.random.default_rng()
        kick = rng.uniform(-1, 1, 3).astype(np.float32)
        self.vel += kick * (0.25 + audio.bass * 0.5)
        self.hue = (self.hue + 47.0 + audio.bass * 60.0) % 360.0

    def on_frame(self, cube, audio):
        # Step + reflect off the cube walls.
        self.pos += self.vel
        s = self.size - 1.0
        for ax in range(3):
            if self.pos[ax] < 0:
                self.pos[ax] = -self.pos[ax]
                self.vel[ax] = -self.vel[ax]
            elif self.pos[ax] > s:
                self.pos[ax] = 2 * s - self.pos[ax]
                self.vel[ax] = -self.vel[ax]
        # Gentle global damping so kicks don't compound forever.
        self.vel *= 0.995

        # Soft ball: glow around nearest LEDs.
        gc   = cube.grid_coords().astype(np.float32)
        dist = np.linalg.norm(gc - self.pos, axis=1)
        glow = 1.0 + audio.bass * 0.6
        ball_v = np.clip(1.0 - dist / glow, 0.0, 1.0) * (0.7 + audio.vol * 0.3)

        # Trail: fade old, replace where ball is brighter.
        self.val *= 0.7
        brighter = ball_v > self.val
        self.val = np.where(brighter, ball_v, self.val)
        self.cur_hue = np.where(brighter, self.hue, self.cur_hue)

        cube.set_hsv(self.cur_hue, 1.0, self.val)
