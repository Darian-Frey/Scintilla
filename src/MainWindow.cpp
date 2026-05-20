#include "MainWindow.h"

#include "renderer/CubeViewport.h"
#include "ui/ColorPickerWidget.h"
#include "ui/FrameInfoPanel.h"
#include "ui/SliceControlWidget.h"
#include "ui/TimelineWidget.h"
#include "core/JsonSerializer.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QDockWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QKeySequence>
#include <QMenuBar>
#include <QMessageBox>
#include <QSpinBox>
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
    , m_mask(std::make_shared<ShapeMask>(kDefaultGrid, ShapeType::Cube)) {

    setWindowTitle(tr("Scintilla"));
    resize(kWindowW, kWindowH);

    m_viewport = new CubeViewport(this);
    m_viewport->setMask(m_mask);
    m_viewport->setTimeline(m_timeline.get());
    m_viewport->setPaintColor(255, 64, 32);
    setCentralWidget(m_viewport);

    buildDocks();
    buildMenus();
    buildToolbar();
    wireSignals();

    statusBar()->showMessage(tr("Ready — paint with left-click, orbit with drag, zoom with wheel."));
}

MainWindow::~MainWindow() = default;

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

    m_colorPicker    = new ColorPickerWidget(this);
    m_sliceControl   = new SliceControlWidget(this);
    m_frameInfo      = new FrameInfoPanel(this);
    m_timelineWidget = new TimelineWidget(this);

    m_sliceControl->setGridSize(m_mask->gridSize());
    m_frameInfo->setMask(m_mask);
    m_frameInfo->setTimeline(m_timeline.get());
    m_timelineWidget->setTimeline(m_timeline.get());
    m_colorPicker->setCurrentColor(255, 64, 32);

    makeDock(tr("Colour"),    m_colorPicker,    Qt::RightDockWidgetArea);
    makeDock(tr("Slice"),     m_sliceControl,   Qt::RightDockWidgetArea);
    makeDock(tr("Frame"),     m_frameInfo,      Qt::RightDockWidgetArea);
    makeDock(tr("Timeline"),  m_timelineWidget, Qt::BottomDockWidgetArea);
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
    m_mask = std::move(newMask);
    m_viewport->setMask(m_mask);
    m_sliceControl->setGridSize(m_mask->gridSize());
    m_frameInfo->setMask(m_mask);
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
    m_mask = std::make_shared<ShapeMask>(gridSize, shape);
    m_timeline->clearAll();
    m_viewport->setMask(m_mask);
    m_sliceControl->setGridSize(gridSize);
    m_frameInfo->setMask(m_mask);

    if (gridSize > 24) {
        statusBar()->showMessage(
            tr("Large grid (%1").arg(gridSize)
                + QStringLiteral("³ = ")
                + tr("%1 LEDs) — performance may degrade. SPEC §3.2.")
                    .arg(gridSize * gridSize * gridSize),
            5000);
    }
}

bool MainWindow::confirmDiscardIfDirty(const QString& reason) {
    if (!m_timeline || !m_timeline->hasContent()) return true;
    const auto ans = QMessageBox::question(
        this, tr("Discard animation?"), reason,
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return ans == QMessageBox::Yes;
}
