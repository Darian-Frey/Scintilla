#pragma once

#include <QMainWindow>
#include <cstdint>
#include <memory>

#include "core/AnimationTimeline.h"
#include "core/ShapeMask.h"

class CubeViewport;

// ── MainWindow ────────────────────────────────────────────────────────────────
//
// Top-level window. Hosts the CubeViewport as its central widget.
//
// Phase 1 is a stub — viewport only, default 8³ cube mask, no UI panels.
// Phase 2 wires up the colour picker, timeline strip, slice controls, and
// info panel. Phase 3 adds the audio device picker; Phase 4 the preset editor.
//
// Owns the AnimationTimeline and the active ShapeMask. The viewport receives
// shared_ptr to the mask and a raw pointer to the timeline.

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    void onVoxelEdited(int x, int y, int z,
                       uint8_t r, uint8_t g, uint8_t b,
                       bool erased);
    void onColorPicked(uint8_t r, uint8_t g, uint8_t b);

private:
    // Build a new ShapeMask, swap into the viewport, reset the timeline.
    // Confirms with the user first if the timeline has content (DEC-006).
    void rebuildMask(int gridSize, ShapeType shape);

    CubeViewport*                      m_viewport = nullptr;
    std::unique_ptr<AnimationTimeline> m_timeline;
    std::shared_ptr<ShapeMask>         m_mask;
};
