#pragma once

#include "VoxelFrame.h"
#include <vector>

// ── VoxelStroke ──────────────────────────────────────────────────────────────
//
// One undo-unit's worth of voxel edits — typically the result of a single
// mouse-drag stroke with the Paint or Erase tool. Each change captures both
// the pre-edit and post-edit state of one voxel so a single command can
// undo or redo the whole batch.
//
// Identity changes (Paint over the same colour, Erase on an off voxel) are
// expected to be filtered by the producer rather than stored.

struct VoxelChange {
    int  x, y, z;
    bool hadValue      = false;   // was the voxel lit before the change
    RGB  oldValue      = {0,0,0}; // valid only when hadValue
    bool willHaveValue = false;   // is the voxel lit after the change
    RGB  newValue      = {0,0,0}; // valid only when willHaveValue
};

struct VoxelStroke {
    int frameIndex = -1;          // the AnimationTimeline frame the stroke targets
    std::vector<VoxelChange> changes;
};
