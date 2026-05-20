#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <tuple>
#include <vector>

// ── Types ─────────────────────────────────────────────────────────────────────

using RGB = std::array<uint8_t, 3>;

struct VoxelKey {
    int x, y, z;
    auto operator<=>(const VoxelKey&) const = default;
};

// ── VoxelFrame ────────────────────────────────────────────────────────────────
//
// A single animation frame. Stores only lit LEDs (sparse encoding).
// Off LEDs are absent from the map — never stored as null or zero.
//
// This matches the JSON wire format: {"x,y,z": [r,g,b]}.
// Key decision DEC-003: sparse encoding is stable at v1.0; do not change.

class VoxelFrame {
public:
    VoxelFrame() = default;
    VoxelFrame(const VoxelFrame&)            = default;
    VoxelFrame& operator=(const VoxelFrame&) = default;
    VoxelFrame(VoxelFrame&&) noexcept        = default;
    VoxelFrame& operator=(VoxelFrame&&) noexcept = default;

    void set(int x, int y, int z, uint8_t r, uint8_t g, uint8_t b);
    void set(VoxelKey key, RGB color);
    void erase(int x, int y, int z);
    void erase(VoxelKey key);
    void clear();

    [[nodiscard]] std::optional<RGB> get(int x, int y, int z) const;
    [[nodiscard]] std::optional<RGB> get(VoxelKey key) const;
    [[nodiscard]] bool isLit(int x, int y, int z) const;

    [[nodiscard]] int litCount() const;
    [[nodiscard]] bool empty() const;

    // Iterate over all lit voxels
    [[nodiscard]] const std::map<VoxelKey, RGB>& voxels() const { return m_voxels; }

    // Flat array of (x,y,z,r,g,b) for renderer upload — only lit LEDs
    [[nodiscard]] std::vector<float> toFlatArray() const;

private:
    std::map<VoxelKey, RGB> m_voxels;
};
