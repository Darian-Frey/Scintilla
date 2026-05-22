"""
JSON-line subprocess runner for Scintilla presets.

Invocation:

    PYTHONPATH=/path/to/Scintilla/presets \
        python3 -m led_cube._runner /path/to/preset.py

Wire protocol — one JSON message per line, both directions. Matches the
contract declared by src/scripting/PresetRunner.h.

App → preset (stdin):

    {"type": "load",
     "cube": {"grid_size": N,
              "shape": "cube|sphere|cylinder|pyramid",
              "positions": [[x, y, z], ...]}}
        Sent once at startup. `positions` is the world-space LED layout in
        the shape mask; if absent, the runner falls back to the full N³
        cube grid for testing convenience.

    {"type": "frame",
     "audio": {"bands": [f, ...],     // length = grid_size
               "rms": f,
               "centroid": f,
               "beat": bool}}
        Sent every render frame.

    {"type": "unload"}
        Optional graceful shutdown. The runner exits when stdin closes too.

Preset → app (stdout):

    {"type": "ready", "name": "..."}              // after load succeeds
    {"type": "frame", "voxels": {"x,y,z": [r, g, b], ...}}
                                                  // sparse, after each frame
    {"type": "error", "message": "...", "line": -1}   // on exception
"""

from __future__ import annotations

import importlib.util
import json
import sys
import time
import traceback
from typing import Type

import numpy as np

from . import Preset
from ._cube_proxy import CubeProxy


# ── Audio namespace passed to preset callbacks ───────────────────────────────


class AudioFrame:
    """The `audio` object presented to on_frame() / on_beat()."""

    def __init__(self, bands: np.ndarray, rms: float, centroid: float,
                 beat: bool, time_s: float) -> None:
        self.spectrum = bands
        self.bands    = bands              # alias used by some presets
        self.vol      = float(rms)
        self.centroid = float(centroid)
        self.beat     = bool(beat)
        self.time     = float(time_s)

        # bass / mid / treb derived by splitting the band array 25 % / 50 % / 25 %.
        n  = max(1, len(bands))
        i1 = max(1, n // 4)
        i2 = max(i1, (n * 3) // 4)
        self.bass = float(bands[:i1].mean())  if i1 > 0  else 0.0
        self.mid  = float(bands[i1:i2].mean()) if i2 > i1 else 0.0
        self.treb = float(bands[i2:].mean())   if i2 < n  else 0.0

    @property
    def waveform(self):
        """Raw time-domain samples. Not yet plumbed — extending BandData
        to include raw PCM is on the Phase 4 step C roadmap. Accessing
        this property surfaces a clear error so preset authors notice
        the gap rather than silently get junk."""
        raise AttributeError(
            "audio.waveform is not available yet — extend BandData to "
            "include raw PCM samples to enable this.")


# ── Loading the user's preset module ─────────────────────────────────────────


def _load_preset_class(path: str) -> Type[Preset]:
    """Import a preset .py and return its single Preset subclass."""
    spec = importlib.util.spec_from_file_location("scintilla_user_preset", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Could not import preset at {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    for value in vars(module).values():
        if isinstance(value, type) and issubclass(value, Preset) and value is not Preset:
            return value
    raise RuntimeError(f"No Preset subclass found in {path}")


# ── Wire-protocol helpers ────────────────────────────────────────────────────


def _write(msg: dict) -> None:
    sys.stdout.write(json.dumps(msg, separators=(",", ":")) + "\n")
    sys.stdout.flush()


def _build_cube(load_msg: dict) -> CubeProxy:
    # Header schema groups cube data under a "cube" subfield. Accept either
    # the nested form or a flat shape (legacy from Step A smoke tests).
    cube_data = load_msg.get("cube") or load_msg
    grid_size = int(cube_data.get("grid_size", 8))
    shape     = str(cube_data.get("shape", "cube"))
    coords    = cube_data.get("positions") or []
    positions = np.asarray(coords, dtype=np.float32).reshape(-1, 3) if coords else None

    if positions is None or positions.size == 0:
        # Fallback for testing: full N³ cube grid centred on the origin.
        # Real runs will receive the actual shape mask via "positions".
        n = grid_size
        offset = -(n - 1) / 2.0
        x, y, z = np.meshgrid(
            np.arange(n, dtype=np.float32) + offset,
            np.arange(n, dtype=np.float32) + offset,
            np.arange(n, dtype=np.float32) + offset,
            indexing="ij",
        )
        positions = np.stack([x, y, z], axis=-1).reshape(-1, 3)

    return CubeProxy(grid_size, shape, positions)


# ── Main message loop ────────────────────────────────────────────────────────


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        _write({"type": "error", "message": "usage: led_cube._runner <preset.py>"})
        return 2

    preset_path = argv[1]

    try:
        preset_cls = _load_preset_class(preset_path)
    except Exception:
        _write({"type": "error", "message": f"load failed: {traceback.format_exc()}"})
        return 1

    cube:       CubeProxy | None = None
    preset:     Preset    | None = None
    start_time: float     | None = None

    for line in sys.stdin:
        try:
            msg = json.loads(line)
        except json.JSONDecodeError as exc:
            _write({"type": "error", "message": f"bad JSON: {exc}"})
            continue

        mtype = msg.get("type", "")

        if mtype == "load":
            try:
                cube       = _build_cube(msg)
                preset     = preset_cls()
                preset.on_load(cube)
                start_time = time.monotonic()
                _write({"type": "ready",
                        "name": getattr(preset_cls, "name", "Preset")})
            except Exception:
                _write({"type":    "error",
                        "message": f"on_load failed: {traceback.format_exc()}",
                        "line":    -1})

        elif mtype == "frame":
            if cube is None or preset is None or start_time is None:
                _write({"type": "error", "message": "frame received before load", "line": -1})
                continue
            try:
                audio_msg = msg.get("audio", {})
                bands     = np.asarray(audio_msg.get("bands", []), dtype=np.float32)
                rms       = float(audio_msg.get("rms",      0.0))
                centroid  = float(audio_msg.get("centroid", 0.0))
                beat      = bool(audio_msg.get("beat",      False))
                t         = time.monotonic() - start_time
                audio     = AudioFrame(bands, rms, centroid, beat, t)

                cube._begin_frame()
                if beat:
                    preset.on_beat(cube, audio)
                preset.on_frame(cube, audio)

                _write({"type": "frame", "voxels": cube._serialise()})
            except Exception:
                _write({"type":    "error",
                        "message": f"on_frame failed: {traceback.format_exc()}",
                        "line":    -1})

        elif mtype == "unload":
            return 0

        else:
            _write({"type": "error",
                    "message": f"unknown message type: {mtype}",
                    "line":    -1})

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
