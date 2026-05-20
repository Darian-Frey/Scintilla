#include "ShapeMask.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

// ── ShapeType helpers ─────────────────────────────────────────────────────────

std::string shapeTypeName(ShapeType s) {
    switch (s) {
        case ShapeType::Cube:     return "cube";
        case ShapeType::Sphere:   return "sphere";
        case ShapeType::Cylinder: return "cylinder";
        case ShapeType::Pyramid:  return "pyramid";
    }
    return "cube";
}

ShapeType shapeTypeFromName(const std::string& name) {
    if (name == "cube")     return ShapeType::Cube;
    if (name == "sphere")   return ShapeType::Sphere;
    if (name == "cylinder") return ShapeType::Cylinder;
    if (name == "pyramid")  return ShapeType::Pyramid;
    throw std::invalid_argument("unknown shape: " + name);
}

// ── ShapeMask ─────────────────────────────────────────────────────────────────
//
// Grid-size cap enforced at construction (DEC-005 / AV-006). Algorithms are a
// direct port of the prototype JavaScript — see prototype/index.html.

ShapeMask::ShapeMask(int gridSize, ShapeType shape)
    : m_gridSize(std::clamp(gridSize, 3, 32))
    , m_shape(shape) {
    build();
}

void ShapeMask::build() {
    m_positions.clear();
    m_indexMap.clear();
    m_positions.reserve(static_cast<size_t>(m_gridSize) * m_gridSize * m_gridSize);

    for (int y = 0; y < m_gridSize; ++y) {
        for (int z = 0; z < m_gridSize; ++z) {
            for (int x = 0; x < m_gridSize; ++x) {
                if (testPosition(x, y, z)) {
                    VoxelKey key{x, y, z};
                    m_indexMap[key] = static_cast<int>(m_positions.size());
                    m_positions.push_back(key);
                }
            }
        }
    }
}

bool ShapeMask::testPosition(int x, int y, int z) const {
    const float c    = static_cast<float>(m_gridSize - 1) / 2.0f;
    const float dx   = static_cast<float>(x) - c;
    const float dy   = static_cast<float>(y) - c;
    const float dz   = static_cast<float>(z) - c;
    const float rSq  = (c + 0.5f) * (c + 0.5f);

    switch (m_shape) {
        case ShapeType::Cube:
            return true;

        case ShapeType::Sphere:
            return (dx * dx + dy * dy + dz * dz) <= rSq;

        case ShapeType::Cylinder:
            return (dx * dx + dz * dz) <= rSq;

        case ShapeType::Pyramid: {
            // Half-width tapers linearly from c (bottom, y=0) to 0 (top, y=n-1).
            const float t  = (m_gridSize > 1)
                           ? static_cast<float>(y) / static_cast<float>(m_gridSize - 1)
                           : 0.0f;
            const float hw = (1.0f - t) * c + 0.5f;
            return std::fabs(dx) <= hw && std::fabs(dz) <= hw;
        }
    }
    return false;
}

bool ShapeMask::contains(int x, int y, int z) const {
    return contains(VoxelKey{x, y, z});
}

bool ShapeMask::contains(VoxelKey key) const {
    return m_indexMap.find(key) != m_indexMap.end();
}

int ShapeMask::instanceIndex(int x, int y, int z) const {
    auto it = m_indexMap.find({x, y, z});
    return (it == m_indexMap.end()) ? -1 : it->second;
}
