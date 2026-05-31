# Writing your own Scintilla script

A Scintilla script is a Python file that drives the cube's LEDs. The
runtime handles audio capture, FFT, beat detection, subprocess launch,
and rendering — your code just decides what colour each LED should be.

Two kinds of scripts:

- **Preset** — reactive. Receives live audio bands every frame and
  paints in response. Best for visualisers that react to music.
- **Animation** — run-once. Your `run()` method runs to completion,
  emitting frames each time you call `cube.frame()`; finalised with
  `cube.play(fps)`. Best for programmatic animations that don't need
  audio.

The runtime detects which type from the base class the script
inherits, and loading the wrong type via the wrong menu item is
caught up front with a clear error. This document walks through both,
the shared `cube` API, common patterns for each, and debugging tips.

If you just want a starting point, copy one of the templates in
[user/reactive/](user/reactive/) or [user/animations/](user/animations/)
to a new file alongside it and edit. The rest of this document explains
what's in there and why.

---

## Quick start (reactive preset)

A working reactive preset is roughly twenty lines:

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

## Declaring a preferred cube (optional)

A script can declare the cube it expects to run on by setting two
optional class attributes:

```python
class MyAnimation(Animation):
    name        = "My animation"
    grid_size   = 24
    shape       = "cube"

    def run(self, cube):
        ...
```

When Scintilla loads the script, it reads `grid_size` and `shape`. If
either differs from the active cube, a small dialog appears: *"My
animation declares a target cube (24³ cube) that differs from the
current cube (8³ sphere). Apply the script's requirements? This will
clear the current animation."* — Apply switches the cube; Cancel
aborts the run. If both already match, no dialog appears.

Recognised `shape` values: `"cube"`, `"sphere"`, `"cylinder"`,
`"pyramid"`, `"torus"`, `"ring"`, `"cross"`, `"custom"`. Unknown
shape names are silently ignored with a status-bar warning.

Both attributes are optional — leave them off if your script
auto-scales to any cube size (like `anim_lorenz.py` does).

**File → New animation script…** pre-fills these attributes with
the cube you're currently working with, so a fresh script starts
matched to the active cube and the dialog stays quiet on the first
run.

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

## Common preset patterns

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

## Animation scripts

While a Preset paints one frame in response to audio, an Animation
runs once and explicitly emits a fixed sequence of frames. No audio
is involved — the script controls its own timing and length.

### Quick start

A working animation is also about twenty lines:

```python
from led_cube import Animation
import numpy as np

class MyAnimation(Animation):
    name = "My animation"

    def run(self, cube):
        gc = cube.grid_coords()
        for t in range(60):
            hue = (gc[:, 0] * (360 / max(1, cube.size - 1)) + t * 12) % 360
            cube.set_hsv(hue, 1.0, 1.0)
            cube.frame()
        cube.play(fps=24)
```

Save it as `presets/user/animations/my_animation.py` and click **Run**
in the Preset editor (or use **File → Run animation script…**). The
timeline fills with 60 frames; FPS is set to whatever `cube.play(fps)`
requested.

### The Animation lifecycle

Just one method:

```python
def run(self, cube): ...
```

Called once when the script is loaded. Inside, you typically:

1. Pre-compute any geometry or trajectory data once at the top.
2. Loop over frame indices, painting + `cube.frame()` per iteration.
3. Finalise with `cube.play(fps)` once at the end.

There's no `on_load` equivalent — initialise state inside `run()` itself
before the main loop. There's no per-frame callback either — the loop
is yours to write, so you control the number of frames directly.

### Emitting frames

Two methods unique to Animation:

| Method            | Effect                                                                                  |
|-------------------|-----------------------------------------------------------------------------------------|
| `cube.frame()`    | Commits the current cube state as one animation frame. The write buffer auto-clears for the next iteration unless you called `cube.retain()` first. |
| `cube.play(fps)`  | Finalises the animation and tells the host to play it at the given FPS (1–60). Call this once after the loop; the script normally returns shortly after. |

The shared `cube` API (`set`, `set_hsv`, `fill`, `clear`, `fade`,
`retain`) all behave the same as for Preset above. The geometry getters
(`positions()`, `grid_coords()`, `distances_from_centre()`, `angles()`,
`centre`) also behave the same — pre-compute them once at the top of
`run()` rather than every iteration.

### Common animation patterns

#### Parametric curve

A single voxel traces a mathematical path through the cube. Lissajous
figures, helices, hypocycloids — anything you can express as
`(x(t), y(t), z(t))`.

```python
def run(self, cube):
    n = cube.size
    cx, cy, cz = (n - 1) * 0.5, (n - 1) * 0.5, (n - 1) * 0.5
    r = n * 0.4
    for t in range(180):
        a = t * np.pi / 30
        x = int(round(cx + np.cos(a) * r))
        y = int(round(cy + np.sin(2 * a) * r * 0.7))
        z = int(round(cz + np.sin(3 * a) * r * 0.7))
        cube.fade(0.9)               # fade existing trail
        cube.set(x, y, z, 255, 0, 0) # paint new head
        cube.retain()                # keep the trail for next iteration
        cube.frame()
    cube.play(fps=24)
```

#### Pre-computed trajectory + auto-scale

For chaotic systems (Lorenz, Rössler) the bounding box isn't known
ahead of time. Generate the full trajectory first, scale it into the
cube with a margin, then walk through. `anim_lorenz.py` uses exactly
this pattern.

```python
def run(self, cube):
    # 1. Generate trajectory.
    N = 1000
    traj = np.zeros((N, 3), dtype=np.float32)
    x, y, z = 0.1, 0.0, 0.0
    sigma, rho, beta = 10.0, 28.0, 8.0 / 3.0
    dt = 0.012
    for i in range(N):
        x += sigma * (y - x) * dt
        y += (x * (rho - z) - y) * dt
        z += (x * y - beta * z) * dt
        traj[i] = (x, y, z)

    # 2. Auto-scale to fit the cube with a 10 % margin.
    n = cube.size
    mn, mx = traj.min(axis=0), traj.max(axis=0)
    scale = (n - 1) * 0.9 / max(1e-6, (mx - mn).max())
    centre = (mn + mx) * 0.5
    cube_centre = np.full(3, (n - 1) * 0.5, dtype=np.float32)
    pts = np.clip(((traj - centre) * scale + cube_centre)
                  .round().astype(int), 0, n - 1)

    # 3. Render one frame per trajectory point.
    for px, py, pz in pts:
        cube.fade(0.92)
        cube.set(int(px), int(py), int(pz), 0, 255, 0)
        cube.retain()
        cube.frame()
    cube.play(fps=30)
```

#### Time-driven field

Treat each frame as a snapshot of a function of position and time.
Identical to the Preset position-driven pattern but with your own
loop index instead of `audio.time`.

```python
def run(self, cube):
    pos = cube.positions()
    r   = np.linalg.norm(pos - cube.centre, axis=1)
    for t_frame in range(120):
        t     = t_frame * 0.1
        field = np.sin(r * 0.6 + t)
        cube.set_hsv(180, 1.0, np.clip(0.5 + field * 0.5, 0, 1))
        cube.frame()
    cube.play(fps=24)
```

#### Trail with retain()

`cube.frame()` clears the buffer for the next iteration unless you
call `cube.retain()` before it. For trail effects: fade existing
buffer, paint new contributions on top, retain, frame.

```python
def run(self, cube):
    for t in range(120):
        cube.fade(0.85)              # fade what's already drawn
        cube.set(t % cube.size, 0, 0, 255, 0, 0)
        cube.retain()                # keep buffer through cube.frame()
        cube.frame()
    cube.play(fps=30)
```

### How long is an animation?

Whatever your loop says. There's no built-in cap; a script that
emits 10 000 frames at `fps=60` produces a 167-second animation. In
practice the timeline soft cap is high enough to handle anything
visually reasonable — but each frame is one entry in the JSON save
file, so keep that in mind for project size.

---

## Performance

- For **presets**: the audio loop runs at ~60 Hz, so aim for
  `on_frame` under ~10 ms on a `cube.size = 24` (≈ 13 800 LEDs).
  Anything slower will start dropping frames.
- For **animations**: the script runs once, so per-iteration speed
  matters less — but a Python loop that takes 100 ms × 1000 frames
  is still a 100-second wait before the timeline fills.
- Prefer vectorised numpy operations over Python loops. A loop over
  `cube.count` will be the bottleneck before anything else.
- Pre-compute geometry once (in `on_load` for presets, at the top of
  `run()` for animations). Anything that only depends on cube
  positions (distances, angles, projections) only needs to be
  computed once.
- Use `cube.set_hsv` with arrays instead of looping `cube.set` per
  LED.

---

## Hot-reload, errors, and debugging

- **Hot-reload (presets only).** Saving a reactive preset (Ctrl+S in
  the in-app editor, or externally) restarts the Python subprocess
  and reloads. State in `on_load` is rebuilt; persistent globals are
  lost. Animation scripts don't hot-reload — save the file, then
  click **Run** in the editor to re-execute.
- **Errors.** Exceptions inside `on_load` / `on_beat` / `on_frame`
  (presets) or `run()` (animations) are caught by the runner and
  surfaced as an error dialog with the traceback. For presets the
  subprocess keeps running and accepts the next hot-reload; for
  animations the run aborts and you fix-and-Run again.
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
