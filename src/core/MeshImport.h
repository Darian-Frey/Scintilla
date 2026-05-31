#pragma once

#include <QString>
#include <QVector3D>
#include <vector>

#include "VoxelFrame.h"   // VoxelKey

// ── MeshImport ───────────────────────────────────────────────────────────────
//
// Load a triangle mesh from disk and voxelise it into a set of grid positions
// suitable for constructing a ShapeMask with the Custom-positions constructor.
// Surface-only — the voxeliser marks the cells each triangle passes through
// (point sampling per triangle), not the interior volume.

namespace MeshImport {

struct Triangle {
    QVector3D v0, v1, v2;
};

// Load triangles from an STL file at `path`. Tries binary STL first by
// matching the file-size formula `84 + 50 * triangleCount`; falls back to
// ASCII STL if the file starts with "solid" but isn't sized correctly for
// binary. Returns an empty vector on failure and writes a human-readable
// reason to `*error` if non-null.
std::vector<Triangle> loadStl(const QString& path, QString* error = nullptr);

// Voxelise a triangle mesh into an N³ grid via point sampling. Each
// triangle contributes a number of samples proportional to its area in
// cube space (between `minSamples` and `maxSamples`), and each sample
// marks the voxel it lands in. The mesh is auto-fit to the cube with
// a small margin so it stays inside the bounding wireframe.
std::vector<VoxelKey> voxelise(const std::vector<Triangle>& tris,
                               int gridSize,
                               int minSamples = 6,
                               int maxSamples = 256);

}  // namespace MeshImport
