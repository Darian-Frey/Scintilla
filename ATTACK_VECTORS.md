# Attack Vectors

> Project-specific failure modes Scintilla must be resilient against.
> Grouped by category. Each vector lists detection method and severity.

**Severity:** Critical (must hold) | Major (regression on release blocks) | Minor (track only).

**Detection categories** (per `development_documentation.md` §ATTACK_VECTORS):
- **Implemented automated** — a test, tool invocation, or CI check.
- **Implemented manual** — a documented review procedure.
- **Not implemented** — vector is real but no check yet exists; honest gap.

---

## Rendering correctness

### AV-001 Non-instanced LED rendering

**Severity:** Critical
**Description.** Any code path that creates a separate scene object, draw call, or buffer per LED — instead of one instanced draw per mesh type — will collapse to single-digit FPS at 24³+ and is a foundational regression.
**Detection.** Not implemented (would require a frame-time test fixture at 24³). Manual review during code review on any change to `src/renderer/CubeViewport.cpp` or `src/renderer/LedInstanceBuffer.cpp`.
**Related decisions.** DEC-001.
**Related features.** F-002 (32³ requirement makes this Critical).
**History.** Identified during prototype work; led to the prototype's instanced-mesh design.

### AV-002 LED removed from instance buffer instead of scale=0

**Severity:** Major
**Description.** Hiding an LED by removing it from the instance buffer (rather than setting its scale to zero) forces a full buffer rebuild every time visibility changes. At 60 Hz with many lit-state changes, this stalls the GPU and reorders indices in ways that complicate paint / pick logic.
**Detection.** Not implemented. Code review on `LedInstanceBuffer::updateFrame()` — instance count must equal the mask's true count, never the lit count.
**Related decisions.** DEC-001.
**History.** Anticipated regression; documented before first implementation.

### AV-003 LED radius drift from tuned values

**Severity:** Major
**Description.** The lit-LED sphere radius (**0.095** per DEC-028, originally 0.38 per DEC-002), ghost-LED radius (0.17, DEC-002), and ghost opacity (0.22, DEC-002) were tuned visually and define the LED dome look. Drift produces a visually wrong product even when no test fails.
**Detection.** Manual: visual comparison against the canonical look on any shader or geometry change. The constants live in `src/renderer/shaders/led.vert` (`kRadius`) and `src/renderer/shaders/ghost.vert` (`kGhostRadius`).
**Related decisions.** DEC-002 (original values + geometry choice, partially superseded), DEC-028 (re-tune to 0.095 for the Phase 3 Fresnel-glow renderer).
**History.** Tuned in the prototype at 0.38 / 0.17 / 0.22; lit radius re-tuned to 0.095 in DEC-028 (2026-05-21) after the Fresnel-glow shader landed and the original 0.38 felt visually too large alongside the new soft halo. The `prototype/index.html` no longer matches the production look on this specific value.

### AV-009 OpenGL < 4.3 fallback

**Severity:** Critical
**Description.** The renderer assumes OpenGL 4.3 Core (instanced rendering, modern shader features). A silent fallback to a lower profile would compile but render incorrectly or not at all. A crash is preferable to a wrong render.
**Detection.** Implemented automated. `src/main.cpp` sets `QSurfaceFormat::setVersion(4, 3)` + Core profile before `QApplication`. `CubeViewport::initializeGL()` must additionally check `glGetString(GL_VERSION)` and refuse to continue if the actual context is < 4.3, showing a clear error dialog.
**Related decisions.** DEC-008.
**History.** Decided during platform selection.

---

## Data model

### AV-004 Dense JSON encoding

**Severity:** Major
**Description.** Switching the on-disk format from sparse `{"x,y,z": [r,g,b]}` to dense (e.g. nested arrays or null-padded dicts) breaks the v1.0 wire-format promise and the Python scripting API (which mirrors the JSON shape). Off LEDs must be absent, not null.
**Detection.** Implemented manual. Code review on any change to `src/core/JsonSerializer.cpp`. A round-trip test fixture (save then load a known-sparse animation, compare key-by-key) is planned for Phase 2.
**Related decisions.** DEC-003.
**History.** Format frozen at v1.0; any change requires a version bump and migration path.

### AV-005 Slice filter deletes hidden voxels

**Severity:** Critical
**Description.** Setting an X/Y/Z slice filter must hide LEDs from the viewport without removing them from the underlying `VoxelFrame`. A regression that deletes voxels on slice change loses user work silently and is unrecoverable without an undo system.
**Detection.** Not implemented (would require an integration test that sets a slice, asserts voxel count unchanged, clears the slice, and asserts the original voxels are still present). Manual review on `CubeViewport`, `SliceControlWidget`, and any paint / erase code path that consults the slice.
**Related decisions.** DEC-004.
**Related features.** F-008.
**History.** Documented before first implementation as the "catastrophically surprising" failure mode.

### AV-006 Grid size > 32

**Severity:** Major
**Description.** Loading a JSON file with `gridSize > 32` or constructing a `ShapeMask` with `n > 32` would either crash (buffer overrun) or open the door to the depth-sort intractability that motivated the cap in the first place.
**Detection.** Implemented automated (planned): `ShapeMask` constructor must `assert(n <= 32)` and the JSON loader must clamp with a warning. UI size slider is capped in the widget.
**Related decisions.** DEC-005.
**History.** Cap was set after 64³ was tried in the prototype and found visually useless.

### AV-007 Shape change without confirmation

**Severity:** Major
**Description.** Changing the shape type silently clears voxels that fall outside the new mask. Doing this without confirmation when frames have content destroys user work.
**Detection.** Implemented manual: `MainWindow::rebuildMask()` checks `timeline.hasContent()` before triggering the shape change, raises a confirmation dialog if so.
**Related decisions.** DEC-006.
**Related features.** F-003.
**History.** Decided before first implementation.

---

## Real-time audio

### AV-008 PortAudio callback does Qt / alloc / mutex work

**Severity:** Critical
**Description.** The PortAudio audio callback runs on the audio thread with strict timing requirements. Any Qt call, heap allocation, or mutex acquisition there causes audible glitches and frame drops. Even a "rare" allocation (e.g. inside a debug log path) is unacceptable.
**Detection.** Not implemented (would require running with AddressSanitizer's allocation hook plus a code-review checklist). Manual: anyone touching `AudioWorker::paCallback()` reads DEC-011 first.
**Related decisions.** DEC-011 (in `docs/D-002-audio-reactive.md`).
**Related features.** F-017, F-018.
**History.** Standard real-time audio invariant; well-documented externally. Captured as a vector here because it is the single easiest way to ship a broken audio mode.

### AV-010 FFT band mapping changes break preset compatibility

**Severity:** Major
**Description.** Presets consume audio features (band amplitudes, onset flags) emitted by `FFTProcessor`. Changing the band layout (e.g. number of bands, frequency cutoffs, normalisation) silently changes every preset's visual output.
**Detection.** Not implemented. Manual: any change to the band mapping in `FFTProcessor` must be paired with a CHANGELOG entry and a re-test of at least `pulse_sphere.py` and `lissajous.py`.
**Related decisions.** DEC-009 … DEC-015.
**Related features.** F-017, F-024.
**History.** Anticipated.

---

## Preset runtime

### AV-011 Python preset without NumPy

**Severity:** Major
**Description.** Every built-in preset assumes NumPy is importable. If the subprocess `python3` lacks NumPy, the preset crashes on import and the engine shows blank frames forever.
**Detection.** Implemented automated (planned): `PresetRunner::start()` runs `python3 -c "import numpy; print(numpy.__version__)"` once at engine init and surfaces a clear UI error if it fails.
**Related decisions.** DEC-016 … DEC-023 (in `docs/D-003-preset-scripting.md`).
**Related features.** F-021, F-025.
**History.** Anticipated; noted in HANDOVER "Things to decide / watch for".

### AV-012 Preset stdin / stdout protocol drift

**Severity:** Major
**Description.** The JSON frame protocol between `PresetRunner` and the Python subprocess is the contract for all presets. Changing the protocol (field names, frame shape, timing) without a version bump silently breaks every existing preset.
**Detection.** Not implemented. Manual: any change to the protocol in `PresetRunner` must be paired with a protocol-version bump documented in `docs/D-003-preset-scripting.md` and an audit of `presets/builtin/`.
**Related decisions.** DEC-016 … DEC-023.
**Related features.** F-021, F-023, F-024.
**History.** Standard wire-protocol concern; flagged for discipline.

### AV-013 Per-LED Python loop in a preset

**Severity:** Minor
**Description.** Iterating `for i in range(N): cube.set(...)` over thousands of LEDs in Python is two orders of magnitude slower than NumPy bulk operations, and presets that do this miss the 60 Hz frame budget at 16³+.
**Detection.** Not implemented. Manual: code review of new presets uses the existing `pulse_sphere.py` and `lissajous.py` as style references — both use vectorised NumPy.
**Related decisions.** DEC-016 … DEC-023.
**Related features.** F-024.
**History.** Style guidance, not a hard error.

---

## Performance budgets

### AV-014 Frame rate below target at grid size

**Severity:** Major
**Description.** Performance targets in `docs/SPEC.md` §4 are: ≤16³ at 60 fps solid, 24³ at 30 fps minimum, 32³ at 20 fps minimum. A regression below any of these is a Major bug.
**Detection.** Not implemented (would require a benchmarking harness — candidate for `BENCHMARKS.md` in a future phase). Manual: ad-hoc fps overlay during development.
**Related features.** F-001, F-002.
**History.** Baseline; numbers may tighten after Phase 1 measurements.
