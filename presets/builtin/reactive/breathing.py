"""
Breathing — slow ambient hue cycle with a gentle pulse synced to audio
energy. The whole cube acts as a single soft light, like a smart bulb
breathing in time with the music.

Edit guide
----------
  a (0.08)            — volume smoothing; lower = slower breath response.
  audio.time * 0.6    — natural breath frequency (~10s cycle); raise for faster.
  brightness terms    — 0.25 floor + breath*0.5 + smoothed_vol*0.5.
  hue formula         — time*8 + centroid*60 controls colour drift speed.
"""
from led_cube import Preset
import numpy as np


class Breathing(Preset):
    name        = "Breathing"
    description = "Slow whole-cube breath. Volume drives intensity, time drives hue."
    author      = "Built-in"
    tags        = ["ambient", "mood", "slow", "no-beat"]

    def on_load(self, cube):
        # Smoothed envelope so the cube doesn't flicker even with bursty audio.
        self.smoothed_vol = 0.0

    def on_frame(self, cube, audio):
        # 1st-order low-pass on volume so the visual breathes rather than jitters.
        a = 0.08
        self.smoothed_vol = a * audio.vol + (1.0 - a) * self.smoothed_vol

        # Base breath at ~ 1/6 Hz (10s cycle) + volume modulation.
        breath = 0.5 + 0.5 * np.sin(audio.time * 0.6)
        bright = 0.25 + breath * 0.5 + self.smoothed_vol * 0.5

        hue = (audio.time * 8.0 + audio.centroid * 60.0) % 360.0
        cube.set_hsv(hue, 0.9, bright)
