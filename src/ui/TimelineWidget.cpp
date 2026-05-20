#include "TimelineWidget.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
    constexpr int kCellSize = 48;

    QString cellStyleFor(QColor dot, bool selected) {
        const QString border = selected
            ? QStringLiteral("border:2px solid #f0f0f0;")
            : QStringLiteral("border:1px solid #444;");
        return QString(
            "QPushButton { background:#181822; color:#ccc; %1 padding:2px; }"
            "QPushButton::indicator { background:%2; }"
        ).arg(border, dot.name());
    }

    QColor firstLitDot(const VoxelFrame& f) {
        const auto& v = f.voxels();
        if (v.empty()) return QColor(40, 40, 50);
        const RGB c = v.begin()->second;
        return QColor(c[0], c[1], c[2]);
    }
}

// ── Construction ─────────────────────────────────────────────────────────────

TimelineWidget::TimelineWidget(QWidget* parent) : QWidget(parent) {
    buildLayout();
}

void TimelineWidget::buildLayout() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);

    // Controls row
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

    // Cell strip
    m_scroll = new QScrollArea(this);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setFixedHeight(kCellSize + 16);

    m_cellsHost   = new QWidget();
    m_cellsLayout = new QHBoxLayout(m_cellsHost);
    m_cellsLayout->setContentsMargins(2, 2, 2, 2);
    m_cellsLayout->setSpacing(2);
    m_cellsLayout->addStretch(1);
    m_scroll->setWidget(m_cellsHost);
    root->addWidget(m_scroll);
}

// ── Timeline wiring ──────────────────────────────────────────────────────────

void TimelineWidget::setTimeline(AnimationTimeline* tl) {
    if (m_timeline) disconnect(m_timeline, nullptr, this, nullptr);
    m_timeline = tl;
    if (!m_timeline) return;

    connect(m_timeline, &AnimationTimeline::timelineStructureChanged,
            this, &TimelineWidget::rebuildCells);
    connect(m_timeline, &AnimationTimeline::currentFrameChanged,
            this, &TimelineWidget::onCurrentChanged);
    connect(m_timeline, &AnimationTimeline::frameContentChanged,
            this, &TimelineWidget::onContentChanged);
    connect(m_timeline, &AnimationTimeline::playbackStateChanged,
            this, &TimelineWidget::onPlaybackStateChanged);

    m_fpsSpin->setValue(m_timeline->fps());
    rebuildCells();
}

// ── Rebuild / update ─────────────────────────────────────────────────────────

void TimelineWidget::rebuildCells() {
    if (!m_timeline) return;
    // Wipe old cells (preserve the trailing stretch which is the last layout item).
    for (auto* cell : m_cells) {
        m_cellsLayout->removeWidget(cell);
        cell->deleteLater();
    }
    m_cells.clear();

    for (int i = 0; i < m_timeline->frameCount(); ++i) {
        auto* b = new QPushButton(QString::number(i + 1), m_cellsHost);
        b->setFixedSize(kCellSize, kCellSize);
        connect(b, &QPushButton::clicked, this, [this, i]() { onCellClicked(i); });
        m_cellsLayout->insertWidget(static_cast<int>(m_cells.size()), b);
        m_cells.push_back(b);
        styleCell(i);
    }
}

void TimelineWidget::onCurrentChanged(int /*idx*/) {
    for (size_t i = 0; i < m_cells.size(); ++i) styleCell(static_cast<int>(i));
    if (!m_cells.empty() && m_timeline) {
        // Scroll the selected cell into view.
        const int idx = m_timeline->currentIndex();
        if (idx >= 0 && static_cast<size_t>(idx) < m_cells.size()) {
            m_scroll->ensureWidgetVisible(m_cells[static_cast<size_t>(idx)]);
        }
    }
}

void TimelineWidget::onContentChanged(int idx) {
    if (idx >= 0 && static_cast<size_t>(idx) < m_cells.size()) styleCell(idx);
}

void TimelineWidget::onPlaybackStateChanged(bool playing) {
    m_playStop->setText(playing ? tr("Stop") : tr("Play"));
}

void TimelineWidget::styleCell(int idx) {
    if (!m_timeline || idx < 0 || static_cast<size_t>(idx) >= m_cells.size()) return;
    const bool selected = (idx == m_timeline->currentIndex());
    m_cells[static_cast<size_t>(idx)]->setStyleSheet(
        cellStyleFor(firstLitDot(m_timeline->frameAt(idx)), selected));
}

// ── Action slots ─────────────────────────────────────────────────────────────

void TimelineWidget::onCellClicked(int idx)      { if (m_timeline) m_timeline->selectFrame(idx); }
void TimelineWidget::onAddFrame()                { if (m_timeline) m_timeline->addFrame(); }
void TimelineWidget::onDuplicateFrame()          { if (m_timeline) m_timeline->duplicateCurrent(); }
void TimelineWidget::onDeleteFrame()             { if (m_timeline) m_timeline->deleteCurrent(); }

void TimelineWidget::onPlayStop() {
    if (!m_timeline) return;
    if (m_timeline->isPlaying()) m_timeline->stop();
    else                          m_timeline->play();
}

void TimelineWidget::onFpsChanged(int fps) {
    if (m_timeline) m_timeline->setFps(fps);
}

void TimelineWidget::onModeChanged(int comboIndex) {
    if (!m_timeline) return;
    m_timeline->setPlaybackMode(
        static_cast<PlaybackMode>(m_modeCombo->itemData(comboIndex).toInt()));
}
