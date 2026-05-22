#include "AudioReactivePanel.h"

#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
    // Smoothing slider 0..100 ↔ decay [0.95, 0.9999] linearly. 0.95 is "snappy
    // bass-only response" and 0.9999 is "slow ambient floor". Linear in decay
    // space is non-uniform in half-life but is the most readable mapping.
    constexpr float kDecayMin = 0.95f;
    constexpr float kDecayMax = 0.9999f;

    float smoothingToDecay(int s) {
        const float t = std::clamp(s, 0, 100) / 100.0f;
        return kDecayMin + t * (kDecayMax - kDecayMin);
    }
}

// ── Construction ─────────────────────────────────────────────────────────────

AudioReactivePanel::AudioReactivePanel(AudioReactiveEngine* engine, QWidget* parent)
    : QWidget(parent), m_engine(engine) {
    buildLayout();
}

void AudioReactivePanel::buildLayout() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    // ── Mode group ──────────────────────────────────────────────────────────
    auto* modeBox = new QGroupBox(tr("Mode"), this);
    auto* modeForm = new QFormLayout(modeBox);

    m_modeCombo = new QComboBox(modeBox);
    m_modeCombo->addItem(tr("Off"),             static_cast<int>(ReactiveMode::Off));
    m_modeCombo->addItem(tr("EQ bars"),         static_cast<int>(ReactiveMode::EqBars));
    m_modeCombo->addItem(tr("Beat pulse"),      static_cast<int>(ReactiveMode::BeatPulse));
    m_modeCombo->addItem(tr("Waveform"),        static_cast<int>(ReactiveMode::WaveformSlice));
    m_modeCombo->addItem(tr("Spectral colour"), static_cast<int>(ReactiveMode::SpectralColour));
    m_modeCombo->addItem(tr("Radial EQ"),       static_cast<int>(ReactiveMode::RadialEq));
    m_modeCombo->addItem(tr("Tunnel"),          static_cast<int>(ReactiveMode::Tunnel));
    m_modeCombo->addItem(tr("Energy floor"),    static_cast<int>(ReactiveMode::EnergyFloor));
    m_modeCombo->addItem(tr("Python preset"),   static_cast<int>(ReactiveMode::PythonPreset));
    modeForm->addRow(tr("Reactive:"), m_modeCombo);
    connect(m_modeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &AudioReactivePanel::onModeIndex);

    m_blendCombo = new QComboBox(modeBox);
    m_blendCombo->addItem(tr("Replace"),  static_cast<int>(ReactiveBlend::Replace));
    m_blendCombo->addItem(tr("Additive"), static_cast<int>(ReactiveBlend::Additive));
    m_blendCombo->addItem(tr("Modulate"), static_cast<int>(ReactiveBlend::Modulate));
    modeForm->addRow(tr("Blend:"), m_blendCombo);
    connect(m_blendCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &AudioReactivePanel::onBlendIndex);

    root->addWidget(modeBox);

    // ── Tuning group ────────────────────────────────────────────────────────
    auto* tuneBox = new QGroupBox(tr("Tuning"), this);
    auto* tuneForm = new QFormLayout(tuneBox);

    auto makeSliderRow = [&](const QString& tip, int min, int max, int initial,
                              QSlider*& slider, QLabel*& valueLabel) {
        auto* row = new QWidget(tuneBox);
        auto* h   = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        slider = new QSlider(Qt::Horizontal, row);
        slider->setRange(min, max);
        slider->setValue(initial);
        slider->setToolTip(tip);
        valueLabel = new QLabel(row);
        valueLabel->setMinimumWidth(48);
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        h->addWidget(slider, 1);
        h->addWidget(valueLabel);
        return row;
    };

    // Sensitivity: 0..200 ↔ 0.0..2.0 (engine multiplier)
    tuneForm->addRow(tr("Sensitivity:"), makeSliderRow(
        tr("Multiplier on band magnitudes after auto-gain. "
           "Higher = more visible reactivity to quiet music."),
        0, 200, 100, m_sensSlider, m_sensLabel));
    m_sensLabel->setText(QStringLiteral("100%"));
    connect(m_sensSlider, &QSlider::valueChanged, this, &AudioReactivePanel::onSensitivity);

    // Smoothing: 0..100 ↔ decay [0.95, 0.9999]
    const int initSmoothing = 75;   // ≈ current default
    tuneForm->addRow(tr("Smoothing:"), makeSliderRow(
        tr("Rolling-max auto-gain decay. Lower = punchier / staccato; "
           "higher = floaty / dreamy. Affects every reactive mode."),
        0, 100, initSmoothing, m_smoothSlider, m_smoothLabel));
    m_smoothLabel->setText(QString::number(initSmoothing));
    connect(m_smoothSlider, &QSlider::valueChanged, this, &AudioReactivePanel::onSmoothing);

    // Hue: 0..359 degrees
    tuneForm->addRow(tr("Hue shift:"), makeSliderRow(
        tr("Rotates the palette around the colour wheel."),
        0, 359, 0, m_hueSlider, m_hueLabel));
    m_hueLabel->setText(QStringLiteral("0°"));
    connect(m_hueSlider, &QSlider::valueChanged, this, &AudioReactivePanel::onHue);

    root->addWidget(tuneBox);

    // ── Waveform-specific group ─────────────────────────────────────────────
    m_waveformGroup = new QGroupBox(tr("Waveform"), this);
    auto* waveForm = new QFormLayout(m_waveformGroup);

    m_speedSpin = new QSpinBox(m_waveformGroup);
    m_speedSpin->setRange(1, 30);
    m_speedSpin->setValue(m_engine ? m_engine->waveformSpeed() : 30);
    m_speedSpin->setToolTip(
        tr("Scroll speed for the Waveform mode's history. "
           "1 = slowest, 30 = fastest = shift every audio frame."));
    waveForm->addRow(tr("Scroll speed:"), m_speedSpin);
    connect(m_speedSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &AudioReactivePanel::onSpeed);

    root->addWidget(m_waveformGroup);
    m_waveformGroup->setEnabled(false);   // only when Waveform mode is active

    // ── Python preset group ─────────────────────────────────────────────────
    m_presetGroup = new QGroupBox(tr("Python preset"), this);
    auto* presetLayout = new QVBoxLayout(m_presetGroup);

    m_presetLoadButton = new QPushButton(tr("Load preset…"), m_presetGroup);
    m_presetLoadButton->setToolTip(
        tr("Pick a .py file. The preset receives live audio bands and produces "
           "voxel frames via a Python subprocess."));
    connect(m_presetLoadButton, &QPushButton::clicked,
            this, [this]() { emit presetLoadRequested(); });
    presetLayout->addWidget(m_presetLoadButton);

    m_presetStatusLabel = new QLabel(tr("(no preset loaded)"), m_presetGroup);
    m_presetStatusLabel->setWordWrap(true);
    presetLayout->addWidget(m_presetStatusLabel);

    root->addWidget(m_presetGroup);
    m_presetGroup->setEnabled(false);     // only when Python preset mode is active

    root->addStretch(1);

    // Push initial tuning values into the engine so the displayed numbers
    // match what's actually running.
    if (m_engine) {
        m_engine->setSensitivity(1.0f);
        m_engine->setDecayPerFrame(smoothingToDecay(initSmoothing));
        m_engine->setHueOffset(0.0f);
    }
}

// ── Slot wiring ──────────────────────────────────────────────────────────────

void AudioReactivePanel::onModeIndex(int idx) {
    const auto mode = static_cast<ReactiveMode>(m_modeCombo->itemData(idx).toInt());
    if (m_waveformGroup) m_waveformGroup->setEnabled(mode == ReactiveMode::WaveformSlice);
    if (m_presetGroup)   m_presetGroup  ->setEnabled(mode == ReactiveMode::PythonPreset);
    emit modeChanged(mode);
}

void AudioReactivePanel::setPresetStatus(const QString& name) {
    if (!m_presetStatusLabel) return;
    m_presetStatusLabel->setText(name.isEmpty()
        ? tr("(no preset loaded)")
        : tr("Loaded: %1").arg(name));
}

void AudioReactivePanel::onBlendIndex(int idx) {
    emit blendChanged(static_cast<ReactiveBlend>(m_blendCombo->itemData(idx).toInt()));
}

void AudioReactivePanel::onSensitivity(int v) {
    const float s = static_cast<float>(v) / 100.0f;
    if (m_engine) m_engine->setSensitivity(s);
    m_sensLabel->setText(QStringLiteral("%1%").arg(v));
}

void AudioReactivePanel::onSmoothing(int v) {
    if (m_engine) m_engine->setDecayPerFrame(smoothingToDecay(v));
    m_smoothLabel->setText(QString::number(v));
}

void AudioReactivePanel::onHue(int v) {
    if (m_engine) m_engine->setHueOffset(static_cast<float>(v));
    m_hueLabel->setText(QStringLiteral("%1°").arg(v));
}

void AudioReactivePanel::onSpeed(int v) {
    if (m_engine) m_engine->setWaveformSpeed(v);
}
