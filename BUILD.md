# Build

> Environment setup and build commands for Scintilla.
> If a step here is wrong, fix it in this file in the same commit as the workaround.

---

## Supported platforms

| Platform | Status | Notes |
|---|---|---|
| Linux (Ubuntu 22.04+ / Debian 12+) | Primary — development host (ThinkPad P15 Gen 2i) | |
| Linux (Fedora / Arch) | Expected to work | Untested as of 2026-05-20 |
| Windows 10 / 11 (MSVC 2022) | Planned | Untested |
| macOS 13+ (Apple Silicon) | Planned | Untested; PortAudio + Qt6 known to work |

---

## Toolchain

| Component | Minimum | Notes |
|---|---|---|
| C++ compiler | C++20 (GCC ≥ 11, Clang ≥ 14, MSVC 2022) | DEC-008 |
| CMake | 3.22 | DEC-008 |
| Ninja | 1.10 | Preferred generator |
| Qt | 6.2 | Modules: Core, Gui, Widgets, OpenGLWidgets |
| OpenGL driver | 4.3 Core profile | Hard requirement; no fallback (AV-009) |
| PortAudio | 19.7 | System package |
| KissFFT | bundled in-tree | MIT; vendored at `third_party/kissfft/` (not yet present — see HANDOVER) |
| Python | 3.10 + NumPy | Required for preset runtime (deferred until Phase 4) |

---

## Dependencies — install per platform

### Ubuntu / Debian

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake ninja-build \
    qt6-base-dev qt6-base-dev-tools libqt6opengl6-dev \
    libgl1-mesa-dev \
    portaudio19-dev \
    python3 python3-numpy
```

### Fedora

```bash
sudo dnf install -y \
    gcc-c++ cmake ninja-build \
    qt6-qtbase-devel qt6-qtbase-private-devel \
    mesa-libGL-devel \
    portaudio-devel \
    python3 python3-numpy
```

### Arch

```bash
sudo pacman -S --needed \
    base-devel cmake ninja \
    qt6-base \
    portaudio \
    python python-numpy
```

### macOS (Homebrew)

```bash
brew install cmake ninja qt@6 portaudio numpy
# Tell CMake where to find Qt6:
export CMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
```

### Windows (MSVC + Qt online installer)

1. Install Visual Studio 2022 with the "Desktop development with C++" workload.
2. Install Qt 6.6+ via the official Qt online installer; select MSVC 2022 64-bit.
3. Install CMake and Ninja (e.g. via `winget install Kitware.CMake Ninja-build.Ninja`).
4. Install PortAudio from source or vcpkg (`vcpkg install portaudio`).
5. Set `CMAKE_PREFIX_PATH` to your Qt install (e.g. `C:\Qt\6.6.0\msvc2022_64`).

---

## Vendoring KissFFT

KissFFT is not packaged in distro repos; vendor it into the source tree.

```bash
cd third_party
mkdir -p kissfft && cd kissfft
curl -L https://raw.githubusercontent.com/mborgerding/kissfft/master/kiss_fft.h -o kiss_fft.h
curl -L https://raw.githubusercontent.com/mborgerding/kissfft/master/kiss_fft.c -o kiss_fft.c
curl -L https://raw.githubusercontent.com/mborgerding/kissfft/master/LICENSES/BSD-3-Clause -o LICENSE
```

The CMakeLists already references `third_party/kissfft/`; once these files exist, the audio pipeline will link cleanly.

---

## Build commands

From the repo root:

### Debug (default)

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/Scintilla
```

### Release

```bash
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
./build-release/Scintilla
```

### Clean rebuild

```bash
rm -rf build
cmake -B build -G Ninja
cmake --build build
```

---

## Running tests

No test harness is wired up yet (TESTING.md will appear once it is). The prototype at `prototype/index.html` is the canonical renderer reference and can be opened in any modern browser without a build step.

---

## Troubleshooting

**`CMake Error: Could not find Qt6`**
Set `CMAKE_PREFIX_PATH` to your Qt install dir, e.g. `export CMAKE_PREFIX_PATH=/opt/Qt/6.6.0/gcc_64`.

**App opens, viewport is black, console says "GL version too old"**
Your driver does not expose OpenGL 4.3 Core. This is a hard requirement (DEC-008 / AV-009); update your driver or run on a machine with a GPU that supports 4.3+.

**`undefined reference to kiss_fft_alloc`**
KissFFT not vendored — see "Vendoring KissFFT" above.

**`OSError: PortAudio not found`** (at runtime, Linux)
Install `libportaudio2` (the runtime library, separate from the `-dev` package).

**Preset loads but every frame is blank**
The preset runtime probably failed to find NumPy. Run `python3 -c "import numpy"` to confirm; install it via your platform's package manager.
