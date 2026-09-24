#include "Simulation/Pathfinder.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>

namespace engine::era {

namespace {

constexpr int kGrid = GridIndex::kGridSize;
constexpr int kNodeCount = kGrid * kGrid;
constexpr int kInf = 1'000'000;
constexpr int kDx[4] = {0, 1, 0, -1};
constexpr int kDz[4] = {-1, 0, 1, 0};

int pack(int x, int z) {
    return z * kGrid + x;
}

bool canEnter(const GridIndex& grid, int x, int z, GridCoord start, GridCoord goal) {
    if (!grid.isInBounds(x, z)) {
        return false;
    }
    const CellType type = grid.getCell(x, z).type;
    if (type == CellType::Building) {
        return (x == start.x && z == start.z) || (x == goal.x && z == goal.z);
    }
    return true;
}

} // namespace

int carrierEnterCost(CellType type) {
    switch (type) {
        case CellType::Road:  return 1;
        case CellType::Trail: return 2;
        default:              return 3; // empty grass
    }
}

void makeManhattanPath(GridCoord start, GridCoord goal, CarrierPath& out) {
    out.length = 0;
    if (out.length < CarrierPath::kMaxCells) {
        out.cells[out.length++] = start;
    }
    int x = start.x;
    int z = start.z;
    while (x != goal.x && out.length < CarrierPath::kMaxCells) {
        x += (goal.x > x) ? 1 : -1;
        out.cells[out.length++] = {x, z};
    }
    while (z != goal.z && out.length < CarrierPath::kMaxCells) {
        z += (goal.z > z) ? 1 : -1;
        out.cells[out.length++] = {x, z};
    }
}

bool findCarrierPath(const GridIndex& grid, GridCoord start, GridCoord goal, CarrierPath& out) {
    out.length = 0;
    if (!grid.isInBounds(start.x, start.z) || !grid.isInBounds(goal.x, goal.z)) {
        makeManhattanPath(start, goal, out);
        return false;
    }
    if (start == goal) {
        out.cells[0] = start;
        out.length = 1;
        return true;
    }

    std::array<int, kNodeCount> g{};
    std::array<int, kNodeCount> parent{};
    std::array<uint8_t, kNodeCount> closed{};
    std::array<uint8_t, kNodeCount> inOpen{};
    g.fill(kInf);
    parent.fill(-1);

    const int startI = pack(start.x, start.z);
    g[startI] = 0;
    inOpen[startI] = 1;
    int openCount = 1;

    auto heur = [&](int x, int z) {
        return (std::abs(x - goal.x) + std::abs(z - goal.z)) * 1;
    };

    while (openCount > 0) {
        int bestI = -1;
        int bestF = kInf;
        int bestX = kGrid;
        int bestZ = kGrid;
        for (int z = 0; z < kGrid; ++z) {
            for (int x = 0; x < kGrid; ++x) {
                const int i = pack(x, z);
                if (!inOpen[i] || closed[i]) {
                    continue;
                }
                const int f = g[i] + heur(x, z);
                if (f < bestF || (f == bestF && (z < bestZ || (z == bestZ && x < bestX)))) {
                    bestF = f;
                    bestI = i;
                    bestX = x;
                    bestZ = z;
                }
            }
        }
        if (bestI < 0) {
            break;
        }

        inOpen[bestI] = 0;
        --openCount;
        closed[bestI] = 1;

        if (bestX == goal.x && bestZ == goal.z) {
            int walk[CarrierPath::kMaxCells];
            int walkLen = 0;
            for (int cur = bestI; cur >= 0 && walkLen < CarrierPath::kMaxCells; cur = parent[cur]) {
                walk[walkLen++] = cur;
            }
            out.length = 0;
            for (int i = walkLen - 1; i >= 0; --i) {
                out.cells[out.length++] = {walk[i] % kGrid, walk[i] / kGrid};
            }
            return out.length >= 1;
        }

        for (int d = 0; d < 4; ++d) {
            const int nx = bestX + kDx[d];
            const int nz = bestZ + kDz[d];
            if (!canEnter(grid, nx, nz, start, goal)) {
                continue;
            }
            const int ni = pack(nx, nz);
            if (closed[ni]) {
                continue;
            }
            const int ng = g[bestI] + carrierEnterCost(grid.getCell(nx, nz).type);
            if (ng < g[ni]) {
                g[ni] = ng;
                parent[ni] = bestI;
                if (!inOpen[ni]) {
                    inOpen[ni] = 1;
                    ++openCount;
                }
            }
        }
    }

    makeManhattanPath(start, goal, out);
    return false;
}

} // namespace engine::era
