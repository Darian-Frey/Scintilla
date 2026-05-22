#pragma once

#include <QWidget>

#include "core/AnimationTimeline.h"

class QPushButton;
class QSpinBox;
class QComboBox;
class QSlider;
class QLabel;

// ── TimelineWidget ───────────────────────────────────────────────────────────
//
// Scrubber + readout style timeline. A horizontal QSlider seeks the current
// frame; a label to its right shows "Frame i / N · t.tts / t.tts · L lit".
// Playback controls (Play/+Frame/Duplicate/Delete) and FPS / Mode live on a
// separate row above the scrubber.

class TimelineWidget : public QWidget {
    Q_OBJECT

public:
    explicit TimelineWidget(QWidget* parent = nullptr);

    void setTimeline(AnimationTimeline* tl);

private slots:
    void onCurrentChanged(int idx);
    void onContentChanged(int idx);
    void onStructureChanged();
    void onPlaybackStateChanged(bool playing);

    void onSliderValueChanged(int value);
    void onPlayStop();
    void onAddFrame();
    void onDuplicateFrame();
    void onDeleteFrame();
    void onFpsChanged(int fps);
    void onModeChanged(int comboIndex);

private:
    void buildLayout();
    void refreshSliderRange();
    void refreshReadout();

    AnimationTimeline* m_timeline = nullptr;

    QPushButton* m_playStop   = nullptr;
    QPushButton* m_addBtn     = nullptr;
    QPushButton* m_dupBtn     = nullptr;
    QPushButton* m_delBtn     = nullptr;
    QSpinBox*    m_fpsSpin    = nullptr;
    QComboBox*   m_modeCombo  = nullptr;

    QSlider*     m_scrubber   = nullptr;
    QLabel*      m_readout    = nullptr;
};
