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
