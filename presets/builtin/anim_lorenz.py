"""
Lorenz attractor — a chaotic particle traces the classic Lorenz system
through the cube, leaving a fading rainbow trail behind it. The
butterfly's two wings emerge after a couple of seconds and the
trajectory keeps weaving between them for the rest of the run.

Designed to look its best at 24³+ where the wings have room to spread,
but works at any cube size from 3³ upward (it auto-scales).

Edit guide
----------
  N_FRAMES (280)       — animation length; raise for a longer butterfly tour.
  fps (30)             — playback rate via cube.play().
  steps_per_frame (4)  — Lorenz integration steps per rendered frame;
                         higher = smoother / faster particle.
  dt (0.012)           — Lorenz integrator step size.
  fade (0.93)          — trail decay per frame; closer to 1.0 = longer tail.
  hue_step (1.6)       — degrees of hue rotation per frame; raise for more
                         colour variety along the trail.
"""
from led_cube import Animation
import numpy as np


class LorenzAttractor(Animation):
    name        = "Lorenz attractor"
    description = "Chaotic particle traces the Lorenz butterfly through the cube."
    author      = "Built-in"
    tags        = ["animation", "chaos", "trails", "no-audio"]

    def run(self, cube):
        # ── Lorenz system parameters — the canonical butterfly. ─────────────
        sigma, rho, beta = 10.0, 28.0, 8.0 / 3.0
        dt              = 0.012
        steps_per_frame = 4
        N_FRAMES        = 280
        fade            = 0.93
        hue_step        = 1.6

        # ── Pre-generate the full trajectory ─────────────────────────────────
        # Running the integrator once up-front lets us auto-scale into the
        # cube before any frame is committed. 500 warm-up steps are
        # discarded so the run starts on the attractor itself rather than
        # spiralling in from the initial seed.
        warm_up = 500
        total   = N_FRAMES * steps_per_frame + warm_up
        traj    = np.zeros((total, 3), dtype=np.float32)
        x, y, z = 0.1, 0.0, 0.0
        for i in range(total):
            dx = sigma * (y - x)
            dy = x * (rho - z) - y
            dz = x * y - beta * z
            x += dx * dt
            y += dy * dt
            z += dz * dt
            traj[i] = (x, y, z)
        traj = traj[warm_up:]

        # ── Auto-scale into the cube ─────────────────────────────────────────
        n = cube.size
        mn, mx = traj.min(axis=0), traj.max(axis=0)
        span   = (mx - mn).max()
        scale  = (n - 1) * 0.92 / max(1e-6, span)
        centre = (mn + mx) * 0.5
        cube_centre = np.full(3, (n - 1) * 0.5, dtype=np.float32)
        cube_traj   = np.clip(
            ((traj - centre) * scale + cube_centre).round().astype(np.int32),
            0, n - 1,
        )

        # ── Coord → flat-index lookup ────────────────────────────────────────
        # Lets us light a single cell in the val/hue arrays in O(1) rather
        # than doing a per-point boolean-mask scan over all LEDs.
        gc = cube.grid_coords()
        coord_to_idx = {tuple(gc[i]): i for i in range(cube.count)}

        val = np.zeros(cube.count, dtype=np.float32)
        hue = np.zeros(cube.count, dtype=np.float32)

        # ── Frame loop ───────────────────────────────────────────────────────
        for frame in range(N_FRAMES):
            val *= fade
            current_hue = (frame * hue_step) % 360.0
            start = frame * steps_per_frame
            for s in range(steps_per_frame):
                idx = coord_to_idx.get(tuple(cube_traj[start + s]))
                if idx is not None:
                    val[idx] = 1.0
                    hue[idx] = current_hue
            cube.set_hsv(hue, 1.0, val)
            cube.frame()

        cube.play(fps=30)
