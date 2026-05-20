# D-001 — Architectural Decisions Log

> **Status**: Active
> **Provenance**: Shane Hartley · Claude
> **Last reviewed**: 2026-05-19

Binding decisions for Scintilla. All decisions here are closed unless explicitly reopened.
New decisions append to this file with the next available ID.

---

## DEC-001 · Instanced mesh rendering only

**Date**: 2026-05-19
**Status**: Closed

**Decision**: All LED rendering uses a single `InstancedMesh` (or equivalent) per mesh type.
Never one scene object per LED.

**Rationale**: At 32³ = 32,768 LEDs, individual scene objects would each have their own
draw call. Even at 8³ = 512 LEDs, the overhead is measurable. Instanced rendering collapses
all LEDs of a given type to one GPU draw call regardless of count.

**Consequence**: Hiding an LED is done by setting its instance matrix scale to (0,0,0), not
by removing it from the instance array. The instance array is rebuilt only when the grid size
or shape changes, not per-frame.

---

## DEC-002 · Spherical LED geometry

**Date**: 2026-05-19
**Status**: Closed

**Decision**: LEDs are rendered as spheres (SphereGeometry r=0.38, 9 lon × 7 lat segments).
Ghost LEDs use r=0.17.

**Rationale**: Spheres read as LEDs. Box voxels read as Minecraft. The original hardware used
5mm dome LEDs. Radius 0.38 with unit spacing (LEDs at integer coordinates) gives a small
visible gap between adjacent LEDs at all grid sizes.

**Consequence**: At very large grids (32³) the segments could be reduced to 6×5 for performance.
This is a rendering hint, not a binding decision.

---

## DEC-003 · Sparse JSON voxel encoding

**Date**: 2026-05-19
**Status**: Closed

**Decision**: Voxel data is stored as a sparse dictionary `{"x,y,z": [r,g,b]}`. Off LEDs are
absent from the dict.

**Rationale**: Dense encoding of an 8³ grid would be 512 entries regardless of how many are
lit. Sparse encoding scales with content, keeps files human-readable, and makes the Python
scripting API natural (`cube.set(x,y,z,r,g,b)` adds a key; `cube.clear()` empties the dict).

**Consequence**: The file format is stable at v1.0. Any parser must treat absent keys as off.
Do not change to dense encoding without a version bump and migration path.

---

## DEC-004 · Slice filter is view-only, not destructive

**Date**: 2026-05-19
**Status**: Closed

**Decision**: Setting a slice filter (sliceX/Y/Z) hides LEDs from the viewport and prevents
painting outside the active slice. It does not delete data from hidden layers.

**Rationale**: Slice view is a navigation aid. Deleting data on slice change would be
catastrophically surprising. Hiding is always reversible (set slice back to −1).

**Consequence**: The slice state must be stored separately from frame data. It is a view
preference, not part of the saved JSON format.

---

## DEC-005 · Grid size cap at 32³

**Date**: 2026-05-19
**Status**: Closed

**Decision**: Maximum grid size is 32³ (32,768 LEDs). 64³ is explicitly not supported.

**Rationale**: 64³ = 262,144 LEDs. Transparent depth-sorted rendering at that density is
intractable at interactive frame rates without a purpose-built order-independent transparency
renderer. Visually, individual LEDs at 64³ are sub-pixel at any reasonable window size.
32³ covers every practical artistic and algorithmic use case.

**Consequence**: The UI size slider is capped at 32. A performance warning is shown for
grids larger than 24³. If a user loads a JSON file with gridSize > 32, clamp to 32 and warn.

---

## DEC-006 · Shape change clears animation data

**Date**: 2026-05-19
**Status**: Closed

**Decision**: Changing the shape type resets all frame voxel data to empty.

**Rationale**: Voxel coordinates that exist in a Cube mask may not exist in a Sphere mask.
Carrying data across a shape change would silently discard out-of-mask LEDs, which is
confusing. A clean reset is predictable.

**Consequence**: The UI must warn the user before clearing if any frames have content.
The confirmation dialog is: "Changing shape will clear all animation data. Continue?"

---

## DEC-007 · Camera uses spherical coordinates

**Date**: 2026-05-19
**Status**: Closed

**Decision**: Camera state is stored as (theta, phi, radius, target) in spherical coordinates.
No dependency on Three.js OrbitControls or equivalent library.

**Rationale**: OrbitControls is not available in Three.js r128 via CDN without a separate
import. Manual implementation is ~20 lines and gives full control over damping, limits, and
behaviour. In a Qt6 native port, the same mathematical model translates directly to
`glm::lookAt` computation.

**Parameters**:
- theta: azimuth angle (radians), unbounded, default π/4
- phi: polar angle (radians), clamped [0.05, π−0.05], default π/3.1
- radius: distance from target, clamped [2, 200], default gridSize × 1.95
- target: world-space Vector3, default (0,0,0)

---

## DEC-008 · Platform target

**Date**: 2026-05-19
**Status**: Closed

**Decision**: Qt6/C++20 native (Option B). QOpenGLWidget viewport, instanced GL rendering,
Qt Widgets UI, QProcess for the future Python script bridge.

**Rationale**: Consistent with ManifeST, Tux-TI83, and the Atari ST engine. Gives full
control over the renderer and GL context, native file associations, and a natural home for
the Python subprocess API via QProcess. No WebView rendering-parity risk.

**Consequences**:
- Renderer is a `CubeViewport : QOpenGLWidget` using `QOpenGLFunctions_4_3_Core`
- OpenGL 4.3 Core profile; MSAA x4; set via `QSurfaceFormat::setDefaultFormat` before `QApplication`
- Shaders compiled from Qt resources (:/shaders/*)
- Instance buffer: two VBOs (on-LEDs and ghost-LEDs), stride 32 bytes (LedInstance struct)
- Build: CMake 3.22+, Ninja, Qt 6.2+
- The prototype `prototype/index.html` remains the canonical renderer reference
