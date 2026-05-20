#include "VoxelFrame.h"

void VoxelFrame::set(int x, int y, int z, uint8_t r, uint8_t g, uint8_t b) {
    m_voxels[{x, y, z}] = {r, g, b};
}

void VoxelFrame::set(VoxelKey key, RGB color) {
    m_voxels[key] = color;
}

void VoxelFrame::erase(int x, int y, int z) {
    m_voxels.erase({x, y, z});
}

void VoxelFrame::erase(VoxelKey key) {
    m_voxels.erase(key);
}

void VoxelFrame::clear() {
    m_voxels.clear();
}

std::optional<RGB> VoxelFrame::get(int x, int y, int z) const {
    return get(VoxelKey{x, y, z});
}

std::optional<RGB> VoxelFrame::get(VoxelKey key) const {
    auto it = m_voxels.find(key);
    if (it == m_voxels.end()) return std::nullopt;
    return it->second;
}

bool VoxelFrame::isLit(int x, int y, int z) const {
    return m_voxels.find({x, y, z}) != m_voxels.end();
}

int VoxelFrame::litCount() const {
    return static_cast<int>(m_voxels.size());
}

bool VoxelFrame::empty() const {
    return m_voxels.empty();
}

std::vector<float> VoxelFrame::toFlatArray() const {
    std::vector<float> out;
    out.reserve(m_voxels.size() * 6);
    for (const auto& [key, color] : m_voxels) {
        out.push_back(static_cast<float>(key.x));
        out.push_back(static_cast<float>(key.y));
        out.push_back(static_cast<float>(key.z));
        out.push_back(static_cast<float>(color[0]) / 255.0f);
        out.push_back(static_cast<float>(color[1]) / 255.0f);
        out.push_back(static_cast<float>(color[2]) / 255.0f);
    }
    return out;
}
