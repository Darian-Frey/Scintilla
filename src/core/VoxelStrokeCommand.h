#pragma once

#include <QUndoCommand>

#include "VoxelStroke.h"

class AnimationTimeline;

// ── VoxelStrokeCommand ──────────────────────────────────────────────────────
//
// QUndoCommand wrapping a VoxelStroke. The stroke is assumed to have already
// been applied to the timeline by the producer (the CubeViewport mutates the
// frame in real time during the drag so the visual feedback is live). The
// first redo() from QUndoStack::push() therefore skips reapplication; later
// redos re-apply normally after a user undo.

class VoxelStrokeCommand : public QUndoCommand {
public:
    VoxelStrokeCommand(AnimationTimeline* timeline, VoxelStroke stroke);

    void undo() override;
    void redo() override;

private:
    AnimationTimeline* m_timeline;
    VoxelStroke        m_stroke;
    bool               m_skipFirstRedo = true;
};
