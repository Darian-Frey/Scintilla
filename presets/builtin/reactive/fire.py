"""
Fire — flames rising from Y=0. Bottom-up cellular automaton: each layer
inherits heat from a randomly-offset cell in the layer below, minus a
per-cell random cooling step. The randomness creates turbulent flicker;
the per-cell offset (without wrap-around) keeps the flames asymmetric.

Volume drives floor intensity, so silence really means a dark cube.
Beats inject a short-lived heat boost into the bottom row, and treble
occasionally ignites a stray spark in the upper half.

Edit guide
----------
  vol gain (1.4)      — multiplier on audio.vol for the floor; raise for hotter
                        fires at lower volumes.
  base_seed (0.0)     — non-audio heat at the floor; 0 = silence is fully dark.
                        Try 0.05 if you want a low background ember even when
                        muted.
  cool_min, cool_max  — per-cell cooling range (0.04–0.22). Lower = taller
                        flames; higher = shorter, snappier flames.
  beat boost          — 0.4 + bass*0.6 in on_beat, decays over ~6 frames.
  spark threshold     — `audio.treb > 0.4` to allow upper-half sparks.
"""
from led_cube import Preset
import numpy as np


class Fire(Preset):
    name        = "Fire"
    description = "Cellular-automaton flame rising from Y=0. Silence = dark."
    author      = "Built-in"
    tags        = ["fire", "rising", "ambient", "volume"]

    def on_load(self, cube):
        s = cube.size
        self.size = s
        self.heat = np.zeros((s, s, s), dtype=np.float32)
        self.rng  = np.random.default_rng(0x712E)
        self.gc   = cube.grid_coords()
        self.beat_boost = 0.0
        # Pre-build broadcasting helpers for the random-walk vectorisation.
        self._xs = np.arange(s).reshape(s, 1)
        self._zs = np.arange(s).reshape(1, s)

    def on_beat(self, cube, audio):
        # A beat ignites the floor — this decays each frame so the kick
        # only lasts a few frames before the audio.vol carries the fire.
        self.beat_boost = max(self.beat_boost, 0.4 + audio.bass * 0.6)

    def on_frame(self, cube, audio):
        N = self.size

        # Snapshot the old heat so the random-walk propagation reads from
        # last frame's values rather than the layer we just rewrote.
        old = self.heat.copy()

        # ── Floor seeding ───────────────────────────────────────────────────
        # Volume is the primary heat source; beat boost is a transient kick;
        # bass adds a steady-state warmth on top.
        intensity = (audio.vol * 1.4
                     + self.beat_boost
                     + audio.bass * 0.35)
        self.beat_boost *= 0.65
        if intensity <= 0.01:
            # No heat to seed — let the existing fire die out instead of
            # propagating zeros (which would just kill it instantly).
            seed = np.zeros((N, N), dtype=np.float32)
        else:
            seed = self.rng.uniform(0.3, 1.0, (N, N)).astype(np.float32) * intensity
        self.heat[:, 0, :] = seed

        # ── Propagation upward (bottom-up cellular automaton) ───────────────
        # Loud audio means less cooling per step → taller flames.
        cool_min = 0.04
        cool_max = 0.22 - audio.vol * 0.10

        for y in range(1, N):
            # Per-cell random horizontal offset in {-1, 0, 1}; clipped at
            # cube faces so heat doesn't wrap around to the opposite side.
            ox = self.rng.integers(-1, 2, (N, N))
            oz = self.rng.integers(-1, 2, (N, N))
            sx = np.clip(self._xs + ox, 0, N - 1)
            sz = np.clip(self._zs + oz, 0, N - 1)
            sampled = old[sx, y - 1, sz]
            cooling = self.rng.uniform(cool_min, cool_max, (N, N)).astype(np.float32)
            self.heat[:, y, :] = np.maximum(0.0, sampled - cooling)

        # ── Treble sparks ───────────────────────────────────────────────────
        # Occasional bright cell in the upper half — like a log popping.
        if audio.treb > 0.4 and self.rng.random() < audio.treb:
            sx = self.rng.integers(0, N)
            sy = self.rng.integers(N // 2, N)
            sz = self.rng.integers(0, N)
            self.heat[sx, sy, sz] = 1.0

        # ── Render ──────────────────────────────────────────────────────────
        vals = np.clip(self.heat[self.gc[:, 0], self.gc[:, 1], self.gc[:, 2]],
                       0.0, 1.0)
        # Black → red → orange → yellow → white as heat rises.
        hue = 60.0 * vals
        sat = np.clip(1.5 - vals * 1.4, 0.0, 1.0)
        cube.set_hsv(hue, sat, vals)
