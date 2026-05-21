# Scintilla

> **Status:** Active
> **Provenance:** Shane Hartley (owner) · Claude (architect, prototype, documentation)
> **Last reviewed:** 2026-05-20
> **Why this status:** Browser prototype validated; Qt6/C++20 desktop scaffold in place (DEC-008); Phase 1 (core renderer) is the next implementation milestone.

A 3D LED array designer that dances to your music. Spiritual successor to the physical
[8×8×8 LED Cube](https://github.com/Darian-Frey/LED_Cube) (2009), removing all hardware constraints — arbitrary grid sizes (3³–32³), full RGB per voxel, multiple array shapes, a frame-based animation timeline, music-reactive playback via PortAudio + KissFFT, and a Python preset scripting engine inspired by Winamp AVS.

## Features

- 3D voxel viewport with orbit/pan/zoom camera
- Four LED array shapes: Cube, Sphere, Cylinder, Pyramid
- Grid sizes 3³–32³ with instanced-mesh rendering
- Full RGB per LED; palette + recent-history colour picker
- Paint / Erase / Fill / Pick tools
- X/Y/Z slice view for layer editing
- Frame-based animation timeline with playback
- JSON save/load (v1.0 wire format)
- Music-reactive modes via PortAudio + KissFFT (planned, Phase 3)
- Python preset scripting with hot-reload (planned, Phase 4)

See [FEATURES.md](FEATURES.md) for the full capability list with priorities and acceptance criteria.

## Quick start

### Prototype (browser)

```bash
xdg-open prototype/index.html    # or open in any modern browser
```

No build step required; the prototype is the canonical renderer reference.

### Desktop build (in progress)

```bash
# Install dependencies — see BUILD.md for per-platform details
sudo apt install -y build-essential cmake ninja-build \
    qt6-base-dev libqt6opengl6-dev libgl1-mesa-dev \
    portaudio19-dev python3 python3-numpy

# Build and run
cmake -B build -G Ninja
cmake --build build
./build/Scintilla
```

The desktop build is in Phase 1 (core renderer); see [HANDOVER.md](HANDOVER.md) for the current implementation state and [ROADMAP.md](ROADMAP.md) for upcoming phases.

## Project structure

```text
.
├── CLAUDE.md, HANDOVER.md       ← AI handoff documents (current state, build order)
├── README.md, FEATURES.md, ROADMAP.md, CHANGELOG.md
├── ARCHITECTURE.md, DECISIONS.md, BUILD.md, ATTACK_VECTORS.md
├── CMakeLists.txt               ← build system (Qt6, PortAudio, KissFFT)
├── LICENSE                      ← MIT (DEC-026)
├── docs/
│   ├── SPEC.md                  ← full feature specification
│   └── D-00N-*.md               ← thematic decision logs (DEC-001 … DEC-023)
├── src/                         ← C++20 source tree
│   ├── core/                    ← VoxelFrame, ShapeMask, AnimationTimeline, JsonSerializer
│   ├── renderer/                ← CubeViewport (QOpenGLWidget), shaders, OrbitCamera
│   ├── audio/                   ← PortAudio + KissFFT pipeline
│   ├── scripting/               ← Python preset runner (QProcess)
│   └── ui/                      ← Qt widgets (colour picker, timeline, slice control)
├── presets/                     ← Python preset runtime
│   ├── led_cube/                ← preset base class + CubeProxy
│   ├── builtin/                 ← shipped presets (target: 20)
│   └── user/                    ← user-authored presets
└── prototype/index.html         ← Three.js r128 reference prototype
```

## Documentation

| File | Purpose |
| --- | --- |
| [CLAUDE.md](CLAUDE.md) | AI development handoff — current state, conventions, invariants |
| [HANDOVER.md](HANDOVER.md) | Recommended build order, missing-files inventory |
| [FEATURES.md](FEATURES.md) | Capability list with priorities and acceptance criteria (F-NNN) |
| [ROADMAP.md](ROADMAP.md) | Phased development plan (Phase 0 … Phase 5+) |
| [ARCHITECTURE.md](ARCHITECTURE.md) | System structure, module responsibilities, threading model |
| [DECISIONS.md](DECISIONS.md) | Indexed log of design decisions (DEC-NNN), reversal conditions |
| [BUILD.md](BUILD.md) | Toolchain, dependencies, build commands, troubleshooting |
| [ATTACK_VECTORS.md](ATTACK_VECTORS.md) | Project-specific failure modes (AV-NNN), detection methods |
| [BUGS.md](BUGS.md) | Catalogue of realised bugs (BUG-NNN), open / fixed / wontfix / deferred |
| [IMPROVEMENTS.md](IMPROVEMENTS.md) | Catalogue of code-quality improvements (IMP-NNN), suggested / applied / declined / deferred |
| [CHANGELOG.md](CHANGELOG.md) | Version history |
| [docs/SPEC.md](docs/SPEC.md) | Authoritative feature specification |

## License

MIT — see [LICENSE](LICENSE) and [DECISIONS.md](DECISIONS.md) DEC-026.
