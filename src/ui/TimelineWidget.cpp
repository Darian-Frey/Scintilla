#include "TimelineWidget.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

// ── Construction ─────────────────────────────────────────────────────────────

TimelineWidget::TimelineWidget(QWidget* parent) : QWidget(parent) {
    buildLayout();
}

void TimelineWidget::buildLayout() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);

    // ── Controls row ─────────────────────────────────────────────────────────
    auto* controls = new QHBoxLayout();
    m_playStop = new QPushButton(tr("Play"), this);
    m_addBtn   = new QPushButton(tr("+ Frame"), this);
    m_dupBtn   = new QPushButton(tr("Duplicate"), this);
    m_delBtn   = new QPushButton(tr("Delete"), this);
    connect(m_playStop, &QPushButton::clicked, this, &TimelineWidget::onPlayStop);
    connect(m_addBtn,   &QPushButton::clicked, this, &TimelineWidget::onAddFrame);
    connect(m_dupBtn,   &QPushButton::clicked, this, &TimelineWidget::onDuplicateFrame);
    connect(m_delBtn,   &QPushButton::clicked, this, &TimelineWidget::onDeleteFrame);
    controls->addWidget(m_playStop);
    controls->addWidget(m_addBtn);
    controls->addWidget(m_dupBtn);
    controls->addWidget(m_delBtn);

    controls->addSpacing(16);
    controls->addWidget(new QLabel(tr("FPS:"), this));
    m_fpsSpin = new QSpinBox(this);
    m_fpsSpin->setRange(1, 60);
    m_fpsSpin->setValue(12);
    connect(m_fpsSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &TimelineWidget::onFpsChanged);
    controls->addWidget(m_fpsSpin);

    controls->addSpacing(16);
    controls->addWidget(new QLabel(tr("Mode:"), this));
    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItem(tr("Play once"), static_cast<int>(PlaybackMode::PlayOnce));
    m_modeCombo->addItem(tr("Loop"),      static_cast<int>(PlaybackMode::Loop));
    m_modeCombo->addItem(tr("Ping-pong"), static_cast<int>(PlaybackMode::PingPong));
    m_modeCombo->setCurrentIndex(1);
    connect(m_modeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &TimelineWidget::onModeChanged);
    controls->addWidget(m_modeCombo);

    controls->addStretch(1);
    root->addLayout(controls);

    // ── Scrubber + readout row ───────────────────────────────────────────────
    auto* scrubRow = new QHBoxLayout();
    m_scrubber = new QSlider(Qt::Horizontal, this);
    m_scrubber->setMinimum(0);
    m_scrubber->setMaximum(0);
    m_scrubber->setSingleStep(1);
    m_scrubber->setPageStep(1);
    m_scrubber->setTracking(true);
    connect(m_scrubber, &QSlider::valueChanged,
            this, &TimelineWidget::onSliderValueChanged);
    scrubRow->addWidget(m_scrubber, 1);

    m_readout = new QLabel(this);
    m_readout->setMinimumWidth(260);
    m_readout->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    scrubRow->addWidget(m_readout);

    root->addLayout(scrubRow);

    refreshReadout();
}

// ── Timeline wiring ──────────────────────────────────────────────────────────

void TimelineWidget::setTimeline(AnimationTimeline* tl) {
    if (m_timeline) disconnect(m_timeline, nullptr, this, nullptr);
    m_timeline = tl;
    if (!m_timeline) return;

    connect(m_timeline, &AnimationTimeline::timelineStructureChanged,
            this, &TimelineWidget::onStructureChanged);
    connect(m_timeline, &AnimationTimeline::currentFrameChanged,
            this, &TimelineWidget::onCurrentChanged);
    connect(m_timeline, &AnimationTimeline::frameContentChanged,
            this, &TimelineWidget::onContentChanged);
    connect(m_timeline, &AnimationTimeline::playbackStateChanged,
            this, &TimelineWidget::onPlaybackStateChanged);

    m_fpsSpin->setValue(m_timeline->fps());
    refreshSliderRange();
    refreshReadout();
}

// ── Timeline → UI ────────────────────────────────────────────────────────────

void TimelineWidget::onStructureChanged() {
    refreshSliderRange();
    refreshReadout();
}

void TimelineWidget::onCurrentChanged(int idx) {
    if (!m_timeline) return;
    QSignalBlocker block(m_scrubber);
    m_scrubber->setValue(idx);
    refreshReadout();
}

void TimelineWidget::onContentChanged(int /*idx*/) {
    refreshReadout();
}

void TimelineWidget::onPlaybackStateChanged(bool playing) {
    m_playStop->setText(playing ? tr("Stop") : tr("Play"));
}

// ── UI → timeline ────────────────────────────────────────────────────────────

void TimelineWidget::onSliderValueChanged(int value) {
    if (!m_timeline) return;
    if (value == m_timeline->currentIndex()) return;
    m_timeline->selectFrame(value);
}

void TimelineWidget::onAddFrame()       { if (m_timeline) m_timeline->addFrame(); }
void TimelineWidget::onDuplicateFrame() { if (m_timeline) m_timeline->duplicateCurrent(); }
void TimelineWidget::onDeleteFrame()    { if (m_timeline) m_timeline->deleteCurrent(); }

void TimelineWidget::onPlayStop() {
    if (!m_timeline) return;
    if (m_timeline->isPlaying()) m_timeline->stop();
    else                         m_timeline->play();
}

void TimelineWidget::onFpsChanged(int fps) {
    if (m_timeline) m_timeline->setFps(fps);
    refreshReadout();           // total duration depends on fps
}

void TimelineWidget::onModeChanged(int comboIndex) {
    if (!m_timeline) return;
    m_timeline->setPlaybackMode(
        static_cast<PlaybackMode>(m_modeCombo->itemData(comboIndex).toInt()));
}

// ── Helpers ──────────────────────────────────────────────────────────────────

void TimelineWidget::refreshSliderRange() {
    if (!m_timeline) return;
    QSignalBlocker block(m_scrubber);
    const int n = m_timeline->frameCount();
    m_scrubber->setMaximum(std::max(0, n - 1));
    m_scrubber->setValue(m_timeline->currentIndex());
    m_scrubber->setEnabled(n > 1);
}

void TimelineWidget::refreshReadout() {
    if (!m_timeline) {
        m_readout->setText(QString());
        return;
    }
    const int    n    = m_timeline->frameCount();
    const int    idx  = m_timeline->currentIndex();
    const int    fps  = std::max(1, m_timeline->fps());
    const int    lit  = m_timeline->currentFrame().litCount();
    const double tNow = static_cast<double>(idx) / fps;
    const double tEnd = static_cast<double>(n)   / fps;
    m_readout->setText(tr("Frame %1 / %2  ·  %3s / %4s  ·  %5 lit")
        .arg(idx + 1).arg(n)
        .arg(tNow, 0, 'f', 2)
        .arg(tEnd, 0, 'f', 2)
        .arg(lit));
}
