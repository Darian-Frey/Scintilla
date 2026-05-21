# Decisions

Append-only log of significant design decisions for Scintilla.
Each entry: `DEC-NNN`, with Decided and Recorded dates (ISO 8601), status, context, alternatives, decision, consequences, and reversal conditions.

**Status vocabulary:** Proposed | Accepted | Superseded by DEC-NNN | Deprecated.

---

## Index

The early decisions (DEC-001 … DEC-023) live in three thematic files under [`docs/`](docs/), grouped by subsystem. Newer decisions (DEC-024 onward) live directly in this file. Process-level and cross-cutting decisions also live here.

| ID | Title | Status | Source |
|---|---|---|---|
| DEC-001 | Instanced mesh rendering only | Accepted | [docs/D-001-decisions.md](docs/D-001-decisions.md) |
| DEC-002 | Spherical LED geometry (r=0.38) | Accepted | [docs/D-001-decisions.md](docs/D-001-decisions.md) |
| DEC-003 | Sparse JSON voxel encoding | Accepted | [docs/D-001-decisions.md](docs/D-001-decisions.md) |
| DEC-004 | Slice filter is view-only | Accepted | [docs/D-001-decisions.md](docs/D-001-decisions.md) |
| DEC-005 | Grid size cap at 32³ | Accepted | [docs/D-001-decisions.md](docs/D-001-decisions.md) |
| DEC-006 | Shape change clears animation data | Accepted | [docs/D-001-decisions.md](docs/D-001-decisions.md) |
| DEC-007 | Camera uses spherical coordinates | Accepted | [docs/D-001-decisions.md](docs/D-001-decisions.md) |
| DEC-008 | Platform target Qt6/C++20 | Accepted | [docs/D-001-decisions.md](docs/D-001-decisions.md) |
| DEC-009 … DEC-015 | Audio pipeline architecture | Accepted | [docs/D-002-audio-reactive.md](docs/D-002-audio-reactive.md) |
| DEC-016 … DEC-023 | Preset scripting system | Accepted | [docs/D-003-preset-scripting.md](docs/D-003-preset-scripting.md) |
| DEC-024 | Decisions split across thematic files | Accepted | this file |
| DEC-025 | `HANDOVER.md` as project-specific extension | Accepted | this file |
| DEC-026 | MIT licence | Accepted | this file |
| DEC-027 | Adopt `BUGS.md` and `IMPROVEMENTS.md` per updated standard | Accepted | this file |
| DEC-028 | Re-tune lit-LED radius to 0.095 for the Fresnel-glow renderer | Accepted | this file |

---

## Cross-cutting decisions

### DEC-024 Decisions split across thematic files

**Decided:** 2026-05-19
**Recorded:** 2026-05-20
**Status:** Accepted
**Authors:** Claude (initial scaffolding session); confirmed by Shane Hartley 2026-05-20
**Related:** the documentation standard (`development_documentation.md` §Maintenance Rules)

**Context.** The `development_documentation.md` standard prescribes a single `DECISIONS.md` file with stable `D-NNN` IDs. The initial scaffolding session captured decisions across three thematic files (`docs/D-001-decisions.md`, `docs/D-002-audio-reactive.md`, `docs/D-003-preset-scripting.md`) using `DEC-NNN` IDs. By the time the standard was being applied recursively, those three files were already mature and reviewed.

**Options.**
- **A. Migrate all 23 existing entries into a single top-level `DECISIONS.md`.** Cleanest match to the standard. Rejected: large rewrite, breaks any external references to the split files, loses the useful thematic grouping that maps onto the subsystem layout.
- **B. Leave the three files as the only decision log; do not create a top-level `DECISIONS.md`.** Cheapest. Rejected: external readers (humans and tooling) will look for `DECISIONS.md` at repo root by convention; the absence is friction.
- **C. Top-level `DECISIONS.md` as a thin index** pointing into the three thematic files for early entries, with cross-cutting and new entries (DEC-024 onward) living in the index itself. Chosen.

**Decision.** Option C. The top-level file is the canonical entry point; the three thematic files remain the authoritative source for their respective entry ranges and continue to receive subsystem-specific additions if needed. New cross-cutting decisions land here.

**Consequences.**
- Globally unique `DEC-NNN` IDs are preserved (no renumbering).
- The index must be kept in sync when new entries are added to the thematic files.
- The standard's "single DECISIONS.md" prescription is loosened in spirit but the audit role is preserved — every decision is reachable from the top-level file in one click.

**Reversal conditions.** Revisit if (a) the cross-reference between top-level index and thematic files goes stale more than once during normal development, or (b) the project adopts an external ADR tool that requires a single source file.

---

### DEC-025 `HANDOVER.md` as project-specific extension

**Decided:** 2026-05-19
**Recorded:** 2026-05-20
**Status:** Accepted
**Authors:** Claude (initial scaffolding); confirmed by Shane Hartley 2026-05-20
**Related:** `CLAUDE.md`, `ROADMAP.md`, `CHANGELOG.md`, `development_documentation.md` §Evolution / Project-specific extensions

**Context.** `HANDOVER.md` exists in the repo root and documents the handover from the initial Claude chat session to Claude Code. The documentation standard does not reserve this name; per the standard's "Project-specific extensions" clause, new document types must be recorded here with a reason.

**Options.**
- **A. Delete `HANDOVER.md` and fold its content into `CLAUDE.md` + `ROADMAP.md` + `CHANGELOG.md`.** Cleanest standard compliance. Rejected: the handover narrative has value as a frozen point-in-time artifact; folding it spreads context across three files.
- **B. Keep `HANDOVER.md` as a distinct document with a defined role.** Chosen.

**Decision.** Option B. `HANDOVER.md` documents the *first* handover from chat-session Claude to Claude Code, and is updated in place at the end of each subsequent significant Claude Code session (see its own §"How to update this file"). It overlaps with `CLAUDE.md` but plays a different role: `CLAUDE.md` is the current-state operating contract; `HANDOVER.md` is the chronological narrative of how state arrived at where it is.

**Consequences.**
- Two living documents must be kept consistent (`HANDOVER.md` and `CLAUDE.md`). Discipline: `CLAUDE.md` is authoritative for current state; `HANDOVER.md` is authoritative for the recommended build order and missing-files inventory.
- The standard's reserved-names list (README, FEATURES, CLAIMS, DECISIONS, ARCHITECTURE, SPEC, ATTACK_VECTORS, BUILD, CHANGELOG, CLAUDE, ROADMAP, VOCABULARY, TESTING, SECURITY, CONTRIBUTING, BENCHMARKS, CITATION) is not violated.

**Reversal conditions.** Revisit if (a) `HANDOVER.md` and `CLAUDE.md` diverge enough to actively mislead readers, or (b) the build order in `HANDOVER.md` is fully delivered and the narrative is no longer load-bearing — at that point HANDOVER folds into `CHANGELOG.md` and the file is retired.

---

### DEC-026 MIT licence

**Decided:** 2026-05-20
**Recorded:** 2026-05-20
**Status:** Accepted
**Authors:** Shane Hartley
**Related:** `LICENSE`

**Context.** `HANDOVER.md` carried "Licence: TBD — Shane to decide — Apache 2.0 or MIT consistent with other repos" as an open question. The documentation standard's Tier 1 requires picking one before first public commit.

**Options.**
- **A. Apache 2.0.** Includes explicit patent grant and contributor-licensing terms. Better for projects that may attract external contributions or downstream commercial use that wants patent-grant clarity. Rejected as overkill for a hobbyist tool.
- **B. MIT.** Minimal, permissive, widely understood. Chosen.
- **C. Defer.** Rejected — Tier 1 requires picking one before first public commit, and a public commit is imminent.

**Decision.** Option B. The MIT licence is the best fit for Scintilla — short, permissive, and aligned with the project's hobbyist / artist-tool framing.

**Consequences.**
- Anyone may use, modify, and redistribute Scintilla, including in commercial products, subject only to keeping the copyright notice and disclaimer.
- KissFFT (BSD-3-Clause) and PortAudio (MIT) are licence-compatible — no friction when those are vendored.
- The Qt6 dependency is LGPLv3, which obliges dynamic linking and the standard LGPL "user can replace the library" clause; this is satisfied by the default Qt-installed-as-shared-library build.

**Reversal conditions.** Revisit only if (a) a contribution-policy issue arises that an explicit patent grant would resolve, or (b) the project takes a corporate funding model that requires CLA-style governance.

---

### DEC-027 Adopt `BUGS.md` and `IMPROVEMENTS.md` per updated standard

**Decided:** 2026-05-21
**Recorded:** 2026-05-21
**Status:** Accepted
**Authors:** Shane Hartley
**Related:** `BUGS.md`, `IMPROVEMENTS.md`, `development_documentation.md` 2026-05-21 revision, Maintenance Rule 8 ("log when found, not silently acted on")

**Context.** The `development_documentation.md` standard was revised on 2026-05-21 to add two new Tier 2 document types — `BUGS.md` (realised failures, backward-looking, `BUG-NNN` IDs) and `IMPROVEMENTS.md` (candidate refactors, `IMP-NNN` IDs) — plus a new Maintenance Rule 8 making the "log discoveries, don't silently act" discipline explicit. The rule is called out as load-bearing for AI-partner workflows specifically, which describes Scintilla precisely (solo developer + Claude Code).

**Options.**

- **A. Adopt both documents now and backfill from the existing commit history.** Chosen. The friction test passes immediately: Phase 1 and Phase 2 already produced 7 bugs and 4 internal refactors that would otherwise survive only as commit-message footnotes. Future revival or audit benefits from the catalogue.
- **B. Defer until the first uncatalogued bug or improvement is missed.** Rejected — the recall window for the existing commit history is already closing; the longer we wait, the more detail we lose.
- **C. Adopt the rule but skip the documents and rely on commit messages alone.** Rejected — commit messages aren't indexed by stable IDs, can't be referenced from CHANGELOG / DECISIONS / ATTACK_VECTORS cross-links, and don't track status (open / wontfix / deferred).

**Decision.** Option A. Created `BUGS.md` and `IMPROVEMENTS.md` at repo root, backfilled `BUG-001…BUG-007` and `IMP-001…IMP-004` from `b5e448b`, `f509844`, and `7b23f96`. `IMP-005` and `IMP-006` added as forward-looking Suggested entries surfaced during the audit. Maintenance Rule 8 is now binding for all future sessions — see the corresponding update in `CLAUDE.md` and the matching feedback memory.

**Consequences.**

- BUGS / IMPROVEMENTS are now part of the cross-reference graph alongside FEATURES, DECISIONS, ATTACK_VECTORS, CLAIMS. Future entries should link both directions.
- The `BUG-` and `IMP-` ID series join the append-only ID rule (Maintenance Rule 3).
- CHANGELOG entries should reference BUG- and IMP- IDs where applicable (CHANGELOG `### Fixed` for bugs, `### Changed` for applied improvements).
- The reserved-names list in DEC-025 (extensions clause) implicitly extends to include `BUGS` and `IMPROVEMENTS` — they're standard Tier 2 names now, not extensions.

**Reversal conditions.** Revisit if (a) the catalogues drift out of date faster than they're maintained (the standard's own discipline failure mode), or (b) Scintilla transitions to using an external bug tracker (GitHub Issues, Linear) as authoritative, at which point BUGS.md becomes redundant per the cost-note friction test.

---

### DEC-028 Re-tune lit-LED radius to 0.095 for the Fresnel-glow renderer

**Decided:** 2026-05-21
**Recorded:** 2026-05-21
**Status:** Accepted
**Authors:** Shane Hartley
**Related:** DEC-002 (original 0.38 value, now partially superseded), `src/renderer/shaders/led.vert`, [ATTACK_VECTORS.md](ATTACK_VECTORS.md) AV-003, [CLAUDE.md](CLAUDE.md) §Notes for Claude Code

**Context.** DEC-002 fixed the lit-LED sphere radius at 0.38, tuned visually against the Phase 1 Lambert+specular shader. Phase 3 introduced a Fresnel-driven additive-glow shader (`led.frag`, see commit `11a9685` discussion) which gives a much more LED-like appearance — bright die at the centre, soft halo. Under that shader the 0.38 sphere felt visually too large compared to the ghost dots (0.17), even with the Fresnel falloff making most of the geometric area dim. Shane requested a re-tune to ~25 % of the original size.

**Options.**

- **A. Leave the sphere at 0.38 and tune the Fresnel exponents to compress the visible bright area further.** Preserves DEC-002 literally. Rejected: the additive glow halo derives from the geometric extent, so the *visible* glow is bounded by the sphere; aggressive Fresnel compression also kills the halo.
- **B. Reduce the sphere radius to 25 % of the original (0.095).** Chosen. Smaller geometric extent means a smaller, sharper bright core, surrounded by the ghost (0.17) as a "dome" — directly evoking real LED hardware where the die sits inside a translucent plastic body. The additive halo still extends naturally from the smaller sphere; in practice the halo is small but visible, and overlapping LEDs still accumulate brightness.
- **C. Reduce both lit and ghost radii proportionally.** Rejected: would compound the visual change. The ghost size already feels right; only the lit size was the complaint.

**Decision.** Option B. `kRadius` in `src/renderer/shaders/led.vert` changes from 0.38 to 0.095. Ghost radius (`kGhostRadius` in `ghost.vert`) stays at 0.17 unchanged. Segment counts (9×7 on, 6×5 ghost) and sphere geometry choice from DEC-002 also stay unchanged — DEC-028 supersedes only the radius value.

**Consequences.**

- Lit LEDs are now visually *smaller* than the ghost dots underneath them — the "die inside a dome" effect. Turning on an LED makes it appear as a tiny bright spot inside the existing larger faint ghost, rather than replacing it with a bigger sphere.
- The additive glow halo is smaller in absolute terms. Overlapping lit LEDs still merge but cover less screen area.
- AV-003 ("LED radius drift from 0.38") needs updating to reflect the new 0.095 bound, otherwise the next reviewer will treat 0.38 as the correct value and the new value as drift.
- CLAUDE.md's "Notes for Claude Code" line ("LED radius (0.38) … treat as constants") needs updating to the new value plus a DEC-028 reference.

**Reversal conditions.** Revisit if (a) the new size feels too small at large grid sizes (the geometric extent doesn't scale with grid), (b) a future bloom / post-processing pipeline lands and the per-fragment glow technique stops being load-bearing, or (c) interactive testing across multiple hardware setups shows the smaller radius reads as "broken pixels" rather than "LED dies" on lower-DPI displays.
