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
