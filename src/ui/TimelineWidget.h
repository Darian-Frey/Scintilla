#pragma once

#include <QWidget>
#include <vector>

#include "core/AnimationTimeline.h"

class QScrollArea;
class QHBoxLayout;
class QPushButton;
class QSpinBox;
class QComboBox;

// ── TimelineWidget ───────────────────────────────────────────────────────────
//
// Horizontal strip of frame cells + playback controls.
// Each cell shows its frame index and a colour dot (first lit voxel's colour).
// Reacts to AnimationTimeline signals and drives back via slot calls.

class TimelineWidget : public QWidget {
    Q_OBJECT

public:
    explicit TimelineWidget(QWidget* parent = nullptr);

    void setTimeline(AnimationTimeline* tl);

private slots:
    void rebuildCells();
    void onCurrentChanged(int idx);
    void onContentChanged(int idx);
    void onPlaybackStateChanged(bool playing);

    void onCellClicked(int idx);
    void onPlayStop();
    void onAddFrame();
    void onDuplicateFrame();
    void onDeleteFrame();
    void onFpsChanged(int fps);
    void onModeChanged(int comboIndex);

private:
    void buildLayout();
    void styleCell(int idx);

    AnimationTimeline* m_timeline = nullptr;

    QScrollArea* m_scroll      = nullptr;
    QWidget*     m_cellsHost   = nullptr;
    QHBoxLayout* m_cellsLayout = nullptr;
    std::vector<QPushButton*> m_cells;

    QPushButton* m_playStop   = nullptr;
    QPushButton* m_addBtn     = nullptr;
    QPushButton* m_dupBtn     = nullptr;
    QPushButton* m_delBtn     = nullptr;
    QSpinBox*    m_fpsSpin    = nullptr;
    QComboBox*   m_modeCombo  = nullptr;
};
