#pragma once

#include <QVector3D>

// ── CameraKeyframe ───────────────────────────────────────────────────────────
//
// Snapshot of the orbit camera state used by the rotation-keyframe feature.
// Linearly interpolated between bracketing keyframes during playback and
// export to produce fly-through animations without scripted motion.
//
// Lives in core/ so JsonSerializer can depend on it without dragging
// MainWindow into the dependency graph.

struct CameraKeyframe {
    float     theta  = 0.0f;
    float     phi    = 0.0f;
    float     radius = 0.0f;
    QVector3D target;
};
