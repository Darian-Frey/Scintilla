#pragma once

#include <QOpenGLFunctions_4_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <QTimer>
#include <memory>

#include <set>

#include "OrbitCamera.h"
#include "LedInstanceBuffer.h"
#include "core/AnimationTimeline.h"
#include "core/ShapeMask.h"
#include "core/VoxelFrame.h"
#include "core/VoxelStroke.h"

// ── Tool ──────────────────────────────────────────────────────────────────────
enum class Tool { Paint, Erase, Fill, Pick };

// ── CubeViewport ──────────────────────────────────────────────────────────────
//
// The 3D viewport. Owns the OpenGL context, shaders, geometry, instance buffers,
// and orbit camera. All rendering decisions from the prototype are preserved here.
//
// Signals up to MainWindow:
//   voxelPainted(x, y, z, r, g, b)   — a LED was changed
//   colorPicked(r, g, b)             — pick tool sampled a colour

class CubeViewport : public QOpenGLWidget, private QOpenGLFunctions_4_3_Core {
    Q_OBJECT

public:
    explicit CubeViewport(QWidget* parent = nullptr);
    ~CubeViewport() override;

    // ── Scene configuration ───────────────────────────────────────────────────
    void setMask(std::shared_ptr<ShapeMask> mask);
    void setTimeline(AnimationTimeline* timeline);

    void setTool(Tool t) { m_tool = t; }
    void setPaintColor(uint8_t r, uint8_t g, uint8_t b) { m_paintColor = {r, g, b}; }
    void setSlice(int x, int y, int z);

    void setShowGhost(bool show);
    void setShowBounds(bool show);
    void setShowAxisGizmo(bool show);
    void setAutoRotate(bool on);
    void setLedRadius(float r);                 // clamped to (0, 1.0]; default 0.095 per DEC-028

    // ── Immediate updates (called from MainWindow when state changes) ─────────
    void refreshFrame();    // re-upload instance buffer from current frame
    void resetCamera();

    // ── Audio reactive mode (Phase 3, F-018) ──────────────────────────────────
    // When a reactive frame is set, it replaces the timeline frame in the
    // viewport without touching the timeline data. Clear with clearReactiveFrame()
    // to return to normal editor display.
    void setReactiveFrame(VoxelFrame frame);
    void clearReactiveFrame();

    [[nodiscard]] OrbitCamera& camera() { return m_camera; }

signals:
    void voxelEdited(int x, int y, int z, uint8_t r, uint8_t g, uint8_t b, bool erased);
    void colorPicked(uint8_t r, uint8_t g, uint8_t b);
    void pickRayMiss();

    // Emitted at mouse-release for Paint/Erase tools. MainWindow wraps the
    // payload in a QUndoCommand for Ctrl+Z support. Identity changes are
    // pre-filtered, so an empty stroke is never emitted.
    void strokeCommitted(VoxelStroke stroke);

protected:
    void initializeGL()                     override;
    void resizeGL(int w, int h)             override;
    void paintGL()                          override;
    void mousePressEvent(QMouseEvent* e)    override;
    void mouseMoveEvent(QMouseEvent* e)     override;
    void mouseReleaseEvent(QMouseEvent* e)  override;
    void wheelEvent(QWheelEvent* e)         override;

private:
    // ── GL resources ─────────────────────────────────────────────────────────
    struct SphereGeo {
        GLuint vao = 0, vbo = 0, ebo = 0;
        int indexCount = 0;
    };
    void buildSphereGeometry(SphereGeo& out, int lonSegs, int latSegs);
    void buildBoxGeometry();
    void buildAxisGizmo();
    void paintAxisGizmo();
    void buildShaders();
    void setupVAOs();
    void uploadScene();   // rebuild instances → upload → setup VAOs

    SphereGeo m_onSphere;    // unit sphere, 9×7 segments — shader applies r=0.38
    SphereGeo m_offSphere;   // unit sphere, 6×5 segments — shader applies r=0.17

    GLuint m_boxVao = 0, m_boxVbo = 0, m_boxEbo = 0;
    int    m_boxLineCount = 0;

    GLuint m_axisVao = 0, m_axisVbo = 0;   // 6 coloured vertices: 3 line segments + tips

    // ── Shader programs ───────────────────────────────────────────────────────
    std::unique_ptr<QOpenGLShaderProgram> m_ledProg;
    std::unique_ptr<QOpenGLShaderProgram> m_ghostProg;
    std::unique_ptr<QOpenGLShaderProgram> m_gridProg;
    std::unique_ptr<QOpenGLShaderProgram> m_axisProg;

    // ── Instance buffer ───────────────────────────────────────────────────────
    LedInstanceBuffer m_instances;

    // ── Scene state ───────────────────────────────────────────────────────────
    std::shared_ptr<ShapeMask> m_mask;
    AnimationTimeline*         m_timeline  = nullptr;

    OrbitCamera m_camera;
    Tool        m_tool       = Tool::Paint;
    RGB         m_paintColor = {255, 0, 0};
    int         m_sliceX     = -1, m_sliceY = -1, m_sliceZ = -1;
    bool        m_showGhost     = true;
    bool        m_showBounds    = true;
    bool        m_showAxisGizmo = true;
    bool        m_initialized   = false;   // initializeGL has run
    float       m_ledRadius     = 0.095f;  // DEC-028 default; user-adjustable via status bar

    // Optional reactive override: when set, takes precedence over m_timeline.
    bool        m_haveReactive  = false;
    VoxelFrame  m_reactiveFrame;

    // ── Auto-rotate ───────────────────────────────────────────────────────────
    QTimer m_rotTimer;

    // ── Mouse tracking ────────────────────────────────────────────────────────
    QPoint  m_pressPos;
    QPoint  m_lastPos;
    bool    m_dragging    = false;
    bool    m_shiftDrag   = false;
    float   m_aspectRatio = 1.0f;

    // ── Picking ───────────────────────────────────────────────────────────────
    // Returns instance index (-1 = miss). Uses GPU depth readback for accuracy.
    int pickInstance(QPoint screenPos);

    void applyTool(int instanceIdx);          // Pick / Fill — one-shot, no undo
    void applyToolStroke(int instanceIdx);    // Paint / Erase — appends to m_currentStroke

    // ── Stroke painting state (active while LMB held with Paint/Erase) ───────
    bool                m_strokeActive = false;
    VoxelStroke         m_currentStroke;
    std::set<VoxelKey>  m_strokedKeys;   // de-dup: don't repaint a voxel twice in one stroke
};
