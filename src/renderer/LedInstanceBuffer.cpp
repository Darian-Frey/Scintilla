#include "LedInstanceBuffer.h"

#include "core/ShapeMask.h"
#include "core/VoxelFrame.h"

void LedInstanceBuffer::rebuildFor(const ShapeMask& mask) {
    const auto& positions = mask.positions();
    const float offset    = mask.worldOffset();

    m_on.assign(positions.size(),    LedInstance{});
    m_ghost.assign(positions.size(), LedInstance{});

    for (size_t i = 0; i < positions.size(); ++i) {
        const auto& p = positions[i];
        const float wx = static_cast<float>(p.x) + offset;
        const float wy = static_cast<float>(p.y) + offset;
        const float wz = static_cast<float>(p.z) + offset;

        // On-LEDs start hidden (no lit voxels yet).
        m_on[i] = LedInstance{wx, wy, wz, 0.0f, 0.0f, 0.0f, /*scale*/ 0.0f, 0.0f};

        // Ghost-LEDs start visible; ghost colour is set in the shader.
        m_ghost[i] = LedInstance{wx, wy, wz, 0.0f, 0.0f, 0.0f, /*scale*/ 1.0f, 0.0f};
    }
    m_dirty = true;
}

namespace {
    bool sliceHidesLed(int x, int y, int z, int sx, int sy, int sz) {
        if (sx >= 0 && x != sx) return true;
        if (sy >= 0 && y != sy) return true;
        if (sz >= 0 && z != sz) return true;
        return false;
    }
}

void LedInstanceBuffer::updateFrame(const ShapeMask& mask,
                                    const VoxelFrame& frame,
                                    int sliceX, int sliceY, int sliceZ) {
    const auto& positions = mask.positions();
    if (m_on.size() != positions.size()) {
        rebuildFor(mask);
    }

    for (size_t i = 0; i < positions.size(); ++i) {
        const auto& p     = positions[i];
        const bool hidden = sliceHidesLed(p.x, p.y, p.z, sliceX, sliceY, sliceZ);

        // Ghost LED: visible iff the mask cell is not slice-filtered.
        m_ghost[i].scale = hidden ? 0.0f : 1.0f;

        // On LED: lit colour from frame, scale = 1 if lit & not hidden.
        auto lit = frame.get(p.x, p.y, p.z);
        if (!lit || hidden) {
            m_on[i].scale = 0.0f;
            m_on[i].r = m_on[i].g = m_on[i].b = 0.0f;
        } else {
            m_on[i].scale = 1.0f;
            m_on[i].r = static_cast<float>((*lit)[0]) / 255.0f;
            m_on[i].g = static_cast<float>((*lit)[1]) / 255.0f;
            m_on[i].b = static_cast<float>((*lit)[2]) / 255.0f;
        }
    }
    m_dirty = true;
}

void LedInstanceBuffer::upload(QOpenGLFunctions_4_3_Core& gl) {
    if (!m_onVbo)    gl.glGenBuffers(1, &m_onVbo);
    if (!m_ghostVbo) gl.glGenBuffers(1, &m_ghostVbo);

    const GLsizeiptr onBytes    = static_cast<GLsizeiptr>(m_on.size()    * sizeof(LedInstance));
    const GLsizeiptr ghostBytes = static_cast<GLsizeiptr>(m_ghost.size() * sizeof(LedInstance));

    gl.glBindBuffer(GL_ARRAY_BUFFER, m_onVbo);
    gl.glBufferData(GL_ARRAY_BUFFER, onBytes, m_on.data(), GL_DYNAMIC_DRAW);

    gl.glBindBuffer(GL_ARRAY_BUFFER, m_ghostVbo);
    gl.glBufferData(GL_ARRAY_BUFFER, ghostBytes, m_ghost.data(), GL_DYNAMIC_DRAW);

    gl.glBindBuffer(GL_ARRAY_BUFFER, 0);
    m_dirty = false;
}

void LedInstanceBuffer::releaseGL(QOpenGLFunctions_4_3_Core& gl) {
    if (m_onVbo)    { gl.glDeleteBuffers(1, &m_onVbo);    m_onVbo = 0; }
    if (m_ghostVbo) { gl.glDeleteBuffers(1, &m_ghostVbo); m_ghostVbo = 0; }
}

bool LedInstanceBuffer::instancePosition(int index, float& outX, float& outY, float& outZ) const {
    if (index < 0 || static_cast<size_t>(index) >= m_on.size()) return false;
    outX = m_on[static_cast<size_t>(index)].px;
    outY = m_on[static_cast<size_t>(index)].py;
    outZ = m_on[static_cast<size_t>(index)].pz;
    return true;
}
