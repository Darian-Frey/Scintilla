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

### Changed
- Project renamed from "LED Cube VX" to **Scintilla** across all documentation and source (2026-05-20).
- Starter files reorganised from flat root into target tree (`src/`, `docs/`, `presets/`, `prototype/`).
- IMP-001 applied — `buildSphereGeometry` takes `SphereGeo&` output parameter for clarity.
- IMP-002 applied — `m_initialized` flag + `uploadScene()` helper enables `setMask` before `initializeGL`.
- IMP-003 applied — `CMAKE_EXPORT_COMPILE_COMMANDS ON` for clangd / IDE Qt header resolution.
- IMP-004 applied — CMakeLists restructured into phased blocks (Phase 2/3/4 sections preserved as commented).

### Fixed

- BUG-001: anchored `core` / `core.*` gitignore patterns to repo root so they no longer match `src/core/`.
- BUG-002: added `<cstdint>` to `VoxelFrame.h`; `uint8_t` now resolves under any consuming TU.
- BUG-003: removed bogus `explicit` on `VoxelFrame` copy ctor; added defaulted move ctor / op=.
- BUG-004: `qt_add_resources` now uses `BASE "src/renderer/shaders"`; shaders embed at `:/shaders/<name>` as expected.
- BUG-005: ghost LEDs now always render regardless of slice (SPEC §3.7 conformance); picker consults slice values directly instead of using `ghost.scale` as a hidden-state proxy (DEC-004 still upheld).
- BUG-006: `MainWindow` Reset Camera action now uses Qt 6's non-deprecated `addAction(text, shortcut, obj, slot)` parameter order.
- BUG-007: superscript `³` in info-panel and status-bar format strings no longer parses as Unicode digit; built via concatenation around the format placeholder.
