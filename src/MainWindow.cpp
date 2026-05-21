#include "MainWindow.h"

#include "renderer/CubeViewport.h"
#include "ui/AudioReactivePanel.h"
#include "ui/ColorPickerWidget.h"
#include "ui/FrameInfoPanel.h"
#include "ui/SliceControlWidget.h"
#include "ui/TimelineWidget.h"
#include "core/JsonSerializer.h"
#include "audio/AudioReactiveEngine.h"
#include "audio/AudioDevicePicker.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
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
#include <QToolBar>

namespace {
    constexpr int kDefaultGrid = 8;
    constexpr int kWindowW     = 1480;
    constexpr int kWindowH     = 860;
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

    m_sliceControl->setGridSize(m_mask->gridSize());
    m_frameInfo->setMask(m_mask);
    m_frameInfo->setTimeline(m_timeline.get());
    m_timelineWidget->setTimeline(m_timeline.get());
    m_colorPicker->setCurrentColor(255, 64, 32);

    makeDock(tr("Colour"),         scrollable(m_colorPicker),  Qt::RightDockWidgetArea);
    makeDock(tr("Slice"),          scrollable(m_sliceControl), Qt::RightDockWidgetArea);
    makeDock(tr("Audio reactive"), scrollable(m_audioPanel),   Qt::RightDockWidgetArea);
    makeDock(tr("Frame"),          scrollable(m_frameInfo),    Qt::RightDockWidgetArea);
    makeDock(tr("Timeline"),       m_timelineWidget,           Qt::BottomDockWidgetArea);
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
    file->addAction(tr("E&xit"),      QKeySequence::Quit,   qApp, &QApplication::quit);

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
    // Audio reactive controls live in their own dock (AudioReactivePanel),
    // not the toolbar — see buildDocks().
}

// ── Signal wiring ────────────────────────────────────────────────────────────

void MainWindow::wireSignals() {
    connect(m_viewport, &CubeViewport::voxelEdited,
            this, &MainWindow::onVoxelEdited);
    connect(m_viewport, &CubeViewport::colorPicked,
            this, &MainWindow::onColorPicked);

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

    // Audio reactive dock → engine lifecycle handled here.
    connect(m_audioPanel, &AudioReactivePanel::modeChanged,
            this, &MainWindow::onReactiveModeChanged);
    connect(m_audioPanel, &AudioReactivePanel::blendChanged,
            this, &MainWindow::onReactiveBlendChanged);
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

void MainWindow::onColorChanged(uint8_t r, uint8_t g, uint8_t b) {
    m_viewport->setPaintColor(r, g, b);
}

void MainWindow::onSliceChanged(int sx, int sy, int sz) {
    m_viewport->setSlice(sx, sy, sz);
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
        this, tr("Open Scintilla project"), QString(),
        tr("Scintilla JSON (*.json);;All files (*)"));
    if (path.isEmpty()) return;

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
            tr("System audio routing: %1 (system default input changed via pactl) @ %2 Hz")
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
