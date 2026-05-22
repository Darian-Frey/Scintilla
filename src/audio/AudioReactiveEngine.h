#pragma once

#include <QObject>
#include <QThread>
#include <memory>
#include "AudioWorker.h"
#include "core/AnimationTimeline.h"
#include "core/ShapeMask.h"
#include "core/VoxelFrame.h"

// ── ReactiveMode and ReactiveBlend (DEC-012, DEC-013) ─────────────────────────

enum class ReactiveMode {
    Off,             // normal editor; engine does nothing
    EqBars,          // frequency bands → vertical LED columns
    BeatPulse,       // onset detection → radial expanding sphere
    WaveformSlice,   // raw waveform → single-axis oscilloscope
    SpectralColour,  // spectral centroid → hue; RMS → brightness
    RadialEq,        // bands → concentric shells from cube centre outward
    Tunnel,          // current spectrum at Z=n-1; past frames recede to Z=0
    EnergyFloor,     // RMS-driven wall rises from Y=0; X axis coloured by band
};

enum class ReactiveBlend {
    Replace,         // reactive output replaces frame display
    Additive,        // reactive colour + base frame colour (clamped 255)
    Modulate,        // reactive amplitude × base frame brightness
};

// ── AudioReactiveEngine ───────────────────────────────────────────────────────
//
// Lives on the main thread. Owns the AudioWorker + QThread.
// Receives BandData via queued signal, maps it to a VoxelFrame based on the
// current ReactiveMode and ReactiveBlend, and emits frameReady().
//
// CubeViewport connects to frameReady() to update its reactive display.
// MainWindow decides whether to append to timeline (capture mode, DEC-014).

class AudioReactiveEngine : public QObject {
    Q_OBJECT

public:
    explicit AudioReactiveEngine(QObject* parent = nullptr);
    ~AudioReactiveEngine() override;

    // ── Configuration ─────────────────────────────────────────────────────────
    void setMask(std::shared_ptr<const ShapeMask> mask);
    void setMode(ReactiveMode mode);
    void setBlend(ReactiveBlend blend);
    void setBaseFrame(const VoxelFrame& frame);  // used when blend ≠ Replace
    void setHueOffset(float hue);                // manual colour rotation (0–360)
    void setSensitivity(float s);                // multiplier on band magnitudes (default 1.0)
    void setDecayPerFrame(float decay);          // band rolling-max decay (0.5–0.9999)

    [[nodiscard]] float hueOffset()  const { return m_hueOffset;   }
    [[nodiscard]] float sensitivity() const { return m_sensitivity; }

    // Waveform scroll speed (1 = slowest, 30 = fastest = "shift every audio
    // frame"). Internally stored as a frames-per-row divider; the newest row
    // refreshes every frame regardless of divider so the live sample stays
    // current.
    void setWaveformSpeed(int speed);
    [[nodiscard]] int waveformSpeed() const;

    [[nodiscard]] ReactiveMode  mode()  const { return m_mode; }
    [[nodiscard]] ReactiveBlend blend() const { return m_blend; }
    [[nodiscard]] bool isRunning()      const { return m_running; }

    // ── Device enumeration (call before start) ────────────────────────────────
    struct DeviceInfo {
        int     index;
        QString name;
        QString hostApi;               // ALSA, PulseAudio, JACK, ASIO, …
        bool    isMonitor;             // PipeWire / PulseAudio monitor source
        bool    recommended;           // known-good for music capture on this platform
        double  defaultSampleRate;     // device's preferred input rate
    };
    [[nodiscard]] static QList<DeviceInfo> enumerateDevices();

    // Sample rates this device actually supports (subset of the common-rates
    // probe set 22050 / 32000 / 44100 / 48000 / 88200 / 96000 / 192000).
    // Returns an empty list if the device doesn't exist or PortAudio fails
    // to initialise.
    [[nodiscard]] static QList<int> supportedSampleRates(int deviceIndex);

    // ── PulseAudio / PipeWire monitor source routing ─────────────────────────
    //
    // PortAudio's ALSA host API can't see PulseAudio monitor sources
    // directly. We enumerate them via `pactl list sources` and use
    // `pactl set-default-source` to redirect "default" to a chosen monitor
    // before opening the PortAudio stream. This is the only reliable way to
    // capture the system audio mix on Ubuntu / PipeWire without a custom
    // PortAudio build.
    struct MonitorSource {
        // PulseAudio's per-source state — tells the user whether picking
        // this one will actually receive any audio. Suspended monitors are
        // dead buckets until the OS routes audio to their sink.
        enum class State { Unknown, Running, Idle, Suspended };

        QString name;        // e.g. "alsa_output.pci-0000_00_1f.3.analog-stereo.monitor"
        QString description; // e.g. "Monitor of Built-in Audio Analog Stereo"
        bool    isDefaultSink = false;   // monitor of the current pactl default sink
        State   state         = State::Unknown;
    };
    [[nodiscard]] static bool                isMonitorRoutingSupported();
    [[nodiscard]] static QList<MonitorSource> enumerateMonitors();

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    // monitorSource: if non-empty, pactl set-default-source <name> is run
    // before PortAudio opens, and deviceIndex is replaced with the resolved
    // "default" device. Empty = use deviceIndex as the literal PortAudio device.
    void start(int deviceIndex, float sampleRate = 44100.0f,
               const QString& monitorSource = QString());
    void stop();

    // ── Capture mode (DEC-014) ────────────────────────────────────────────────
    void startCapture(AnimationTimeline* timeline);
    void stopCapture();
    [[nodiscard]] bool isCapturing() const { return m_capturing; }

public slots:
    void onBands(BandData data);   // queued from AudioWorker

signals:
    void frameReady(VoxelFrame frame);      // connect to CubeViewport::setReactiveFrame
    void frameCaptured(VoxelFrame frame);   // connect to MainWindow for timeline append
    void beatDetected();                    // connect to any beat-flash UI
    void errorOccurred(QString message);

private:
    // ── Mode implementations (each returns a VoxelFrame) ─────────────────────
    [[nodiscard]] VoxelFrame modeEqBars(const BandData& d) const;
    [[nodiscard]] VoxelFrame modeBeatPulse(const BandData& d);
    [[nodiscard]] VoxelFrame modeWaveformSlice(const BandData& d);   // mutates m_waveformHistory
    [[nodiscard]] VoxelFrame modeSpectralColour(const BandData& d) const;
    [[nodiscard]] VoxelFrame modeRadialEq(const BandData& d) const;
    [[nodiscard]] VoxelFrame modeTunnel(const BandData& d);          // mutates m_tunnelHistory
    [[nodiscard]] VoxelFrame modeEnergyFloor(const BandData& d) const;

    // Applies blend to combine reactive frame with base frame (DEC-013)
    [[nodiscard]] VoxelFrame applyBlend(VoxelFrame reactive) const;

    // Colour helpers
    [[nodiscard]] static std::array<uint8_t,3> hsvToRgb(float h, float s, float v);
    [[nodiscard]] static std::array<uint8_t,3> bandColor(int bandIdx, int totalBands,
                                                          float magnitude, float hueOffset);

    // ── State ─────────────────────────────────────────────────────────────────
    std::shared_ptr<const ShapeMask> m_mask;
    ReactiveMode  m_mode      = ReactiveMode::Off;
    ReactiveBlend m_blend     = ReactiveBlend::Replace;
    VoxelFrame    m_baseFrame;
    float         m_hueOffset = 0.0f;
    float         m_sensitivity = 1.0f;
    bool          m_running   = false;
    bool          m_capturing = false;
    AnimationTimeline* m_captureTarget = nullptr;

    // BeatPulse state — expanding sphere animation
    struct PulseState {
        float radius  = 0.0f;
        float brightness = 1.0f;
        std::array<uint8_t,3> color = {255,255,255};
        bool active = false;
    };
    PulseState m_pulse;

    // WaveformSlice state — rolling history (DEC-012 "map one axis to time").
    // m_waveformHistory[z][x] = y height for sample at age z, band index x.
    // -1 means no voxel at that cell. z=0 is oldest, z=gridSize-1 is newest.
    std::vector<std::vector<int>> m_waveformHistory;
    int m_waveformScrollDivider = 1;   // frames per row shift; 1 = shift every frame
    int m_waveformFrameCounter  = 0;   // counts up to the divider

    // Tunnel mode state — circular buffer of N band snapshots, one per Z
    // slice. m_tunnelHead points to the slot where the next frame will be
    // written. Memory cost is gridSize × 32 floats; trivial.
    std::vector<std::array<float, 32>> m_tunnelHistory;
    int m_tunnelHead = 0;

    // Worker thread
    QThread*     m_thread = nullptr;
    AudioWorker* m_worker = nullptr;
};
