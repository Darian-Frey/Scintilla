#pragma once

#include <QMainWindow>
#include <QVector3D>
#include <cstdint>
#include <map>
#include <memory>

#include <QDateTime>

#include "audio/FFTProcessor.h"   // for BandData (used in onReactivePresetBands slot)
#include "core/AnimationTimeline.h"
#include "core/ShapeMask.h"
#include "core/VoxelStroke.h"     // for VoxelStroke (used in onStrokeCommitted slot)

class CubeViewport;
class ColorPickerWidget;
class SliceControlWidget;
class TimelineWidget;
class FrameInfoPanel;
class AudioReactiveEngine;
class AudioReactivePanel;
class PresetEditorPanel;
class PresetRunner;
class QActionGroup;
class QAction;
class QTimer;
class QUndoStack;

enum class Tool;
enum class ReactiveMode;
enum class ReactiveBlend;

// ── CameraKeyframe ───────────────────────────────────────────────────────────
//
// Snapshot of the orbit camera state used by the rotation-keyframe feature.
// Linearly interpolated between bracketing keyframes during playback and
// export to produce fly-through animations without scripted motion.
struct CameraKeyframe {
    float     theta  = 0.0f;
    float     phi    = 0.0f;
    float     radius = 0.0f;
    QVector3D target;
};

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
    void onStrokeCommitted(VoxelStroke stroke);

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
    void onImportMesh();

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

    // ── Animation scripts (run-once cube.frame() / cube.play() model) ────────
    void onRunAnimationScript();              // File menu — opens picker
    void onNewAnimationScript();              // File menu — creates from template
    void runAnimationScript(const QString& path);   // helper used by both + editor
    void onAnimationComplete(int fps);

    // ── Preset scripting (Phase 4 step C — live reactive mode) ───────────────
    void onLoadReactivePreset();           // panel "Load preset…" button
    void onReactivePresetBands(BandData d);// engine → runner per-frame routing

    // ── Clipboard (frame / slice copy-paste) ─────────────────────────────────
    void onCopy();
    void onPaste();

    // ── Animation export (GIF / MP4 / WebM via ffmpeg) ───────────────────────
    void onExportAnimation();

    // ── Camera keyframes (fly-through animation) ─────────────────────────────
    void onSetCameraKeyframe();
    void onClearCameraKeyframe();
    void onClearAllCameraKeyframes();
    void onPlaybackCameraTick(int frameIdx);   // drives camera during playback

private:
    void buildMenus();
    void buildToolbar();
    void buildDocks();
    void wireSignals();

    void rebuildMask(int gridSize, ShapeType shape);
    void applyMask(std::shared_ptr<ShapeMask> mask);   // propagate to all consumers (BUG-011)
    bool confirmDiscardIfDirty(const QString& reason);
    bool saveTo(const QString& path);
    void ensurePresetRunner();   // lazy-construct + wire signals on first use

    // If the script declares `grid_size` / `shape` class attributes that
    // differ from the current cube, prompt the user to apply them. Returns
    // false only if the user cancels — true means "proceed with the run".
    bool applyScriptRequirementsIfNeeded(const QString& path);

    CubeViewport*                       m_viewport       = nullptr;
    ColorPickerWidget*                  m_colorPicker    = nullptr;
    SliceControlWidget*                 m_sliceControl   = nullptr;
    TimelineWidget*                     m_timelineWidget = nullptr;
    FrameInfoPanel*                     m_frameInfo      = nullptr;
    PresetEditorPanel*                  m_presetEditor   = nullptr;

    std::unique_ptr<AnimationTimeline>  m_timeline;
    std::shared_ptr<ShapeMask>          m_mask;

    QActionGroup*                       m_toolGroup      = nullptr;
    QString                             m_currentPath;   // last save/open path

    QUndoStack*                         m_undoStack      = nullptr;   // voxel stroke history

    // ── Clipboard ─────────────────────────────────────────────────────────────
    // In-memory voxel clipboard for Edit → Copy / Paste. Holds the entire
    // frame when no slice is active, or just the active slice's voxels.
    VoxelFrame                          m_clipboard;
    bool                                m_clipboardHasContent = false;

    // Cached slice state — mirrored from SliceControlWidget so Copy/Paste
    // know which slice (if any) to restrict themselves to.
    int                                 m_sliceX = -1, m_sliceY = -1, m_sliceZ = -1;

    // ── Camera keyframes ──────────────────────────────────────────────────────
    // Sparse map: frame index → camera state. Frames between keyframes
    // interpolate; frames outside the extremes clamp to the nearest one.
    // Session-only — not persisted in the JSON save format.
    std::map<int, CameraKeyframe>       m_cameraKeyframes;

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

    // Animation mode — true while an Animation script's frames are being
    // collected into the timeline. Discriminator inside onPresetFrameReady.
    bool     m_animationMode = false;

    // Error throttling — without this a misconfigured preset (e.g. an
    // Animation loaded as a reactive Python preset, then audio started)
    // can emit dozens of errors per second, each opening a modal dialog
    // and effectively locking the UI thread. See onPresetError.
    qint64   m_lastPresetErrorMs   = 0;
    int      m_suppressedErrors    = 0;
};
