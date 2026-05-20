# SPEC.md — Scintilla Feature Specification

> **Status**: Active
> **Provenance**: Shane Hartley · Claude
> **Last reviewed**: 2026-05-19

---

## 1. Product overview

Scintilla is a desktop application for designing and animating 3D LED arrays. It replaces the
physical constraint of fixed hardware with a configurable virtual grid. The primary workflow is:

1. Choose a shape and grid size
2. Paint LEDs by hand, one frame at a time (or multiple frames for animation)
3. Play back the animation in the 3D viewport
4. Save to JSON; optionally run a Python/Java script to generate frames programmatically
5. Export to GIF or video for sharing

---

## 2. Prototype complete (v0.1)

The following is fully working in `prototype/index.html`:

| Feature | Notes |
|---------|-------|
| 3D viewport with orbit/pan/zoom | Manual spherical camera; no OrbitControls |
| Four LED array shapes | Cube, Sphere, Cylinder, Pyramid |
| Grid sizes 3³–24³ | Instanced mesh; capped at 24 in prototype, 32 in production |
| Ghost LED overlay | Shows addressable positions; toggleable |
| Bounding box wireframe | Toggleable |
| Auto-rotate | Continuous camTheta increment |
| Paint / Erase / Fill / Pick tools | Fill respects active slice |
| X/Y/Z slice view | Each axis independently; −1 = show all |
| RGB colour picker + sliders | Native `<input type=color>` + R/G/B range inputs |
| Colour palette (18 presets) | |
| Recent colour history (16 slots) | LIFO, deduped |
| Frame timeline | Add, delete, duplicate, click to select |
| Animation playback | Variable FPS, play/stop |
| JSON save | Sparse voxel dict, shape + gridSize metadata |
| JSON load | Full state restore |
| Info panel | LED count, lit count, frame info |

---

## 3. Production requirements (v1.0)

### 3.1 Rendering

- All prototype rendering behaviour preserved
- LED radius: 0.38 units (constant across grid sizes)
- Ghost LED radius: 0.17 units, opacity 0.22
- Bounding box wireframe opacity: 0.11
- Camera: perspective 45°, near 0.1, far 500
- Background: #06060f (near-black blue-tinted)
- Ambient light 0.65 + directional 0.7 from (2,3,1.5) + fill -0.2 from (-2,-1,-2)
- Pixel ratio capped at 2× (retina support without excessive overdraw)

### 3.2 Grid sizes

| Size | LED count | Notes |
|------|-----------|-------|
| 3³  | 27        | Minimum |
| 8³  | 512       | Default |
| 16³ | 4,096     | Medium |
| 24³ | 13,824    | Large — warn user |
| 32³ | 32,768    | Maximum |

Sizes above 24³ should display a performance warning on first use.

### 3.3 Shapes

Shape mask is computed at grid construction time. Changing shape resets all frame data (with
confirmation dialog if any frames have content).

| Shape | Algorithm |
|-------|-----------|
| Cube | All (x,y,z) in [0,n)³ |
| Sphere | `(x−c)²+(y−c)²+(z−c)² ≤ (c+0.5)²` where c=(n−1)/2 |
| Cylinder | `(x−c)²+(z−c)² ≤ (c+0.5)²`, all y |
| Pyramid | `\|x−c\| ≤ hw` and `\|z−c\| ≤ hw` where hw=(1−y/(n−1))·c+0.5 |
| Torus | *(v1.1)* `(sqrt((x−c)²+(z−c)²) − R)² + (y−c)² ≤ r²` |
| Ring | *(v1.1)* Cylinder with hollow core |

### 3.4 Tools

| Tool | Behaviour |
|------|-----------|
| Paint | Click sets LED to current colour. Drag paints stroke. |
| Erase | Click/drag clears LEDs (sets to off). |
| Fill | Click floods all visible LEDs in current slice with current colour. |
| Pick | Click samples LED colour into current colour. |
| Select | *(v1.1)* Drag to select a region; supports copy/paste/delete. |
| Mirror | *(v1.1)* Toggle X/Y/Z symmetry for paint tool. |

### 3.5 Animation timeline

- Minimum 1 frame, no maximum (warn above 500)
- Per-frame duration (default 1 / fps seconds, overridable per-frame in v1.1)
- Playback modes: play-once, loop, ping-pong
- Frame thumbnail shows first lit LED colour as dot indicator
- Drag to reorder frames *(v1.1)*

### 3.6 Colour system

- Current colour: [R, G, B] integers 0–255
- Palette: 18 presets (fixed; user-editable palette in v1.1)
- Recent history: 16 slots, LIFO, deduplicated by exact RGB match
- Hex input field *(v1.0 production)*
- HSV colour mode toggle *(v1.1)*

### 3.7 Slice view

- Three independent sliders: X, Y, Z
- Value −1 = show all; value N = show only layer N
- Slice filter applies to viewport rendering AND tool application
  (cannot paint on a hidden LED)
- Ghost mesh always shows full shape regardless of slice (provides context)

### 3.8 JSON format (v1.0 — stable)

```json
{
  "version": "1.0",
  "shape": "cube",
  "gridSize": 8,
  "fps": 12,
  "frames": [
    {
      "duration": 1.0,
      "voxels": {
        "x,y,z": [r, g, b]
      }
    }
  ]
}
```

- Coordinates are zero-indexed integers as a comma-separated string key
- Off LEDs are absent from the voxels dict (sparse encoding)
- `duration` is in seconds (default 1/fps); added in v1.0 production
- Format is the wire protocol between the app and the Python scripting API

### 3.9 Python scripting API (v1.1)

The app launches a Python subprocess and communicates via stdin/stdout JSON.

```python
import led_cube as cube   # thin stdlib bundled with app

cube.size     # int — gridSize
cube.shape    # str — shape name
cube.mask     # list of (x,y,z) — all addressable positions

cube.set(x, y, z, r, g, b)
cube.get(x, y, z)        # → [r,g,b] or None
cube.fill(r, g, b)       # fill all LEDs in shape mask
cube.clear()
cube.copy_frame(n)       # deep-copy frame n into working buffer
cube.frame()             # commit working buffer as next frame
cube.play(fps=12)        # finalise; app receives full animation
```

Script output arrives as a `{"frames": [...]}` JSON response which is appended to the timeline.

### 3.10 Export (v1.1)

| Format | Method |
|--------|--------|
| JSON | Native (v1.0) |
| GIF | Canvas capture per frame → gif.js or ffmpeg subprocess |
| MP4 | Canvas capture → ffmpeg subprocess |
| PNG sequence | Canvas capture per frame |
| C array | Generate C byte array for hardware flash (original LED cube format) |

---

## 4. Performance targets

| Grid size | Target frame rate |
|-----------|-------------------|
| ≤ 16³    | 60 fps solid |
| 24³       | 30 fps minimum |
| 32³       | 20 fps minimum |

All LED rendering via a single `InstancedMesh` draw call. Scale-to-zero for hidden instances
(off LEDs or slice-filtered) rather than removing from the instance array.

---

## 5. Platform notes

### Option A — Tauri 2 (Rust + WebView)
- Frontend: existing HTML/Three.js prototype, minimal changes
- Backend: Rust handles file I/O, subprocess spawning for script API, native dialogs
- Packaging: single binary, cross-platform
- Risk: WebView rendering parity across platforms; no control over Three.js version in system WebView
  → bundle the renderer via Vite/Rollup

### Option B — Qt6/C++20 native
- Renderer: QOpenGLWidget with instanced GL draw calls (GLSL shaders)
- UI: Qt Widgets (consistent with ManifeST, Tux-TI83 style)
- Script API: QProcess subprocess
- Packaging: standard CMake + CPack
- Risk: more initial work; GLSL shader authoring for the LED glow effect

---

## 6. Out of scope (explicit non-goals)

- Network/cloud sync
- Collaborative editing
- Embedded hardware flashing (future plugin, not core)
- 64³ grid (262K LEDs — depth sort is intractable; hard limit at 32³)
