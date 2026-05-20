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
