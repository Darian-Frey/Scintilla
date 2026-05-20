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
    // Slice-hidden ghosts render smaller and dimmer so the active editing
    // layer stands out clearly. Geometry size scales linearly with iScale
    // in ghost.vert; opacity scales with vScale in ghost.frag.
    //
    // Slice-hidden cells that ARE lit (painted on another layer) render at
    // an intermediate scale and use a higher base opacity (kGlowOpacity in
    // ghost.frag), so the user can see which voxels are painted across the
    // whole cube while editing one slice.
    constexpr float kHiddenGhostDim   = 0.4f;
    constexpr float kHiddenLitGlow    = 0.55f;

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
        auto       lit    = frame.get(p.x, p.y, p.z);

        // Ghost LED: always visible (SPEC §3.7). Three states:
        //   - active slice          → full size, default grey
        //   - hidden + unlit        → small, dim grey ("vague outline")
        //   - hidden + lit          → medium, COLOURED glow (paint visible
        //                             across layers while slicing)
        // ghost.frag interprets a non-zero colour as the glow state.
        if (!hidden) {
            m_ghost[i].scale = 1.0f;
            m_ghost[i].r = m_ghost[i].g = m_ghost[i].b = 0.0f;
        } else if (lit) {
            m_ghost[i].scale = kHiddenLitGlow;
            m_ghost[i].r = static_cast<float>((*lit)[0]) / 255.0f;
            m_ghost[i].g = static_cast<float>((*lit)[1]) / 255.0f;
            m_ghost[i].b = static_cast<float>((*lit)[2]) / 255.0f;
        } else {
            m_ghost[i].scale = kHiddenGhostDim;
            m_ghost[i].r = m_ghost[i].g = m_ghost[i].b = 0.0f;
        }

        // On LED: lit colour from frame, scale = 1 if lit & not slice-hidden.
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
