#include "MainWindow.h"

#include "renderer/CubeViewport.h"
#include "ui/AudioReactivePanel.h"
#include "ui/ColorPickerWidget.h"
#include "ui/FrameInfoPanel.h"
#include "ui/PresetEditorPanel.h"
#include "ui/SliceControlWidget.h"
#include "ui/TimelineWidget.h"
#include "core/JsonSerializer.h"
#include "core/VoxelStrokeCommand.h"
#include "audio/AudioReactiveEngine.h"
#include "audio/AudioDevicePicker.h"
#include "scripting/PresetRunner.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QImage>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUndoStack>
#include <QDockWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollArea>
#include <QSlider>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <cmath>

namespace {
    constexpr int kDefaultGrid = 8;
    constexpr int kWindowW     = 1480;
    constexpr int kWindowH     = 860;

    // ── Camera-keyframe interpolation helpers ────────────────────────────────
    //
    // Theta wraps modulo 2π — pick the shorter angular direction so a 350°→10°
    // sweep doesn't blow through every angle in between. Phi/radius/target
    // are simple linear lerps.

    constexpr float kPi    = 3.14159265358979323846f;
    constexpr float kTwoPi = 2.0f * kPi;

    float lerpAngle(float a, float b, float t) {
        float d = std::fmod((b - a) + kPi, kTwoPi);
        if (d < 0) d += kTwoPi;
        d -= kPi;
        return a + d * t;
    }

    CameraKeyframe interpolateKeyframe(
        const std::map<int, CameraKeyframe>& keys, int frame) {

        if (keys.empty()) return {};
        auto next = keys.lower_bound(frame);
        if (next == keys.end())       return std::prev(next)->second;   // past last
        if (next->first == frame)      return next->second;              // exact hit
        if (next == keys.begin())     return next->second;              // before first
        auto prev = std::prev(next);
        const float t =
            float(frame - prev->first) / float(next->first - prev->first);
        CameraKeyframe out;
        out.theta  = lerpAngle(prev->second.theta, next->second.theta, t);
        out.phi    = std::lerp(prev->second.phi,    next->second.phi,    t);
        out.radius = std::lerp(prev->second.radius, next->second.radius, t);
        out.target = prev->second.target * (1.0f - t) + next->second.target * t;
        return out;
    }

    void applyKeyframe(OrbitCamera& cam, const CameraKeyframe& k) {
        cam.setTheta (k.theta);
        cam.setPhi   (k.phi);
        cam.setRadius(k.radius);
        cam.setTarget(k.target);
    }

    // ── Python script type detection ─────────────────────────────────────────
    //
    // Each script must subclass either Preset (reactive — driven by audio
    // bands) or Animation (run-once — emits frames itself). Loading the
    // wrong type into the wrong run mode causes runtime errors per audio
    // frame, so we sniff the file content up-front to refuse before any
    // subprocess is started.

    enum class ScriptType { Unknown, Preset, Animation };

    ScriptType detectScriptType(const QString& path) {
        QFile f(path);
        if (!f.open(QFile::ReadOnly | QFile::Text)) return ScriptType::Unknown;
        const QString content = QString::fromUtf8(f.readAll());
        // Match `class X(Animation)` or `class X(Preset)` permissively over
        // whitespace. Doesn't catch `from led_cube import Animation as A;
        // class X(A)` but that's an unusual idiom and the runtime still
        // catches the mismatch via the runner's error path.
        static const QRegularExpression reAnimation(
            QStringLiteral("class\\s+\\w+\\s*\\(\\s*Animation\\s*\\)"));
        static const QRegularExpression rePreset(
            QStringLiteral("class\\s+\\w+\\s*\\(\\s*Preset\\s*\\)"));
        if (reAnimation.match(content).hasMatch()) return ScriptType::Animation;
        if (rePreset.match(content).hasMatch())    return ScriptType::Preset;
        return ScriptType::Unknown;
    }
}

// ── Construction ─────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_timeline(std::make_unique<AnimationTimeline>(this))
    , m_mask(std::make_shared<ShapeMask>(kDefaultGrid, ShapeType::Cube))
    , m_audioEngine(std::make_unique<AudioReactiveEngine>(this)) {

    setWindowTitle(tr("Scintilla"));
    resize(kWindowW, kWindowH);

    m_viewport = new CubeViewport(this);
    m_viewport->setMask(m_mask);
    m_viewport->setTimeline(m_timeline.get());
    m_viewport->setPaintColor(255, 64, 32);
    setCentralWidget(m_viewport);

    m_audioEngine->setMask(m_mask);

    m_undoStack = new QUndoStack(this);
    m_undoStack->setUndoLimit(200);   // bounded depth — voxel strokes can be large

    buildDocks();
    buildMenus();
    buildToolbar();
    wireSignals();

    // LED size slider lives as a permanent widget in the status bar — it's
    // a render preference the user occasionally tweaks, doesn't justify a
    // dock (would compound the right-side vertical-stacking problem), but
    // wants live feedback (so a modal dialog would be wrong too).
    //
    // Slider 5..100 → radius 0.025..0.5 linearly. Default 19 ≈ 0.095 (DEC-028).
    auto* sizeLabel  = new QLabel(tr("LED size:"), this);
    auto* sizeSlider = new QSlider(Qt::Horizontal, this);
    auto* sizeValue  = new QLabel(this);
    sizeSlider->setRange(5, 100);
    sizeSlider->setValue(19);                // matches the DEC-028 default radius
    sizeSlider->setFixedWidth(140);
    sizeValue->setMinimumWidth(40);
    sizeValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    sizeValue->setText(QStringLiteral("0.095"));
    connect(sizeSlider, &QSlider::valueChanged, this, [this, sizeValue](int v) {
        const float r = static_cast<float>(v) * 0.005f;   // 5→0.025, 100→0.500
        m_viewport->setLedRadius(r);
        sizeValue->setText(QString::number(r, 'f', 3));
    });
    statusBar()->addPermanentWidget(sizeLabel);
    statusBar()->addPermanentWidget(sizeSlider);
    statusBar()->addPermanentWidget(sizeValue);

    statusBar()->showMessage(tr("Ready — paint with left-click, orbit with drag, zoom with wheel."));
}

MainWindow::~MainWindow() {
    // Engine stops PortAudio cleanly in its destructor; explicit stop here
    // ensures the worker thread joins before the timeline is torn down.
    if (m_audioEngine) m_audioEngine->stop();
}

// ── Dock construction ────────────────────────────────────────────────────────

void MainWindow::buildDocks() {
    auto makeDock = [&](const QString& title, QWidget* w, Qt::DockWidgetArea area) -> QDockWidget* {
        auto* d = new QDockWidget(title, this);
        d->setObjectName(title);
        d->setWidget(w);
        d->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
        addDockWidget(area, d);
        return d;
    };

    // Wraps a panel in a scroll area so the dock can shrink below the panel's
    // natural sizeHint() — without this every right-side dock contributes its
    // full preferred height to the window's minimum, and the user can't
    // resize the window shorter than the sum of all of them.
    auto scrollable = [](QWidget* inner) -> QWidget* {
        auto* s = new QScrollArea;
        s->setWidget(inner);
        s->setWidgetResizable(true);
        s->setFrameShape(QFrame::NoFrame);
        s->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        return s;
    };

    m_colorPicker    = new ColorPickerWidget(this);
    m_sliceControl   = new SliceControlWidget(this);
    m_frameInfo      = new FrameInfoPanel(this);
    m_timelineWidget = new TimelineWidget(this);
    m_audioPanel     = new AudioReactivePanel(m_audioEngine.get(), this);
    m_presetEditor   = new PresetEditorPanel(this);
    connect(m_presetEditor, &PresetEditorPanel::runRequested,
            this, &MainWindow::runAnimationScript);

    m_sliceControl->setGridSize(m_mask->gridSize());
    m_frameInfo->setMask(m_mask);
    m_frameInfo->setTimeline(m_timeline.get());
    m_timelineWidget->setTimeline(m_timeline.get());
    m_colorPicker->setCurrentColor(255, 64, 32);

    // IDE-style layout: editor on the left, viewport in the centre,
    // controls on the right, timeline along the bottom.
    auto* editorDock = makeDock(tr("Preset editor"),
                                m_presetEditor,             Qt::LeftDockWidgetArea);
    // Editor wants a chunky default width — code is unreadable in a 200 px
    // column. The user can still resize / float / hide it like any dock.
    m_presetEditor->setMinimumWidth(360);
    editorDock->resize(440, editorDock->height());

    makeDock(tr("Colour"),         scrollable(m_colorPicker),  Qt::RightDockWidgetArea);
    makeDock(tr("Slice"),          scrollable(m_sliceControl), Qt::RightDockWidgetArea);
    makeDock(tr("Audio reactive"), scrollable(m_audioPanel),   Qt::RightDockWidgetArea);
    makeDock(tr("Frame"),          scrollable(m_frameInfo),    Qt::RightDockWidgetArea);

    makeDock(tr("Timeline"), m_timelineWidget, Qt::BottomDockWidgetArea);

    // Ensure the editor really does land at a usable starting width even
    // after Qt's auto-distribution between dock areas.
    resizeDocks({editorDock}, {440}, Qt::Horizontal);
}

// ── Menus ────────────────────────────────────────────────────────────────────

void MainWindow::buildMenus() {
    auto* mbar = menuBar();

    // ─── File ────────────────────────────────────────────────────────────────
    auto* file = mbar->addMenu(tr("&File"));
    file->addAction(tr("&New"),       QKeySequence::New,    this, &MainWindow::onNew);
    file->addAction(tr("&Open…"),     QKeySequence::Open,   this, &MainWindow::onOpen);
    file->addAction(tr("&Save"),      QKeySequence::Save,   this, &MainWindow::onSave);
    file->addAction(tr("Save &As…"),  QKeySequence::SaveAs, this, &MainWindow::onSaveAs);
    file->addSeparator();
    file->addAction(tr("&Run preset…"), this, &MainWindow::onRunPreset);
    file->addAction(tr("&New animation script…"), this, &MainWindow::onNewAnimationScript);
    file->addAction(tr("Run &animation script…"), this, &MainWindow::onRunAnimationScript);
    file->addAction(tr("&Export animation…"), this, &MainWindow::onExportAnimation);
    file->addSeparator();
    file->addAction(tr("E&xit"),      QKeySequence::Quit,   qApp, &QApplication::quit);

    // ─── Edit ────────────────────────────────────────────────────────────────
    auto* edit = mbar->addMenu(tr("&Edit"));
    auto* undoAct = m_undoStack->createUndoAction(this, tr("&Undo"));
    undoAct->setShortcut(QKeySequence::Undo);
    edit->addAction(undoAct);
    auto* redoAct = m_undoStack->createRedoAction(this, tr("&Redo"));
    redoAct->setShortcut(QKeySequence::Redo);
    edit->addAction(redoAct);
    edit->addSeparator();
    edit->addAction(tr("&Copy"),  QKeySequence::Copy,  this, &MainWindow::onCopy);
    edit->addAction(tr("&Paste"), QKeySequence::Paste, this, &MainWindow::onPaste);

    // ─── View ────────────────────────────────────────────────────────────────
    auto* view = mbar->addMenu(tr("&View"));
    auto add_toggle = [&](const QString& text, bool initial, void (MainWindow::*slot)(bool)) -> QAction* {
        auto* a = view->addAction(text);
        a->setCheckable(true);
        a->setChecked(initial);
        connect(a, &QAction::toggled, this, slot);
        return a;
    };
    add_toggle(tr("Show &Ghost LEDs"),       true,  &MainWindow::onToggleGhost);
    add_toggle(tr("Show &Bounds"),            true,  &MainWindow::onToggleBounds);
    add_toggle(tr("Show &Axis indicator"),    true,  &MainWindow::onToggleAxisGizmo);
    add_toggle(tr("A&uto-rotate"),           false, &MainWindow::onToggleAutoRotate);
    view->addSeparator();
    view->addAction(tr("&Reset camera"), QKeySequence(Qt::Key_R), this, &MainWindow::onResetCamera);
    view->addSeparator();
    view->addAction(tr("&Set camera keyframe"),
                    QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K),
                    this, &MainWindow::onSetCameraKeyframe);
    view->addAction(tr("C&lear camera keyframe"),
                    this, &MainWindow::onClearCameraKeyframe);
    view->addAction(tr("Clear &all camera keyframes"),
                    this, &MainWindow::onClearAllCameraKeyframes);

    // ─── Shape ───────────────────────────────────────────────────────────────
    auto* shape = mbar->addMenu(tr("&Shape"));
    auto* shapeGroup = new QActionGroup(this);
    shapeGroup->setExclusive(true);
    auto add_shape = [&](const QString& name, ShapeType s, bool checked) {
        auto* a = shape->addAction(name);
        a->setCheckable(true);
        a->setChecked(checked);
        a->setData(static_cast<int>(s));
        shapeGroup->addAction(a);
    };
    add_shape(tr("&Cube"),     ShapeType::Cube,     true);
    add_shape(tr("&Sphere"),   ShapeType::Sphere,   false);
    add_shape(tr("Cy&linder"), ShapeType::Cylinder, false);
    add_shape(tr("&Pyramid"),  ShapeType::Pyramid,  false);
    connect(shapeGroup, &QActionGroup::triggered, this, [this](QAction* a) {
        onShapeChanged(a->data().toInt());
    });

    shape->addSeparator();
    shape->addAction(tr("&Grid size…"), this, [this]() {
        bool ok = false;
        const int n = QInputDialog::getInt(
            this, tr("Grid size"),
            tr("Grid size (3–32 — DEC-005):"),
            m_mask->gridSize(), 3, 32, 1, &ok);
        if (ok) onGridSizeChanged(n);
    });

    // ─── Audio ───────────────────────────────────────────────────────────────
    auto* audio = mbar->addMenu(tr("&Audio"));
    audio->addAction(tr("Select &input device…"), this, &MainWindow::onPickAudioDevice);
    audio->addSeparator();
    m_captureAction = audio->addAction(tr("&Capture to timeline"));
    m_captureAction->setCheckable(true);
    m_captureAction->setEnabled(false);   // enabled once a reactive mode is on
    connect(m_captureAction, &QAction::toggled, this, &MainWindow::onCaptureToggled);
}

// ── Toolbar (tool selection) ─────────────────────────────────────────────────

void MainWindow::buildToolbar() {
    auto* bar = addToolBar(tr("Tools"));
    bar->setObjectName(QStringLiteral("ToolsBar"));
    bar->setMovable(true);

    m_toolGroup = new QActionGroup(this);
    m_toolGroup->setExclusive(true);

    auto add_tool = [&](const QString& text, const QString& shortcut, Tool t, bool checked) {
        auto* a = bar->addAction(text);
        a->setCheckable(true);
        a->setChecked(checked);
        a->setShortcut(QKeySequence(shortcut));
        a->setData(static_cast<int>(t));
        m_toolGroup->addAction(a);
    };
    add_tool(tr("Paint"), QStringLiteral("P"), Tool::Paint, true);
    add_tool(tr("Erase"), QStringLiteral("E"), Tool::Erase, false);
    add_tool(tr("Fill"),  QStringLiteral("F"), Tool::Fill,  false);
    add_tool(tr("Pick"),  QStringLiteral("K"), Tool::Pick,  false);

    connect(m_toolGroup, &QActionGroup::triggered, this, [this](QAction* a) {
        m_viewport->setTool(static_cast<Tool>(a->data().toInt()));
        statusBar()->showMessage(tr("Tool: %1").arg(a->text()), 1500);
    });

    // Mirror toggles — duplicate each Paint/Erase stroke across the cube's
    // X/Y/Z midplanes. All three combine (up to 8x mirroring).
    bar->addSeparator();
    auto add_mirror = [&](const QString& text, const QString& tip,
                          void (CubeViewport::*setter)(bool)) {
        auto* a = bar->addAction(text);
        a->setCheckable(true);
        a->setToolTip(tip);
        connect(a, &QAction::toggled, this, [this, setter](bool on) {
            (m_viewport->*setter)(on);
        });
    };
    add_mirror(tr("Mirror X"),
               tr("Mirror each stroke across the X midplane."),
               &CubeViewport::setMirrorX);
    add_mirror(tr("Mirror Y"),
               tr("Mirror each stroke across the Y midplane."),
               &CubeViewport::setMirrorY);
    add_mirror(tr("Mirror Z"),
               tr("Mirror each stroke across the Z midplane."),
               &CubeViewport::setMirrorZ);

    // Audio reactive controls live in their own dock (AudioReactivePanel),
    // not the toolbar — see buildDocks().
}

// ── Signal wiring ────────────────────────────────────────────────────────────

void MainWindow::wireSignals() {
    connect(m_viewport, &CubeViewport::voxelEdited,
            this, &MainWindow::onVoxelEdited);
    connect(m_viewport, &CubeViewport::colorPicked,
            this, &MainWindow::onColorPicked);
    connect(m_viewport, &CubeViewport::strokeCommitted,
            this, &MainWindow::onStrokeCommitted);

    // Adding/deleting/replacing frames invalidates any pending undo records
    // (their saved frameIndex may now point at a different frame or none).
    // Camera keyframes are indexed by frame too, so drop them as well.
    connect(m_timeline.get(), &AnimationTimeline::timelineStructureChanged,
            this, [this]() {
                if (m_undoStack) m_undoStack->clear();
                m_cameraKeyframes.clear();
            });

    // During playback, drive the camera from the keyframe sequence so the
    // user can author fly-throughs without scripting them by hand.
    connect(m_timeline.get(), &AnimationTimeline::currentFrameChanged,
            this, &MainWindow::onPlaybackCameraTick);

    connect(m_colorPicker, &ColorPickerWidget::colorChanged,
            this, &MainWindow::onColorChanged);

    connect(m_sliceControl, &SliceControlWidget::sliceChanged,
            this, &MainWindow::onSliceChanged);

    connect(m_audioEngine.get(), &AudioReactiveEngine::frameReady,
            this, &MainWindow::onReactiveFrame);
    connect(m_audioEngine.get(), &AudioReactiveEngine::frameCaptured,
            this, &MainWindow::onReactiveFrameCaptured);
    connect(m_audioEngine.get(), &AudioReactiveEngine::errorOccurred,
            this, &MainWindow::onAudioError);
    connect(m_audioEngine.get(), &AudioReactiveEngine::bandsReadyForPreset,
            this, &MainWindow::onReactivePresetBands);

    // Audio reactive dock → engine lifecycle handled here.
    connect(m_audioPanel, &AudioReactivePanel::modeChanged,
            this, &MainWindow::onReactiveModeChanged);
    connect(m_audioPanel, &AudioReactivePanel::blendChanged,
            this, &MainWindow::onReactiveBlendChanged);
    connect(m_audioPanel, &AudioReactivePanel::presetLoadRequested,
            this, &MainWindow::onLoadReactivePreset);
}

// ── Slots ────────────────────────────────────────────────────────────────────

void MainWindow::onVoxelEdited(int /*x*/, int /*y*/, int /*z*/,
                                uint8_t /*r*/, uint8_t /*g*/, uint8_t /*b*/,
                                bool /*erased*/) {
    // Frame thumbnail + info panel refresh themselves via AnimationTimeline
    // signals — nothing else to do here.
}

void MainWindow::onColorPicked(uint8_t r, uint8_t g, uint8_t b) {
    m_colorPicker->setCurrentColor(r, g, b);   // also pushes to history
    m_viewport->setPaintColor(r, g, b);
}

void MainWindow::onStrokeCommitted(VoxelStroke stroke) {
    if (!m_undoStack) return;
    // The viewport already applied the stroke during the drag, so the
    // QUndoCommand's first redo() is suppressed; subsequent redo() calls
    // re-apply after a user-driven undo.
    m_undoStack->push(new VoxelStrokeCommand(m_timeline.get(), std::move(stroke)));
}

void MainWindow::onCopy() {
    if (!m_timeline) return;
    const VoxelFrame& src = m_timeline->currentFrame();

    m_clipboard = VoxelFrame{};
    int copied = 0;
    for (const auto& [k, c] : src.voxels()) {
        // Slice acts as the selection: skip voxels outside any active slice.
        if (m_sliceX >= 0 && k.x != m_sliceX) continue;
        if (m_sliceY >= 0 && k.y != m_sliceY) continue;
        if (m_sliceZ >= 0 && k.z != m_sliceZ) continue;
        m_clipboard.set(k.x, k.y, k.z, c[0], c[1], c[2]);
        ++copied;
    }
    m_clipboardHasContent = (copied > 0);
    statusBar()->showMessage(
        copied > 0 ? tr("Copied %1 voxels.").arg(copied)
                   : tr("Nothing to copy (no lit voxels in selection)."),
        2500);
}

void MainWindow::onPaste() {
    if (!m_timeline || !m_clipboardHasContent) {
        statusBar()->showMessage(tr("Clipboard is empty — copy something first."),
                                 2500);
        return;
    }
    if (!m_mask) return;

    // Build a single VoxelStroke covering every paste change so the whole
    // paste is one undo step. Clipboard voxels with no current value enter
    // as additions; clipboard voxels overwriting an existing colour record
    // the old colour for undo.
    VoxelStroke stroke;
    stroke.frameIndex = m_timeline->currentIndex();

    VoxelFrame& dst = m_timeline->currentFrame();
    int applied = 0, skipped = 0;
    for (const auto& [k, c] : m_clipboard.voxels()) {
        if (!m_mask->contains(k.x, k.y, k.z)) { ++skipped; continue; }
        const auto cur = dst.get(k.x, k.y, k.z);
        // Identity guard — skip if the destination already has this colour.
        if (cur && (*cur)[0] == c[0] && (*cur)[1] == c[1] && (*cur)[2] == c[2]) continue;

        VoxelChange ch{};
        ch.x = k.x; ch.y = k.y; ch.z = k.z;
        if (cur) { ch.hadValue = true; ch.oldValue = *cur; }
        ch.willHaveValue = true;
        ch.newValue      = c;
        stroke.changes.push_back(ch);

        dst.set(k.x, k.y, k.z, c[0], c[1], c[2]);
        ++applied;
    }
    if (applied > 0) {
        m_timeline->notifyCurrentFrameEdited();
        if (m_undoStack) {
            m_undoStack->push(new VoxelStrokeCommand(m_timeline.get(), std::move(stroke)));
        }
    }
    const QString msg = skipped > 0
        ? tr("Pasted %1 voxels (%2 outside mask).").arg(applied).arg(skipped)
        : tr("Pasted %1 voxels.").arg(applied);
    statusBar()->showMessage(msg, 2500);
}

// ── Camera keyframes ─────────────────────────────────────────────────────────

void MainWindow::onSetCameraKeyframe() {
    if (!m_timeline || !m_viewport) return;
    const int idx = m_timeline->currentIndex();
    const auto& cam = m_viewport->camera();
    m_cameraKeyframes[idx] = {cam.theta(), cam.phi(), cam.radius(), cam.target()};
    statusBar()->showMessage(
        tr("Camera keyframe set at frame %1 (%2 total).")
            .arg(idx + 1).arg(m_cameraKeyframes.size()),
        3000);
}

void MainWindow::onClearCameraKeyframe() {
    if (!m_timeline) return;
    const int idx = m_timeline->currentIndex();
    if (m_cameraKeyframes.erase(idx) > 0) {
        statusBar()->showMessage(
            tr("Camera keyframe at frame %1 cleared.").arg(idx + 1), 2500);
    } else {
        statusBar()->showMessage(
            tr("No camera keyframe at frame %1.").arg(idx + 1), 2500);
    }
}

void MainWindow::onClearAllCameraKeyframes() {
    if (m_cameraKeyframes.empty()) {
        statusBar()->showMessage(tr("No camera keyframes to clear."), 2500);
        return;
    }
    const int n = static_cast<int>(m_cameraKeyframes.size());
    m_cameraKeyframes.clear();
    statusBar()->showMessage(tr("Cleared %1 camera keyframes.").arg(n), 2500);
}

void MainWindow::onPlaybackCameraTick(int frameIdx) {
    // Only drive the camera during actual playback — leave the user's manual
    // orbit alone while they're stepping through frames or editing.
    if (m_cameraKeyframes.empty() || !m_timeline || !m_timeline->isPlaying()) return;
    applyKeyframe(m_viewport->camera(),
                  interpolateKeyframe(m_cameraKeyframes, frameIdx));
    m_viewport->update();
}

// ── Animation export ─────────────────────────────────────────────────────────

void MainWindow::onExportAnimation() {
    if (!m_timeline || m_timeline->frameCount() == 0) {
        QMessageBox::information(this, tr("Nothing to export"),
            tr("The timeline is empty."));
        return;
    }

    QString selectedFilter;
    QString path = QFileDialog::getSaveFileName(
        this, tr("Export animation"), QString(),
        tr("MP4 video (*.mp4);;GIF image (*.gif);;WebM video (*.webm);;PNG sequence (*.png)"),
        &selectedFilter);
    if (path.isEmpty()) return;

    // Linux's native save dialog doesn't append the filter's extension. If
    // the user typed "testone" with the GIF filter active, fix it to
    // "testone.gif" so ffmpeg picks the right container.
    const bool hasKnownExt =
           path.endsWith(QStringLiteral(".mp4"),  Qt::CaseInsensitive)
        || path.endsWith(QStringLiteral(".gif"),  Qt::CaseInsensitive)
        || path.endsWith(QStringLiteral(".webm"), Qt::CaseInsensitive)
        || path.endsWith(QStringLiteral(".png"),  Qt::CaseInsensitive);
    if (!hasKnownExt) {
        if      (selectedFilter.contains(QStringLiteral(".gif")))  path += QStringLiteral(".gif");
        else if (selectedFilter.contains(QStringLiteral(".webm"))) path += QStringLiteral(".webm");
        else if (selectedFilter.contains(QStringLiteral(".png")))  path += QStringLiteral(".png");
        else                                                       path += QStringLiteral(".mp4");
    }
    const bool isGif = path.endsWith(QStringLiteral(".gif"), Qt::CaseInsensitive);
    const bool isPng = path.endsWith(QStringLiteral(".png"), Qt::CaseInsensitive);

    // PNG sequence doesn't need ffmpeg — QImage::save handles it directly.
    QString ffmpeg;
    if (!isPng) {
        ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
        if (ffmpeg.isEmpty()) {
            QMessageBox::warning(this, tr("ffmpeg not found"),
                tr("ffmpeg is required to export to %1.\n\n"
                   "Install it via your package manager — on Ubuntu:\n"
                   "    sudo apt install ffmpeg")
                    .arg(isGif ? QStringLiteral("GIF") : QStringLiteral("video")));
            return;
        }
    }

    // Suspend the reactive engine and playback so the export sees clean
    // timeline frames, not whatever the engine is currently overlaying.
    const bool reactiveWasRunning = m_audioEngine && m_audioEngine->isRunning();
    if (reactiveWasRunning) m_audioEngine->stop();
    m_viewport->clearReactiveFrame();
    const int  origFrame  = m_timeline->currentIndex();
    const bool wasPlaying = m_timeline->isPlaying();
    if (wasPlaying) m_timeline->stop();

    // Snapshot the current camera so we can restore it after the export —
    // applying keyframes mid-loop will mutate the live camera.
    const auto& cam = m_viewport->camera();
    const CameraKeyframe savedCamera{cam.theta(), cam.phi(), cam.radius(), cam.target()};
    const bool hasKeyframes = !m_cameraKeyframes.empty();
    auto driveCamera = [this, hasKeyframes](int i) {
        if (!hasKeyframes) return;
        applyKeyframe(m_viewport->camera(),
                      interpolateKeyframe(m_cameraKeyframes, i));
    };

    // Probe frame 0 to discover the actual pixel size of the framebuffer
    // (devicePixelRatio means QWidget::size() may not match the texture's
    // pixel count). Round to even dims since H.264 needs that.
    m_timeline->selectFrame(0);
    driveCamera(0);
    QApplication::processEvents();
    QImage probe = m_viewport->grabFramebuffer().convertToFormat(QImage::Format_RGBA8888);
    if (probe.isNull()) {
        QMessageBox::warning(this, tr("Export failed"),
            tr("Failed to capture the viewport framebuffer."));
        m_timeline->selectFrame(origFrame);
        return;
    }
    // Video codecs need even dimensions; PNG can take whatever the
    // framebuffer gives us. Computing both so the loop below can pick.
    const int w = isPng ? probe.width()  : (probe.width()  / 2) * 2;
    const int h = isPng ? probe.height() : (probe.height() / 2) * 2;
    if (w != probe.width() || h != probe.height()) {
        probe = probe.copy(0, 0, w, h);
    }

    // PNG path resolves a basename + directory for the numbered files.
    // E.g. user picks "/tmp/myanim.png" → writes "/tmp/myanim_0001.png", etc.
    QString pngBase, pngDir;
    if (isPng) {
        const QFileInfo fi(path);
        pngDir  = fi.absolutePath();
        pngBase = fi.completeBaseName();          // "myanim" from "myanim.png"
    }

    const int fps = std::max(1, m_timeline->fps());
    QStringList args;
    if (!isPng) {
        args = {
            QStringLiteral("-y"),
            QStringLiteral("-f"),            QStringLiteral("rawvideo"),
            QStringLiteral("-pixel_format"), QStringLiteral("rgba"),
            QStringLiteral("-video_size"),   QString("%1x%2").arg(w).arg(h),
            QStringLiteral("-framerate"),    QString::number(fps),
            QStringLiteral("-i"),            QStringLiteral("-"),
        };
        if (isGif) {
            // Single-pipeline palettegen → paletteuse keeps GIF size reasonable
            // and avoids the two-pass dance with a temp file.
            args << QStringLiteral("-vf")
                 << QStringLiteral("split[a][b];[a]palettegen=stats_mode=diff[p];"
                                   "[b][p]paletteuse=dither=bayer:bayer_scale=5:diff_mode=rectangle");
        } else {
            args << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p")
                 << QStringLiteral("-crf")     << QStringLiteral("20");
        }
        args << path;
    }

    QProcess proc;
    if (!isPng) {
        proc.start(ffmpeg, args);
        if (!proc.waitForStarted(3000)) {
            QMessageBox::warning(this, tr("Export failed"),
                tr("ffmpeg failed to start: %1").arg(proc.errorString()));
            m_timeline->selectFrame(origFrame);
            return;
        }
    }

    const int n = m_timeline->frameCount();
    QProgressDialog progress(
        tr("Exporting frame 1 of %1…").arg(n),
        tr("Cancel"), 0, n, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(200);

    // Track PNG outputs so we can clean up on cancel.
    QStringList pngFiles;

    auto writeFrame = [&](int frameIdx, const QImage& img) -> bool {
        if (isPng) {
            const QString file = QString("%1/%2_%3.png")
                .arg(pngDir, pngBase,
                     QString("%1").arg(frameIdx + 1, 4, 10, QChar('0')));
            if (!img.save(file, "PNG")) return false;
            pngFiles.append(file);
            return true;
        }
        proc.write(reinterpret_cast<const char*>(img.constBits()),
                   static_cast<qint64>(img.sizeInBytes()));
        // Backpressure: wait for the OS pipe buffer to drain so we don't
        // outrun ffmpeg's input on long timelines.
        return proc.waitForBytesWritten(5000);
    };

    bool cancelled = false;
    if (!writeFrame(0, probe)) {
        QMessageBox::warning(this, tr("Export failed"),
            isPng ? tr("Failed to write the first PNG file.")
                  : tr("ffmpeg stalled while accepting frame 1."));
        if (!isPng) { proc.kill(); proc.waitForFinished(1000); }
        m_timeline->selectFrame(origFrame);
        return;
    }
    progress.setValue(1);

    for (int i = 1; i < n; ++i) {
        if (progress.wasCanceled()) { cancelled = true; break; }
        progress.setLabelText(tr("Exporting frame %1 of %2…").arg(i + 1).arg(n));
        m_timeline->selectFrame(i);
        driveCamera(i);
        QApplication::processEvents();
        QImage img = m_viewport->grabFramebuffer().convertToFormat(QImage::Format_RGBA8888);
        if (img.width() != w || img.height() != h) img = img.copy(0, 0, w, h);
        if (!writeFrame(i, img)) { cancelled = true; break; }
        progress.setValue(i + 1);
    }

    if (!isPng) {
        proc.closeWriteChannel();
        // GIF's palette pass needs more headroom than a straight video encode.
        proc.waitForFinished(isGif ? 60000 : 15000);
    }

    // Restore frame + camera so the user's editing context is unchanged.
    m_timeline->selectFrame(origFrame);
    applyKeyframe(m_viewport->camera(), savedCamera);
    m_viewport->update();

    if (cancelled) {
        if (!isPng) {
            proc.kill();
            proc.waitForFinished(1000);
            QFile::remove(path);
        } else {
            // Leave nothing half-finished on disk.
            for (const QString& f : pngFiles) QFile::remove(f);
        }
        statusBar()->showMessage(tr("Export cancelled."), 3000);
        return;
    }
    if (!isPng) {
        if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
            const QString err = QString::fromUtf8(proc.readAllStandardError());
            QMessageBox::warning(this, tr("Export failed"),
                tr("ffmpeg exited with code %1.\n\n%2")
                    .arg(proc.exitCode())
                    .arg(err.isEmpty() ? tr("(no stderr output)") : err));
            return;
        }
    }
    if (isPng) {
        statusBar()->showMessage(
            tr("Exported %1 PNG frames to %2/%3_NNNN.png")
                .arg(n).arg(pngDir, pngBase), 5000);
    } else {
        statusBar()->showMessage(tr("Exported %1 frames to %2").arg(n).arg(path), 5000);
    }
}

void MainWindow::onColorChanged(uint8_t r, uint8_t g, uint8_t b) {
    m_viewport->setPaintColor(r, g, b);
}

void MainWindow::onSliceChanged(int sx, int sy, int sz) {
    m_viewport->setSlice(sx, sy, sz);
    m_sliceX = sx;
    m_sliceY = sy;
    m_sliceZ = sz;
}

// ── File menu ────────────────────────────────────────────────────────────────

void MainWindow::onNew() {
    if (!confirmDiscardIfDirty(tr("Start a new project?"))) return;
    m_currentPath.clear();
    rebuildMask(kDefaultGrid, ShapeType::Cube);
    m_timeline->clearAll();
}

void MainWindow::onOpen() {
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open"), QString(),
        tr("Scintilla projects and scripts (*.json *.py);;"
           "Scintilla JSON (*.json);;Python scripts (*.py);;"
           "All files (*)"));
    if (path.isEmpty()) return;

    // Dispatch by extension. .py files load into the editor without
    // running so the user can review or edit before invoking Run.
    if (path.endsWith(QStringLiteral(".py"), Qt::CaseInsensitive)) {
        if (!QFileInfo(path).exists()) {
            QMessageBox::warning(this, tr("Open failed"),
                tr("File does not exist: %1").arg(path));
            return;
        }
        if (m_presetEditor) {
            m_presetEditor->loadFile(path);
            m_presetEditor->parentWidget()->show();
            m_presetEditor->raise();
        }
        statusBar()->showMessage(
            tr("Loaded %1 — click Run or use File → Run animation script…")
                .arg(QFileInfo(path).fileName()),
            5000);
        return;
    }

    auto newMask = std::shared_ptr<ShapeMask>();
    const LoadResult r = JsonSerializer::load(path, &newMask, m_timeline.get());
    if (!r.ok) {
        QMessageBox::warning(this, tr("Open failed"), r.errorMessage);
        return;
    }
    if (r.gridSizeClamped) {
        QMessageBox::information(
            this, tr("Grid size clamped"),
            tr("The file requested a grid size larger than 32; clamped to 32 (DEC-005)."));
    }
    applyMask(std::move(newMask));
    m_currentPath = path;
    statusBar()->showMessage(tr("Loaded %1").arg(path), 3000);
}

void MainWindow::onSave() {
    if (m_currentPath.isEmpty()) { onSaveAs(); return; }
    saveTo(m_currentPath);
}

void MainWindow::onSaveAs() {
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save Scintilla project"), QString(),
        tr("Scintilla JSON (*.json)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) path += ".json";
    if (saveTo(path)) m_currentPath = path;
}

bool MainWindow::saveTo(const QString& path) {
    QString err;
    if (!JsonSerializer::save(path, *m_mask, *m_timeline, &err)) {
        QMessageBox::warning(this, tr("Save failed"), err);
        return false;
    }
    statusBar()->showMessage(tr("Saved to %1").arg(path), 3000);
    return true;
}

// ── View menu ────────────────────────────────────────────────────────────────

void MainWindow::onToggleGhost(bool on)      { m_viewport->setShowGhost(on); }
void MainWindow::onToggleBounds(bool on)     { m_viewport->setShowBounds(on); }
void MainWindow::onToggleAxisGizmo(bool on)  { m_viewport->setShowAxisGizmo(on); }
void MainWindow::onToggleAutoRotate(bool on) { m_viewport->setAutoRotate(on); }
void MainWindow::onResetCamera()             { m_viewport->resetCamera(); }

// ── Shape menu ───────────────────────────────────────────────────────────────

void MainWindow::onShapeChanged(int shapeIndex) {
    const auto s = static_cast<ShapeType>(shapeIndex);
    if (s == m_mask->shape()) return;
    rebuildMask(m_mask->gridSize(), s);
}

void MainWindow::onGridSizeChanged(int n) {
    if (n == m_mask->gridSize()) return;
    rebuildMask(n, m_mask->shape());
}

// ── Internal helpers ─────────────────────────────────────────────────────────

void MainWindow::rebuildMask(int gridSize, ShapeType shape) {
    if (!confirmDiscardIfDirty(tr("Changing shape or grid size clears the animation. Continue?"))) {
        return;
    }
    m_timeline->clearAll();
    applyMask(std::make_shared<ShapeMask>(gridSize, shape));

    if (gridSize > 24) {
        statusBar()->showMessage(
            tr("Large grid (%1").arg(gridSize)
                + QStringLiteral("³ = ")
                + tr("%1 LEDs) — performance may degrade. SPEC §3.2.")
                    .arg(gridSize * gridSize * gridSize),
            5000);
    }
}

// Single point of mask propagation — viewport, slice control, info panel,
// audio engine, and audio base frame all stay in sync (BUG-011).
void MainWindow::applyMask(std::shared_ptr<ShapeMask> mask) {
    m_mask = std::move(mask);
    m_viewport->setMask(m_mask);
    m_sliceControl->setGridSize(m_mask->gridSize());
    m_frameInfo->setMask(m_mask);
    m_audioEngine->setMask(m_mask);
    if (m_timeline) {
        m_audioEngine->setBaseFrame(m_timeline->currentFrame());
    }
    // Keep the running preset (if any) in sync with the cube geometry; the
    // runner re-sends "load" so the preset's on_load sees the new mask.
    if (m_presetRunner) {
        m_presetRunner->setCubeMeta(m_mask->gridSize(),
                                    QString::fromStdString(shapeTypeName(m_mask->shape())),
                                    m_mask->count());
    }
}

bool MainWindow::confirmDiscardIfDirty(const QString& reason) {
    if (!m_timeline || !m_timeline->hasContent()) return true;
    const auto ans = QMessageBox::question(
        this, tr("Discard animation?"), reason,
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return ans == QMessageBox::Yes;
}

// ── Audio reactive slots (Phase 3) ───────────────────────────────────────────

void MainWindow::onPickAudioDevice() {
    AudioDevicePicker dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    m_audioDeviceIndex   = dlg.selectedDeviceIndex();
    m_audioSampleRate    = dlg.selectedSampleRate();
    m_audioMonitorSource = dlg.selectedMonitorSource();

    if (!m_audioMonitorSource.isEmpty()) {
        statusBar()->showMessage(
            tr("System audio routing: %1 (per-stream redirect via pactl) @ %2 Hz")
                .arg(m_audioMonitorSource)
                .arg(static_cast<double>(m_audioSampleRate)),
            6000);
    } else {
        statusBar()->showMessage(
            tr("Audio device set: index %1 @ %2 Hz")
                .arg(m_audioDeviceIndex)
                .arg(static_cast<double>(m_audioSampleRate)),
            4000);
    }
}

void MainWindow::onReactiveModeChanged(ReactiveMode mode) {
    if (mode == ReactiveMode::Off) {
        m_audioEngine->setMode(ReactiveMode::Off);
        m_audioEngine->stop();
        m_viewport->clearReactiveFrame();
        m_captureAction->setChecked(false);
        m_captureAction->setEnabled(false);
        return;
    }

    if (m_audioDeviceIndex < 0 && m_audioMonitorSource.isEmpty()) {
        QMessageBox::information(
            this, tr("Pick an audio device first"),
            tr("Use Audio → Select input device… to choose a monitor / loopback source, "
               "then re-select the reactive mode."));
        return;
    }

    m_audioEngine->setMode(mode);
    m_audioEngine->setBaseFrame(m_timeline ? m_timeline->currentFrame() : VoxelFrame());
    if (!m_audioEngine->isRunning()) {
        m_audioEngine->start(m_audioDeviceIndex, m_audioSampleRate, m_audioMonitorSource);
    }
    m_captureAction->setEnabled(true);
}

void MainWindow::onReactiveBlendChanged(ReactiveBlend blend) {
    m_audioEngine->setBlend(blend);
    // Refresh the base frame at the moment the blend changes so the user
    // gets a predictable visual reference (DEC-013: base is "currently
    // selected frame, frozen").
    m_audioEngine->setBaseFrame(m_timeline ? m_timeline->currentFrame() : VoxelFrame());
}

void MainWindow::onCaptureToggled(bool on) {
    if (on) {
        m_audioEngine->startCapture(m_timeline.get());
        statusBar()->showMessage(
            tr("Capture started — frames are being appended to the timeline."), 4000);
    } else {
        m_audioEngine->stopCapture();
        statusBar()->showMessage(tr("Capture stopped."), 2000);
    }
}

void MainWindow::onReactiveFrame(VoxelFrame f) {
    m_viewport->setReactiveFrame(std::move(f));
}

void MainWindow::onReactiveFrameCaptured(VoxelFrame f) {
    if (!m_timeline) return;
    // Append the captured frame as a new timeline entry. DEC-014 soft-cap
    // at 500 frames: stop capturing automatically when we hit it.
    if (m_timeline->frameCount() >= 500) {
        if (m_captureAction) m_captureAction->setChecked(false);
        statusBar()->showMessage(
            tr("Capture stopped — timeline reached the 500-frame soft cap."), 5000);
        return;
    }
    m_timeline->addFrame();
    m_timeline->currentFrame() = std::move(f);
    m_timeline->notifyCurrentFrameEdited();
}

// ── Preset scripting (Phase 4 step B — offline playback) ────────────────────

namespace {
    constexpr int kPresetPreviewFrames   = 120;     // ~10 s at 12 fps timeline
    constexpr int kPresetPlaybackTickMs  = 16;      // ~60 Hz tick
}

void MainWindow::onRunPreset() {
    if (m_presetRunner && m_presetRunner->isLoaded()) {
        QMessageBox::information(this, tr("Preset already running"),
            tr("A preset is currently playing. Wait for it to finish or open "
               "a new project to interrupt."));
        return;
    }

    const QString defaultDir = QDir(QCoreApplication::applicationDirPath())
                                   .absoluteFilePath(QStringLiteral("../presets/builtin/reactive"));
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Run preset"), defaultDir,
        tr("Python presets (*.py)"));
    if (path.isEmpty()) return;

    // If the user picked an Animation script, the offline-synthetic-audio
    // path would just spam frame errors; route it through the animation
    // path which knows how to receive its frames.
    if (detectScriptType(path) == ScriptType::Animation) {
        runAnimationScript(path);
        return;
    }

    if (!confirmDiscardIfDirty(tr("Running a preset appends generated frames to the timeline. Continue?"))) {
        return;
    }

    ensurePresetRunner();
    m_presetRunner->setCubeMeta(m_mask->gridSize(),
                                QString::fromStdString(shapeTypeName(m_mask->shape())),
                                m_mask->count());

    m_presetFramesRequested = kPresetPreviewFrames;
    m_presetFramesSent      = 0;
    m_presetFramesReceived  = 0;

    // Clear the timeline before populating with new generated frames.
    m_timeline->clearAll();
    statusBar()->showMessage(tr("Loading preset %1…").arg(QFileInfo(path).fileName()),
                             4000);

    m_presetRunner->loadPreset(path);

    // Drive the offline playback via a timer. Synthetic audio (slow sine
    // bands + periodic beats) so presets that ignore the audio argument
    // still animate, and presets that use it have something to react to.
    if (!m_presetPlaybackTimer) {
        m_presetPlaybackTimer = new QTimer(this);
        connect(m_presetPlaybackTimer, &QTimer::timeout,
                this, &MainWindow::tickPresetPlayback);
    }
    m_presetPlaybackTimer->start(kPresetPlaybackTickMs);
}

void MainWindow::tickPresetPlayback() {
    if (!m_presetRunner || !m_presetRunner->isLoaded()) {
        // Subprocess died or wasn't ready yet; let onPresetError handle the
        // surface message.
        if (m_presetFramesSent >= m_presetFramesRequested) {
            m_presetPlaybackTimer->stop();
        }
        return;
    }

    if (m_presetFramesSent >= m_presetFramesRequested) {
        // All frames queued — stop the timer; the runner may still be
        // returning the last few responses, which onPresetFrameReady will
        // handle as they arrive.
        m_presetPlaybackTimer->stop();
        m_presetRunner->unload();
        statusBar()->showMessage(
            tr("Preset playback queued %1 frames; received %2 so far.")
                .arg(m_presetFramesRequested)
                .arg(m_presetFramesReceived),
            4000);
        return;
    }

    // Synthetic audio: each band a phase-shifted sine, RMS a slow oscillation,
    // beat every ~30 frames. Lets the playback drive both audio-naive and
    // audio-driven presets to a believable result.
    BandData bd;
    const int n = m_mask->gridSize();
    const float t = static_cast<float>(m_presetFramesSent) * 0.05f;
    for (int i = 0; i < n && i < static_cast<int>(bd.bands.size()); ++i) {
        bd.bands[static_cast<size_t>(i)] =
            0.5f + 0.35f * std::sin(t + static_cast<float>(i) * 0.6f);
    }
    bd.rms      = 0.25f + 0.15f * std::sin(t * 0.7f);
    bd.centroid = 0.5f  + 0.25f * std::sin(t * 0.3f);
    bd.beat     = (m_presetFramesSent % 30 == 0);

    m_presetRunner->pushFrame(bd);
    ++m_presetFramesSent;
}

void MainWindow::onPresetFrameReady(VoxelFrame f) {
    // Discriminator: three modes can be feeding frames in.
    //   1. Animation script — frames append to the timeline as they arrive.
    //   2. Offline preset preview — driven by a QTimer with synthetic audio.
    //   3. Live reactive mode — frames go to the viewport overlay.
    if (m_animationMode) {
        if (!m_timeline) return;
        if (m_presetFramesReceived == 0) {
            m_timeline->currentFrame() = std::move(f);
            m_timeline->notifyCurrentFrameEdited();
        } else {
            m_timeline->addFrame();
            m_timeline->currentFrame() = std::move(f);
            m_timeline->notifyCurrentFrameEdited();
        }
        ++m_presetFramesReceived;
        return;
    }

    const bool offline = m_presetPlaybackTimer && m_presetPlaybackTimer->isActive();

    if (offline) {
        if (m_presetFramesReceived == 0) {
            // First frame replaces the (already-cleared) initial frame so the
            // timeline starts at frame 1 with content, not an empty frame + new.
            m_timeline->currentFrame() = std::move(f);
            m_timeline->notifyCurrentFrameEdited();
        } else {
            m_timeline->addFrame();
            m_timeline->currentFrame() = std::move(f);
            m_timeline->notifyCurrentFrameEdited();
        }
        ++m_presetFramesReceived;
        return;
    }

    // Live reactive mode: drive the viewport's reactive overlay, and if the
    // capture toggle is on, append to the timeline the same way the built-in
    // modes do (mirrors onReactiveFrameCaptured's 500-frame soft cap).
    m_viewport->setReactiveFrame(f);
    if (m_captureAction && m_captureAction->isChecked() && m_timeline) {
        if (m_timeline->frameCount() >= 500) {
            m_captureAction->setChecked(false);
            statusBar()->showMessage(
                tr("Capture stopped — timeline reached the 500-frame soft cap."), 5000);
            return;
        }
        m_timeline->addFrame();
        m_timeline->currentFrame() = std::move(f);
        m_timeline->notifyCurrentFrameEdited();
    }
}

void MainWindow::onPresetLoaded(const QString& name) {
    statusBar()->showMessage(tr("Preset loaded: %1").arg(name), 3000);
    if (m_audioPanel)   m_audioPanel->setPresetStatus(name);
    if (m_presetEditor && m_presetRunner) {
        m_presetEditor->loadFile(m_presetRunner->filePath());
    }
}

// Construct the PresetRunner the first time it's needed and wire its signals.
// Both the offline File→Run preset path and the live reactive mode share the
// same runner instance.
void MainWindow::ensurePresetRunner() {
    if (m_presetRunner) return;
    m_presetRunner = std::make_unique<PresetRunner>(this);
    connect(m_presetRunner.get(), &PresetRunner::frameReady,
            this, &MainWindow::onPresetFrameReady);
    connect(m_presetRunner.get(), &PresetRunner::presetLoaded,
            this, &MainWindow::onPresetLoaded);
    connect(m_presetRunner.get(), &PresetRunner::errorOccurred,
            this, &MainWindow::onPresetError);
    connect(m_presetRunner.get(), &PresetRunner::presetUnloaded,
            this, [this]() {
                // Only clear the Audio panel's "Loaded: X" indicator. The
                // editor's file is independent of the runner's lifecycle —
                // animations call unload() right after run() returns, and
                // clearing the editor there would make the file disappear
                // exactly when the user wants to iterate on it.
                if (m_audioPanel) m_audioPanel->setPresetStatus(QString());
            });
    connect(m_presetRunner.get(), &PresetRunner::hotReloaded,
            this, [this]() {
                statusBar()->showMessage(tr("Preset hot-reloaded."), 1500);
            });
    connect(m_presetRunner.get(), &PresetRunner::animationComplete,
            this, &MainWindow::onAnimationComplete);
}

// ── Animation scripts (cube.frame() / cube.play() model) ────────────────────

void MainWindow::onRunAnimationScript() {
    const QString defaultDir = QDir(QCoreApplication::applicationDirPath())
                                   .absoluteFilePath(QStringLiteral("../presets/builtin/animations"));
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Run animation script"), defaultDir,
        tr("Python animation (*.py)"));
    if (path.isEmpty()) return;
    runAnimationScript(path);
}

void MainWindow::onNewAnimationScript() {
    // Locate the template alongside the user/ directory; we walk a few
    // likely places matching where the runtime looks for presets.
    QString templatePath;
    for (const QString& base : {
             QCoreApplication::applicationDirPath() + QStringLiteral("/presets"),
             QCoreApplication::applicationDirPath() + QStringLiteral("/../presets"),
             QDir::currentPath() + QStringLiteral("/presets"),
         }) {
        const QString candidate = base + QStringLiteral("/user/animations/_animation_template.py");
        if (QFileInfo(candidate).exists()) { templatePath = candidate; break; }
    }
    if (templatePath.isEmpty()) {
        QMessageBox::warning(this, tr("Template missing"),
            tr("Could not find presets/user/animations/_animation_template.py next "
               "to the binary. Reinstall to restore the bundled templates."));
        return;
    }

    const QString defaultPath = QFileInfo(templatePath).absolutePath()
                                  + QStringLiteral("/my_animation.py");
    QString path = QFileDialog::getSaveFileName(
        this, tr("New animation script"), defaultPath,
        tr("Python animation (*.py)"));
    if (path.isEmpty()) return;
    if (!path.endsWith(QStringLiteral(".py"), Qt::CaseInsensitive)) {
        path += QStringLiteral(".py");
    }

    // Copy template to the target path. Refuse to clobber unless the user
    // chose the existing file via the save dialog (which already prompted).
    QFile src(templatePath);
    if (!src.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("Could not read template"),
            tr("%1: %2").arg(templatePath, src.errorString()));
        return;
    }
    const QByteArray content = src.readAll();
    src.close();

    QFile dst(path);
    if (!dst.open(QFile::WriteOnly | QFile::Truncate | QFile::Text)) {
        QMessageBox::warning(this, tr("Could not create script"),
            tr("%1: %2").arg(path, dst.errorString()));
        return;
    }
    dst.write(content);
    dst.close();

    // Load into the editor and surface the editor tab so the user starts
    // editing immediately.
    if (m_presetEditor) {
        m_presetEditor->loadFile(path);
        m_presetEditor->parentWidget()->show();
        m_presetEditor->raise();
    }
    statusBar()->showMessage(
        tr("Created %1 — edit, then click Run.").arg(QFileInfo(path).fileName()),
        5000);
}

void MainWindow::runAnimationScript(const QString& path) {
    if (path.isEmpty() || !QFileInfo(path).exists()) {
        QMessageBox::warning(this, tr("Script missing"),
            tr("Animation script does not exist: %1").arg(path));
        return;
    }
    if (detectScriptType(path) == ScriptType::Preset) {
        QMessageBox::warning(this, tr("Wrong script type"),
            tr("%1 is a reactive Preset, not an Animation. To play it with "
               "audio, switch the Audio reactive panel to \"Python preset\" "
               "and use its Load preset… button.")
                .arg(QFileInfo(path).fileName()));
        return;
    }
    if (!confirmDiscardIfDirty(
            tr("Running an animation script clears the timeline and replaces it "
               "with the generated frames. Continue?"))) {
        return;
    }

    ensurePresetRunner();
    m_presetRunner->setCubeMeta(m_mask->gridSize(),
                                QString::fromStdString(shapeTypeName(m_mask->shape())),
                                m_mask->count());

    m_animationMode         = true;
    m_presetFramesReceived  = 0;
    // Disable the offline-playback path so its onPresetFrameReady branch
    // isn't accidentally entered.
    if (m_presetPlaybackTimer) m_presetPlaybackTimer->stop();

    m_timeline->clearAll();
    statusBar()->showMessage(tr("Running %1…").arg(QFileInfo(path).fileName()), 4000);
    m_presetRunner->loadPreset(path);
}

void MainWindow::onAnimationComplete(int fps) {
    if (!m_animationMode) return;   // ignore stray play messages
    m_animationMode = false;
    if (m_timeline) m_timeline->setFps(std::clamp(fps, 1, 60));
    statusBar()->showMessage(
        tr("Animation finished — %1 frames at %2 fps.")
            .arg(m_presetFramesReceived).arg(fps),
        5000);
    // The runner subprocess is still alive; tear it down so a fresh run
    // (or a different script) starts clean.
    if (m_presetRunner) m_presetRunner->unload();
}

// ── Preset scripting (Phase 4 step C — live reactive mode) ───────────────────

void MainWindow::onLoadReactivePreset() {
    const QString defaultDir = QDir(QCoreApplication::applicationDirPath())
                                   .absoluteFilePath(QStringLiteral("../presets/builtin/reactive"));
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Load reactive preset"), defaultDir,
        tr("Python presets (*.py)"));
    if (path.isEmpty()) return;

    // Refuse Animation scripts up front. Loading one here and then enabling
    // audio sends one error per audio frame from the subprocess — at ~60 Hz
    // each was opening a modal warning and effectively locking the UI.
    if (detectScriptType(path) == ScriptType::Animation) {
        QMessageBox::warning(this, tr("Wrong script type"),
            tr("%1 is an Animation script (run-once). The Python preset "
               "reactive mode needs a Preset script.\n\n"
               "Use File → Run animation script… to play this file instead.")
                .arg(QFileInfo(path).fileName()));
        return;
    }

    ensurePresetRunner();
    m_presetRunner->setCubeMeta(m_mask->gridSize(),
                                QString::fromStdString(shapeTypeName(m_mask->shape())),
                                m_mask->count());
    m_presetRunner->loadPreset(path);
}

void MainWindow::onReactivePresetBands(BandData d) {
    // The engine emits this only when mode == PythonPreset. If no preset
    // has been loaded yet, skip silently — the user will see "(no preset
    // loaded)" in the panel and can click Load preset… at any time.
    if (!m_presetRunner || !m_presetRunner->isLoaded()) return;
    m_presetRunner->pushFrame(d);
}

void MainWindow::onPresetError(const QString& msg, int line) {
    if (m_presetPlaybackTimer) m_presetPlaybackTimer->stop();

    // If errors arrive while the audio engine is feeding the runner, stop
    // the engine immediately — otherwise we'd be in a feedback loop where
    // each band push triggers another error per audio frame. Without this
    // an Animation loaded into the Python-preset reactive mode floods the
    // UI thread with modal dialogs and locks up the system.
    if (m_audioEngine && m_audioEngine->isRunning()
        && m_audioEngine->mode() == ReactiveMode::PythonPreset) {
        m_audioEngine->setMode(ReactiveMode::Off);
        m_audioEngine->stop();
        m_viewport->clearReactiveFrame();
        if (m_captureAction) {
            m_captureAction->setChecked(false);
            m_captureAction->setEnabled(false);
        }
    }

    // Throttle dialogs to at most one per 5 seconds. Any additional errors
    // in that window are counted and shown in the next dialog.
    constexpr qint64 kThrottleMs = 5000;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_lastPresetErrorMs != 0 && (now - m_lastPresetErrorMs) < kThrottleMs) {
        ++m_suppressedErrors;
        statusBar()->showMessage(
            tr("Preset error suppressed (%1 since last shown).").arg(m_suppressedErrors),
            3000);
        return;
    }
    m_lastPresetErrorMs = now;

    QString detail = (line >= 0)
        ? tr("%1\n\n(line %2)").arg(msg).arg(line)
        : msg;
    if (m_suppressedErrors > 0) {
        detail += tr("\n\n(%1 additional errors suppressed.)").arg(m_suppressedErrors);
        m_suppressedErrors = 0;
    }
    QMessageBox::warning(this, tr("Preset error"), detail);
}

void MainWindow::onAudioError(const QString& msg) {
    QMessageBox::warning(this, tr("Audio error"), msg);
    // Drop back to Off mode on error so the UI reflects reality. The panel's
    // mode combo will follow once we emit its modeChanged signal — for now
    // we just stop the engine cleanly so the next user action starts fresh.
    m_audioEngine->setMode(ReactiveMode::Off);
    m_audioEngine->stop();
    m_viewport->clearReactiveFrame();
    m_captureAction->setChecked(false);
    m_captureAction->setEnabled(false);
}
