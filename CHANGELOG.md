# Changelog

Format follows [Keep a Changelog](https://keepachangelog.com).
Entries reference feature IDs (`F-NNN`), decision IDs (`DEC-NNN`), attack-vector IDs (`AV-NNN`), bug IDs (`BUG-NNN`), and improvement IDs (`IMP-NNN`) where applicable.

## [Unreleased]

### Added
- Initial project scaffolding: Qt6/C++20 source tree, CMakeLists, Python preset runtime starter files.
- Browser prototype (Three.js r128) at `prototype/index.html` — canonical renderer reference.
- Tier 1 + Tier 2 + ATTACK_VECTORS documentation set per `development_documentation.md`.
- MIT licence (DEC-026).
- Phase 1 — core renderer with instanced LED viewport, orbit camera, CPU ray-pick (F-001, F-002, F-003, F-013, F-015).
- Phase 2 — editor UI: colour picker, slice control, timeline, frame info panel, JSON I/O (F-004–F-012, F-014).
- Phase 2 polish — corner XYZ axis triad (F-042) and slice glow for lit cells on hidden layers.
- `BUGS.md` and `IMPROVEMENTS.md` adopted per updated documentation standard (DEC-027).
- Phase 3 — audio reactive engine on PortAudio + KissFFT (F-016, F-017, F-018). Eight reactive modes (EQ bars, Beat pulse, Waveform, Spectral colour, Radial EQ, Tunnel, Energy floor, Python preset), three blend modes (Replace, Additive, Modulate), tuning controls (sensitivity, smoothing, hue shift), Waveform scroll-speed slider.
- Phase 3 — capture-to-timeline toggle (F-020): live reactive frames append to the timeline with a 500-frame soft cap.
- Phase 3 polish — Fresnel-glow LED shader for the visual hot-die look; LED-size slider in the status bar; three additional reactive modes (Radial EQ, Tunnel, Energy floor); pre-stream PipeWire / PulseAudio monitor-source routing.
- `src/util/CrashHandler.{h,cpp}` — `SIGSEGV`/`SIGABRT` backtrace via `backtrace_symbols_fd`, with `-rdynamic` so executable symbols resolve in traces.
- Phase 4 — Python preset scripting (F-021, F-022, F-023, F-025) across four steps. Step A: `presets/led_cube/` runtime with `Preset` base class, `CubeProxy`, and the JSON-line subprocess runner. Step B: `src/scripting/PresetRunner.{h,cpp}` (QProcess lifecycle + `QFileSystemWatcher` hot-reload) plus File → Run preset… offline playback. Step C: live `ReactiveMode::PythonPreset` routes the audio engine's `BandData` through the runner. Step D: `src/ui/PresetEditorPanel` + `src/ui/PythonHighlighter` for in-app editing with hot-reload on save.
- Phase 5 — 20-preset built-in library (F-024) covering beat-reactive, particle/trail, geometric, spectrum, ambient, and simulation categories. Each preset's docstring includes an Edit guide naming its main tunables.
- `presets/INSTRUCTIONS.md` — Python preset and animation authoring guide.
- Phase 6 — quality-of-life paint tools. Stroke painting (F-026): left-click on a voxel begins a stroke; drag extends it through every voxel touched. Right-mouse drag still orbits as an escape hatch. Undo/redo (F-027): `QUndoStack` (200-deep) with Edit menu, Ctrl+Z, Ctrl+Shift+Z. Mirror tools (F-029): toolbar X/Y/Z toggles for up to 8x symmetry; mirror copies write regardless of slice. Region copy/paste (F-028): Edit menu Copy / Paste with the active slice acting as the selection; paste merges so existing content under it isn't erased.
- Right-click LED context menu: right-click a lit voxel opens a small menu with "Edit colour…" (QColorDialog pre-loaded with current colour) and "Clear". Both go through the `VoxelStrokeCommand` path so they're undoable.
- Phase 7 — animation export pipeline. File → Export animation… encodes the timeline to MP4 (H.264 CRF 20), GIF (palettegen + paletteuse), WebM, or a PNG sequence via an ffmpeg subprocess. Even-dimension rounding for codec compatibility, progress dialog with cancel, audio engine paused for the export so its overlay doesn't bleed in. PNG sequence (F-034 partial — through the dispatcher) skips ffmpeg and writes numbered `<base>_NNNN.png` files directly. Camera keyframes (F-036): View → Set camera keyframe (Ctrl+Shift+K) captures the orbit camera state at the current frame; sparse keyframes interpolate (shortest-path angular for theta) during playback or export to produce fly-throughs.
- Animation script model: new `Animation` base class alongside `Preset`. Scripts inheriting it run once via `run(self, cube)` and emit frames via `cube.frame()`; `cube.play(fps)` finalises and tells the host what playback rate to use. Wire protocol gains a `play` message; `PresetRunner` exposes `animationComplete(int fps)`. File menu gains "New animation script…" (copies the template to a new file and opens it in the editor) and "Run animation script…". File → Open auto-dispatches by extension — `.json` loads as a project, `.py` loads into the editor without running. `PresetEditorPanel` gains a Run button.
- Per-LED brightness: Colour dock gains a brightness slider plus a `QSpinBox` for precise entry (0–100 %). The emitted colour is the dimmed result; the hex and R/G/B sliders always show the base; the Pick tool resets brightness to 100 % so picked colours paint exactly as sampled. Python API mirrors via an optional `brightness=0.0..1.0` kwarg on `cube.set`, `set_pos`, `set_all`, `fill`.
- Phase 8 — `USER_MANUAL.md`: end-user-facing tour of every program feature, interface, keyboard-shortcut table, and troubleshooting.
- `presets/builtin/animations/anim_spiral.py` — demo Animation: voxel walks a rising 3D spiral.
- `presets/builtin/animations/anim_lorenz.py` — demo Animation: Lorenz attractor with a rainbow trail; auto-scales into any grid size.
- `media/scintilla-demo.gif` and `media/lorenz.gif` shown as hero animations on the README.

### Changed
- Project renamed from "LED Cube VX" to **Scintilla** across all documentation and source (2026-05-20).
- Starter files reorganised from flat root into target tree (`src/`, `docs/`, `presets/`, `prototype/`).
- IMP-001 applied — `buildSphereGeometry` takes `SphereGeo&` output parameter for clarity.
- IMP-002 applied — `m_initialized` flag + `uploadScene()` helper enables `setMask` before `initializeGL`.
- IMP-003 applied — `CMAKE_EXPORT_COMPILE_COMMANDS ON` for clangd / IDE Qt header resolution.
- IMP-004 applied — CMakeLists restructured into phased blocks (Phase 2/3/4 sections preserved as commented).
- IMP-007 applied — PortAudio discovered via `pkg_check_modules` instead of the (non-existent upstream) `find_package(PortAudio)`; deviation from D-002 §DEC-009 documented in the CMake comment block.
- IMP-009 applied — `AudioRouting.{h,cpp}` and the `CrashHandler` audio-restore hook removed after BUG-013's per-stream `pactl move-source-output` approach removed any global state to manage.
- Fire preset rewritten as a proper bottom-up cellular automaton — random per-cell horizontal walks (no `np.roll` wrap-around), audio-volume-driven floor seeding so silence is fully dark, per-cell random cooling for flicker, beat injection that decays over ~6 frames.
- Timeline widget redesigned: per-frame cell strip replaced with a single horizontal scrubber slider and a right-aligned readout showing `Frame i / N · t.tts / t.tts · L lit`.
- `presets/` directory split: built-in and user areas each get `reactive/` and `animations/` subdirectories so audio-reactive presets and run-once animation scripts live in clearly distinct folders. Templates renamed/moved accordingly.
- Every preset docstring grew an `Edit guide` block listing its 3–6 main tunable knobs.
- LED shader (`led.frag`) updated to scale the white-hot core mix and intensity envelope by the LED's brightness so dim colours look dim rather than washing out (BUG-017).
- Painting `(0,0,0)` (whether via RGB sliders all zero or the brightness slider at 0 %) now routes through the erase path instead of storing a black-lit voxel (BUG-016).

### Fixed

- BUG-001: anchored `core` / `core.*` gitignore patterns to repo root so they no longer match `src/core/`.
- BUG-002: added `<cstdint>` to `VoxelFrame.h`; `uint8_t` now resolves under any consuming TU.
- BUG-003: removed bogus `explicit` on `VoxelFrame` copy ctor; added defaulted move ctor / op=.
- BUG-004: `qt_add_resources` now uses `BASE "src/renderer/shaders"`; shaders embed at `:/shaders/<name>` as expected.
- BUG-005: ghost LEDs now always render regardless of slice (SPEC §3.7 conformance); picker consults slice values directly instead of using `ghost.scale` as a hidden-state proxy (DEC-004 still upheld).
- BUG-006: `MainWindow` Reset Camera action now uses Qt 6's non-deprecated `addAction(text, shortcut, obj, slot)` parameter order.
- BUG-007: superscript `³` in info-panel and status-bar format strings no longer parses as Unicode digit; built via concatenation around the format placeholder.
- BUG-008: `BUILD.md` KissFFT vendor recipe corrected — BSD-3-Clause licence, `COPYING` filename, full file list including `_kiss_fft_guts.h` and `kiss_fft_log.h`.
- BUG-009: `paCallback` signature updated to `const PaStreamCallbackTimeInfo*` to match `PaStreamCallback*` exactly; `struct PaStreamCallbackTimeInfo` forward-declared in `AudioWorker.h` to keep `portaudio.h` out of the public header.
- BUG-010: CMake `project(... LANGUAGES C CXX)` so vendored `kiss_fft.c` actually compiles; `kissfft` target marked `AUTOMOC OFF AUTOUIC OFF AUTORCC OFF`.
- BUG-011: `MainWindow::applyMask` helper propagates mask changes to viewport, slice control, frame info, audio engine, and audio base frame so reactive modes don't get stuck at the previous grid size.
- BUG-012: `WaveformSlice` reactive mode now keeps a rolling history per-axis row (`m_waveformHistory`), producing the topographic-heightfield look DEC-012 specifies instead of a single oscillating line.
- BUG-013: System-audio routing now uses per-stream `pactl move-source-output` instead of the global `set-default-source`; PipeWire's `pcm.pipewire` plugin no longer silently captures from the mic when a monitor source is picked.
- BUG-014: `move-source-output` matcher now looks for `[Scintilla]` in `application.name` instead of `application.process.id` (which `pcm.pipewire` doesn't set); poll timeout extended to 3 s for PortAudio's slower source-output registration.
- BUG-015: Loading an Animation script as a Python-preset reactive mode no longer locks up the UI. Two-layer defence: regex-based script-type detection refuses the mismatch up front, plus `onPresetError` throttles dialogs to 1 per 5 s and force-stops the audio engine on first error in PythonPreset mode.
- BUG-016: Painting `(0,0,0)` now routes through the erase path so the voxel turns off instead of being stored as a black-lit cell that the Fresnel shader rendered with a white rim.
- BUG-017: LED shader now scales the white-hot core mix and the intensity envelope by `max(R, G, B)`, so dim LEDs read as dim instead of all converging to white at the centre.
