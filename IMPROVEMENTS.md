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

## Applied

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
