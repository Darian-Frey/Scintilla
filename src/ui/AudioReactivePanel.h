#pragma once

#include <QWidget>

#include "audio/AudioReactiveEngine.h"

class QComboBox;
class QSlider;
class QSpinBox;
class QLabel;
class QGroupBox;

// ── AudioReactivePanel ──────────────────────────────────────────────────────
//
// Single home for every audio-reactive control. Lives in a dock widget on the
// right side of MainWindow.
//
// Mode group:
//   - Reactive mode  (Off / EQ bars / Beat pulse / Waveform / Spectral)
//   - Blend mode     (Replace / Additive / Modulate)
//
// Tuning group (apply to every reactive mode):
//   - Sensitivity   — multiplier on band magnitudes after rolling-max
//   - Smoothing     — exposes FFTProcessor::setDecayPerFrame on a 0..100 scale
//   - Hue shift     — rotates the palette around the colour wheel
//
// Waveform group (only enabled when ReactiveMode::WaveformSlice is active):
//   - Scroll speed  — frames-per-row divider, 1..30, 30 = fastest
//
// The panel writes through to the engine and reads-back nothing — the engine
// is the source of truth.

class AudioReactivePanel : public QWidget {
    Q_OBJECT

public:
    explicit AudioReactivePanel(AudioReactiveEngine* engine, QWidget* parent = nullptr);

signals:
    // MainWindow connects to this so it can drive the device-pick / start /
    // stop / capture-action lifecycle without the panel knowing about them.
    void modeChanged(ReactiveMode mode);
    void blendChanged(ReactiveBlend blend);

private slots:
    void onModeIndex(int);
    void onBlendIndex(int);
    void onSensitivity(int);   // slider 0..200 → 0.0..2.0
    void onSmoothing(int);     // slider 0..100 → 0.95..0.9999
    void onHue(int);           // slider 0..359 (degrees)
    void onSpeed(int);

private:
    void buildLayout();

    AudioReactiveEngine* m_engine = nullptr;

    QComboBox*  m_modeCombo    = nullptr;
    QComboBox*  m_blendCombo   = nullptr;

    QSlider*    m_sensSlider   = nullptr;
    QLabel*     m_sensLabel    = nullptr;

    QSlider*    m_smoothSlider = nullptr;
    QLabel*     m_smoothLabel  = nullptr;

    QSlider*    m_hueSlider    = nullptr;
    QLabel*     m_hueLabel     = nullptr;

    QGroupBox*  m_waveformGroup = nullptr;
    QSpinBox*   m_speedSpin     = nullptr;
};
