"""
DNA — twin spiral strands plus connecting rungs every few rows. Behaves
like Helix but with explicit cross-strands so it reads as a double-helix
ladder rather than a free twin spiral.

Edit guide
----------
  twist (0.55)        — coil tightness.
  spread (s*0.32)     — distance between the two strands.
  rung_y modulo (2)   — rung row spacing; raise to 3 or 4 for sparser rungs.
  h_strand1, h_strand2,
  h_rung              — the three palette hues; swap freely for new colour schemes.
"""
from led_cube import Preset
import numpy as np


class DNA(Preset):
    name        = "DNA"
    description = "Double helix with cross-strand rungs every other row."
    author      = "Built-in"
    tags        = ["geometric", "biology", "motion"]

    def on_load(self, cube):
        self.size = cube.size

    def on_frame(self, cube, audio):
        s   = self.size
        gc  = cube.grid_coords()
        cx, cz = (s - 1) * 0.5, (s - 1) * 0.5

        spin   = audio.time * (0.8 + audio.vol * 1.6)
        twist  = 0.55
        spread = s * 0.32

        ang   = gc[:, 1] * twist + spin
        sa, ca = np.sin(ang), np.cos(ang)
        s1x, s1z = cx + ca * spread, cz + sa * spread
        s2x, s2z = cx - ca * spread, cz - sa * spread

        d1 = np.hypot(gc[:, 0] - s1x, gc[:, 2] - s1z)
        d2 = np.hypot(gc[:, 0] - s2x, gc[:, 2] - s2z)

        brush = 0.65 + audio.treb * 0.4
        strand1 = np.clip(1.0 - d1 / brush, 0, 1)
        strand2 = np.clip(1.0 - d2 / brush, 0, 1)

        # Rungs: every other Y row, on the line connecting the two strands.
        rung_y    = (gc[:, 1] % 2 == 0)
        # Distance from LED to the chord between (s1x, s1z) and (s2x, s2z).
        midx      = (s1x + s2x) * 0.5
        midz      = (s1z + s2z) * 0.5
        # Perpendicular distance via cross product magnitude / length.
        chord_dx  = s2x - s1x
        chord_dz  = s2z - s1z
        chord_len = np.hypot(chord_dx, chord_dz) + 1e-6
        rel_x     = gc[:, 0] - midx
        rel_z     = gc[:, 2] - midz
        # Project onto chord; reject if outside [-len/2, +len/2].
        t         = (rel_x * chord_dx + rel_z * chord_dz) / (chord_len ** 2)
        on_chord  = np.abs(t) < 0.5
        perp      = np.abs(rel_x * chord_dz - rel_z * chord_dx) / chord_len
        rung      = rung_y & on_chord & (perp < 0.6)

        val = np.maximum.reduce([strand1, strand2, rung * 0.7])
        val *= 0.7 + audio.vol * 0.3

        # Hue: strand1 gets warm, strand2 gets cool, rungs get middle.
        h_strand1 = 20.0
        h_strand2 = 200.0
        h_rung    = 120.0
        hue = np.where(strand1 > strand2, h_strand1, h_strand2)
        hue = np.where(rung, h_rung, hue).astype(np.float32)
        cube.set_hsv(hue, 1.0, val)
