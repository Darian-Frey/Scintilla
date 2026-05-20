"""
New preset — rename this file and the class below, then edit on_frame().
See docs/D-003-preset-scripting.md for the full API reference.

Quick reference:

  audio.bass / .mid / .treb     float [0,1] — frequency band energy
  audio.vol                     float [0,1] — RMS amplitude
  audio.beat                    bool  — True for one frame on onset
  audio.centroid                float [0,1] — spectral centroid
  audio.spectrum                np.float32 (gridSize,) — band magnitudes
  audio.waveform                np.float32 (1024,) — raw PCM samples
  audio.time                    float — seconds since preset loaded

  cube.size                     int — gridSize
  cube.count                    int — number of LEDs
  cube.centre                   np.float32 (3,) — world centre
  cube.positions()              np.float32 (N,3) — LED world positions
  cube.distances_from_centre()  np.float32 (N,)  — dist from centre
  cube.grid_coords()            np.int32   (N,3)  — integer grid coords
  cube.set_hsv(h, s, v)        h in degrees [0,360]; s,v in [0,1]; each float or (N,)
  cube.set_all(colors)          np.uint8 (N,3) — set all LEDs at once
  cube.fill(r, g, b)
  cube.clear()
  cube.fade(factor)             multiply brightness by factor (useful for trails)
  cube.retain()                 keep this frame as base for next frame
"""

from led_cube import Preset
import numpy as np


class NewPreset(Preset):
    name        = "New Preset"
    description = "Describe what your preset does here."
    author      = ""
    tags        = []

    def on_load(self, cube):
        # Initialise any state variables here
        pass

    def on_frame(self, cube, audio):
        # Update LEDs here — called ~60 times per second
        # Example: fill with a colour that pulses with the bass
        brightness = audio.bass * 0.8 + audio.vol * 0.2
        hue        = audio.centroid * 240.0
        cube.set_hsv(hue, 1.0, brightness)
