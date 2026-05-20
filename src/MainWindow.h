#pragma once

#include <QMainWindow>
#include <cstdint>
#include <memory>

#include "core/AnimationTimeline.h"
#include "core/ShapeMask.h"

class CubeViewport;
class ColorPickerWidget;
class SliceControlWidget;
class TimelineWidget;
class FrameInfoPanel;
class QActionGroup;

enum class Tool;

// ── MainWindow ────────────────────────────────────────────────────────────────
//
// Composes the central CubeViewport with four dock widgets (colour picker,
// slice control, frame info on the right; timeline at the bottom) plus menus
// and a tool toolbar. Owns the AnimationTimeline and the active ShapeMask.

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    // ── Viewport callbacks ───────────────────────────────────────────────────
    void onVoxelEdited(int x, int y, int z,
                       uint8_t r, uint8_t g, uint8_t b,
                       bool erased);
    void onColorPicked(uint8_t r, uint8_t g, uint8_t b);

    // ── UI → state ────────────────────────────────────────────────────────────
    void onColorChanged(uint8_t r, uint8_t g, uint8_t b);
    void onSliceChanged(int sliceX, int sliceY, int sliceZ);

    // ── Menu actions ─────────────────────────────────────────────────────────
    void onNew();
    void onOpen();
    void onSave();
    void onSaveAs();

    void onToggleGhost(bool on);
    void onToggleBounds(bool on);
    void onToggleAutoRotate(bool on);
    void onResetCamera();

    void onShapeChanged(int shapeIndex);
    void onGridSizeChanged(int n);

private:
    void buildMenus();
    void buildToolbar();
    void buildDocks();
    void wireSignals();

    void rebuildMask(int gridSize, ShapeType shape);
    bool confirmDiscardIfDirty(const QString& reason);
    bool saveTo(const QString& path);

    CubeViewport*                       m_viewport       = nullptr;
    ColorPickerWidget*                  m_colorPicker    = nullptr;
    SliceControlWidget*                 m_sliceControl   = nullptr;
    TimelineWidget*                     m_timelineWidget = nullptr;
    FrameInfoPanel*                     m_frameInfo      = nullptr;

    std::unique_ptr<AnimationTimeline>  m_timeline;
    std::shared_ptr<ShapeMask>          m_mask;

    QActionGroup*                       m_toolGroup      = nullptr;
    QString                             m_currentPath;   // last save/open path
};
