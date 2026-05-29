# Bugs

Catalogue of bugs discovered during development. Per Maintenance Rule 8 of
`development_documentation.md`, bugs are logged here when found, not silently
fixed. The author decides whether to fix immediately, defer, or leave alone.

**Status vocabulary:** open | fixed | wontfix | deferred.
**Severity vocabulary:** low | medium | high.

This document was adopted on 2026-05-21 (DEC-027). The entries below were
backfilled from commits `b5e448b`, `f509844`, and `7b23f96` — all surfaced
and resolved before the document existed.

---

## Open

*(none)*

## Fixed

### BUG-017: LED shader washed dim colours to near-white at the front-facing centre

**Status:** fixed (2026-05-30, in `f545f41`)
**Found:** 2026-05-30 (interactive verification of the per-LED brightness slider — Shane painted five LEDs at 100/75/50/25/0 % red and observed only marginal brightness differences with a near-white core on all of them)
**Location:** [src/renderer/shaders/led.frag](src/renderer/shaders/led.frag) `main()`
**Severity:** medium (visual fidelity — dim LEDs didn't read as dim)
**Description.** The Fresnel-glow fragment shader produced the hot-die look by mixing `vColor` toward `vec3(1.8)` with strength `core * 0.9` at the front-facing centre. The white mix target was constant regardless of `vColor`'s magnitude, so dim colours like `vec3(0.25, 0, 0)` (25 %-bright red) ended up almost pure white at the centre. Brightness gradients were visually indistinguishable.
**Reproduction.** Paint a row of voxels at 25 / 50 / 75 / 100 % red. All four render with similar white-cored hot spots; the brightness signal is invisible.
**Notes.** Scaled both the white target and the mix factor by `ledBrightness = max(R, G, B)`: full-bright LEDs keep the hot-die look, dim ones preserve their colour identity. Also scaled the intensity envelope (`dome*0.85 + core*1.6`) by `ledBrightness` so dim LEDs are perceptibly less bright in addition to keeping their hue. Paired with BUG-016's paint-as-erase routing so the brightness=0 case is genuinely off rather than rendering as a black-lit cell.

### BUG-016: Painting RGB (0,0,0) stored as a "lit" black voxel rendered with a white Fresnel rim

**Status:** fixed (2026-05-30, in `f545f41`)
**Found:** 2026-05-30 (same interactive session as BUG-017 — the brightness-0 LED looked white, not off)
**Location:** [src/renderer/CubeViewport.cpp](src/renderer/CubeViewport.cpp) `applyToolStroke`
**Severity:** medium (rendering surprise — user expected (0,0,0) to mean "off")
**Description.** `VoxelFrame::set` stored `(0,0,0)` as a lit voxel of black, and the Fresnel-glow shader rendered any lit voxel with a white-tinted core regardless of vColor magnitude. So painting black left an LED visibly glowing instead of going dark. The brightness slider at 0 % emits `(0,0,0)` to the viewport, putting the issue front-and-centre.
**Reproduction.** Set R/G/B sliders all to zero (or brightness slider to 0 %), click a voxel. The LED lights up as a white-edged sphere instead of going off.
**Notes.** Fixed by detecting `m_paintColor == (0,0,0)` in `applyToolStroke` and routing the change through the erase path so the voxel disappears from the frame instead of being stored as a black-lit cell. Mirror copies inherit the same treatment. The shader fix in BUG-017 also reduces the symptom for any leftover edge cases — even if a black-lit cell does slip through, ledBrightness=0 now scales the centre and envelope to zero.

### BUG-015: Animation script loaded into Python-preset reactive mode floods UI with modal dialogs

**Status:** fixed (2026-05-30, in `11d95aa`)
**Found:** 2026-05-30 (Shane: "I set audio reactive mode to Python Preset and then loaded anim_lorenz.py then I set the audio input and a lot of error messages came up, the program glitched out and everything froze")
**Location:** [src/MainWindow.cpp](src/MainWindow.cpp) `onPresetError`, `onLoadReactivePreset`, `runAnimationScript`, `onRunPreset`
**Severity:** high (full UI lockup, system became unresponsive enough to require external intervention)
**Description.** The Python runtime accepts two script types — `Preset` (reactive, driven by per-frame audio bands) and `Animation` (run-once, emits frames via `cube.frame()`). The `Run preset` / `Load preset` flows both spawn a subprocess that expects to receive `frame` messages each audio frame; the runner explicitly rejects `frame` messages when the loaded script is an `Animation`. With audio enabled and an Animation loaded as a reactive preset, the audio engine emitted band updates at ~60 Hz, MainWindow pushed each one to the runner, the runner replied with `{"type":"error","message":"frame received in animation mode (script already finished)"}` per push, and `onPresetError` opened a modal `QMessageBox::warning` for each error. At 60 dialogs per second the UI thread saturated and the system locked up — the audio thread kept pumping, so the loop didn't self-resolve.
**Reproduction.** Pre-fix build: Audio reactive panel → set Reactive: Python preset → Load preset… → pick `presets/builtin/animations/anim_lorenz.py` → Audio → Select input device → pick any monitor source. The Lorenz animation runs once, then a flood of "Preset error" modals begins and the UI freezes.
**Notes.** Two-layer fix:

  1. **Up-front detection.** A new `detectScriptType()` helper in MainWindow regex-scans the file for `class X(Animation)` vs `class X(Preset)` before starting the subprocess. `onLoadReactivePreset` and `onRunPreset` refuse Animation files with a clear "use File → Run animation script…" message; `runAnimationScript` refuses Preset files. `onRunPreset` also auto-reroutes Animation files through the animation path.
  2. **Error throttle + auto-stop.** `onPresetError` now (a) stops the audio engine immediately if it's running in PythonPreset mode when an error arrives, breaking the bands → frame → error feedback loop, and (b) caps modal dialogs at one per 5 seconds. Suppressed errors are counted and reported in the status bar; the next dialog appends "(N additional errors suppressed.)".

  The detection covers the common cause; the throttle is a safety net for any future path that might leak per-frame errors.

### BUG-014: Source-output matcher used `application.process.id`, which `pcm.pipewire` doesn't set

**Status:** fixed (2026-05-22, this session)
**Found:** 2026-05-22 (Shane: "whichever input I select I keep getting [the error]")
**Location:** [src/audio/AudioWorker.cpp](src/audio/AudioWorker.cpp) — `start()`, the `move-source-output` polling loop
**Severity:** high (BUG-013's fix didn't actually work — the polling loop always failed, falling back to "capture from system default input" which on this machine is the mic)
**Description.** The BUG-013 patch identified our PortAudio source-output by matching `application.process.id = "<pid>"` against `pactl list source-outputs`. Diagnostic capture of the actual fields produced by the ALSA `pcm.pipewire` plugin showed that property is **never set**:

  ```text
  Properties:
      application.name = "PipeWire ALSA [aplay]"
      node.name = "alsa_capture.aplay"
      device.description = "ALSA Capture [aplay]"
      ...
      module-stream-restore.id = "source-output-by-application-name:PipeWire ALSA [aplay]"
  ```

  No `application.process.id`, no `application.process.binary` — `pcm.pipewire` populates only the ALSA-plugin-facing properties. Matching on PID never found our entry, the 1 s timeout expired every time, the error dialog fired, and the stream continued capturing from the system mic.
**Reproduction.** Phase-3-polish build (`2fdc659` + BUG-013 patch but pre-fix): launch Scintilla, pick any `[system audio]` monitor, start a reactive mode, speak into the laptop mic — the visualisation reacts to the mic, and the "Couldn't find Scintilla's source-output" dialog appears.
**Notes.** Switched the matcher from PID to `application.name` substring `[Scintilla]`. The `pcm.pipewire` plugin always writes the binary's name into `PipeWire ALSA [<binary>]`, and our executable is named `Scintilla` (per `add_executable(Scintilla ...)` in CMakeLists). Same change also bumps the poll timeout from 1 s (20 × 50 ms) to 3 s (60 × 50 ms) because PortAudio's first source-output registration is noticeably slower than aplay's. If the matcher ever needs to widen again, `node.name` substring `Scintilla` is a reliable second-best.

### BUG-013: System-audio routing via `pactl set-default-source` was wrong scope on PipeWire

**Status:** fixed (2026-05-22, this session)
**Found:** 2026-05-22 (Shane consulted Claude-web after observing mic + monitor bleed in every input pick; diagnostic commands run from this session confirmed the architectural mismatch)
**Location:** [src/audio/AudioWorker.cpp](src/audio/AudioWorker.cpp) `start()`, plus the now-deleted [src/audio/AudioRouting.{h,cpp}](src/audio/AudioRouting.h)
**Severity:** high (the headline Phase 3 feature — system-audio capture — wasn't doing what the user expected, and the workaround mutated global system state with crash-handler complexity to justify it)
**Description.** The Phase 3 polish commit (`2fdc659`) introduced `AudioRouting::setDefaultSource` which redirected audio capture by running `pactl set-default-source <monitor>` before opening PortAudio's `default` device. On this PipeWire system (Ubuntu, server "PulseAudio (on PipeWire 1.0.5)"), three things broke that approach:

  1. `pcm.!default` is `type pipewire` (the PipeWire-native ALSA plugin), not `pcm.pulse` — capture bypasses libpulse entirely. Diagnostic test: `PULSE_SOURCE="<monitor>" arecord -D default ...` produced a source-output bound to **Source 62 (the mic)**, not the requested monitor.
  2. The PipeWire `pcm.pipewire` plugin's auto-routing (`capture_node = "-1"` per `/usr/share/alsa/alsa.conf.d/50-pipewire.conf`) doesn't reliably observe a default-source change for an in-progress connection.
  3. Even when it did land, the change persisted system-wide until the user manually reverted — which is what motivated the (now-also-unnecessary) audio-restore hook in `CrashHandler`.

**Reproduction.** On Ubuntu/PipeWire pre-fix: Audio → pick a `[system audio]` monitor → click any reactive mode → play music → `pactl list source-outputs` shows the `Source:` field as the system mic, not the monitor. Audio capture is whatever the user's mic happens to pick up, plus any system audio the mic acoustically catches.
**Notes.** Replaced with **per-stream `pactl move-source-output`** after `Pa_StartStream`: the worker polls `pactl list source-outputs` looking for an entry matching its own PID, then issues `pactl move-source-output <id> <monitor>` to redirect just that one stream. Verified per Test 2 in the diagnostic run — moving arecord's source-output from Source 62 (mic) to Source 57 (monitor) worked cleanly. No global state changed; no cleanup needed on stop or crash. `AudioRouting.{h,cpp}` deleted entirely (IMP-009). The `CrashHandler` itself stays (still useful for stack traces) but the audio-restore hook is removed. Pre-move data in the ring buffer is discarded so the visualisation starts cleanly on the monitor.

### BUG-012: WaveformSlice mode lacked time history (DEC-012 "map one axis to time")

**Status:** fixed (2026-05-21, this session)
**Found:** 2026-05-21 (Phase 3 interactive verification — Shane compared the running mode against a reference heightfield image)
**Location:** [src/audio/AudioReactiveEngine.cpp](src/audio/AudioReactiveEngine.cpp) `modeWaveformSlice`
**Severity:** medium (mode functional but didn't fulfil the spec's intent)
**Description.** DEC-012 specifies WaveformSlice as "map one axis (user-selectable X, Y, or Z) to **time**. Each LED on that axis represents one audio sample." The Phase 3 implementation drew the current band magnitudes onto a single Z=centre slice each frame — no time history. Visually it looked like a static line oscillating; the user expected a rolling heightfield where past samples persist as the cube fills up (topographic-map effect).
**Reproduction.** Launch Scintilla, Audio → Reactive: Waveform with music playing. The lit voxels form a single thin line across the Z=centre slice rather than spreading into a 2D heightfield across Z.
**Notes.** Added `std::vector<std::vector<int>> m_waveformHistory` to the engine, indexed `[z][x] → y`. Each frame: compute the current row from band magnitudes, shift `m_waveformHistory[z] = m_waveformHistory[z+1]` for z=0..n-2 (oldest data falls off the back), drop the new row at `z=n-1`. Render the whole heightfield. Storage and shift cost are trivial at the 32³ cap. The history is cleared on mode change (away from Waveform) and on mask change (grid-size variations would otherwise leave stale rows behind). One detail noted but not implemented: DEC-012 says "colour to sign" but the engine only sees absolute band magnitudes, never signed time-domain samples, so positive-vs-negative colour mapping isn't possible without restructuring `FFTProcessor` to also expose the raw waveform. Recorded as a future-consideration in the notes here; not worth a separate IMP yet.

### BUG-011: Mask change didn't propagate to `AudioReactiveEngine`; reactive modes stuck at the old grid size

**Status:** fixed (2026-05-21, this session)
**Found:** 2026-05-21 (Phase 3 interactive verification — Shane set a 20³ cube and observed reactive output remained an 8×8×8 block in the corner)
**Location:** [src/MainWindow.cpp](src/MainWindow.cpp) — `rebuildMask()` and `onOpen()`
**Severity:** high (user-visible regression in the Phase 3 acceptance path)
**Description.** The Phase 3 constructor wires `m_audioEngine->setMask(m_mask)` once, but neither `rebuildMask()` (Shape menu or grid-size dialog) nor `onOpen()` (loading a JSON) repeated the call. The engine kept its initial 8³ mask, so its reactive-mode helpers (`modeEqBars`, `modeBeatPulse`, `modeSpectralColour`, `modeWaveformSlice`) generated VoxelFrames sized for an 8³ grid. The viewport then displayed those frames against a 20³ shape, voxels with x/y/z ≥ 8 were absent from the frame, and only the bottom-left 8×8×8 corner showed reactive paint.
**Reproduction.** Launch Scintilla → Audio → select a monitor source → Reactive: Spectral colour with music playing → Shape → Grid size… → 20. The lit region remains a small corner; the rest of the cube is dark.
**Notes.** Extracted `applyMask()` helper that does all the propagation (viewport, slice control, frame info, audio engine, audio base frame) and replaced the manual fan-out in both call sites. Prevents future call-sites from re-introducing the same miss.

### BUG-010: `project(... LANGUAGES CXX)` silently skipped `third_party/kissfft/kiss_fft.c`

**Status:** fixed (2026-05-21, this session)
**Found:** 2026-05-21 (Phase 3 first link attempt)
**Location:** [CMakeLists.txt:6](CMakeLists.txt)
**Severity:** high (linker errors blocked the whole Phase 3 build)
**Description.** The original Phase 1 CMakeLists declared `project(Scintilla ... LANGUAGES CXX)`. With C disabled, CMake silently refused to compile `third_party/kissfft/kiss_fft.c` — `libkissfft.a` was produced but contained only the auto-generated MOC stub (`mocs_compilation.cpp.o`), not the actual FFT object code. Linking Scintilla then failed with `undefined reference to kiss_fft_alloc` / `kiss_fft`.
**Reproduction.** With the project line at `LANGUAGES CXX`, `ar tv build/libkissfft.a` lists only the MOC compilation unit; the expected `kiss_fft.c.o` is absent. Linking errors only show at the executable link step.
**Notes.** Two changes: (1) `LANGUAGES C CXX` so .c sources actually compile, (2) `set_target_properties(kissfft PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)` to skip the unnecessary MOC sweep over a vendored C-only library. The MOC stub linked harmlessly before so #2 is cleanup, not strictly required for correctness.

### BUG-009: scaffolded `paCallback` declaration didn't match `PaStreamCallback*`

**Status:** fixed (2026-05-21, this session)
**Found:** 2026-05-21 (Phase 3 first build attempt — AudioWorker.cpp compile error)
**Location:** [src/audio/AudioWorker.h:67-72](src/audio/AudioWorker.h)
**Severity:** high (blocked Phase 3 link)
**Description.** The scaffolded `AudioWorker.h` declared the static `paCallback` with `const void* timeInfo` and `unsigned long statusFlags` to keep `portaudio.h` out of the header. PortAudio's `PaStreamCallback*` typedef in `Pa_OpenStream`'s 7th parameter, however, requires `const PaStreamCallbackTimeInfo*` exactly — function-pointer types are matched by exact signature, not by pointer-compatibility. The `unsigned long` flags happened to match (`PaStreamCallbackFlags` is `typedef unsigned long`), but the `const void*` for `timeInfo` did not match `const PaStreamCallbackTimeInfo*`.
**Reproduction.** With the original signature, GCC emits `invalid conversion from 'int (*)(const void*, ...)' to 'int (*)(const void*, ..., const PaStreamCallbackTimeInfo*, ...)' [-fpermissive]` at the `Pa_OpenStream(..., &AudioWorker::paCallback, ...)` call site.
**Notes.** Fix: forward-declare `struct PaStreamCallbackTimeInfo;` at the top of `AudioWorker.h`, then use `const PaStreamCallbackTimeInfo*` in the callback declaration. The forward declaration is sufficient because the cpp includes `portaudio.h` and provides the full type when the function is defined. The abstraction goal (no portaudio.h leak into consumers) is preserved.

### BUG-008: BUILD.md KissFFT vendor recipe wrong (licence, file list, paths)

**Status:** fixed (2026-05-21, this session)
**Found:** 2026-05-21 (Phase 3 prerequisites — actually vendoring KissFFT for the first time)
**Location:** [BUILD.md:29](BUILD.md), [BUILD.md:88-100](BUILD.md)
**Severity:** medium (broke the documented vendor flow; new contributors following the docs verbatim would have produced a non-compiling tree)
**Description.** The Phase 1 BUILD.md, written before KissFFT was actually vendored, contained three inaccuracies that surfaced when the recipe was first run end-to-end: (a) the dependencies table called the licence "MIT" — it's BSD-3-Clause per `SPDX-License-Identifier` in `kiss_fft.h`; (b) the `curl` line fetched `LICENSES/BSD-3-Clause` which doesn't exist in the upstream tree — the licence file is `COPYING`; (c) the recipe omitted `_kiss_fft_guts.h`, an internal header that `kiss_fft.c` includes, so a tree following the docs would fail to compile with `_kiss_fft_guts.h: No such file or directory`.
**Reproduction.** Follow BUILD.md "Vendoring KissFFT" verbatim, then attempt to build the audio target. The first `kiss_fft.c` translation unit fails on a missing include.
**Notes.** Fixed inline during the Phase 3 vendor step (active-blocker exception per Maintenance Rule 8). Recipe now fetches `kiss_fft.h`, `kiss_fft.c`, `_kiss_fft_guts.h`, `kiss_fft_log.h`, and `COPYING`. DEC-026 already had the correct BSD-3-Clause attribution in its consequences section; only BUILD.md was stale.
**History.** Initial 2026-05-21 fix added `_kiss_fft_guts.h` but missed a fourth required file: `_kiss_fft_guts.h` itself includes `kiss_fft_log.h`. Discovered when the first KissFFT build attempt failed with `fatal error: kiss_fft_log.h: No such file or directory`. Re-fetched in the same session and BUILD.md updated to match.

### BUG-001: `core` gitignore pattern matched `src/core/` directory

**Status:** fixed (2026-05-20, pre-`b5e448b`)
**Found:** 2026-05-20 (initial commit staging)
**Location:** [.gitignore:96](.gitignore)
**Severity:** medium
**Description.** The `.gitignore` patterns `core` and `core.*` were intended to catch Unix core-dump files at the repo root. They also matched the `src/core/` directory, silently excluding `src/core/ShapeMask.h` and `src/core/VoxelFrame.h` from staging. The bug was caught only because `git status` after `git add .` showed those files as neither staged nor untracked.
**Reproduction.** With a bare `core` line in `.gitignore`, run `git add . && git status src/core/`. Files in `src/core/` are silently excluded.
**Notes.** Fixed by anchoring the patterns to the repo root: `/core` and `/core.*` instead of `core` and `core.*`. Tooling reference: `git check-ignore -v <path>` was used to identify the matching rule.

### BUG-002: `VoxelFrame.h` lacked `<cstdint>` include for `uint8_t`

**Status:** fixed (2026-05-20, in `f509844`)
**Found:** 2026-05-20 (Phase 1 build attempt)
**Location:** [src/core/VoxelFrame.h:4](src/core/VoxelFrame.h)
**Severity:** medium
**Description.** The pre-existing scaffolded `VoxelFrame.h` used `uint8_t` as a typedef-name for `std::array<uint8_t, 3>` (the `RGB` alias) and in method signatures, but did not include `<cstdint>`. The header had compiled under the original chat-session context (probably via transitive include) but failed under our toolchain when consumed in isolation.
**Reproduction.** `g++ -std=c++20 -fsyntax-only -Isrc src/core/VoxelFrame.cpp` errors with `error: 'uint8_t' was not declared in this scope`.
**Notes.** Added `#include <cstdint>` to the header. Standard library guarantee: `<array>` does NOT have to drag in `<cstdint>`.

### BUG-003: `VoxelFrame.h` `explicit` copy constructor blocked `std::move` into a new variable

**Status:** fixed (2026-05-20, in `f509844`)
**Found:** 2026-05-20 (Phase 1 build attempt — `AnimationTimeline::moveFrame`)
**Location:** [src/core/VoxelFrame.h:30](src/core/VoxelFrame.h)
**Severity:** medium
**Description.** The pre-existing scaffolded `VoxelFrame` declared its copy constructor as `explicit VoxelFrame(const VoxelFrame&) = default;`. This unusual qualifier prevents copy-initialisation syntax (`VoxelFrame moved = std::move(m_frames[from]);`), which is what `AnimationTimeline::moveFrame()` does. The implicit move constructor was also suppressed because an explicit copy was defined.
**Reproduction.** Compile `AnimationTimeline.cpp` against the original header.
**Notes.** Removed the `explicit` qualifier and added explicit `= default` move-ctor and move-assignment. The `explicit`-on-copy-ctor pattern has no real-world rationale here and was likely a scaffolding mistake.

### BUG-004: `qt_add_resources` lacked `BASE` arg, shaders embedded at wrong resource path

**Status:** fixed (2026-05-20, in `f509844`)
**Found:** 2026-05-20 (Phase 1 first run — `QOpenGLShader: Unable to open file ":/shaders/led.vert"`)
**Location:** [CMakeLists.txt:88](CMakeLists.txt)
**Severity:** high (full render pipeline non-functional — shader programs failed to link, segfault on `QOpenGLShaderProgram::bind()` of un-linked program)
**Description.** Without a `BASE` argument, `qt_add_resources` computes resource aliases as the FILES paths relative to the current source dir. With `PREFIX "/shaders"` and `FILES src/renderer/shaders/led.vert`, the resulting resource path was `:/shaders/src/renderer/shaders/led.vert`, not `:/shaders/led.vert` as the shader compile code expected.
**Reproduction.** Launch the Phase 1 binary; every shader load logs `Unable to open file ":/shaders/*.vert"`; all three shader programs fail to link; `paintGL` segfaults.
**Notes.** Added `BASE "src/renderer/shaders"` so file paths are made relative to the shader directory before alias construction. Result: aliases are `led.vert`, `led.frag`, etc.; full resource paths are `:/shaders/led.vert` as intended.

### BUG-005: Ghost LEDs hidden when slice-filtered (violated SPEC §3.7)

**Status:** fixed (2026-05-20, in `7b23f96`)
**Found:** 2026-05-20 (Phase 2 slice-view manual test)
**Location:** [src/renderer/LedInstanceBuffer.cpp:60](src/renderer/LedInstanceBuffer.cpp) (`updateFrame`)
**Severity:** medium (UX correctness — interior layers had no spatial context when sliced)
**Description.** `LedInstanceBuffer::updateFrame` set `m_ghost[i].scale = 0` for any cell outside the active slice, meaning slice-hidden ghost LEDs were not rendered at all. SPEC §3.7 says explicitly: *"Ghost mesh always shows full shape regardless of slice (provides context)."* The implementation deviated from the spec.
**Reproduction.** Set the X/Y/Z slider to any integer in [0, gridSize-1]. The cube outline disappears; only the active layer's ghost LEDs render.
**Notes.** Fixed in two steps: (a) `m_ghost[i].scale = 1.0f` unconditionally (later refined to `kHiddenGhostDim = 0.4` for slice-hidden cells so they read as faint outline rather than full ghosts); (b) `CubeViewport::pickInstance` was using `ghost.scale < 0.001` as a "slice-hidden, cannot paint" sentinel — this stopped working when ghost.scale stopped being binary. Changed the picker to consult `m_sliceX/Y/Z` against the mask position directly. DEC-004 (cannot paint hidden layers) still upheld.

### BUG-006: Deprecated `addAction(text, obj, slot, shortcut)` parameter order

**Status:** fixed (2026-05-20, in `7b23f96`)
**Found:** 2026-05-20 (Phase 2 MainWindow build — IDE diagnostic)
**Location:** [src/MainWindow.cpp:111](src/MainWindow.cpp) (Reset Camera action wiring)
**Severity:** low
**Description.** `QMenu::addAction(text, obj, slot, shortcut)` is deprecated in Qt 6; the new signature is `addAction(text, shortcut, obj, slot)`. All my File-menu actions had the new order; one View-menu action had the old order. Compiled with a `-Wdeprecated-declarations` warning that the IDE surfaced as a Hint.
**Reproduction.** Compile with Qt 6.4+ and inspect compiler / IDE diagnostics for `'addAction' is deprecated`.
**Notes.** Trivial reorder of `this` / `&MainWindow::onResetCamera` / `QKeySequence(Qt::Key_R)` to put the shortcut second.

### BUG-007: `QString::arg("...%1³...")` parsed `³` as digit, substituting arg #23

**Status:** fixed (2026-05-20, in `7b23f96`)
**Found:** 2026-05-20 (Phase 2 launch — runtime warning spam)
**Location:** [src/ui/FrameInfoPanel.cpp:44](src/ui/FrameInfoPanel.cpp); also [src/MainWindow.cpp:310](src/MainWindow.cpp)
**Severity:** low (cosmetic — warning spam plus malformed output)
**Description.** `QString::arg` accepts `%N` placeholders where `N` is parsed greedily including Unicode digits. The literal `Grid: %2³` was parsed as `%2³` → arg #23 (since `³` U+00B3 is Unicode digit-3). Qt emitted runtime warnings: `QString::arg(): the replacement "%2³" contains non-ASCII digits; it is currently being interpreted as the 23-th substitution.`
**Reproduction.** Launch Scintilla; the `FrameInfoPanel::refresh()` call on init produces three lines of warning output before the window appears.
**Notes.** Fixed by splitting the format strings so `³` is never adjacent to a `%N` placeholder — concatenate the superscript-bearing fragment with `QStringLiteral("³")` rather than embedding it in the format. Two call sites needed the fix.

## Won't Fix

*(none)*

## Deferred

*(none)*
