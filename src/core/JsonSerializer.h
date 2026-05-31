#pragma once

#include <QString>
#include <map>
#include <memory>

#include "CameraKeyframe.h"
#include "ShapeMask.h"

class AnimationTimeline;

// ── JsonSerializer ────────────────────────────────────────────────────────────
//
// Reads and writes the Scintilla wire format (DEC-003, SPEC §3.8). Schema:
//
//   { "version": "1.1",
//     "shape":   "cube" | "sphere" | "cylinder" | "pyramid"
//              | "torus" | "ring" | "cross" | "custom",
//     "gridSize": int,
//     "fps":      int,
//     "customPositions": [[x, y, z], …]    // only when shape == "custom"
//     "cameraKeyframes": [                  // optional
//        { "frame": int, "theta": f, "phi": f, "radius": f,
//          "target": [x, y, z] }, … ],
//     "frames":  [ { "duration": float,
//                    "voxels":   { "x,y,z": [r, g, b] } } ] }
//
// - Off LEDs are absent from the voxels dict (AV-004).
// - Files with gridSize > 32 are clamped to 32 with a warning (AV-006).
// - Unknown shape names produce an error rather than silently defaulting.
// - v1.0 files (no customPositions / cameraKeyframes) load fine; the new
//   fields default to empty.

struct LoadResult {
    bool      ok              = false;
    QString   errorMessage;
    int       gridSize        = 0;
    ShapeType shape           = ShapeType::Cube;
    int       fps             = 12;
    bool      gridSizeClamped = false;            // true if input had gridSize > 32
    std::map<int, CameraKeyframe> cameraKeyframes;// empty unless the file had them
};

class JsonSerializer {
public:
    // Save current scene to path. Returns true on success.
    // On failure, if errorOut is non-null it receives a human-readable message.
    [[nodiscard]] static bool save(const QString& path,
                                   const ShapeMask& mask,
                                   const AnimationTimeline& timeline,
                                   const std::map<int, CameraKeyframe>& cameraKeyframes,
                                   QString* errorOut = nullptr);

    // Load from path. On success, replaces *maskOut and *timelineOut. The
    // mask is reconstructed from the file's (shape, gridSize) pair, or from
    // the explicit customPositions array when shape == "custom".
    [[nodiscard]] static LoadResult load(const QString& path,
                                         std::shared_ptr<ShapeMask>* maskOut,
                                         AnimationTimeline* timelineOut);
};
