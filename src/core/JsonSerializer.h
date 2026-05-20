#pragma once

#include <QString>
#include <memory>
#include "ShapeMask.h"

class AnimationTimeline;

// ── JsonSerializer ────────────────────────────────────────────────────────────
//
// Reads and writes the Scintilla v1.0 wire format (DEC-003, SPEC §3.8):
//
//   { "version": "1.0",
//     "shape":    "cube" | "sphere" | "cylinder" | "pyramid",
//     "gridSize": int,
//     "fps":      int,
//     "frames":  [ { "duration": float,
//                    "voxels":   { "x,y,z": [r, g, b] } } ] }
//
// - Off LEDs are absent from the voxels dict (AV-004).
// - Files with gridSize > 32 are clamped to 32 with a warning (AV-006).
// - Unknown shape names produce an error rather than silently defaulting.

struct LoadResult {
    bool      ok            = false;
    QString   errorMessage;
    int       gridSize      = 0;
    ShapeType shape         = ShapeType::Cube;
    int       fps           = 12;
    bool      gridSizeClamped = false;     // true if input had gridSize > 32
};

class JsonSerializer {
public:
    // Save current scene to path. Returns true on success.
    // On failure, if errorOut is non-null it receives a human-readable message.
    [[nodiscard]] static bool save(const QString& path,
                                   const ShapeMask& mask,
                                   const AnimationTimeline& timeline,
                                   QString* errorOut = nullptr);

    // Load from path. On success, replaces *maskOut and *timelineOut. The
    // mask is reconstructed from the file's (shape, gridSize) pair.
    [[nodiscard]] static LoadResult load(const QString& path,
                                         std::shared_ptr<ShapeMask>* maskOut,
                                         AnimationTimeline* timelineOut);
};
