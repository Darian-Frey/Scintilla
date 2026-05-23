#include "VoxelStrokeCommand.h"

#include "AnimationTimeline.h"

VoxelStrokeCommand::VoxelStrokeCommand(AnimationTimeline* timeline, VoxelStroke stroke)
    : m_timeline(timeline), m_stroke(std::move(stroke)) {
    const int n = static_cast<int>(m_stroke.changes.size());
    setText(QObject::tr("Edit %1 voxel%2").arg(n).arg(n == 1 ? "" : "s"));
}

void VoxelStrokeCommand::redo() {
    if (m_skipFirstRedo) { m_skipFirstRedo = false; return; }
    if (!m_timeline) return;
    if (m_stroke.frameIndex < 0 || m_stroke.frameIndex >= m_timeline->frameCount()) return;

    if (m_timeline->currentIndex() != m_stroke.frameIndex) {
        m_timeline->selectFrame(m_stroke.frameIndex);
    }
    auto& frame = m_timeline->currentFrame();
    for (const auto& c : m_stroke.changes) {
        if (c.willHaveValue) frame.set(c.x, c.y, c.z, c.newValue[0], c.newValue[1], c.newValue[2]);
        else                 frame.erase(c.x, c.y, c.z);
    }
    m_timeline->notifyCurrentFrameEdited();
}

void VoxelStrokeCommand::undo() {
    if (!m_timeline) return;
    if (m_stroke.frameIndex < 0 || m_stroke.frameIndex >= m_timeline->frameCount()) return;

    if (m_timeline->currentIndex() != m_stroke.frameIndex) {
        m_timeline->selectFrame(m_stroke.frameIndex);
    }
    auto& frame = m_timeline->currentFrame();
    for (const auto& c : m_stroke.changes) {
        if (c.hadValue) frame.set(c.x, c.y, c.z, c.oldValue[0], c.oldValue[1], c.oldValue[2]);
        else            frame.erase(c.x, c.y, c.z);
    }
    m_timeline->notifyCurrentFrameEdited();
}
