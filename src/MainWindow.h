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
class AudioReactiveEngine;
class AudioReactivePanel;
class PresetRunner;
class QActionGroup;
class QAction;
class QTimer;

enum class Tool;
enum class ReactiveMode;
enum class ReactiveBlend;

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
    void onToggleAxisGizmo(bool on);
    void onToggleAutoRotate(bool on);
    void onResetCamera();

    void onShapeChanged(int shapeIndex);
    void onGridSizeChanged(int n);

    // ── Audio reactive (Phase 3) ─────────────────────────────────────────────
    void onPickAudioDevice();
    void onReactiveModeChanged(ReactiveMode mode);
    void onReactiveBlendChanged(ReactiveBlend blend);
    void onCaptureToggled(bool on);
    void onReactiveFrame(VoxelFrame f);
    void onReactiveFrameCaptured(VoxelFrame f);
    void onAudioError(const QString& msg);

    // ── Preset scripting (Phase 4 — offline playback for step B) ─────────────
    void onRunPreset();
    void onPresetFrameReady(VoxelFrame f);
    void onPresetLoaded(const QString& name);
    void onPresetError(const QString& msg, int line);
    void tickPresetPlayback();

private:
    void buildMenus();
    void buildToolbar();
    void buildDocks();
    void wireSignals();

    void rebuildMask(int gridSize, ShapeType shape);
    void applyMask(std::shared_ptr<ShapeMask> mask);   // propagate to all consumers (BUG-011)
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

    // ── Audio reactive plumbing ───────────────────────────────────────────────
    std::unique_ptr<AudioReactiveEngine> m_audioEngine;
    AudioReactivePanel* m_audioPanel    = nullptr;
    QAction*    m_captureAction         = nullptr;
    int         m_audioDeviceIndex      = -1;
    float       m_audioSampleRate       = 44100.0f;
    QString     m_audioMonitorSource;            // empty unless user picked one

    // ── Preset scripting (Phase 4) ────────────────────────────────────────────
    std::unique_ptr<PresetRunner> m_presetRunner;
    QTimer*  m_presetPlaybackTimer = nullptr;
    int      m_presetFramesRequested = 0;       // total frames to send
    int      m_presetFramesSent      = 0;       // sent so far
    int      m_presetFramesReceived  = 0;       // returned by the preset so far
};
