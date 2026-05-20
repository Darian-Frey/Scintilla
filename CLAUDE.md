# CLAUDE.md — Scintilla

> **Status**: Active — prototype complete, moving to production build
> **Provenance**: Shane Hartley (owner) · Claude (architect, prototype author)
> **Last reviewed**: 2026-05-19
> **Why this status**: Prototype validated in browser; handing off to Claude Code for desktop build

## What this project is

Scintilla is an interactive 3D voxel animation tool — a spiritual successor to the physical
8×8×8 LED cube hardware project (github.com/Darian-Frey/LED_Cube, 2009). The virtual version
removes all hardware constraints: arbitrary grid sizes (3³–32³), full RGB per voxel, multiple
LED array shapes, a frame-based animation timeline, and a JSON save/load format designed to
serve as the wire protocol for a future Python/Java scripting API.

## Current state

A fully working browser prototype lives in `prototype/index.html`. It uses Three.js r128 with
manual orbit controls (no OrbitControls dependency), instanced mesh rendering, and vanilla JS.
Everything described in `docs/SPEC.md` under "Prototype complete" is implemented and working.

## Target platform

**DECISION REQUIRED before scaffolding**: choose one of:

- **A) Tauri (Rust + WebView)** — port the existing HTML/Three.js frontend into a Tauri 2
  desktop shell. Fast path; keeps the renderer as-is. Consistent with SkinVolt experience.
- **B) Qt6/C++ native** — rewrite renderer in QOpenGLWidget with instanced GL calls. Consistent
  with ManifeST, Tux-TI83, Atari ST engine. More initial work; better long-term extensibility
  for the script engine and file associations.

Shane will confirm target. Do not scaffold `CMakeLists.txt` or `Cargo.toml` until confirmed.

## Architecture overview

```
Scintilla
├── Core data model
│   ├── VoxelFrame          — Map<"x,y,z", [r,g,b]> per frame
│   ├── AnimationTimeline   — Vec<VoxelFrame> + fps + playback state
│   └── ShapeMask           — bool[x][y][z] defining addressable LEDs
│
├── Renderer
│   ├── InstancedLEDMesh    — one draw call for all LEDs (on + ghost)
│   ├── OrbitCamera         — theta/phi/radius spherical coords, manual
│   └── SliceFilter         — per-axis visibility mask applied at render
│
├── Editor
│   ├── Tools               — Paint, Erase, Fill (slice flood), Pick
│   ├── ColorPicker         — RGB sliders + palette + recent history
│   └── FrameTimeline       — thumbnail strip, add/delete/duplicate
│
└── IO
    ├── JSONSerializer      — save/load VoxelFrame[] with shape metadata
    └── ScriptBridge        — [FUTURE] subprocess pipe to Python/Java
```

## Key design decisions (binding)

1. **Instanced mesh only** — never one object per LED. At 24³ = 13,824 LEDs this is non-negotiable.
2. **Spherical LEDs** — SphereGeometry(r=0.38) gives the classic LED dome look. Radius is constant
   regardless of grid size; only camera distance scales.
3. **JSON wire format** — voxels stored as `{"x,y,z": [r,g,b]}` sparse dict. Off-LEDs are absent,
   not stored as null. This keeps files small and makes the scripting API natural.
4. **Shape mask is generative** — never stored per-frame. Changing shape clears animation data
   (acceptable; shapes are set at project creation).
5. **Slice filter is view-only** — slicing X/Y/Z hides LEDs from the viewport but does not delete
   them from the frame data. Paint respects the active slice (can only paint visible LEDs).
6. **Grid size cap at 32³** — 32,768 LEDs max. 64³ (262K) causes depth-sort issues and is visually
   useless. 24³ should be flagged as "large" in UI. See docs/SPEC.md §Performance.

## JSON save format (v1.0)

```json
{
  "version": "1.0",
  "shape": "cube",
  "gridSize": 8,
  "fps": 12,
  "frames": [
    {
      "voxels": {
        "0,0,0": [255, 0, 0],
        "1,0,0": [0, 255, 0]
      }
    }
  ]
}
```

## Feature backlog (priority order)

1. **Stroke painting** — click-drag paints a line of LEDs (currently only click-per-LED)
2. **Undo/redo** — command stack, Ctrl+Z / Ctrl+Shift+Z
3. **Script console** — paste Python, execute against `cube` API, see result as new frame(s)
4. **GIF/video export** — capture frames from renderer, encode via ffmpeg subprocess
5. **Rotation keyframes** — store camera angle per frame for export fly-through
6. **Copy/paste across frames** — select a region, copy, paste into another frame
7. **Mirror tools** — paint with X/Y/Z symmetry

## Shapes implemented

| Shape    | Mask rule                                          |
|----------|----------------------------------------------------|
| Cube     | All positions in grid                              |
| Sphere   | `dx²+dy²+dz² ≤ (center+0.5)²`                    |
| Cylinder | `dx²+dz² ≤ (center+0.5)²` (full Y height)        |
| Pyramid  | Half-width at each Y level scales linearly 1→0    |

Planned additions: Torus, Ring (hollow cylinder), Cross, custom imported meshes.

## Python scripting API (planned interface)

```python
# Thin stdlib — subprocess feeds JSON frames back to the host app
cube.size        # gridSize
cube.shape       # shape name
cube.set(x, y, z, r, g, b)   # set single LED
cube.get(x, y, z)             # returns [r,g,b] or None
cube.fill(r, g, b)            # fill all LEDs in mask
cube.clear()                  # clear all LEDs
cube.frame()                  # commit current state as a new frame
cube.play(fps=12)             # finalise animation
```

## Repository layout (target)

```
scintilla/
├── CLAUDE.md               ← this file
├── README.md
├── docs/
│   ├── SPEC.md             ← feature spec and architecture detail
│   ├── D-001-decisions.md  ← binding architectural decisions log
│   └── D-002-backlog.md    ← feature backlog with priority/status
├── prototype/
│   └── index.html          ← working browser prototype (reference)
├── src/                    ← production source (scaffolded after platform decision)
└── scripts/                ← Python scripting API examples
```

## Dev environment

- Machine: ThinkPad P15 Gen 2i, Linux (Ubuntu)
- IDE: Antigravity + Claude Code
- Preferred C++ standard: C++20
- Preferred build: CMake with Ninja
- Preferred Qt: Qt6

## Notes for Claude Code

- Shane uses the project-scaffold documentation standard. New docs get IDs in the D-NNN series.
- Binding decisions go in `docs/D-001-decisions.md` with the date and rationale.
- Do not refactor the prototype renderer logic without confirming — it is the reference implementation.
- The ghost LED opacity (0.22) and LED radius (0.38) were tuned visually; treat as constants.
- `sliceX/Y/Z = -1` means "show all"; this convention must be preserved in any port.
