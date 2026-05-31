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
        case ShapeType::Torus:    return "torus";
        case ShapeType::Ring:     return "ring";
        case ShapeType::Cross:    return "cross";
        case ShapeType::Custom:   return "custom";
    }
    return "cube";
}

ShapeType shapeTypeFromName(const std::string& name) {
    if (name == "cube")     return ShapeType::Cube;
    if (name == "sphere")   return ShapeType::Sphere;
    if (name == "cylinder") return ShapeType::Cylinder;
    if (name == "pyramid")  return ShapeType::Pyramid;
    if (name == "torus")    return ShapeType::Torus;
    if (name == "ring")     return ShapeType::Ring;
    if (name == "cross")    return ShapeType::Cross;
    if (name == "custom")   return ShapeType::Custom;
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

ShapeMask::ShapeMask(int gridSize, std::vector<VoxelKey> positions)
    : m_gridSize(std::clamp(gridSize, 3, 32))
    , m_shape(ShapeType::Custom) {
    // Dedupe + clamp incoming positions to the valid grid range. We don't
    // trust callers (the mesh voxeliser, primarily) to have done it
    // already, and a bad position downstream would corrupt the index map.
    std::set<VoxelKey> uniq;
    for (const VoxelKey& k : positions) {
        if (k.x < 0 || k.x >= m_gridSize) continue;
        if (k.y < 0 || k.y >= m_gridSize) continue;
        if (k.z < 0 || k.z >= m_gridSize) continue;
        uniq.insert(k);
    }
    m_positions.reserve(uniq.size());
    for (const VoxelKey& k : uniq) {
        m_indexMap[k] = static_cast<int>(m_positions.size());
        m_positions.push_back(k);
    }
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

        case ShapeType::Torus: {
            // Donut centred at the cube centre, with its hole along the
            // Y axis. Major radius R is the distance from the cube centre
            // to the centre of the tube; minor radius r is the tube
            // thickness. Inside the torus iff:
            //   (sqrt(dx² + dz²) - R)² + dy² ≤ r²
            const float R     = c * 0.62f;
            const float r     = c * 0.32f;
            const float ringR = std::sqrt(dx * dx + dz * dz) - R;
            return (ringR * ringR + dy * dy) <= (r * r);
        }

        case ShapeType::Ring: {
            // Hollow cylinder along the Y axis. Outer wall matches Cylinder;
            // inner wall removes a coaxial hole through the middle.
            const float radial = dx * dx + dz * dz;
            const float innerR = c * 0.55f;
            return radial <= rSq && radial >= (innerR * innerR);
        }

        case ShapeType::Cross: {
            // Three perpendicular arms intersecting at the centre. Each
            // arm extends the full grid along one axis and is `a` half-
            // thick across the other two. Inside iff the point lies in
            // any arm.
            const float a = c * 0.32f + 0.5f;
            const bool xArm = std::fabs(dy) <= a && std::fabs(dz) <= a;
            const bool yArm = std::fabs(dx) <= a && std::fabs(dz) <= a;
            const bool zArm = std::fabs(dx) <= a && std::fabs(dy) <= a;
            return xArm || yArm || zArm;
        }

        case ShapeType::Custom:
            // Should never reach here — Custom masks are built via the
            // vector-of-positions constructor and bypass testPosition().
            return false;
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
