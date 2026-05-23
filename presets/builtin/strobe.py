"""
Strobe — full-cube flash on every beat, hue rotating each hit. Fades
between beats so silent stretches dim out instead of strobing dark.
Best on tracks with strong onsets — washes out into a hue cycler on
soft material.

Edit guide
----------
  decay (0.65)        — how fast the flash dims between beats.
  audio.vol * 0.15    — ambient floor so silence isn't pitch black.
  hue step (47.0)     — colour jump per beat; primes give a nice non-repeating
                        rotation.
"""
from led_cube import Preset


class Strobe(Preset):
    name        = "Strobe"
    description = "Full-cube flash on each beat with a rotating hue."
    author      = "Built-in"
    tags        = ["beat", "flash", "party", "fast"]

    def on_load(self, cube):
        self.brightness = 0.0
        self.hue        = 0.0

    def on_beat(self, cube, audio):
        # Stronger flash for stronger bass.
        self.brightness = 1.0
        self.hue = (self.hue + 47.0 + audio.bass * 60.0) % 360.0

    def on_frame(self, cube, audio):
        # Fast decay so it really feels like a strobe.
        self.brightness *= 0.65 - audio.vol * 0.05
        self.brightness = max(self.brightness, audio.vol * 0.15)   # ambient floor
        cube.set_hsv(self.hue, 1.0, self.brightness)
