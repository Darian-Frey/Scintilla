"""CubeProxy — the `cube` object available inside preset methods."""

from __future__ import annotations
import numpy as np
from typing import Sequence


class CubeProxy:
    """Wraps the LED array. Presets read geometry and write colours through this object."""

    def __init__(self, grid_size: int, shape: str, positions: np.ndarray):
        """
        Args:
            grid_size:  int — gridSize (3–32)
            shape:      str — "cube" | "sphere" | "cylinder" | "pyramid"
            positions:  np.float32 shape (N, 3) — world positions of all LEDs
        """
        self._size      = grid_size
        self._shape     = shape
        self._pos       = positions                              # (N, 3) float32
        self._count     = len(positions)
        self._centre    = positions.mean(axis=0)                 # (3,) float32
        self._dists     = np.linalg.norm(positions - self._centre, axis=1)  # (N,)
        self._colors    = np.zeros((self._count, 3), dtype=np.uint8)
        self._retain    = False
        self._grid_coords: np.ndarray | None = None

    # ── Properties ────────────────────────────────────────────────────────────

    @property
    def size(self) -> int:        return self._size
    @property
    def shape(self) -> str:       return self._shape
    @property
    def count(self) -> int:       return self._count
    @property
    def centre(self) -> np.ndarray: return self._centre

    # ── Geometry ──────────────────────────────────────────────────────────────

    def positions(self) -> np.ndarray:
        """np.float32 shape (N, 3) — world-space LED positions."""
        return self._pos

    def grid_coords(self) -> np.ndarray:
        """np.int32 shape (N, 3) — integer grid coordinates."""
        if self._grid_coords is None:
            self._grid_coords = np.round(
                self._pos - self._pos.min(axis=0)
            ).astype(np.int32)
        return self._grid_coords

    def distances_from_centre(self) -> np.ndarray:
        """np.float32 shape (N,) — Euclidean distance of each LED from centre."""
        return self._dists

    def angles(self) -> np.ndarray:
        """np.float32 shape (N, 2) — (theta, phi) spherical coords in radians."""
        d = self._pos - self._centre
        theta = np.arctan2(d[:, 0], d[:, 2])
        phi   = np.arctan2(np.sqrt(d[:, 0]**2 + d[:, 2]**2), d[:, 1])
        return np.stack([theta, phi], axis=1)

    # ── Single-LED write ──────────────────────────────────────────────────────

    def set(self, x: int, y: int, z: int, r: int, g: int, b: int) -> None:
        """Set a single LED by grid coordinates. Silently ignored if not in mask."""
        gc = self.grid_coords()
        mask = (gc[:, 0] == x) & (gc[:, 1] == y) & (gc[:, 2] == z)
        self._colors[mask] = [r, g, b]

    def set_pos(self, pos: Sequence[float], r: int, g: int, b: int) -> None:
        """Set the LED nearest to world position pos."""
        d = np.linalg.norm(self._pos - np.array(pos), axis=1)
        self._colors[np.argmin(d)] = [r, g, b]

    # ── Bulk write (preferred for performance) ────────────────────────────────

    def set_all(self, colors: np.ndarray) -> None:
        """Set all LEDs from np.uint8 array shape (N, 3)."""
        self._colors[:] = colors

    def set_hsv(self, h, s, v) -> None:
        """Set all LEDs from HSV values.

        h, s, v can each be:
          - a scalar float (applied to all LEDs)
          - a np.ndarray shape (N,) (per-LED values)

        h is in degrees [0, 360]. s, v in [0, 1].
        """
        N = self._count
        h_ = np.broadcast_to(np.asarray(h, dtype=np.float32), (N,)) % 360
        s_ = np.clip(np.broadcast_to(np.asarray(s, dtype=np.float32), (N,)), 0, 1)
        v_ = np.clip(np.broadcast_to(np.asarray(v, dtype=np.float32), (N,)), 0, 1)
        self._colors[:] = _hsv_to_rgb_array(h_, s_, v_)

    def set_mask(self, mask: np.ndarray, r: int, g: int, b: int) -> None:
        """Set only LEDs where mask (bool array shape (N,)) is True."""
        self._colors[mask] = [r, g, b]

    # ── Convenience ───────────────────────────────────────────────────────────

    def fill(self, r: int, g: int, b: int) -> None:
        self._colors[:] = [r, g, b]

    def clear(self) -> None:
        self._colors[:] = 0

    def fade(self, factor: float) -> None:
        """Multiply all current brightness by factor [0, 1]. Useful for trails."""
        self._colors[:] = (self._colors.astype(np.float32) * factor).astype(np.uint8)

    def retain(self) -> None:
        """Keep current frame as starting state for next frame (for trails/decay).

        Without retain(), the write buffer is cleared to black before each on_frame().
        Call at the END of on_frame() when you want the previous frame to persist.
        """
        self._retain = True

    # ── Internal ──────────────────────────────────────────────────────────────

    def _get_colors(self) -> np.ndarray:
        return self._colors

    def _begin_frame(self) -> None:
        """Called by the runner before on_frame(). Clears buffer unless retain() was set."""
        if not self._retain:
            self._colors[:] = 0
        self._retain = False

    def _serialise(self) -> dict:
        """Serialise current LED colours to JSON-compatible dict (sparse)."""
        voxels = {}
        gc = self.grid_coords()
        for i in range(self._count):
            r, g, b = self._colors[i]
            if r or g or b:
                key = f"{gc[i,0]},{gc[i,1]},{gc[i,2]}"
                voxels[key] = [int(r), int(g), int(b)]
        return voxels


# ── Helpers ───────────────────────────────────────────────────────────────────

def _hsv_to_rgb_array(h: np.ndarray, s: np.ndarray, v: np.ndarray) -> np.ndarray:
    """Vectorised HSV → RGB. h in [0,360], s and v in [0,1]. Returns uint8 (N,3)."""
    h6 = h / 60.0
    i  = np.floor(h6).astype(np.int32) % 6
    f  = h6 - np.floor(h6)
    p  = v * (1 - s)
    q  = v * (1 - f * s)
    t  = v * (1 - (1 - f) * s)

    rgb = np.zeros((len(h), 3), dtype=np.float32)
    for idx, (ri, gi, bi) in enumerate([(v,t,p),(q,v,p),(p,v,t),(p,q,v),(t,p,v),(v,p,q)]):
        m = i == idx
        rgb[m, 0] = ri[m] if hasattr(ri, '__len__') else ri
        rgb[m, 1] = gi[m] if hasattr(gi, '__len__') else gi
        rgb[m, 2] = bi[m] if hasattr(bi, '__len__') else bi

    return (rgb * 255).clip(0, 255).astype(np.uint8)
