#pragma once

#include <QOpenGLFunctions_4_3_Core>
#include <cstdint>
#include <vector>

class ShapeMask;
class VoxelFrame;

// ── LedInstance ───────────────────────────────────────────────────────────────
//
// Per-instance vertex attributes for the LED renderer. Layout matches led.vert:
//
//   offset  0   vec3   worldPos    — attribute location 2
//   offset 12   vec3   color (RGB) — attribute location 3
//   offset 24   float  scale       — attribute location 4
//   offset 28   float  _pad        (alignment to 32-byte stride)
//
// Hidden LEDs use scale = 0 (DEC-001 / AV-002 — never remove from the buffer).

struct LedInstance {
    float px = 0.0f, py = 0.0f, pz = 0.0f;
    float r  = 0.0f, g  = 0.0f, b  = 0.0f;
    float scale = 0.0f;
    float _pad  = 0.0f;
};
static_assert(sizeof(LedInstance) == 32,
              "LedInstance must be 32 bytes — renderer stride is hardcoded");

// ── LedInstanceBuffer ─────────────────────────────────────────────────────────
//
// Owns two CPU-side instance arrays (on-LEDs and ghost-LEDs) plus their GPU VBOs.
// Membership in the arrays is fixed by the ShapeMask — only per-LED color and
// scale change per frame. Instance index for a position is the index returned
// by ShapeMask::instanceIndex().

class LedInstanceBuffer {
public:
    LedInstanceBuffer() = default;
    ~LedInstanceBuffer() = default;

    // Build instance arrays for the given mask. Both arrays are sized to
    // mask.count(); world positions are filled; color is zero; on-LEDs start
    // hidden (scale=0); ghost-LEDs start visible (scale=1).
    // GL upload happens on the next upload() call.
    void rebuildFor(const ShapeMask& mask);

    // Update on-LED color and scale from a frame. The slice filter hides any
    // LED whose x/y/z does not match an active slice index (−1 = show all).
    // Ghost-LED scale is also driven by the slice filter (DEC-004 / AV-005).
    void updateFrame(const ShapeMask& mask,
                     const VoxelFrame& frame,
                     int sliceX, int sliceY, int sliceZ);

    // Upload current CPU arrays to GPU. Requires a valid GL context.
    void upload(QOpenGLFunctions_4_3_Core& gl);

    // Release GPU resources. Call from CubeViewport::~ with the GL context current.
    void releaseGL(QOpenGLFunctions_4_3_Core& gl);

    // ── Accessors ─────────────────────────────────────────────────────────────
    [[nodiscard]] GLuint onVbo()    const { return m_onVbo; }
    [[nodiscard]] GLuint ghostVbo() const { return m_ghostVbo; }
    [[nodiscard]] int    count()    const { return static_cast<int>(m_on.size()); }

    [[nodiscard]] const std::vector<LedInstance>& onInstances()    const { return m_on; }
    [[nodiscard]] const std::vector<LedInstance>& ghostInstances() const { return m_ghost; }

    // World-space position of instance index (false if out of range).
    [[nodiscard]] bool instancePosition(int index,
                                        float& outX, float& outY, float& outZ) const;

private:
    std::vector<LedInstance> m_on;
    std::vector<LedInstance> m_ghost;

    GLuint m_onVbo    = 0;
    GLuint m_ghostVbo = 0;
    bool   m_dirty    = true;
};
