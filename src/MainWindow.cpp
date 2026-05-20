#include "MainWindow.h"

#include "renderer/CubeViewport.h"

#include <QStatusBar>

namespace {
    constexpr int  kDefaultGrid = 8;
    constexpr int  kWindowW     = 1280;
    constexpr int  kWindowH     = 720;
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_timeline(std::make_unique<AnimationTimeline>(this))
    , m_mask(std::make_shared<ShapeMask>(kDefaultGrid, ShapeType::Cube)) {

    setWindowTitle(tr("Scintilla"));
    resize(kWindowW, kWindowH);

    m_viewport = new CubeViewport(this);
    m_viewport->setMask(m_mask);
    m_viewport->setTimeline(m_timeline.get());
    m_viewport->setPaintColor(255, 64, 32);    // warm red — Phase 1 default

    setCentralWidget(m_viewport);
    statusBar()->showMessage(
        tr("Phase 1 stub — drag to orbit, wheel to zoom, click to paint, shift-drag to pan."));

    connect(m_viewport, &CubeViewport::voxelEdited,
            this, &MainWindow::onVoxelEdited);
    connect(m_viewport, &CubeViewport::colorPicked,
            this, &MainWindow::onColorPicked);
}

MainWindow::~MainWindow() = default;

void MainWindow::onVoxelEdited(int /*x*/, int /*y*/, int /*z*/,
                                uint8_t /*r*/, uint8_t /*g*/, uint8_t /*b*/,
                                bool /*erased*/) {
    // Phase 1: no-op — the viewport already mutated the current frame and
    // refreshed itself via the timeline's content-changed signal. Phase 2
    // will route this to the timeline thumbnail strip and the info panel.
}

void MainWindow::onColorPicked(uint8_t r, uint8_t g, uint8_t b) {
    m_viewport->setPaintColor(r, g, b);
}

void MainWindow::rebuildMask(int gridSize, ShapeType shape) {
    // DEC-006: ask before clearing if any frames have content.
    if (m_timeline && m_timeline->hasContent()) {
        // Phase 2 will surface a QMessageBox::question() here. For now we
        // refuse the rebuild silently — the only caller path is internal and
        // there is no UI to trigger it in Phase 1.
        return;
    }
    m_mask = std::make_shared<ShapeMask>(gridSize, shape);
    m_timeline->clearAll();
    m_viewport->setMask(m_mask);
}
