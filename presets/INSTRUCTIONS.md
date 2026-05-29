# Writing your own Scintilla preset

A preset is a Python file that produces one frame of voxel colours per
audio frame, driven by the cube's audio analysis (bands, RMS, centroid,
beat detection). The runtime takes care of audio capture, FFT, beat
onset detection, and rendering — your code just decides what colour
each LED should be.

This document walks through writing a new preset from scratch, the API
your code talks to, common patterns the built-in presets use, and how
to debug a misbehaving preset.

If you just want a starting point, copy one of the templates in
[user/reactive/](user/reactive/) or [user/animations/](user/animations/)
to a new file alongside it and edit. The rest of this document explains
what's in there and why.

---

## Quick start

A working preset is roughly twenty lines:

```python
from led_cube import Preset
import numpy as np

class MyPreset(Preset):
    name        = "My preset"
    description = "Whole-cube bass pulse with a slowly rotating hue."
    author      = "you"
    tags        = ["bass", "ambient"]

    def on_load(self, cube):
        self.hue = 0.0

    def on_frame(self, cube, audio):
        self.hue = (self.hue + 0.4) % 360
        cube.set_hsv(self.hue, 1.0, audio.bass)
```

Save it as `presets/user/reactive/my_preset.py`, open it via the Audio
reactive panel's **Load preset…** button (with reactive mode set to
**Python preset**), and Scintilla will start streaming live audio into
it. Save your edits in the in-app editor and the preset hot-reloads.

---

## File layout

```
presets/
├── builtin/
│   ├── reactive/        20 shipped audio-reactive presets
│   └── animations/      shipped run-once animation scripts
├── user/
│   ├── reactive/        drop your own reactive presets here
│   │   └── _template.py
│   └── animations/      drop your own animation scripts here
│       └── _animation_template.py
└── led_cube/            runtime (do not edit)
```

Files starting with `_` are excluded from the preset browser (so the
templates don't show up as runnable entries).

A reactive **Preset** file must:

- import `Preset` from `led_cube`
- define exactly one class that subclasses `Preset`
- (optionally) set class attributes `name`, `description`, `author`, `tags`

A run-once **Animation** file must:

- import `Animation` from `led_cube`
- define exactly one class that subclasses `Animation`
- implement `run(self, cube)` — emit frames via `cube.frame()`, then
  call `cube.play(fps)` once at the end

The runtime imports the file, finds whichever subclass it can, and
runs the matching protocol. Anything else in the file (helper
functions, module constants, other classes) is fine and ignored.

---

## The lifecycle

Your class implements up to three callbacks. All three are optional;
the base class provides no-op defaults.

```python
def on_load(self, cube): ...
def on_beat(self, cube, audio): ...
def on_frame(self, cube, audio): ...
```

**`on_load(cube)`** runs exactly once after the cube proxy is built —
when the preset is first loaded, and again every time the cube's grid
size or shape changes. Initialise persistent state (counters, buffers,
position caches) here.

**`on_beat(cube, audio)`** runs once per detected onset, immediately
before the matching `on_frame`. Use it for impulse-style events:
spawning new particles, picking a fresh hue, kicking a velocity.

**`on_frame(cube, audio)`** runs every audio frame (~60 Hz in live
mode). This is where you actually paint the cube. The runner clears
the cube's write buffer before calling you unless the previous frame
called `cube.retain()`.

---

## The `audio` object

`audio` is passed to `on_frame` and `on_beat`. It exposes the live
audio analysis the engine has performed since the last frame:

| Attribute        | Type    | Range  | Meaning                                          |
|------------------|---------|--------|--------------------------------------------------|
| `audio.bands`    | ndarray | 0..1   | Per-band magnitude, length = `cube.size`         |
| `audio.spectrum` | ndarray | 0..1   | Alias for `audio.bands`                          |
| `audio.bass`    | float   | 0..1   | Mean of the lowest 25 % of bands                 |
| `audio.mid`      | float   | 0..1   | Mean of the middle 50 % of bands                 |
| `audio.treb`     | float   | 0..1   | Mean of the highest 25 % of bands                |
| `audio.vol`      | float   | 0..1   | RMS amplitude (the "loudness")                   |
| `audio.centroid` | float   | 0..1   | Spectral centroid; 0 = bassy, 1 = bright/treble  |
| `audio.beat`     | bool    | —      | True for exactly one frame on each detected onset |
| `audio.time`     | float   | sec    | Seconds since the preset was loaded              |

`audio.waveform` is reserved for raw PCM samples; accessing it raises
`AttributeError` until that data is plumbed through the bridge.

`audio.bands` is the most flexible source — `bass`/`mid`/`treb` are
just convenience reductions over slices of it.

---

## The `cube` object

`cube` is your interface to the LED array. It carries geometry and a
write buffer for the current frame.

### Geometry (read-only)

| Member                              | Returns        | Meaning                              |
|-------------------------------------|----------------|--------------------------------------|
| `cube.size`                         | int            | grid size (3–32)                     |
| `cube.shape`                        | str            | `"cube"`, `"sphere"`, `"cylinder"`, `"pyramid"` |
| `cube.count`                        | int            | number of active LEDs                |
| `cube.centre`                       | ndarray (3,)   | world-space cube centre              |
| `cube.positions()`                  | ndarray (N, 3) | world-space LED positions            |
| `cube.grid_coords()`                | ndarray (N, 3) | integer grid coordinates             |
| `cube.distances_from_centre()`      | ndarray (N,)   | per-LED distance from `cube.centre`  |
| `cube.angles()`                     | ndarray (N, 2) | `(theta, phi)` spherical coordinates |

The arrays are stable for the preset's lifetime — pre-compute anything
position-derived once in `on_load` rather than every frame.

### Writes

| Method                                | Effect                                             |
|---------------------------------------|----------------------------------------------------|
| `cube.set(x, y, z, r, g, b)`          | Set one LED by grid coords                         |
| `cube.set_pos((x, y, z), r, g, b)`    | Set the LED nearest world position                 |
| `cube.set_all(colors)`                | Write a (N, 3) uint8 array                         |
| `cube.set_hsv(h, s, v)`               | Bulk HSV write — each arg scalar or (N,)           |
| `cube.set_mask(mask, r, g, b)`        | Write the LEDs where `mask` (bool, (N,)) is True   |
| `cube.fill(r, g, b)`                  | Whole cube to one colour                           |
| `cube.clear()`                        | Whole cube off                                     |
| `cube.fade(factor)`                   | Multiply all LED brightness by `factor` in [0, 1]  |
| `cube.retain()`                       | Skip the next frame's automatic clear              |

`set_hsv` is the workhorse — most built-in presets compute per-LED hue
and brightness arrays of shape `(cube.count,)` and pass them in one
call. Hue is in **degrees** (0–360), saturation and value in 0–1.

---

## Patterns

### Beat-triggered impulse

Use `on_beat` to set a state variable that decays in `on_frame`.
Strobe and Pulse Sphere both work this way.

```python
def on_beat(self, cube, audio):
    self.brightness = 1.0

def on_frame(self, cube, audio):
    self.brightness *= 0.65        # decay
    cube.set_hsv(120, 1.0, self.brightness)
```

### Trails (fading history)

Keep the colour history in your own brightness array and fade it each
frame. New events bump cells back up to full brightness. This is what
Rain, Fireworks, and Matrix rain all do.

```python
def on_load(self, cube):
    self.val = np.zeros(cube.count, dtype=np.float32)
    self.hue = np.zeros(cube.count, dtype=np.float32)

def on_frame(self, cube, audio):
    self.val *= 0.78               # trail fade
    # ... write new events into self.val / self.hue ...
    cube.set_hsv(self.hue, 1.0, self.val)
```

You can also use `cube.fade(0.78)` to fade the RGB buffer in place,
but maintaining your own `val` array gives you finer control because
you can update hue and brightness independently.

### Position-driven field

Map LED position to a value via vectorised numpy math. Plasma and
Galaxy use this pattern. Pre-compute the position projection in
`on_load` so it costs nothing per frame.

```python
def on_load(self, cube):
    pos = cube.positions()
    self.r = np.linalg.norm(pos - cube.centre, axis=1)

def on_frame(self, cube, audio):
    field = np.sin(self.r * 0.6 + audio.time * 2.0)
    cube.set_hsv(180, 1.0, np.clip(0.5 + field * 0.5, 0, 1))
```

### Frequency-band mapping

Each LED looks up a band magnitude by its X column or radial position.
VU meter and Spectrum waterfall both work this way.

```python
def on_frame(self, cube, audio):
    gc = cube.grid_coords()
    col_height = audio.bands[gc[:, 0]] * (cube.size - 1)
    on = gc[:, 1] <= col_height
    cube.set_hsv(120, 1.0, on.astype(np.float32))
```

### Particle systems

Track each particle as a row in a numpy array (position, velocity,
hue, life). Step the whole array vectorised; render by finding nearest
LEDs.

```python
def on_load(self, cube):
    self.particles = np.zeros((0, 7), dtype=np.float32)   # x,y,z,vx,vy,vz,life

def on_beat(self, cube, audio):
    n = int(8 + audio.bass * 16)
    new = np.zeros((n, 7), dtype=np.float32)
    new[:, 0:3] = cube.size / 2                           # spawn at centre
    new[:, 3:6] = np.random.normal(0, 0.4, (n, 3))        # random velocity
    new[:, 6]   = 1.0                                     # full life
    self.particles = np.vstack([self.particles, new])

def on_frame(self, cube, audio):
    self.particles[:, 0:3] += self.particles[:, 3:6]      # step
    self.particles[:, 6]   *= 0.92                        # life decays
    self.particles = self.particles[self.particles[:, 6] > 0.05]
    # ... render to cube ...
```

---

## Performance

- The audio loop runs at ~60 Hz. Aim for `on_frame` under ~10 ms on
  a `cube.size = 24` (≈ 13 800 LEDs).
- Prefer vectorised numpy operations over Python loops. A loop over
  `cube.count` will be the bottleneck before anything else.
- Pre-compute geometry in `on_load`. Anything that only depends on
  cube positions (distances, angles, projections) only needs to be
  computed once.
- Use `cube.set_hsv` with arrays instead of looping `cube.set` per
  LED.

---

## Hot-reload, errors, and debugging

- **Hot-reload.** Saving the file (Ctrl+S in the in-app editor, or
  saving externally) restarts the Python subprocess and reloads your
  preset. State in `on_load` is rebuilt; persistent globals are lost.
- **Errors.** Exceptions inside `on_load` / `on_beat` / `on_frame`
  are caught by the runner and surfaced as an error dialog with the
  traceback. The subprocess keeps running and will accept the next
  hot-reload.
- **`print()`** writes go to Scintilla's terminal stderr, prefixed
  with `[preset] subprocess stderr:`. Useful for sanity-checking
  values.
- **Single-step.** If the cube goes blank, suspect a NaN/inf in your
  brightness array — `cube.set_hsv` clips to [0, 1] but a NaN
  propagates as zero. Print `np.isnan(val).any()` to verify.

---

## Common pitfalls

- **Loading a frame before `on_load`.** Don't read state in
  `on_frame` that isn't initialised in `on_load`. The runner calls
  `on_load` exactly once before the first `on_frame`.
- **Mutating `cube.positions()`.** The position arrays are shared
  with the runtime — copy them with `.copy()` before mutating.
- **Hue out of range.** `cube.set_hsv` does `% 360`, so negative hues
  are fine, but a `nan` hue becomes 0 (red). Guard against `nan` in
  the value channel especially.
- **`audio.bands` length.** Length equals `cube.size`. If you resize
  the cube while a preset runs, `on_load` re-runs and your buffers
  rebuild — but only if you reinitialise them there.
- **Shape != cube.** Non-cube shapes have `cube.count < cube.size**3`.
  Iterating by `(x, y, z)` in `range(cube.size)` and calling
  `cube.set(x, y, z, …)` silently skips voxels outside the mask.
  Prefer per-LED arrays of length `cube.count`.

---

## Where to look next

- [user/reactive/_template.py](user/reactive/_template.py) — reactive-preset scaffold
- [user/animations/_animation_template.py](user/animations/_animation_template.py) — animation-script scaffold
- [builtin/reactive/](builtin/reactive/) — twenty working presets across six visual categories
- [builtin/animations/](builtin/animations/) — shipped run-once animation scripts
- [led_cube/_cube_proxy.py](led_cube/_cube_proxy.py) — the CubeProxy
  implementation if you want to know exactly what each method does
- [led_cube/_runner.py](led_cube/_runner.py) — wire-protocol runner and
  AudioFrame class definition

Read a built-in preset close to what you want to make and adapt it.
The library is short on purpose.
