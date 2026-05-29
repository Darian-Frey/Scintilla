# Scintilla user manual

Scintilla is an interactive 3D voxel animation tool. You sculpt LEDs in
a virtual cube, frame by frame, and play the result back as an
animation — optionally driven by live audio. This manual covers every
feature in the program from a "what do I do?" perspective. If you're
authoring Python presets, see also [presets/INSTRUCTIONS.md](presets/INSTRUCTIONS.md).

---

## Contents

1. [Quick start — your first animation](#quick-start)
2. [The interface](#the-interface)
3. [Painting voxels](#painting-voxels)
4. [Frames and the timeline](#frames-and-the-timeline)
5. [Shapes and grid size](#shapes-and-grid-size)
6. [Camera controls](#camera-controls)
7. [Audio reactivity](#audio-reactivity)
8. [Python presets](#python-presets)
9. [Exporting animations](#exporting-animations)
10. [Saving and loading projects](#saving-and-loading-projects)
11. [Keyboard shortcuts](#keyboard-shortcuts)
12. [Troubleshooting](#troubleshooting)

---

## Quick start

A two-minute first animation:

1. Pick a colour from the **Colour** dock on the right (click in the
   colour preview or the palette).
2. **Left-click** an LED in the cube viewport. It lights up.
3. **Left-click-drag** across other LEDs — the stroke paints all of
   them in one continuous gesture.
4. At the bottom, click **+ Frame** to add a second frame. Paint
   something different.
5. Click **Play**. The cube cycles through your frames.
6. **File → Save As…** to save the project as JSON, or
   **File → Export animation…** to render it to MP4/GIF.

---

## The interface

```
┌─────────────────────────────────────────────────────────────────────┐
│ File  Edit  View  Shape  Audio                                      │   ← menu bar
├─────────────────────────────────────────────────────────────────────┤
│ Paint  Erase  Fill  Pick  │ Mirror X  Mirror Y  Mirror Z            │   ← toolbar
├──────────────────────────────────────────────┬──────────────────────┤
│                                              │ Colour               │
│                                              │ Slice                │
│           3D viewport                        │ Audio reactive       │   ← right docks
│                                              │ Frame info           │
│                                              │                      │
├──────────────────────────────────────────────┴──────────────────────┤
│ [Timeline] [Preset editor]                                          │   ← tabbed bottom dock
│ Play  +Frame  Duplicate  Delete  FPS  Mode                          │
│ [============●===========] Frame 12 / 60 · 1.00s / 5.00s · 14 lit   │
├─────────────────────────────────────────────────────────────────────┤
│ Ready                                        LED size: ───●── 0.095 │   ← status bar
└─────────────────────────────────────────────────────────────────────┘
```

All docks are draggable. Drag a title bar to detach a dock; drag it
back to redock. Docks remember position across sessions.

---

## Painting voxels

### Tools

Four tools live on the toolbar. Click the icon, or press the
shortcut letter.

| Tool   | Shortcut | What it does                                              |
|--------|----------|-----------------------------------------------------------|
| Paint  | `P`      | Click or drag to light LEDs in the current paint colour.  |
| Erase  | `E`      | Click or drag to turn LEDs off.                           |
| Fill   | `F`      | One click fills every LED in the current slice (or all).  |
| Pick   | `K`      | Click an LED to sample its colour into the paint colour.  |

### Stroke painting

With **Paint** or **Erase** active, **left-click-drag** paints every
LED the cursor passes over as one continuous stroke. Single clicks
also work — a single-click is just a stroke of one LED. Each stroke
is one undo step.

If you click in empty space (no LED under the cursor), drag instead
**orbits the camera**. So you don't need to switch tools just to
rotate the view.

**Right-mouse drag** always orbits the camera, regardless of the
active tool. This is the no-modifier escape hatch for moving the view
while painting.

### Mirror tools

Three toolbar toggles — **Mirror X / Y / Z** — duplicate each Paint
or Erase stroke across the cube's midplane on that axis. Combine
toggles for multi-plane symmetry:

- Mirror X alone: 2-way symmetry (left/right)
- Mirror X + Y: 4-way symmetry
- All three: 8-way radial symmetry

Mirror copies that land outside the active shape mask (e.g. on the
flat side of a sphere) are silently skipped. The whole symmetric
group counts as one undo step.

### Slice editing

The **Slice** dock on the right hides everything outside the chosen
X / Y / Z layer so you can paint deep inside the cube without obscuring
LEDs in front. Use the spinners or sliders to pick a slice on each
axis; check **all** to show that axis again. **Show all layers** is a
shortcut to clear all three slices.

When a slice is active:

- Painting only affects LEDs in that slice.
- Copy / Paste affect only that slice (see below).
- Mirror is the exception — mirrored copies are written regardless of
  slice (otherwise symmetry would break while slicing).

### Colour selection

The **Colour** dock combines four ways to pick a colour:

- Hex field — type `#RRGGBB` directly.
- RGB sliders — drag each channel.
- Palette — click any swatch.
- Recent — the last eight colours you used.

The current colour is shown in the preview at the top. The **Pick**
tool also pushes a sampled colour into Recent.

### Fill

With the **Fill** tool active, one click fills every LED in the
current slice with the active paint colour. With no slice active,
Fill paints the whole cube. (Fill is not yet undoable.)

---

## Frames and the timeline

The timeline lives in the tabbed bottom dock.

```
Play  +Frame  Duplicate  Delete   FPS: 12  Mode: Loop
[================●==========================]   Frame 12 / 60 · 1.00s / 5.00s · 14 lit
```

| Control          | Effect                                                   |
|------------------|----------------------------------------------------------|
| **Play / Stop**  | Start / stop animation playback.                         |
| **+ Frame**      | Append an empty frame.                                   |
| **Duplicate**    | Copy the current frame into a new frame after it.        |
| **Delete**       | Remove the current frame (timeline collapses to 1 if empty). |
| **FPS**          | Playback rate, 1–60. Total duration in the readout uses this. |
| **Mode**         | `Play once`, `Loop`, or `Ping-pong`.                     |
| **Scrubber**     | Drag the slider to jump to any frame.                    |
| **Readout**      | Live `Frame N / total · elapsed / total · lit-voxel count`. |

The viewport always shows whichever frame the scrubber is on. Edits
go to that frame.

### Copy / paste

`Ctrl+C` snapshots the current frame's lit voxels to an in-memory
clipboard. `Ctrl+V` pastes them into the current frame, **merging** —
existing voxels that aren't under any clipboard cell stay; cells
under clipboard voxels are overwritten.

If a slice is active, copy and paste both restrict themselves to
that slice — making it easy to copy one X-layer to another frame, or
to a different layer in the same frame.

Paste is a single undo step. The clipboard is session-only — it
clears when you quit.

### Undo / redo

`Ctrl+Z` undoes the last voxel stroke; `Ctrl+Shift+Z` (or `Ctrl+Y`)
redoes it. The stack holds 200 strokes. Each mouse-drag is one
stroke. Each paste is one stroke. Each mirrored stroke is one stroke
regardless of how many voxels it touches.

Adding / deleting / replacing frames clears the undo stack — frame
indices in the saved strokes would otherwise be invalidated.

---

## Shapes and grid size

The **Shape** menu picks the cube's LED mask. Five shapes are built
in:

| Shape    | Mask                                                          |
|----------|---------------------------------------------------------------|
| Cube     | All positions in the grid.                                    |
| Sphere   | LEDs inside the inscribed sphere.                             |
| Cylinder | LEDs inside a cylinder along the Y axis.                      |
| Pyramid  | Square base, half-width shrinks linearly to a point at the top. |

**Shape → Grid size…** sets the cube resolution (3–32 per side, so up
to 32 768 LEDs). Larger grids look more detailed but are heavier on
the GPU. 24³ is flagged as "large" — anything past that and your
GPU's hot.

Changing shape or grid size **clears the animation** (a shape change
is destructive — the voxel coordinates wouldn't map across shapes).
You'll be asked to confirm if you have unsaved content.

---

## Camera controls

### Manual orbit

- **Left-drag in empty space**: orbit (rotate the view around the
  cube centre).
- **Right-drag**: also orbits, regardless of tool.
- **Shift+left-drag** or **middle-mouse drag**: pan (slide the target
  point sideways).
- **Mouse wheel**: zoom in / out.

The **View → Reset camera** menu item (shortcut `R`) returns the camera
to the default angle and distance.

### View toggles

| **View** menu        | Effect                                            |
|----------------------|---------------------------------------------------|
| Show Ghost LEDs      | Render dark "ghost" spheres for unlit LEDs.       |
| Show Bounds          | Wireframe around the active shape's bounding box. |
| Show Axis indicator  | Small XYZ gizmo in the lower-left corner.         |
| Auto-rotate          | Camera spins slowly around the cube.              |
| Reset camera         | Restore default theta / phi / radius (shortcut `R`). |

### Camera keyframes

Use these to author a fly-through that runs during playback or
export.

1. Go to a frame, orbit the camera to the angle you want.
2. **View → Set camera keyframe** (`Ctrl+Shift+K`).
3. Repeat at later frames with different angles.
4. Press Play — the camera interpolates smoothly between your
   keyframes for the duration of the animation.

The interpolation:

- Uses **shortest-path** angular rotation, so a 350° → 10° sweep goes
  the short way.
- Linearly interpolates `phi`, `radius`, and the target point.
- Frames before the first keyframe use that first keyframe; frames
  after the last use that last one.

**View → Clear camera keyframe** removes the keyframe at the current
frame. **Clear all camera keyframes** wipes the lot.

Outside of playback or export the camera is left alone — your manual
orbit isn't fighting the keyframe system. Adding / deleting any
frame clears the keyframe map (frame indices would shift).

Keyframes are **session-only** — they aren't saved into the JSON
project file (yet).

### LED size slider

The bottom-right status bar has a slider that controls the visual
LED radius (0.025–0.500). The default 0.095 was tuned for the
Fresnel-glow renderer; bigger values make the cube look more like
solid pixels, smaller values look more like point sources.

---

## Audio reactivity

The **Audio reactive** dock controls live audio-driven LED behaviour.

### Picking an input device

**Audio → Select input device…** opens a picker listing every input
PortAudio reports. Monitor sources (system audio loopback) are
flagged. The picker shows the running state (RUNNING / IDLE /
SUSPENDED) — SUSPENDED sources will appear silent until the OS routes
audio through them.

You also pick the **sample rate** here — common rates are probed and
only supported ones appear in the list.

### Reactive modes

Set **Reactive** in the panel. Modes:

| Mode             | Behaviour                                                       |
|------------------|-----------------------------------------------------------------|
| Off              | Editor mode only — no audio processing.                         |
| EQ bars          | Frequency bands → vertical columns rising from Y=0.             |
| Beat pulse       | Onset detector triggers an expanding shell from the centre.     |
| Waveform         | Raw waveform → scrolling oscilloscope on a single axis.         |
| Spectral colour  | Spectral centroid → hue; RMS → brightness.                      |
| Radial EQ        | Bands → concentric shells from centre outward.                  |
| Tunnel           | Current spectrum on the far Z slice, past frames recede.        |
| Energy floor     | RMS-driven wall rises from Y=0; X axis tinted by band.          |
| Python preset    | Custom .py file drives the frames (see [Python presets](#python-presets)). |

Selecting any mode other than Off starts audio capture if it isn't
already running.

### Blend

The **Blend** combo controls how the reactive output combines with
the timeline frame:

| Blend     | Effect                                                              |
|-----------|---------------------------------------------------------------------|
| Replace   | Reactive frame replaces the timeline display.                       |
| Additive  | Reactive RGB is added to the timeline frame's RGB (clamped to 255). |
| Modulate  | Reactive brightness scales the timeline frame's brightness.         |

### Tuning sliders

| Slider       | Range  | Effect                                                       |
|--------------|--------|--------------------------------------------------------------|
| Sensitivity  | 0–200% | Multiplier on band magnitudes after auto-gain.               |
| Smoothing    | 0–100  | Auto-gain decay constant. Low = snappy; high = floaty.       |
| Hue shift    | 0–359° | Rotates the palette around the colour wheel.                 |

### Waveform group

Only enabled when **Waveform** mode is active. **Scroll speed** (1–30)
controls how fast the history scrolls — 1 is slow / dreamy, 30 is one
column shift per frame.

### Capture toggle

The **Audio → Capture** menu item appends each reactive frame to the
timeline (with a 500-frame soft cap). Use this to "record" a live
performance into an animation you can play back without audio later.

---

## Python presets

Two ways to run a preset:

### Offline preview

**File → Run preset…** runs a preset over 120 synthesised audio
frames and appends the result to the timeline. Useful for previewing
what a preset looks like without needing live audio.

### Live reactive mode

1. Set **Reactive: Python preset** in the Audio reactive dock.
2. Click **Load preset…** (button appears in the Python preset
   group of the panel) and pick a `.py` file.
3. The preset receives live audio bands and drives the cube.

The path of the loaded preset shows under the button.

### The in-app editor

The **Preset editor** tab in the bottom dock shows the source of the
currently loaded preset, with Python syntax highlighting. Edit the
file, press **Ctrl+S** (or click **Save**), and the preset hot-reloads
without interrupting playback.

A `●` next to the filename means the file has unsaved changes.

### Writing your own

Drop new `.py` files in `presets/user/reactive/` (audio-reactive
presets) or `presets/user/animations/` (run-once animation scripts).
The shipped library lives in the corresponding `presets/builtin/`
subfolders. See [presets/INSTRUCTIONS.md](presets/INSTRUCTIONS.md)
for the full authoring guide.

---

## Exporting animations

**File → Export animation…** renders the timeline to a video file via
an `ffmpeg` subprocess (ffmpeg must be on your system PATH — on
Ubuntu install with `sudo apt install ffmpeg`).

1. Pick an output path. The extension chooses the format:
   - `.mp4` — H.264, CRF 20 (high quality, small files).
   - `.gif` — palette-generated GIF (decent quality, larger files).
   - `.webm` — currently uses the MP4 encoding args inside a WebM
     container.
2. The dialog auto-appends the right extension if you forget.
3. A progress dialog tracks frame N of M. Cancel deletes the
   half-written file.
4. Camera keyframes drive the camera during the export — the rendered
   video matches what you'd see during playback.
5. The audio reactive engine is paused for the export so its overlay
   doesn't bleed into the captured frames.

The exported video uses the timeline's FPS and the viewport's current
pixel size (rounded down to even dimensions for codec compatibility).
For a higher-resolution export, resize the viewport before exporting.

---

## Saving and loading projects

**File → New** (`Ctrl+N`) starts a fresh project. Asks to confirm
discard if your timeline isn't empty.

**File → Open…** (`Ctrl+O`) loads a Scintilla JSON file. The file
carries shape, grid size, FPS, and every frame. Grid sizes > 32 are
clamped on load (binding decision DEC-005).

**File → Save** (`Ctrl+S`) saves to the current path; **Save As…**
(`Ctrl+Shift+S`) prompts for a new one.

### JSON format (v1.0)

```json
{
  "version": "1.0",
  "shape": "cube",
  "gridSize": 8,
  "fps": 12,
  "frames": [
    { "voxels": { "0,0,0": [255, 0, 0], "1,0,0": [0, 255, 0] } }
  ]
}
```

Voxels are sparse — off LEDs are absent from the map. Hand-editing
the JSON is supported (and the same wire format Python presets use).

---

## Keyboard shortcuts

| Shortcut          | Action                                  |
|-------------------|-----------------------------------------|
| `Ctrl+N`          | New project                             |
| `Ctrl+O`          | Open project                            |
| `Ctrl+S`          | Save                                    |
| `Ctrl+Shift+S`    | Save As                                 |
| `Ctrl+Z`          | Undo                                    |
| `Ctrl+Shift+Z`    | Redo (also `Ctrl+Y`)                    |
| `Ctrl+C`          | Copy frame / slice voxels to clipboard  |
| `Ctrl+V`          | Paste clipboard into current frame      |
| `P`               | Switch to Paint tool                    |
| `E`               | Switch to Erase tool                    |
| `F`               | Switch to Fill tool                     |
| `K`               | Switch to Pick tool                     |
| `R`               | Reset camera                            |
| `Ctrl+Shift+K`    | Set camera keyframe at current frame    |
| `Ctrl+Q`          | Quit                                    |

In the Preset editor, **`Ctrl+S`** saves the preset file (triggers a
hot-reload). All other text-edit shortcuts behave normally.

---

## Troubleshooting

### "ffmpeg not found"

Install ffmpeg via your system package manager. On Ubuntu:

```
sudo apt install ffmpeg
```

### "No Python interpreter with numpy was found"

Python presets need a Python 3 with `numpy`. On Ubuntu:

```
sudo apt install python3-numpy
```

If you use conda / miniforge, any of `~/miniconda3/bin/python3`,
`~/anaconda3/bin/python3`, `~/miniforge3/bin/python3`, or
`~/mambaforge/bin/python3` are auto-detected.

### Audio capture silent

- Make sure the source you picked is **RUNNING** or **IDLE** in the
  device picker, not **SUSPENDED**.
- If you're trying to capture system audio output, pick a monitor /
  loopback source (flagged as "Monitor of …" in the picker).
- On PipeWire systems, the per-stream routing fix runs automatically;
  if a specific input isn't capturing, try the **default** entry.

### Preset shows blank cube

Check the terminal for `[preset] error: …` lines. The most common
causes are NaN in a brightness array (guard with `np.nan_to_num`) or
an exception in `on_frame` (the error dialog will show the
traceback).

### Animation playback skips frames

The timeline's FPS may be set higher than your machine can render.
Lower the FPS, or shrink the grid size — 24³ and above can stress
older GPUs.

### Right-side docks crowd the window

Drag any dock's title bar to detach it as a floating window, or
collapse a dock by dragging its title bar onto another dock's title
to tabify them.

---

## Where to find help with the source

The codebase ships with several developer-facing documents:

- [CLAUDE.md](CLAUDE.md) — high-level project context for AI partners.
- [docs/SPEC.md](docs/SPEC.md) — feature spec and architecture detail.
- [DECISIONS.md](DECISIONS.md) — binding architectural decisions log.
- [BUGS.md](BUGS.md) / [IMPROVEMENTS.md](IMPROVEMENTS.md) — open work
  catalogue.
- [presets/INSTRUCTIONS.md](presets/INSTRUCTIONS.md) — Python preset
  authoring guide.
