#pragma once

#include <set>
#include <string>
#include <vector>
#include "VoxelFrame.h"

// ── ShapeType ─────────────────────────────────────────────────────────────────

enum class ShapeType {
    Cube,
    Sphere,
    Cylinder,
    Pyramid,
    // v1.1: Torus, Ring
};

[[nodiscard]] std::string shapeTypeName(ShapeType s);
[[nodiscard]] ShapeType shapeTypeFromName(const std::string& name);

// ── ShapeMask ─────────────────────────────────────────────────────────────────
//
// Defines the set of addressable LED positions for a given shape and grid size.
// Generated once at construction; rebuilt when shape or gridSize changes.
//
// Algorithms are a direct port of the prototype JavaScript (see prototype/index.html).
// Key decision DEC-006: changing shape clears animation data. This class is
// immutable after construction — always construct a new ShapeMask on shape change.

class ShapeMask {
public:
    ShapeMask(int gridSize, ShapeType shape);

    [[nodiscard]] int gridSize() const { return m_gridSize; }
    [[nodiscard]] ShapeType shape() const { return m_shape; }
    [[nodiscard]] int count() const { return static_cast<int>(m_positions.size()); }

    // All addressable positions in iteration order (used for renderer instance buffer)
    [[nodiscard]] const std::vector<VoxelKey>& positions() const { return m_positions; }

    // O(log n) membership test
    [[nodiscard]] bool contains(int x, int y, int z) const;
    [[nodiscard]] bool contains(VoxelKey key) const;

    // World-space offset to centre the grid at origin (used by renderer)
    // LED at grid position (x,y,z) sits at world position (x+offset, y+offset, z+offset)
    [[nodiscard]] float worldOffset() const { return -static_cast<float>(m_gridSize - 1) / 2.0f; }

    // Instance index for a given grid position (-1 if not in mask)
    [[nodiscard]] int instanceIndex(int x, int y, int z) const;

private:
    int m_gridSize;
    ShapeType m_shape;
    std::vector<VoxelKey> m_positions;
    std::map<VoxelKey, int> m_indexMap;   // position → index in m_positions

    void build();
    [[nodiscard]] bool testPosition(int x, int y, int z) const;
};
