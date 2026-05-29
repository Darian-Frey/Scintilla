# Improvements

Catalogue of code-quality improvements, refactors, and architectural changes
proposed during development. Per Maintenance Rule 8 of
`development_documentation.md`, improvements are logged here when noticed,
not silently applied. The author decides whether to apply, defer, or
decline.

This is the dual of `BUGS.md`: bugs are broken; improvements work but
could be better.

**Status vocabulary:** suggested | applied | declined | deferred.
**Effort vocabulary:** trivial | small | medium | large.

This document was adopted on 2026-05-21 (DEC-027). The Applied entries
below were backfilled from commits `f509844` and `7b23f96` — refactors
that landed before the document existed.

---

## Suggested

### IMP-011: WebM export feeds H.264 encoder args into a WebM container

**Status:** suggested
**Found:** 2026-05-23 (during the Phase 7 export pipeline implementation; flagged in the ROADMAP "loose ends" section but not previously logged)
**Location:** [src/MainWindow.cpp](src/MainWindow.cpp) — `onExportAnimation`, the ffmpeg args branch
**Effort:** small
**Description.** The export pipeline branches by output extension into GIF (palettegen filter) and "video" (H.264 + yuv420p + CRF 20) paths. The video path is used for both `.mp4` and `.webm` outputs. ffmpeg silently picks a codec compatible with the WebM container (VP9 in modern builds), so the file is playable, but the CRF range and pixel format aren't optimal for VP9 and the user pays a noticeable file-size penalty.
**Proposal.** Add a third branch matching `.webm` and use `-c:v libvpx-vp9 -crf 30 -b:v 0 -pix_fmt yuv420p` (or `yuv420p10le` for 10-bit). Optionally `-cpu-used 4` for a faster encode at slight quality cost. Detection is a one-line `path.endsWith(".webm", Qt::CaseInsensitive)` alongside the existing `isGif` check.
**Trade-offs.** Three or four extra lines of dispatch. WebM exports become semantically correct and produce smaller files for equivalent visual quality. No effect on MP4/GIF paths.
**Notes.** Current behaviour produces a working WebM via ffmpeg's default-codec choice, so this isn't broken — just suboptimal. Worth doing when WebM export gets meaningful use, or as a quick polish alongside any other export pipeline change.

### IMP-010: Fill tool not wrapped in `VoxelStrokeCommand` — bulk fills skip the undo stack

**Status:** suggested
**Found:** 2026-05-23 (during the stroke painting + undo work; flagged in the ROADMAP "loose ends" section but not previously logged)
**Location:** [src/renderer/CubeViewport.cpp](src/renderer/CubeViewport.cpp) — `applyTool`, `Tool::Fill` case
**Effort:** small
**Description.** The Phase 6 stroke painting + undo work wrapped Paint and Erase mutations in `VoxelStrokeCommand` so each stroke is one undoable step. The Fill tool was left as a direct frame mutation. As a result Ctrl+Z cannot undo a Fill — by far the most destructive single click in the tool palette.
**Proposal.** In the Fill branch, iterate the target voxels (already done for the mutation), build a `VoxelStroke` with one `VoxelChange` per modified voxel capturing the pre-edit and post-edit colours, push as a `VoxelStrokeCommand`. The pattern is exactly what `applyToolStroke` does, just executed in one click instead of incrementally.
**Trade-offs.** At the 32³ grid cap a Fill stroke records up to 32 768 changes — large but compressible and well within the 200-entry stack limit. Memory is bounded.
**Notes.** Should also respect the active slice (Fill already does) so the captured stroke matches what got painted. After implementation the Fill tool gets first-class undo / redo for free.

### IMP-005: Spatial index for ray-pick instead of O(N) linear scan

**Status:** suggested
**Found:** 2026-05-21 (during 2026-05-21 documentation pass)
**Location:** [src/renderer/CubeViewport.cpp:380](src/renderer/CubeViewport.cpp) (`pickInstance`)
**Effort:** medium
**Description.** `pickInstance` iterates over every instance in the mask (up to 32,768 at the max grid size) doing a ray-sphere intersection per voxel. At small grids this is invisible; at 32³ the per-click cost is noticeable on a low-spec machine.
**Proposal.** Build a uniform-grid spatial index when the mask is rebuilt: a 3D array of indices keyed by integer voxel position. The picker walks the ray voxel-by-voxel using a DDA-style traversal and tests intersection only against candidates in the cells the ray enters.
**Trade-offs.** Adds a few KB of memory per mask plus rebuild cost on shape/size change (already an expensive operation, so amortised easily). Adds code complexity vs. the current trivial brute-force loop. Probably unjustified until profiling on a 32³ project actually shows a click-latency problem.
**Notes.** Worth keeping the brute-force path as a fallback / reference. Not on the critical path for Phase 3 (audio).

### IMP-006: Hot-reload shader source from disk during development

**Status:** suggested
**Found:** 2026-05-21 (during 2026-05-21 documentation pass)
**Location:** [src/renderer/CubeViewport.cpp](src/renderer/CubeViewport.cpp) (`buildShaders`)
**Effort:** small
**Description.** During Phase 2 polish (slice-glow, axis triad) I rebuilt and relaunched the binary repeatedly to iterate on shader constants. The Qt resource system embeds shader sources at build time, so each tweak required a full CMake build cycle.
**Proposal.** In Debug builds only, load shader sources from disk (`src/renderer/shaders/*`) instead of from `:/shaders/*`, with a `QFileSystemWatcher` triggering recompile + `update()` on file save. Release builds keep the embedded resource path.
**Trade-offs.** A small fork in `buildShaders()` between Debug and Release paths. Risk: shader paths in source code drift from resource paths (mitigated by deriving both from a single constant). Worth it only if shader iteration becomes frequent — currently we have 8 shaders and won't touch most of them again until Phase 5 presets.
**Notes.** Standard pattern in game-engine UIs. Defer until Phase 5 (presets) or until shader tweaking becomes painful again.

### IMP-008: Marshal `AudioWorker::setGridSize` via Qt::QueuedConnection (cross-thread race)

**Status:** suggested
**Found:** 2026-05-21 (investigating BUG-011)
**Location:** [src/audio/AudioReactiveEngine.cpp:39](src/audio/AudioReactiveEngine.cpp) `AudioReactiveEngine::setMask`
**Effort:** small
**Description.** `AudioReactiveEngine::setMask` (main thread) calls `m_worker->setGridSize(...)` directly. `AudioWorker` lives on the audio QThread; its `setGridSize` updates `m_fft.setGridSize(...)` which mutates `FFTProcessor::m_rollingMax`, `m_bandEdges`, etc. The drain loop on the worker thread reads/writes those same fields inside `process()`. A grid-size change while audio is running is therefore a data race — undefined behaviour. In practice the user has to stop audio before changing size for this not to fire, but the API doesn't enforce it.
**Proposal.** Add `setGridSize` as a public slot on `AudioWorker` and call it via `QMetaObject::invokeMethod(m_worker, "setGridSize", Qt::QueuedConnection, Q_ARG(int, n))`. The change then lands at the start of the worker thread's next event-loop iteration, between FFT frames. Alternative: stop the engine before the change and restart after — heavier but also resolves it.
**Trade-offs.** Adds a one-frame delay before the new grid size takes effect on FFT band count. That's invisible to the user (one audio frame is ~23 ms at 1024-sample FFTs). Doesn't increase code complexity meaningfully. The alternative (stop/restart) costs a PortAudio stream open which is ~50 ms — noticeable click.
**Notes.** Worth doing before any feature that resizes the grid while audio is live (e.g. a "size animator" or BPM-synced grid). Until then it's a latent issue.

## Applied

### IMP-009: Delete `AudioRouting` after fixing BUG-013 — no global state to manage

**Status:** applied (2026-05-22, this session)
**Found:** 2026-05-22 (during the BUG-013 fix)
**Location:** [src/audio/](src/audio/) (files removed), [src/main.cpp](src/main.cpp), [CMakeLists.txt](CMakeLists.txt)
**Effort:** trivial
**Description.** `AudioRouting` existed solely to manage the global side-effect of `pactl set-default-source`: capture the original default before mutating, restore it on clean exit (`QApplication::aboutToQuit`), restore it on crash (signal-safe `fork`+`execve` registered as a `CrashHandler` hook). Once BUG-013 swapped the global-default approach for per-stream `pactl move-source-output`, none of that infrastructure had anything to manage — the move-source-output redirect lives only for the stream's lifetime and disappears when PortAudio closes.
**Proposal.** Delete `src/audio/AudioRouting.{h,cpp}`. Remove the `AudioRouting::signalSafeRestore` hook registration and the `QApplication::aboutToQuit` lambda from `main.cpp`. Remove the source files from `CMakeLists.txt`. Keep `CrashHandler` itself — its value (stack traces on fatal signals) is independent of audio routing.
**Trade-offs.** Removes ~140 lines of code and the dependency on `pactl get-default-source` at startup. Loses the "explicit save-and-restore" model in case the per-stream approach turns out to have edge cases we haven't seen — but that's a hypothetical against a confirmed-working alternative.
**Notes.** Side benefit: simpler mental model for anyone reading the audio pipeline. The decision tree shrinks from "are we mutating global state? have we saved? have we restored on every exit path including crashes?" to "did Pa_StartStream succeed? if yes, move this one source-output."

### IMP-007: `pkg_check_modules` instead of `find_package(PortAudio)`

**Status:** applied (2026-05-21, this session)
**Found:** 2026-05-21 (Phase 3 CMakeLists wiring — confirming the D-002 illustrative example)
**Location:** [CMakeLists.txt:30-34](CMakeLists.txt)
**Effort:** trivial
**Description.** D-002 §DEC-009 shows the CMake invocation as `find_package(PortAudio REQUIRED)` + `target_link_libraries(... PortAudio::PortAudio)`. That syntax assumes either an upstream `FindPortAudio.cmake` (which CMake does not ship) or a vendor-provided CMake config (which the `portaudio19-dev` Debian package does not install). Following D-002 literally produces "Could NOT find PortAudio" at configure time on every Debian-derived system.
**Proposal.** Use `find_package(PkgConfig REQUIRED)` + `pkg_check_modules(PORTAUDIO REQUIRED IMPORTED_TARGET portaudio-2.0)` + `target_link_libraries(... PkgConfig::PORTAUDIO)`. PortAudio installs `portaudio-2.0.pc` on every platform that ships a dev package, so this approach is more portable than a manual `FindPortAudio.cmake` shim.
**Trade-offs.** Adds a soft dependency on `pkg-config` being installed (universal on Linux, present via Homebrew on macOS, available via MSYS / vcpkg on Windows). The D-002 example syntax was illustrative rather than binding, so this isn't a DEC reversal — but the deviation is worth recording so a reader who compares D-002 against `CMakeLists.txt` understands the gap.
**Notes.** A future `FindPortAudio.cmake` shim under `cmake/` (so the `PortAudio::PortAudio` target name from D-002 works as-written) is possible if the deviation ever becomes confusing. For now the deviation is documented in the CMakeLists comment block.

### IMP-001: `buildSphereGeometry(SphereGeo& out, ...)` output-parameter signature

**Status:** applied (2026-05-20, in `f509844`)
**Found:** 2026-05-20 (Phase 1 CubeViewport.cpp implementation)
**Location:** [src/renderer/CubeViewport.h:72](src/renderer/CubeViewport.h), [src/renderer/CubeViewport.cpp](src/renderer/CubeViewport.cpp)
**Effort:** trivial
**Description.** The original scaffolded signature `void buildSphereGeometry(float radius, int lonSegs, int latSegs)` had no way to specify which sphere it was building, since the viewport owns two (`m_onSphere`, `m_offSphere`). The original author probably intended to call it twice but the API didn't support it.
**Proposal.** Add a `SphereGeo&` output parameter as the first argument: `void buildSphereGeometry(SphereGeo& out, int lonSegs, int latSegs)`. Removed the unused `radius` parameter at the same time — shaders bake in the radius via `kRadius` / `kGhostRadius` constants.
**Trade-offs.** Diverges from the original scaffold's signature, but the scaffold was provably unusable as-written, so this is correction rather than rewrite. Adds nothing to maintenance burden.
**Notes.** Also moved the `SphereGeo` struct declaration up so the helper signature could reference it.

### IMP-002: `m_initialized` flag + `uploadScene()` helper for deferred GPU work

**Status:** applied (2026-05-20, in `f509844`)
**Found:** 2026-05-20 (Phase 1 CubeViewport.cpp implementation)
**Location:** [src/renderer/CubeViewport.h:103](src/renderer/CubeViewport.h), [src/renderer/CubeViewport.cpp](src/renderer/CubeViewport.cpp)
**Effort:** small
**Description.** `MainWindow` constructs the viewport, then calls `setMask()` and `setTimeline()` before `show()`. Qt's `QOpenGLWidget` calls `initializeGL()` only after the widget is first shown, so GL resources don't exist when those setters run. The naïve implementation would either crash on first GL call or require the caller to defer all setup until after `show()`.
**Proposal.** Add a `bool m_initialized = false` flag set true at the end of `initializeGL()`. Add a private helper `uploadScene()` that rebuilds the instance buffer and uploads to GPU. `setMask()` and `setTimeline()` defer GPU work via `if (m_initialized) uploadScene();`. `initializeGL()` calls `uploadScene()` once the GL context exists if `m_mask` was set pre-show.
**Trade-offs.** One extra boolean and one extra method. Alternative was making MainWindow defer `setMask`/`setTimeline` until after `show()`, but that contaminates the caller with rendering-layer initialisation timing.
**Notes.** Pattern is standard for `QOpenGLWidget` ownership boundaries.

### IMP-003: `CMAKE_EXPORT_COMPILE_COMMANDS ON` for clangd / IDE indexing

**Status:** applied (2026-05-20, in `f509844`)
**Found:** 2026-05-20 (Phase 1, while debugging IDE diagnostics for Qt include paths)
**Location:** [CMakeLists.txt:21](CMakeLists.txt)
**Effort:** trivial
**Description.** The IDE's clangd-based diagnostic engine reported every Qt header (`<QOpenGLFunctions_4_3_Core>`, `<QMouseEvent>`, etc.) as not found, cascading into hundreds of false-positive errors. CMake's `compile_commands.json` was not being emitted, so clangd had no Qt include paths.
**Proposal.** Add `set(CMAKE_EXPORT_COMPILE_COMMANDS ON)` to `CMakeLists.txt`. clangd auto-discovers `build/compile_commands.json` and IDE diagnostics resolve correctly.
**Trade-offs.** Negligible — emits one extra file in the build dir. No build-time cost.
**Notes.** Worth doing on day one of any CMake project with non-trivial dependencies.

### IMP-004: Phased CMakeLists.txt with commented-out Phase 2/3/4 sections

**Status:** applied (2026-05-20, in `f509844`)
**Found:** 2026-05-20 (Phase 1 — original scaffold referenced files for all four phases at once)
**Location:** [CMakeLists.txt](CMakeLists.txt)
**Effort:** small
**Description.** The original scaffolded CMakeLists listed sources, headers, dependencies (PortAudio, KissFFT), and shader files for all four phases at once. Configure-time failures (`find_package(PortAudio REQUIRED)`, missing KissFFT vendored source, missing `.cpp` files for Phase 2/3/4) blocked Phase 1 from ever building.
**Proposal.** Restructure into clearly-marked Phase 1 / Phase 2 / Phase 3 / Phase 4 blocks. Each phase's sources, headers, and dependencies live in their own group. Phase 2+ blocks are commented out and ship with `# Phase N — uncomment when implementing …` markers, ready to flip on as each phase comes online.
**Trade-offs.** Slightly more visual noise in the build file. Strongly outweighed by the fact that Phase 1 builds without artificial barriers, and there's no risk of someone uncommenting one half of a Phase N dependency without the other.
**Notes.** Phase 2 was uncommented during commit `7b23f96`. Phase 3 (audio) and Phase 4 (scripting) blocks remain commented.

## Declined

*(none)*

## Deferred

*(none)*
