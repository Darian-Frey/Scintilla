#pragma once

#include <QWidget>
#include <memory>

#include "core/AnimationTimeline.h"
#include "core/ShapeMask.h"

class QLabel;

// ── FrameInfoPanel ───────────────────────────────────────────────────────────
//
// Read-only stats panel: current frame index, lit-LED count, addressable
// total, FPS, grid size, shape. Refreshed on every timeline signal.

class FrameInfoPanel : public QWidget {
    Q_OBJECT

public:
    explicit FrameInfoPanel(QWidget* parent = nullptr);

    void setTimeline(AnimationTimeline* tl);
    void setMask(std::shared_ptr<ShapeMask> mask);

private slots:
    void refresh();

private:
    void buildLayout();

    AnimationTimeline*         m_timeline = nullptr;
    std::shared_ptr<ShapeMask> m_mask;

    QLabel* m_frameLabel = nullptr;
    QLabel* m_litLabel   = nullptr;
    QLabel* m_fpsLabel   = nullptr;
    QLabel* m_gridLabel  = nullptr;
};
