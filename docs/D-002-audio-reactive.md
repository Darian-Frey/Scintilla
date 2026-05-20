# D-002 — Audio Reactive Feature

> **Status**: Active
> **Provenance**: Shane Hartley · Claude
> **Last reviewed**: 2026-05-19
> **Why this status**: Feature agreed for v1.1; decisions made, ready for implementation

---

## Overview

The audio reactive feature allows the LED array to respond in real time to audio playing on
the computer — not the microphone input, but the loopback of the system output mix. It runs
as a parallel mode alongside the normal frame editor; the timeline is preserved and can be
written to during a reactive session ("capture" mode).

---

## DEC-009 · Audio capture library: PortAudio

**Date**: 2026-05-19
**Status**: Closed

**Decision**: Use PortAudio for audio capture.

**Rationale**: PortAudio abstracts over PulseAudio, PipeWire (via PulseAudio compat), and
ALSA. A single `Pa_OpenStream()` call against a monitor source (the loopback of the output
mix) works on Shane's PipeWire system via the libpulse compatibility layer. The alternative —
`libpulse` directly — gives more control but is ~300 lines of boilerplate for what is
essentially the same result. PortAudio is a single `find_package(PortAudio)` in CMake and
well understood.

**Monitor source on Linux**: PipeWire exposes the output mix as a `monitor` capture device.
The user selects it in a device picker at first launch; the choice is saved to QSettings.

**Consequence**: Add to CMakeLists.txt:
```cmake
find_package(PortAudio REQUIRED)
target_link_libraries(Scintilla PRIVATE PortAudio::PortAudio)
```

---

## DEC-010 · FFT library: KissFFT (vendored)

**Date**: 2026-05-19
**Status**: Closed

**Decision**: Use KissFFT, vendored as `third_party/kissfft/`.

**Rationale**: KissFFT is two files (`kiss_fft.h`, `kiss_fft.c`) and MIT licensed. It handles
complex FFT at the sizes needed (512–2048 point) at well above 60fps on any modern CPU.
FFTW3 is faster but requires a separate install and LGPL licence management. KissFFT vendored
means zero external dependency for the FFT path — `git subtree add` or just copy the two
files.

**FFT size**: 1024 samples (≈23ms at 44100Hz). This gives 512 positive-frequency bins before
the Nyquist bin. Log-spaced mapping to N frequency bands (N = gridSize, 3–32).

**Windowing**: Hann window applied before FFT to reduce spectral leakage.

**Consequence**: Add to CMakeLists.txt:
```cmake
add_library(kissfft STATIC third_party/kissfft/kiss_fft.c)
target_include_directories(kissfft PUBLIC third_party/kissfft)
target_link_libraries(Scintilla PRIVATE kissfft)
```

---

## DEC-011 · Threading model: capture thread + signal bridge

**Date**: 2026-05-19
**Status**: Closed

**Decision**: Audio capture and FFT run on a dedicated `QThread`. Results are bridged to the
main thread via Qt signals (queued connection). The main thread drives LED updates from a
`QTimer` at the render rate (capped at 60fps).

**Architecture**:
```
[PortAudio callback] → ring buffer → [AudioWorker : QObject on QThread]
    → FFT → band array → signal bandsReady(BandData)
        → [AudioReactiveEngine on main thread]
            → mapToFrame(BandData, ReactiveMode, baseFrame)
                → VoxelFrame → CubeViewport::setReactiveFrame()
```

**Ring buffer**: Lock-free single-producer single-consumer (SPSC) ring buffer. The PortAudio
callback is the producer (runs on PortAudio's internal thread). The AudioWorker consumer
reads on the QThread. Use a simple power-of-two index buffer; no std::mutex in the callback.

**Rationale**: The PortAudio callback runs on a real-time thread; no Qt objects, no allocations,
no mutexes allowed in it. The ring buffer is the only safe bridge.

**Consequence**: `AudioWorker` must not touch any Qt GUI objects. `AudioReactiveEngine` on
the main thread is the only object that touches `CubeViewport`.

---

## DEC-012 · Reactive modes

**Date**: 2026-05-19
**Status**: Closed

**Decision**: Four reactive modes for v1.1, with a clear extension point for more.

```cpp
enum class ReactiveMode {
    Off,             // normal editor mode
    EqBars,          // frequency bands → columns (the classic equaliser)
    BeatPulse,       // onset detection → radial pulse from centre
    WaveformSlice,   // raw waveform → single axis oscilloscope
    SpectralColour,  // spectral centroid drives hue; amplitude drives brightness
};
```

**EqBars** — divide FFT into N log-spaced bands (N = gridSize). Each band drives a vertical
column of LEDs. Height = band magnitude (normalised). Colour: HSV hue mapped from band
index (bass = red/orange, highs = cyan/blue) or user-set colour per band.

**BeatPulse** — onset detection via high-pass filtered energy ratio. On beat: emit a sphere
of lit LEDs from the centre that expands outward over 4–8 frames, fading in brightness.
Colour: current spectral centroid → hue.

**WaveformSlice** — map one axis (user-selectable X, Y, or Z) to time. Each LED on that
axis represents one audio sample. Amplitude maps to a second axis offset, colour to sign.
Looks like a classic oscilloscope running through the cube.

**SpectralColour** — the whole shape fills with one colour. Hue = spectral centroid
(0Hz=red, 22kHz=violet). Saturation = constant. Brightness = RMS amplitude. Good for
ambient/drone music.

**Extension**: `ReactiveMode` is an enum class; new modes add a value and a case in
`AudioReactiveEngine::mapToFrame()`. No other changes needed.

---

## DEC-013 · Layer blend mode

**Date**: 2026-05-19
**Status**: Closed

**Decision**: Reactive output can either replace the current frame display or layer over it.
Three blend modes, user-selectable:

```cpp
enum class ReactiveBlend {
    Replace,        // reactive output replaces frame display entirely
    Additive,       // reactive colour added to base frame colour (clamped 255)
    Modulate,       // reactive amplitude multiplies base frame brightness
};
```

**Rationale**: Layering (Additive/Modulate) lets a user hand-paint a static pattern and have
the audio animate colour or brightness over it. Replace is the "clean slate" mode for pure
reactive use. Both are useful; the choice belongs to the user.

**Base frame**: When blend ≠ Replace, the base is the currently selected frame in the timeline
(frozen; timeline playback is paused during reactive mode).

---

## DEC-014 · Reactive capture (record to timeline)

**Date**: 2026-05-19
**Status**: Closed

**Decision**: A "Capture" button records the reactive output to the animation timeline in
real time, writing a new frame on each beat (BeatPulse mode) or on a fixed time interval
(other modes, interval = 1/fps seconds).

**Behaviour**:
1. User activates reactive mode + capture.
2. Each record tick: the current `VoxelFrame` computed by `AudioReactiveEngine` is
   deep-copied and appended to the timeline.
3. Capture stops when the user clicks Stop or the timeline reaches 500 frames (soft cap).
4. On stop: timeline jumps to the first captured frame; user can play back, edit, export.

**Rationale**: This turns the audio reactive feature into a generative animation tool —
the music writes the animation. Combined with GIF export (v1.1) it produces shareable
content directly from a performance.

**Consequence**: `AudioReactiveEngine` emits `frameReady(VoxelFrame)` on each tick.
`MainWindow` decides whether to display-only or append-to-timeline based on capture state.

---

## DEC-015 · Frequency band mapping: log-spaced

**Date**: 2026-05-19
**Status**: Closed

**Decision**: FFT bins are grouped into N log-spaced frequency bands, where N = gridSize
(3–32). Log spacing gives more bands to bass frequencies where musical content is richer.

**Band edges** (Hz) for N=8, 44100Hz sample rate, 1024-point FFT:
```
[20, 60, 120, 250, 500, 1000, 4000, 20000]
```
Computed as: `f_k = f_min * (f_max / f_min)^(k/N)` for k = 0..N.

**Magnitude normalisation**: rolling max per band with slow decay (τ = 3 seconds).
This ensures the visualisation fills the full LED height range regardless of input level,
without needing manual gain control.

**Consequence**: `FFTProcessor` exposes `std::array<float, 32> bands` (always 32 entries;
only the first `gridSize` are used). Fixed-size array avoids allocation in the audio path.

---

## New source files

```
src/
└── audio/
    ├── AudioWorker.h/.cpp        — QObject on QThread; PortAudio open/close, ring buffer consumer, FFT
    ├── FFTProcessor.h/.cpp       — KissFFT wrapper; windowing, band mapping, onset detection
    ├── AudioReactiveEngine.h/.cpp— Main-thread coordinator; mode dispatch; frame generation
    └── AudioDevicePicker.h/.cpp  — QDialog for selecting the monitor source at first launch

third_party/
└── kissfft/
    ├── kiss_fft.h
    └── kiss_fft.c
```

---

## CMakeLists.txt additions

```cmake
# PortAudio
find_package(PortAudio REQUIRED)
target_link_libraries(Scintilla PRIVATE PortAudio::PortAudio)

# KissFFT (vendored)
add_library(kissfft STATIC third_party/kissfft/kiss_fft.c)
target_include_directories(kissfft PUBLIC third_party/kissfft)
target_link_libraries(Scintilla PRIVATE kissfft)
```

---

## MainWindow changes

- Toolbar: add ReactiveMode dropdown + ReactiveBlend dropdown + Capture toggle button
- Left panel: add audio device selector button (opens AudioDevicePicker)
- `AudioReactiveEngine` owned by `MainWindow`; `frameReady` signal connected to
  `CubeViewport::setReactiveFrame()` (queued connection — engine is on main thread but
  the signal may fire at high rate; viewport batches to render rate via `QTimer`)

---

## CubeViewport changes

- Add `void setReactiveFrame(VoxelFrame frame)` slot
- When a reactive frame is set, it is displayed in place of the timeline frame
- Reactive frame is never written to the timeline unless capture is active
- Normal editor interaction (paint/erase) is disabled during reactive mode

---

## Deferred to v1.2

- MIDI clock sync (beat trigger via external MIDI instead of onset detection)
- Per-band colour mapping editor (custom colour gradient per frequency range)
- BPM tap tempo override
- Audio file playback (react to a file rather than live system audio)
