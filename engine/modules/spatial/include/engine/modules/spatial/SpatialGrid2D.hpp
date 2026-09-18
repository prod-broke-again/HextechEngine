#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>

namespace engine::spatial {

template <typename T, size_t Width = 32, size_t Height = 32>
class SpatialGrid2D {
public:
    static constexpr size_t kWidth = Width;
    static constexpr size_t kHeight = Height;

    constexpr bool inBounds(int x, int z) const noexcept {
        return x >= 0 && static_cast<size_t>(x) < Width &&
               z >= 0 && static_cast<size_t>(z) < Height;
    }

    T& at(int x, int z) {
        return cells[static_cast<size_t>(z)][static_cast<size_t>(x)];
    }

    const T& at(int x, int z) const {
        return cells[static_cast<size_t>(z)][static_cast<size_t>(x)];
    }

    void clear() {
        for (auto& row : cells) {
            row.fill(T{});
        }
    }

    template <typename Predicate>
    std::vector<std::pair<int, int>> queryRadius(int cx, int cz, float radius, Predicate&& pred) const {
        std::vector<std::pair<int, int>> results;
        int minX = std::max(0, static_cast<int>(std::floor(static_cast<float>(cx) - radius)));
        int maxX = std::min(static_cast<int>(Width) - 1, static_cast<int>(std::ceil(static_cast<float>(cx) + radius)));
        int minZ = std::max(0, static_cast<int>(std::floor(static_cast<float>(cz) - radius)));
        int maxZ = std::min(static_cast<int>(Height) - 1, static_cast<int>(std::ceil(static_cast<float>(cz) + radius)));

        const float r2 = radius * radius;
        for (int z = minZ; z <= maxZ; ++z) {
            for (int x = minX; x <= maxX; ++x) {
                float dx = static_cast<float>(x - cx);
                float dz = static_cast<float>(z - cz);
                if (dx * dx + dz * dz <= r2) {
                    if (pred(cells[static_cast<size_t>(z)][static_cast<size_t>(x)], x, z)) {
                        results.emplace_back(x, z);
                    }
                }
            }
        }
        return results;
    }

    template <typename Predicate>
    std::optional<std::pair<int, int>> findNearest(int cx, int cz, float maxRadius, Predicate&& pred) const {
        std::optional<std::pair<int, int>> best;
        float bestDistSq = maxRadius * maxRadius;

        int minX = std::max(0, static_cast<int>(std::floor(static_cast<float>(cx) - maxRadius)));
        int maxX = std::min(static_cast<int>(Width) - 1, static_cast<int>(std::ceil(static_cast<float>(cx) + maxRadius)));
        int minZ = std::max(0, static_cast<int>(std::floor(static_cast<float>(cz) - maxRadius)));
        int maxZ = std::min(static_cast<int>(Height) - 1, static_cast<int>(std::ceil(static_cast<float>(cz) + maxRadius)));

        for (int z = minZ; z <= maxZ; ++z) {
            for (int x = minX; x <= maxX; ++x) {
                if (pred(cells[static_cast<size_t>(z)][static_cast<size_t>(x)], x, z)) {
                    float dx = static_cast<float>(x - cx);
                    float dz = static_cast<float>(z - cz);
                    float distSq = dx * dx + dz * dz;
                    if (distSq < bestDistSq) {
                        bestDistSq = distSq;
                        best = std::make_pair(x, z);
                    }
                }
            }
        }
        return best;
    }

    std::array<std::array<T, Width>, Height> cells{};
};

} // namespace engine::spatial
