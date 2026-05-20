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

    [[nodiscard]] ReactiveMode  mode()  const { return m_mode; }
    [[nodiscard]] ReactiveBlend blend() const { return m_blend; }
    [[nodiscard]] bool isRunning()      const { return m_running; }

    // ── Device enumeration (call before start) ────────────────────────────────
    struct DeviceInfo { int index; QString name; bool isMonitor; };
    [[nodiscard]] static QList<DeviceInfo> enumerateDevices();

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    void start(int deviceIndex, float sampleRate = 44100.0f);
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
    [[nodiscard]] VoxelFrame modeWaveformSlice(const BandData& d) const;
    [[nodiscard]] VoxelFrame modeSpectralColour(const BandData& d) const;

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

    // Worker thread
    QThread*     m_thread = nullptr;
    AudioWorker* m_worker = nullptr;
};
