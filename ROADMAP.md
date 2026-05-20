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

## Phase 1 — Core renderer

**Goal:** App window opens, ghost cube visible, orbit camera works.
**Status:** Not started
**Features delivered:** F-001 (in part), F-002 (in part), F-003, F-013, F-015
**Deliverables:**
- [ ] `src/core/AnimationTimeline.cpp`
- [ ] `src/renderer/OrbitCamera.h/.cpp` (header missing)
- [ ] `src/renderer/LedInstanceBuffer.h/.cpp` (header missing)
- [ ] `src/renderer/CubeViewport.cpp` — primary task; see HANDOVER §Phase 1
- [ ] `src/MainWindow.h/.cpp` — minimal stub that hosts the viewport
**Acceptance:** `cmake -B build -G Ninja && ninja -C build` succeeds; app opens; default 8³ cube shape visible; mouse orbits, wheel zooms.

---

## Phase 2 — Editor

**Goal:** Painting, timeline, JSON I/O.
**Status:** Not started
**Features delivered:** F-001 (complete), F-002 (complete), F-004, F-005, F-006, F-007, F-008, F-009, F-010, F-011, F-012, F-014
**Deliverables:**
- [ ] `src/core/JsonSerializer.cpp`
- [ ] `src/ui/ColorPickerWidget.h/.cpp` (header missing)
- [ ] `src/ui/SliceControlWidget.h/.cpp` (header missing)
- [ ] `src/ui/TimelineWidget.h/.cpp` (header missing)
- [ ] Tool dispatch and active-tool state in `MainWindow`
**Acceptance:** Paint LEDs, add frames, play animation, save to JSON, reload it.

---

## Phase 3 — Audio reactive

**Goal:** Music plays through Scintilla and LEDs react via the four built-in modes.
**Status:** Not started
**Features delivered:** F-016, F-017, F-018
**Deliverables:**
- [ ] Vendor `third_party/kissfft/` (kiss_fft.h + kiss_fft.c, MIT)
- [ ] `src/audio/AudioDevicePicker.h/.cpp` (header missing)
- [ ] `src/audio/FFTProcessor.cpp`
- [ ] `src/audio/AudioWorker.cpp` — PortAudio stream + SPSC ring buffer
- [ ] `src/audio/AudioReactiveEngine.cpp` — four reactive modes
**Acceptance:** Select a monitor source, play music, switch reactive modes, see cube react in real time. PortAudio callback verified clean (no Qt / alloc / mutex — AV-008).

---

## Phase 4 — Preset scripting

**Goal:** Load Python preset; edit in-app; hot-reload works.
**Status:** Not started
**Features delivered:** F-021, F-022, F-023, F-025
**Deliverables:**
- [ ] `presets/led_cube/__init__.py` + `Preset` base class
- [ ] `src/scripting/PresetRunner.cpp` — QProcess lifecycle + JSON frame protocol + QFileSystemWatcher
- [ ] `src/scripting/PresetEditorPanel.h/.cpp` (header missing)
**Acceptance:** Load `presets/builtin/pulse_sphere.py`, music plays, cube pulses on beat; edit `on_beat()` in-app, save, hot-reload fires within one frame.

---

## Phase 5 — Preset library

**Goal:** 20-preset built-in library complete.
**Status:** Not started (2 / 20 present)
**Features delivered:** F-024
**Deliverables:**
- [ ] 18 remaining built-in presets per `docs/D-003-preset-scripting.md` §DEC-021
- [ ] All presets exercise NumPy bulk operations (not per-LED Python loops)
**Acceptance:** All 20 presets load and run at 8³ minimum; preset gallery screenshot captured for release notes.

---

## Future phases (uncommitted)

- **Phase 6 — Mode blending and capture-to-timeline** (F-019, F-020).
- **Phase 7 — Quality-of-life paint tools** (F-026 stroke painting, F-027 undo/redo, F-029 mirror tools).
- **Phase 8 — Export pipeline** (F-032 GIF, F-033 MP4, F-034 PNG sequence, F-035 C array).
- **Phase 9 — Extended shapes and selection** (F-028 region select, F-031 torus/ring, F-039 custom mesh).
- **Phase 10 — Public release** — itch.io packaging, code signing, demo video.
