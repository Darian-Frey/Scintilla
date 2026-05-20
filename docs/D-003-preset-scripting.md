# D-003 — Preset Scripting System

> **Status**: Active
> **Provenance**: Shane Hartley · Claude
> **Last reviewed**: 2026-05-19
> **Why this status**: Feature agreed for v1.1 alongside audio reactivity; decisions made

---

## Overview

The preset scripting system allows users to define how the LED cube reacts to music in code,
inspired by Winamp's AVS and Milkdrop. Each preset is a Python class file. The app ships
~20 built-in presets; users write their own in an in-app editor with live hot-reload.

The system builds on the audio pipeline defined in D-002. `AudioReactiveEngine` feeds an
`AudioSnapshot` to the `PresetRunner` each frame; the runner executes the active preset and
converts its output to a `VoxelFrame` for the viewport.

---

## DEC-016 · Scripting language: Python class files

**Date**: 2026-05-19
**Status**: Closed

**Decision**: Presets are Python `.py` files, each defining a class that inherits from
`led_cube.Preset`. No custom expression language.

**Rationale**: Milkdrop's expression language was powerful but cryptic. Python is familiar
to the target audience (independent researchers, generative artists, demosceners). NumPy
vectorised operations are fast enough for 32³ = 32,768 LEDs at 60fps (< 1ms per frame for
typical patterns). A custom expression language would need a parser, evaluator, debugger,
and documentation — Python gives all of that for free.

**Speed floor**: Per-LED Python loops over 32,768 iterations run in ~8ms on a modern CPU —
marginal at 60fps. Presets that loop in Python should be flagged; the `cube` API provides
vectorised NumPy operations (`set_all`, `set_hsv`, `distances_from_centre`) to avoid this.
At 16³ = 4,096 LEDs, Python loops are fine (~1ms). Shipped presets must use NumPy for
grids ≥ 16³.

**Consequence**: The app bundles a Python interpreter (CPython via embedding, or subprocess
as already planned in CLAUDE.md). NumPy must be available in the Python environment.

---

## DEC-017 · Preset format: single-file Python class

**Date**: 2026-05-19
**Status**: Closed

**Decision**: Each preset is a single `.py` file containing one class inheriting `Preset`.
Metadata (name, description, tags, author) is declared as class attributes.

**File naming**: `snake_case.py`. Class name is `PascalCase`. The loader discovers all `.py`
files in the `presets/` directory and instantiates the first `Preset` subclass found.

**Template**:
```python
from led_cube import Preset, off
import numpy as np

class MyPreset(Preset):
    name        = "My Preset"
    description = "What it does in one sentence."
    author      = "Your name"
    tags        = ["beat", "ambient"]   # for preset browser filtering

    def on_load(self, cube):
        """Called once when preset is selected. Initialise state here."""
        pass

    def on_frame(self, cube, audio):
        """Called every frame (~60fps). Update LEDs based on audio."""
        pass

    def on_beat(self, cube, audio):
        """Optional. Called only on beat onset (audio.beat == True)."""
        pass

    def on_unload(self, cube):
        """Optional. Called when switching away from this preset."""
        pass
```

**Consequence**: Users can share presets as single `.py` files. Preset browser shows
metadata. The `presets/builtin/` directory is read-only; user presets go in `presets/user/`.

---

## DEC-018 · Audio API available to presets

**Date**: 2026-05-19
**Status**: Closed

**Decision**: Presets receive an `AudioSnapshot` object with the following interface.
All values are updated once per frame before `on_frame()` is called.

```python
# Scalar values — all float [0, 1] unless noted
audio.bass       # low-frequency band energy (approx 20–250Hz)
audio.mid        # mid-frequency band energy (approx 250–4000Hz)
audio.treb       # high-frequency band energy (approx 4000–20000Hz)
audio.vol        # RMS amplitude
audio.beat       # bool — onset detected this frame (True for one frame only)
audio.bpm        # estimated tempo (float, Hz, requires ~4 beats to stabilise)
audio.centroid   # spectral centroid normalised [0=bass, 1=treble]

# Arrays (NumPy)
audio.spectrum   # np.float32 array, shape (gridSize,) — log-spaced band magnitudes
audio.waveform   # np.float32 array, shape (1024,) — raw PCM samples, normalised [-1, 1]

# Time
audio.time       # float — seconds elapsed since this preset was loaded
audio.frame      # int   — frame counter since preset loaded
```

**All values are read-only** — presets must not modify the `audio` object.

---

## DEC-019 · Cube API available to presets

**Date**: 2026-05-19
**Status**: Closed

**Decision**: Presets receive a `CubeProxy` object wrapping the current `ShapeMask` and
a write buffer that becomes the next `VoxelFrame`.

```python
# Properties
cube.size              # int — gridSize
cube.mask              # ShapeMask — which positions are addressable
cube.count             # int — number of LEDs in mask
cube.centre            # np.float32 array shape (3,) — world-space centre [cx, cy, cz]

# NumPy bulk accessors (preferred for speed)
cube.positions()       # np.float32 shape (N, 3) — LED world positions
cube.grid_coords()     # np.int32   shape (N, 3) — LED integer grid coordinates
cube.distances_from_centre()  # np.float32 shape (N,) — precomputed Euclidean distance
cube.angles()          # np.float32 shape (N, 2) — (theta, phi) spherical coords

# Single-LED write
cube.set(x, y, z, r, g, b)         # int coords, uint8 RGB
cube.set_pos(pos, r, g, b)         # pos is a (3,) array or tuple

# Bulk write (preferred — avoids per-LED Python overhead)
cube.set_all(colors)               # np.uint8 shape (N, 3) — all LEDs at once
cube.set_hsv(h, s, v)             # each can be float (broadcast) or np.float32 shape (N,)
cube.set_mask(mask, r, g, b)       # np.bool_ shape (N,) — set only masked LEDs

# Convenience
cube.fill(r, g, b)
cube.clear()                       # all off
cube.fade(factor)                  # multiply all current brightness by factor [0, 1]
```

**The write buffer is cleared to black before each `on_frame()` call** unless the preset
calls `cube.retain()` at the end of `on_frame()` to keep the previous frame as the starting
state (useful for trail/decay effects).

---

## DEC-020 · Preset runner: subprocess with hot-reload

**Date**: 2026-05-19
**Status**: Closed

**Decision**: Presets run in a Python subprocess (consistent with the scripting API plan in
CLAUDE.md). The `PresetRunner` class manages the subprocess lifecycle and communicates via
stdin/stdout JSON, reusing the protocol designed for the script bridge.

**Hot-reload**: The in-app editor saves the preset file; `PresetRunner` detects the file
change (QFileSystemWatcher) and restarts the preset subprocess within one frame, calling
`on_load()` on the new version. The old frame remains visible during the restart gap (~50ms).

**Error handling**: If `on_frame()` raises an exception, the error is caught in the subprocess,
serialised as a JSON error message, sent to the host, and displayed in the error console.
The preset is paused (last good frame frozen) until the user edits and saves again. This
means errors never crash the app and the feedback loop is tight.

**Sandboxing**: v1.1 runs with no sandboxing — the user is writing their own code.
v1.2 will add optional sandboxing (restricted builtins, no filesystem access) for sharing
presets with untrusted authors.

---

## DEC-021 · Built-in preset library (20 presets)

**Date**: 2026-05-19
**Status**: Closed

**Decision**: Ship 20 built-in presets in `presets/builtin/`, organised by character.

### Beat-driven (respond primarily to onset and bass)

| Preset | File | Description |
|--------|------|-------------|
| Pulse Sphere | `pulse_sphere.py` | Shell expands from centre on beat, fades to nothing |
| Shockwave | `shockwave.py` | Cube wireframe flares on beat, shrinks back |
| Starburst | `starburst.py` | Rays shoot from centre on beat, fade as they travel |
| Throb | `throb.py` | Whole shape breathes; brightness tracks bass envelope |

### Frequency-mapped (columns, rings, bands)

| Preset | File | Description |
|--------|------|-------------|
| EQ Tower | `eq_tower.py` | Log-spaced bars rising from base; classic equaliser in 3D |
| Spectrum Rings | `spectrum_rings.py` | Concentric shells, each a frequency band |
| Helix | `helix.py` | Double helix wrapping Y axis; frequency drives twist rate |
| Waterfall | `waterfall.py` | Spectrum scrolls down Y over time; history visible in depth |

### Fluid / motion (continuous movement driven by audio)

| Preset | File | Description |
|--------|------|-------------|
| Vortex | `vortex.py` | Rotating colour spiral; bass drives angular velocity |
| Plasma | `plasma.py` | Perlin-noise colour field; centroid shifts hue |
| Aurora | `aurora.py` | Horizontal sine waves drifting in Z; mid drives amplitude |
| Fire | `fire.py` | Upward flame simulation; bass adds fuel, vol controls height |

### Geometric (mathematical forms reacting to tempo)

| Preset | File | Description |
|--------|------|-------------|
| Lissajous | `lissajous.py` | 3D parametric Lissajous with fading trails; freq drives ratios |
| Orbit | `orbit.py` | Point orbiting centre; trail decays; beat changes orbit params |
| Crystal | `crystal.py` | Rotating icosahedral structure; bass drives pulse on faces |
| Kaleidoscope | `kaleidoscope.py` | Triaxial mirror of a single octant; mid drives the source pattern |

### Ambient (slow, mood-based)

| Preset | File | Description |
|--------|------|-------------|
| Deep Space | `deep_space.py` | Sparse stars; beat triggers nova at random position |
| Breath | `breath.py` | Slow sine-wave brightness pulse, rate tracks BPM |
| Sunset | `sunset.py` | Horizontal colour gradient drifting with centroid |
| Drift | `drift.py` | Slow random-walk colour clouds; audio gently disturbs them |

---

## DEC-022 · In-app preset editor UI

**Date**: 2026-05-19
**Status**: Closed

**Decision**: A dockable panel containing a code editor, preset browser, and error console.

**Layout**:
```
┌─────────────────────────────────────────────────┐
│  Preset Browser (filterable by tag)             │
│  [builtin] Pulse Sphere ▶  [user] My Preset     │
├──────────────────────────────────────────────────┤
│  Code editor (syntax highlighted Python)         │
│  Line numbers, bracket matching, Ctrl+S to save  │
│                                                  │
│  def on_frame(self, cube, audio):                │
│      ...                                         │
├──────────────────────────────────────────────────┤
│  Error console                                   │
│  [12:03:44] NameError: name 'hsv' is not defined │
└─────────────────────────────────────────────────┘
```

**Code editor**: QPlainTextEdit with a Python syntax highlighter (QSyntaxHighlighter subclass).
Full IDE-quality editing is out of scope for v1.1 — no autocomplete, no linter. A "Open in
external editor" button opens the file in the system default `.py` editor (xdg-open).
Hot-reload fires on file save whether from the in-app editor or an external editor.

**Preset browser**: QListWidget with a tag filter bar. Built-in presets are shown with a lock
icon (read-only). Double-click to load; single-click to preview name/description.

**New preset workflow**: "New preset" button creates a copy of the template in `presets/user/`,
opens it in the editor, and loads it. User can rename by renaming the file.

---

## DEC-023 · Preset + audio reactive mode interaction

**Date**: 2026-05-19
**Status**: Closed

**Decision**: The preset scripting system *replaces* the simpler built-in reactive modes
(EqBars, BeatPulse, etc.) defined in D-002 for users who want full control. Both coexist:

- **Reactive modes** (D-002): zero-configuration, one dropdown to select, no code.
  Suitable for immediate use, casual users, the itch demo.
- **Preset scripts** (D-003): full control, Python code, hot-reload editor.
  Suitable for power users, generative artists, the itch "advanced" audience.

The UI presents them in a single selector: reactive modes first (labelled "Built-in"),
then user presets, then built-in presets. Switching between them is instant.

**Capture mode** (DEC-014) works identically for both — the current LED frame is recorded
to the timeline regardless of whether it came from a reactive mode or a preset script.

---

## New source files

```
src/scripting/
├── PresetRunner.h/.cpp     — QObject managing Python subprocess + hot-reload
├── CubeProxy.h/.cpp        — Python-facing cube API, converts to/from VoxelFrame
├── AudioSnapshot.h/.cpp    — Serialisable snapshot of BandData for subprocess IPC
└── PresetEditorPanel.h/.cpp— Dockable QWidget: browser + editor + error console

presets/
├── builtin/                — 20 built-in presets (read-only at runtime)
│   ├── pulse_sphere.py
│   ├── eq_tower.py
│   ├── helix.py
│   └── ... (17 more)
├── user/                   — User presets (read/write)
└── _led_cube/              — Python package exposed to all presets
    ├── __init__.py         — re-exports Preset, hsv, off, etc.
    ├── preset.py           — Preset base class
    ├── cube_proxy.py       — CubeProxy Python side (receives JSON, exposes NumPy API)
    └── audio_snapshot.py   — AudioSnapshot Python side
```

---

## CMakeLists.txt additions

No new C++ dependencies. The Python subprocess reuses the mechanism already planned in
CLAUDE.md. `QFileSystemWatcher` is part of Qt Core (already linked).

```cmake
# Copy preset library and built-in presets to build directory
add_custom_target(copy_presets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/presets
        ${CMAKE_BINARY_DIR}/presets
    COMMENT "Copying presets"
)
add_dependencies(Scintilla copy_presets)
```

---

## Example preset: EQ Tower

```python
# presets/builtin/eq_tower.py
from led_cube import Preset
import numpy as np

class EqTower(Preset):
    name        = "EQ Tower"
    description = "Classic equaliser bars rising from base. Log-spaced bands."
    author      = "Built-in"
    tags        = ["frequency", "classic", "bars"]

    # Hue per band — bass red/orange, mids green/cyan, highs blue/violet
    BAND_HUES = np.linspace(0, 270, 32, dtype=np.float32)

    def on_frame(self, cube, audio):
        coords = cube.grid_coords()          # (N, 3) int32
        xs, ys, zs = coords[:,0], coords[:,1], coords[:,2]

        # Map each LED's X to a frequency band
        band_idx = (xs * audio.spectrum.size // cube.size).clip(0, len(audio.spectrum)-1)
        band_mag  = audio.spectrum[band_idx]     # (N,) magnitude for each LED's band

        # LED is lit if its Y position is below the band height
        band_height = (band_mag * cube.size).astype(np.int32)
        lit         = ys < band_height[:]

        # Colour: hue from band index, brightness from magnitude
        hues   = self.BAND_HUES[band_idx]
        bright = np.where(lit, band_mag * (1.0 + audio.vol * 0.3), 0.0)

        cube.set_hsv(hues, np.ones(cube.count), bright)
```

---

## Example preset: Lissajous

```python
# presets/builtin/lissajous.py
from led_cube import Preset
import numpy as np

class Lissajous(Preset):
    name        = "Lissajous"
    description = "3D parametric Lissajous figure with fading trails."
    author      = "Built-in"
    tags        = ["geometric", "trails", "frequency"]

    def on_load(self, cube):
        self.trail = np.zeros((cube.count,), dtype=np.float32)  # brightness decay
        # Frequency ratios driven by bass:mid:treb
        self.a, self.b, self.c = 3.0, 2.0, 1.0

    def on_frame(self, cube, audio):
        # Update frequency ratios slowly from audio
        self.a = 1.0 + audio.bass  * 3.0
        self.b = 1.0 + audio.mid   * 2.0
        self.c = 1.0 + audio.treb  * 1.5

        # Parametric point on the Lissajous curve
        t  = audio.time * 1.5
        cx, cy, cz = cube.centre
        r  = cube.size * 0.45

        px = cx + r * np.sin(self.a * t)
        py = cy + r * np.sin(self.b * t + np.pi / 4)
        pz = cz + r * np.sin(self.c * t + np.pi / 2)

        # Find closest LED to the curve point
        pos  = cube.positions()
        dist = np.linalg.norm(pos - np.array([px, py, pz]), axis=1)
        hit  = dist < 0.8

        # Decay existing trail, add new point
        self.trail *= (0.88 - audio.vol * 0.05)
        self.trail[hit] = 1.0

        hue = (audio.centroid * 240 + audio.time * 20) % 360
        cube.set_hsv(hue, 1.0, self.trail)
        cube.retain()  # don't clear — trails persist frame-to-frame
```
