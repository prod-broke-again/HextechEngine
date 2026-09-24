#pragma once

#include "Simulation/Components.hpp"

#include <array>

namespace engine::era {

struct GridCoord {
    int x = 0;
    int z = 0;

    bool operator==(const GridCoord& other) const { return x == other.x && z == other.z; }
};

struct CarrierPath {
    static constexpr int kMaxCells = 96;
    std::array<GridCoord, kMaxCells> cells{};
    int length = 0;
};

// Orthogonal A* (no diagonals). Roads cost less so couriers prefer them.
[[nodiscard]] int carrierEnterCost(CellType type);
void makeManhattanPath(GridCoord start, GridCoord goal, CarrierPath& out);
bool findCarrierPath(const GridIndex& grid, GridCoord start, GridCoord goal, CarrierPath& out);

} // namespace engine::era
