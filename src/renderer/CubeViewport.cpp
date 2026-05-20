#include "CubeViewport.h"

#include <QDebug>
#include <QMouseEvent>
#include <QWheelEvent>
#include <cmath>
#include <cstddef>
#include <limits>

namespace {
    constexpr int   kOnLonSegs       = 9;
    constexpr int   kOnLatSegs       = 7;
    constexpr int   kGhostLonSegs    = 6;
    constexpr int   kGhostLatSegs    = 5;
    constexpr int   kRotTimerMs      = 16;       // ~60 fps
    constexpr float kAutoRotateSpeed = 0.005f;
    constexpr float kPickRadius      = 0.5f;     // slightly > on-LED visual radius
    constexpr int   kClickSlopPx     = 3;        // mouse movement tolerated for click vs drag
    constexpr float kPi              = 3.14159265358979323846f;
}

// ── Construction ─────────────────────────────────────────────────────────────

CubeViewport::CubeViewport(QWidget* parent)
    : QOpenGLWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    connect(&m_rotTimer, &QTimer::timeout, this, [this]() {
        m_camera.setTheta(m_camera.theta() + kAutoRotateSpeed);
        update();
    });
}

CubeViewport::~CubeViewport() {
    if (!context()) return;
    makeCurrent();
    m_instances.releaseGL(*this);
    auto del = [this](SphereGeo& g) {
        if (g.vao) glDeleteVertexArrays(1, &g.vao);
        if (g.vbo) glDeleteBuffers(1,      &g.vbo);
        if (g.ebo) glDeleteBuffers(1,      &g.ebo);
    };
    del(m_onSphere);
    del(m_offSphere);
    if (m_boxVao) glDeleteVertexArrays(1, &m_boxVao);
    if (m_boxVbo) glDeleteBuffers(1,      &m_boxVbo);
    if (m_boxEbo) glDeleteBuffers(1,      &m_boxEbo);
    doneCurrent();
}

// ── Scene configuration ──────────────────────────────────────────────────────

void CubeViewport::setMask(std::shared_ptr<ShapeMask> mask) {
    m_mask = std::move(mask);
    if (m_mask) m_camera.fitToGrid(m_mask->gridSize());
    if (m_initialized) {
        makeCurrent();
        uploadScene();
        doneCurrent();
    }
    update();
}

void CubeViewport::setTimeline(AnimationTimeline* tl) {
    if (m_timeline) {
        disconnect(m_timeline, nullptr, this, nullptr);
    }
    m_timeline = tl;
    if (m_timeline) {
        connect(m_timeline, &AnimationTimeline::currentFrameChanged,
                this, [this](int) { refreshFrame(); });
        connect(m_timeline, &AnimationTimeline::frameContentChanged,
                this, [this](int) { refreshFrame(); });
    }
    refreshFrame();
}

void CubeViewport::setSlice(int x, int y, int z) {
    m_sliceX = x; m_sliceY = y; m_sliceZ = z;
    refreshFrame();
}

void CubeViewport::setShowGhost(bool show)  { m_showGhost  = show; update(); }
void CubeViewport::setShowBounds(bool show) { m_showBounds = show; update(); }

void CubeViewport::setAutoRotate(bool on) {
    if (on) m_rotTimer.start(kRotTimerMs);
    else    m_rotTimer.stop();
}

void CubeViewport::refreshFrame() {
    if (!m_initialized || !m_mask) return;
    makeCurrent();
    static const VoxelFrame kEmpty;
    const VoxelFrame& f = m_timeline ? m_timeline->currentFrame() : kEmpty;
    m_instances.updateFrame(*m_mask, f, m_sliceX, m_sliceY, m_sliceZ);
    m_instances.upload(*this);
    doneCurrent();
    update();
}

void CubeViewport::resetCamera() {
    m_camera.resetAngles();
    if (m_mask) m_camera.fitToGrid(m_mask->gridSize());
    update();
}

// ── GL setup ─────────────────────────────────────────────────────────────────

void CubeViewport::initializeGL() {
    initializeOpenGLFunctions();

    const char* verStr = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    qDebug() << "OpenGL version:" << verStr;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.024f, 0.024f, 0.059f, 1.0f);   // SPEC §3.1 — #06060f

    buildShaders();
    buildSphereGeometry(m_onSphere,  kOnLonSegs,    kOnLatSegs);
    buildSphereGeometry(m_offSphere, kGhostLonSegs, kGhostLatSegs);
    buildBoxGeometry();

    m_initialized = true;
    if (m_mask) uploadScene();
}

void CubeViewport::resizeGL(int w, int h) {
    m_aspectRatio = (h > 0) ? static_cast<float>(w) / static_cast<float>(h) : 1.0f;
    glViewport(0, 0, w, h);
}

void CubeViewport::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!m_mask || m_instances.count() == 0) return;

    const QMatrix4x4 vp     = m_camera.projMatrix(m_aspectRatio) * m_camera.viewMatrix();
    const QVector3D  camPos = m_camera.position();

    // ─── On-LEDs (opaque) ────────────────────────────────────────────────────
    glDepthMask(GL_TRUE);
    m_ledProg->bind();
    m_ledProg->setUniformValue("uVP",       vp);
    m_ledProg->setUniformValue("uLightDir", QVector3D(2.0f, 3.0f, 1.5f).normalized());
    m_ledProg->setUniformValue("uCamPos",   camPos);
    glBindVertexArray(m_onSphere.vao);
    glDrawElementsInstanced(GL_TRIANGLES, m_onSphere.indexCount, GL_UNSIGNED_INT, nullptr,
                            m_instances.count());
    glBindVertexArray(0);
    m_ledProg->release();

    // ─── Ghost-LEDs (transparent) ────────────────────────────────────────────
    if (m_showGhost) {
        glDepthMask(GL_FALSE);
        m_ghostProg->bind();
        m_ghostProg->setUniformValue("uVP", vp);
        glBindVertexArray(m_offSphere.vao);
        glDrawElementsInstanced(GL_TRIANGLES, m_offSphere.indexCount, GL_UNSIGNED_INT, nullptr,
                                m_instances.count());
        glBindVertexArray(0);
        m_ghostProg->release();
        glDepthMask(GL_TRUE);
    }

    // ─── Bounding box wireframe ──────────────────────────────────────────────
    if (m_showBounds && m_boxVao != 0) {
        glDepthMask(GL_FALSE);
        m_gridProg->bind();
        m_gridProg->setUniformValue("uVP", vp);
        glBindVertexArray(m_boxVao);
        glDrawElements(GL_LINES, m_boxLineCount, GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
        m_gridProg->release();
        glDepthMask(GL_TRUE);
    }
}

// ── Mouse / wheel ────────────────────────────────────────────────────────────

void CubeViewport::mousePressEvent(QMouseEvent* e) {
    m_pressPos  = e->pos();
    m_lastPos   = e->pos();
    m_dragging  = false;
    m_shiftDrag = (e->modifiers() & Qt::ShiftModifier) || (e->buttons() & Qt::MiddleButton);
}

void CubeViewport::mouseMoveEvent(QMouseEvent* e) {
    if (!(e->buttons() & (Qt::LeftButton | Qt::MiddleButton | Qt::RightButton))) return;
    const int dx = e->pos().x() - m_lastPos.x();
    const int dy = e->pos().y() - m_lastPos.y();
    m_lastPos    = e->pos();
    if ((e->pos() - m_pressPos).manhattanLength() > kClickSlopPx) m_dragging = true;
    if (m_shiftDrag) m_camera.pan(dx, dy);
    else             m_camera.orbit(dx, dy);
    update();
}

void CubeViewport::mouseReleaseEvent(QMouseEvent* e) {
    if (!m_dragging && e->button() == Qt::LeftButton) {
        const int idx = pickInstance(e->pos());
        if (idx >= 0)        applyTool(idx);
        else                 emit pickRayMiss();
    }
    m_dragging  = false;
    m_shiftDrag = false;
}

void CubeViewport::wheelEvent(QWheelEvent* e) {
    const int ticks = e->angleDelta().y() / 120;
    if (ticks != 0) {
        m_camera.zoom(ticks);
        update();
    }
}

// ── Geometry builders ────────────────────────────────────────────────────────

void CubeViewport::buildSphereGeometry(SphereGeo& out, int lonSegs, int latSegs) {
    // Unit sphere — shader applies the per-mesh radius constant (DEC-002).
    std::vector<float> verts;
    std::vector<unsigned int> idx;
    verts.reserve(static_cast<size_t>(latSegs + 1) * (lonSegs + 1) * 6);

    for (int lat = 0; lat <= latSegs; ++lat) {
        const float theta = static_cast<float>(lat) * kPi / static_cast<float>(latSegs);
        const float sinT = std::sin(theta);
        const float cosT = std::cos(theta);
        for (int lon = 0; lon <= lonSegs; ++lon) {
            const float phi  = static_cast<float>(lon) * 2.0f * kPi / static_cast<float>(lonSegs);
            const float sinP = std::sin(phi);
            const float cosP = std::cos(phi);
            const float x = sinT * cosP;
            const float y = cosT;
            const float z = sinT * sinP;
            verts.push_back(x); verts.push_back(y); verts.push_back(z);   // position
            verts.push_back(x); verts.push_back(y); verts.push_back(z);   // normal == position for unit sphere
        }
    }

    for (int lat = 0; lat < latSegs; ++lat) {
        for (int lon = 0; lon < lonSegs; ++lon) {
            const unsigned int first  = static_cast<unsigned int>(lat * (lonSegs + 1) + lon);
            const unsigned int second = first + static_cast<unsigned int>(lonSegs + 1);
            idx.push_back(first);
            idx.push_back(second);
            idx.push_back(first + 1);
            idx.push_back(second);
            idx.push_back(second + 1);
            idx.push_back(first + 1);
        }
    }

    if (!out.vao) glGenVertexArrays(1, &out.vao);
    if (!out.vbo) glGenBuffers(1,      &out.vbo);
    if (!out.ebo) glGenBuffers(1,      &out.ebo);

    glBindVertexArray(out.vao);
    glBindBuffer(GL_ARRAY_BUFFER, out.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, out.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(idx.size() * sizeof(unsigned int)),
                 idx.data(), GL_STATIC_DRAW);
    glBindVertexArray(0);

    out.indexCount = static_cast<int>(idx.size());
}

void CubeViewport::buildBoxGeometry() {
    // 12 edges of a unit cube, vertices in [-0.5, +0.5]; scale to grid in setupVAOs.
    // We build the box at the current grid extent so vertex positions are already in world space.
    const float n      = m_mask ? static_cast<float>(m_mask->gridSize()) : 8.0f;
    const float half   = n * 0.5f;

    const float v[] = {
        -half, -half, -half,   +half, -half, -half,
        +half, +half, -half,   -half, +half, -half,
        -half, -half, +half,   +half, -half, +half,
        +half, +half, +half,   -half, +half, +half,
    };
    const unsigned int e[] = {
        0,1, 1,2, 2,3, 3,0,        // back face
        4,5, 5,6, 6,7, 7,4,        // front face
        0,4, 1,5, 2,6, 3,7,        // verticals
    };

    if (!m_boxVao) glGenVertexArrays(1, &m_boxVao);
    if (!m_boxVbo) glGenBuffers(1,      &m_boxVbo);
    if (!m_boxEbo) glGenBuffers(1,      &m_boxEbo);

    glBindVertexArray(m_boxVao);
    glBindBuffer(GL_ARRAY_BUFFER, m_boxVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_boxEbo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(e), e, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glBindVertexArray(0);

    m_boxLineCount = static_cast<int>(sizeof(e) / sizeof(unsigned int));
}

void CubeViewport::buildShaders() {
    auto compile = [&](const QString& vertPath, const QString& fragPath)
        -> std::unique_ptr<QOpenGLShaderProgram> {
        auto p = std::make_unique<QOpenGLShaderProgram>();
        if (!p->addShaderFromSourceFile(QOpenGLShader::Vertex, vertPath))
            qWarning() << "vert" << vertPath << p->log();
        if (!p->addShaderFromSourceFile(QOpenGLShader::Fragment, fragPath))
            qWarning() << "frag" << fragPath << p->log();
        if (!p->link()) qWarning() << "link" << vertPath << p->log();
        return p;
    };

    m_ledProg   = compile(":/shaders/led.vert",   ":/shaders/led.frag");
    m_ghostProg = compile(":/shaders/ghost.vert", ":/shaders/ghost.frag");
    m_gridProg  = compile(":/shaders/grid.vert",  ":/shaders/grid.frag");
}

void CubeViewport::setupVAOs() {
    auto bindSphereVao = [&](SphereGeo& geo, GLuint instanceVbo) {
        glBindVertexArray(geo.vao);

        // Per-vertex attributes — locations 0 (position) and 1 (normal)
        glBindBuffer(GL_ARRAY_BUFFER, geo.vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                              6 * sizeof(float),
                              reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                              6 * sizeof(float),
                              reinterpret_cast<void*>(3 * sizeof(float)));

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, geo.ebo);

        // Per-instance attributes — locations 2 (worldPos), 3 (color), 4 (scale)
        glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE,
                              sizeof(LedInstance),
                              reinterpret_cast<void*>(offsetof(LedInstance, px)));
        glVertexAttribDivisor(2, 1);
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE,
                              sizeof(LedInstance),
                              reinterpret_cast<void*>(offsetof(LedInstance, r)));
        glVertexAttribDivisor(3, 1);
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE,
                              sizeof(LedInstance),
                              reinterpret_cast<void*>(offsetof(LedInstance, scale)));
        glVertexAttribDivisor(4, 1);

        glBindVertexArray(0);
    };

    bindSphereVao(m_onSphere,  m_instances.onVbo());
    bindSphereVao(m_offSphere, m_instances.ghostVbo());
}

void CubeViewport::uploadScene() {
    if (!m_mask) return;
    m_instances.rebuildFor(*m_mask);
    if (m_timeline) {
        m_instances.updateFrame(*m_mask, m_timeline->currentFrame(),
                                m_sliceX, m_sliceY, m_sliceZ);
    }
    m_instances.upload(*this);

    // Box geometry depends on grid size, so rebuild whenever the mask changes.
    buildBoxGeometry();

    setupVAOs();
}

// ── Picking ──────────────────────────────────────────────────────────────────
//
// CPU ray-sphere intersection against instance centres. Handles both on-LEDs
// and ghost LEDs — the user must be able to paint a currently-unlit LED.
// Slice-filtered LEDs (ghost scale == 0) are excluded.

int CubeViewport::pickInstance(QPoint screenPos) {
    if (!m_mask || m_instances.count() == 0) return -1;

    const float ndcX = (2.0f * static_cast<float>(screenPos.x())) / static_cast<float>(width())  - 1.0f;
    const float ndcY = 1.0f - (2.0f * static_cast<float>(screenPos.y())) / static_cast<float>(height());

    const QMatrix4x4 invVP =
        (m_camera.projMatrix(m_aspectRatio) * m_camera.viewMatrix()).inverted();

    const QVector4D nearH = invVP * QVector4D(ndcX, ndcY, -1.0f, 1.0f);
    const QVector4D farH  = invVP * QVector4D(ndcX, ndcY, +1.0f, 1.0f);
    if (nearH.w() == 0.0f || farH.w() == 0.0f) return -1;

    const QVector3D nearP(nearH.x() / nearH.w(), nearH.y() / nearH.w(), nearH.z() / nearH.w());
    const QVector3D farP (farH.x()  / farH.w(),  farH.y()  / farH.w(),  farH.z()  / farH.w());
    const QVector3D rayDir = (farP - nearP).normalized();

    int   best  = -1;
    float bestT = std::numeric_limits<float>::infinity();

    const auto& positions = m_mask->positions();

    for (int i = 0; i < m_instances.count(); ++i) {
        // DEC-004: slice filter prevents painting on hidden layers. Now that
        // ghost LEDs render through the slice (SPEC §3.7), we consult the
        // slice values directly rather than using ghost.scale as a proxy.
        const auto& p = positions[static_cast<size_t>(i)];
        if (m_sliceX >= 0 && p.x != m_sliceX) continue;
        if (m_sliceY >= 0 && p.y != m_sliceY) continue;
        if (m_sliceZ >= 0 && p.z != m_sliceZ) continue;

        float ix, iy, iz;
        if (!m_instances.instancePosition(i, ix, iy, iz)) continue;

        const QVector3D oc(nearP.x() - ix, nearP.y() - iy, nearP.z() - iz);
        const float b    = QVector3D::dotProduct(oc, rayDir);
        const float c    = QVector3D::dotProduct(oc, oc) - kPickRadius * kPickRadius;
        const float disc = b * b - c;
        if (disc < 0.0f) continue;

        const float t = -b - std::sqrt(disc);
        if (t < 0.0f) continue;
        if (t < bestT) { bestT = t; best = i; }
    }
    return best;
}

void CubeViewport::applyTool(int instanceIdx) {
    if (!m_mask || instanceIdx < 0
        || static_cast<size_t>(instanceIdx) >= m_mask->positions().size()) return;

    const VoxelKey k = m_mask->positions()[static_cast<size_t>(instanceIdx)];

    switch (m_tool) {
        case Tool::Paint:
            if (m_timeline) {
                m_timeline->currentFrame().set(k.x, k.y, k.z,
                                               m_paintColor[0], m_paintColor[1], m_paintColor[2]);
                m_timeline->notifyCurrentFrameEdited();
            }
            emit voxelEdited(k.x, k.y, k.z,
                             m_paintColor[0], m_paintColor[1], m_paintColor[2], false);
            break;

        case Tool::Erase:
            if (m_timeline) {
                m_timeline->currentFrame().erase(k.x, k.y, k.z);
                m_timeline->notifyCurrentFrameEdited();
            }
            emit voxelEdited(k.x, k.y, k.z, 0, 0, 0, true);
            break;

        case Tool::Pick:
            if (m_timeline) {
                auto color = m_timeline->currentFrame().get(k.x, k.y, k.z);
                if (color) {
                    m_paintColor = *color;
                    emit colorPicked((*color)[0], (*color)[1], (*color)[2]);
                }
            }
            break;

        case Tool::Fill:
            if (m_timeline) {
                auto& frame = m_timeline->currentFrame();
                for (const auto& p : m_mask->positions()) {
                    if (m_sliceX >= 0 && p.x != m_sliceX) continue;
                    if (m_sliceY >= 0 && p.y != m_sliceY) continue;
                    if (m_sliceZ >= 0 && p.z != m_sliceZ) continue;
                    frame.set(p.x, p.y, p.z,
                              m_paintColor[0], m_paintColor[1], m_paintColor[2]);
                }
                m_timeline->notifyCurrentFrameEdited();
            }
            break;
    }
}
