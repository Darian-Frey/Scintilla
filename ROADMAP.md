# Roadmap

> Phased development plan. Phases are append-only — mark Complete with an ISO date, do not delete.
> Each phase lists the features (`F-NNN`) it delivers. Detailed file-level work items live in `HANDOVER.md`.

---

## Phase 0 — Scaffolding and decisions (Complete)

**Goal:** Architecture decided, prototype validated, starter files in tree.
**Status:** Complete (2026-05-20)
**Features delivered:** none (foundation only)
**Deliverables:**
- [x] Browser prototype (`prototype/index.html`) — canonical renderer reference
- [x] Binding decisions DEC-001…DEC-023 across `docs/D-00N-*.md`
- [x] Qt6/C++20 source tree scaffolded
- [x] Documentation standard adopted (`development_documentation.md`); Tier 1 + Tier 2 + ATTACK_VECTORS in place

**Acceptance:** All architectural decisions closed; no production code compiled yet.

---

## Phase 1 — Core renderer (Complete)

**Goal:** App window opens, ghost cube visible, orbit camera works.
**Status:** Complete (2026-05-20, commit `f509844`)
**Features delivered:** F-001 (in part), F-002 (in part), F-003, F-013, F-015
**Deliverables:**
- [x] `src/core/AnimationTimeline.cpp`
- [x] `src/renderer/OrbitCamera.h/.cpp`
- [x] `src/renderer/LedInstanceBuffer.h/.cpp`
- [x] `src/renderer/CubeViewport.cpp` — instanced LED rendering, picking via depth readback
- [x] `src/MainWindow.h/.cpp` — viewport host

**Acceptance:** CMake build succeeds; app opens; default 8³ cube shape visible; mouse orbits, wheel zooms.

---

## Phase 2 — Editor (Complete)

**Goal:** Painting, timeline, JSON I/O.
**Status:** Complete (2026-05-20, commit `7b23f96`; polish `a0b9c78` 2026-05-21)
**Features delivered:** F-001 (complete), F-002 (complete), F-004, F-005, F-006, F-007, F-008, F-009, F-010, F-011, F-012, F-014, F-042 (corner axis gizmo)
**Deliverables:**
- [x] `src/core/JsonSerializer.cpp`
- [x] `src/ui/ColorPickerWidget.h/.cpp`
- [x] `src/ui/SliceControlWidget.h/.cpp`
- [x] `src/ui/TimelineWidget.h/.cpp`
- [x] Tool dispatch and active-tool state in `MainWindow`

**Acceptance:** Paint LEDs, add frames, play animation, save to JSON, reload it. Met.

---

## Phase 3 — Audio reactive (Complete)

**Goal:** Music plays through Scintilla and LEDs react via the built-in modes.
**Status:** Complete (2026-05-21, commit `11a9685`; polish `2fdc659` + PipeWire fix `43a596e` 2026-05-22)
**Features delivered:** F-016, F-017, F-018, F-019 (blend modes), F-020 (capture-to-timeline). Phase 6 was folded in once the blend/capture work proved trivial relative to Phase 3's scope.
**Deliverables:**
- [x] Vendored `third_party/kissfft/` (kiss_fft.c, BSD-3-Clause)
- [x] `src/audio/AudioDevicePicker.h/.cpp`
- [x] `src/audio/FFTProcessor.cpp`
- [x] `src/audio/AudioWorker.cpp` — PortAudio stream + SPSC ring buffer
- [x] `src/audio/AudioReactiveEngine.cpp` — eight reactive modes (four original + EQ bars, Beat pulse, Waveform, Spectral, Radial EQ, Tunnel, Energy floor)
- [x] Per-stream PipeWire routing fix (BUG-014) so non-default audio inputs actually capture
- [x] CrashHandler (`src/util/CrashHandler.cpp`) for backtrace-on-SIGSEGV

**Acceptance:** Music plays, reactive modes react in real time. PortAudio callback verified clean (no Qt / alloc / mutex — AV-008). Met.

---

## Phase 4 — Preset scripting (Complete)

**Goal:** Load Python preset; edit in-app; hot-reload works.
**Status:** Complete (2026-05-22 to 2026-05-23, commits `fdb0b56` → `9a55f74`)
**Features delivered:** F-021, F-022, F-023, F-025
**Deliverables:**
- [x] **Step A** — `presets/led_cube/` runtime (`Preset` base + `CubeProxy` + `_runner.py` JSON wire protocol)
- [x] **Step B** — `src/scripting/PresetRunner.cpp` (QProcess lifecycle, JSON frame protocol, QFileSystemWatcher) plus File → Run preset… offline playback
- [x] **Step C** — Live `ReactiveMode::PythonPreset` routes the audio engine's `BandData` through the runner; results overlay the viewport and optionally capture to the timeline
- [x] **Step D** — `src/ui/PresetEditorPanel.h/.cpp` + `src/ui/PythonHighlighter.h/.cpp` — in-app Python editor with syntax highlighting, Ctrl+S save, hot-reload

**Acceptance:** Load any built-in preset, music plays, cube reacts; edit `on_beat()` in-app, save, hot-reload fires within one frame. Met.

---

## Phase 5 — Preset library (Complete)

**Goal:** 20-preset built-in library complete.
**Status:** Complete (2026-05-23, commit `23a939d`; docs `6c14a56`; Fire rework `5a60b71`)
**Features delivered:** F-024
**Deliverables:**
- [x] 18 new built-in presets across six categories:
  - Beat-reactive: Ripple, Fireworks, Strobe, Bouncing ball
  - Particle / trails: Rain, Starfield, Matrix rain, Snake
  - Geometric: Helix, DNA, Galaxy, Kaleidoscope
  - Spectrum: VU meter, Spectrum waterfall
  - Ambient / no-beat: Plasma, Breathing, Rainbow cycle
  - Simulation: Fire
- [x] Each preset uses NumPy bulk operations rather than per-LED Python loops
- [x] Per-preset `Edit guide` block in each docstring naming the main tunable knobs
- [x] `presets/INSTRUCTIONS.md` — long-form preset-authoring guide

**Acceptance:** All 20 presets load and run; verified by piping load + frame messages through `led_cube._runner` for every preset. Met.

---

## Phase 6 — Quality-of-life paint tools (Complete)

**Goal:** Stroke painting, undo/redo, mirror tools — friction-removers for content creation.
**Status:** Complete (2026-05-23, commits `af84517` + `53db4f8`)
**Features delivered:** F-026 (stroke painting), F-027 (undo/redo), F-029 (mirror tools), F-028 (region copy/paste — implemented as slice-aware Edit menu Copy/Paste)
**Deliverables:**
- [x] Drag-to-paint: a left-click on a voxel begins a stroke; drag extends it through every voxel touched. Right-mouse drag still orbits as an escape hatch.
- [x] `src/core/VoxelStroke.h` + `src/core/VoxelStrokeCommand.cpp` — QUndoCommand wrapping a stroke; 200-deep undo stack with Edit menu + Ctrl+Z / Ctrl+Shift+Z
- [x] Toolbar Mirror X / Y / Z toggles (up to 8x symmetry); mirrors that land outside the shape mask are silently skipped
- [x] Edit → Copy / Paste with the active slice acting as the selection; merges by default so paste doesn't erase content under it

**Acceptance:** Drag paints a continuous trail; Ctrl+Z reverts a full stroke; mirror toggles produce symmetric output; Copy/Paste round-trips a slice between frames. Met.

---

## Phase 7 — Animation export (Complete)

**Goal:** Render the timeline to a video file via ffmpeg.
**Status:** Complete (2026-05-23, commit `53db4f8`)
**Features delivered:** F-032 (GIF), F-033 (MP4), F-036 (camera keyframes for fly-throughs)
**Deliverables:**
- [x] File → Export animation… — ffmpeg subprocess, raw RGBA stdin, progress dialog, cancel
- [x] Format dispatch by file extension (.mp4 / .gif / .webm); auto-appends extension if user omits it
- [x] GIF uses `palettegen` + `paletteuse` for decent dithered palette
- [x] MP4 uses H.264 CRF 20 + yuv420p; dimensions rounded to even for codec compatibility
- [x] Audio reactive engine paused for the export so its overlay doesn't bleed into captures
- [x] Camera keyframes (View → Set camera keyframe, Ctrl+Shift+K) — shortest-path angular interpolation between sparse keyframes; drives both playback preview and export

**Acceptance:** Export a 60-frame animation to MP4 and GIF; play in an external viewer; camera fly-through visible if keyframes set. Met.

**Not yet delivered:** F-034 (PNG sequence) and F-035 (C array dump for the original hardware project) — out of scope for the v1 export pass.

---

## Phase 8 — User-facing documentation (Complete)

**Goal:** End-user manual covering every program feature.
**Status:** Complete (2026-05-23, commit `31eab37`)
**Deliverables:**
- [x] `USER_MANUAL.md` — interface walkthrough, painting tools, timeline, shapes, camera, audio reactivity, presets, export, JSON format, keyboard shortcuts, troubleshooting
- [x] `presets/INSTRUCTIONS.md` — preset-author guide (already delivered in Phase 5)

**Acceptance:** A new user can produce a saved + exported animation by reading USER_MANUAL.md without other docs. Met.

---

## Future phases (uncommitted)

- **Phase 9 — Extended export** (F-034 PNG sequence, F-035 C-array hardware dump). Both are short follow-ups to Phase 7's export pipeline.
- **Phase 10 — Extended shapes** (F-031 torus, ring, cross; F-039 custom imported meshes). Shape mask system is already pluggable.
- **Phase 11 — Persistence polish** — extend the JSON save format to carry camera keyframes (currently session-only).
- **Phase 12 — Public release** — itch.io / github releases packaging, code signing, README polish, demo video.

**Loose ends in current phases:**
- Fill is not yet undoable (logged as IMP — same pattern as the stroke commands, just hasn't been wrapped).
- WebM container currently uses MP4 encoding args; a true VP9 path would be more correct.
