#include "systems/Pathfinder.hpp"

#include "core/Config.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <queue>
#include <utility>
#include <vector>

namespace {

constexpr float SQRT_2 = 1.41421356237f;

struct Cell {
    int x = 0;
    int y = 0;
};

struct Direction {
    int dx = 0;
    int dy = 0;
    float cost = 1.0f;
};

struct OpenNode {
    int x = 0;
    int y = 0;
    float gCost = 0.0f;
    float hCost = 0.0f;

    float FCost() const {
        return gCost + hCost;
    }
};

struct OpenNodeCompare {
    bool operator()(const OpenNode& lhs, const OpenNode& rhs) const {
        if (lhs.FCost() == rhs.FCost()) {
            return lhs.hCost > rhs.hCost;
        }

        return lhs.FCost() > rhs.FCost();
    }
};

enum class NavCell : unsigned char { Blocked, Walkable, DoorOpen, DoorClosed, DoorLockedOwned, DoorLockedBlocked };

struct NavigationGrid {
    int width = 0;
    int height = 0;
    std::vector<NavCell> cells;
};

int ToIndex(int x, int y, int width) {
    return y * width + x;
}

bool IsInsideMap(int x, int y, const WorldMap& map) {
    return x >= 0 && y >= 0 && x < map.GetWidth() && y < map.GetHeight();
}

bool IsInsideGrid(int x, int y, const NavigationGrid& grid) {
    return x >= 0 && y >= 0 && x < grid.width && y < grid.height;
}

Cell WorldToTile(Vector2 position) {
    return {static_cast<int>(std::floor(position.x / Config::TILE_SIZE)), static_cast<int>(std::floor(position.y / Config::TILE_SIZE))};
}

Vector2 TileToWorldCenter(int x, int y) {
    return {(static_cast<float>(x) + 0.5f) * Config::TILE_SIZE, (static_cast<float>(y) + 0.5f) * Config::TILE_SIZE};
}

NavCell GetCell(const NavigationGrid& grid, int x, int y) {
    return grid.cells[static_cast<size_t>(ToIndex(x, y, grid.width))];
}

void SetCell(NavigationGrid& grid, int x, int y, NavCell value) {
    grid.cells[static_cast<size_t>(ToIndex(x, y, grid.width))] = value;
}

float OctileDistance(int x1, int y1, int x2, int y2) {
    const float dx = static_cast<float>(std::abs(x1 - x2));
    const float dy = static_cast<float>(std::abs(y1 - y2));

    constexpr float D = 1.0f;
    constexpr float D2 = SQRT_2;

    return D * (dx + dy) + (D2 - 2.0f * D) * std::min(dx, dy);
}

bool IsDoorCell(NavCell cell) {
    return cell == NavCell::DoorOpen || cell == NavCell::DoorClosed || cell == NavCell::DoorLockedOwned ||
           cell == NavCell::DoorLockedBlocked;
}

bool IsPassableCell(NavCell cell) {
    return cell == NavCell::Walkable || cell == NavCell::DoorOpen || cell == NavCell::DoorClosed || cell == NavCell::DoorLockedOwned;
}

bool IsWalkable(int x, int y, const NavigationGrid& grid) {
    if (!IsInsideGrid(x, y, grid)) {
        return false;
    }

    return IsPassableCell(GetCell(grid, x, y));
}

NavigationGrid BuildNavigationGrid(const WorldMap& map, const TileRegistry& tileReg, const EntityManager& em, EntityID actorId) {
    NavigationGrid grid;
    grid.width = map.GetWidth();
    grid.height = map.GetHeight();

    const int tileCount = grid.width * grid.height;
    grid.cells.resize(static_cast<size_t>(tileCount), NavCell::Blocked);

    // 1. Base map walkability.
    for (int y = 0; y < grid.height; ++y) {
        for (int x = 0; x < grid.width; ++x) {
            const int tileId = map.GetTile(x, y);
            const TileDef* tileDef = tileReg.GetTileDef(tileId);

            if (tileDef != nullptr && tileDef->walkable) {
                SetCell(grid, x, y, NavCell::Walkable);
            } else {
                SetCell(grid, x, y, NavCell::Blocked);
            }
        }
    }

    // 2. Dynamic blocking constructions.
    // Doors are handled later and override the underlying cell.
    for (size_t i = 0; i < em.active.size(); ++i) {
        if (!em.active[i] || !em.hasConstruction[i] || !em.hasTransform[i]) {
            continue;
        }

        // Unfinished blueprints should not block navigation yet.
        if (em.hasBlueprint[i] && !em.blueprints[i].isFinished) {
            continue;
        }

        if (em.hasDoor[i]) {
            continue;
        }

        if (!em.constructions[i].isWall) {
            continue;
        }

        const int x = static_cast<int>(std::floor(em.transforms[i].position.x / Config::TILE_SIZE));

        const int y = static_cast<int>(std::floor(em.transforms[i].position.y / Config::TILE_SIZE));

        if (!IsInsideMap(x, y, map)) {
            continue;
        }

        SetCell(grid, x, y, NavCell::Blocked);
    }

    // 3. Doors override the tile/construction underneath.
    for (size_t i = 0; i < em.active.size(); ++i) {
        if (!em.active[i] || !em.hasDoor[i] || !em.hasTransform[i]) {
            continue;
        }
        // Unfinished blueprints should not block navigation yet.
        if (em.hasBlueprint[i] && !em.blueprints[i].isFinished) {
            continue;
        }

        const int x = static_cast<int>(std::floor(em.transforms[i].position.x / Config::TILE_SIZE));

        const int y = static_cast<int>(std::floor(em.transforms[i].position.y / Config::TILE_SIZE));

        if (!IsInsideMap(x, y, map)) {
            continue;
        }

        const DoorComponent& door = em.doors[i];

        if (door.state == DoorState::OPEN) {
            SetCell(grid, x, y, NavCell::DoorOpen);
        } else if (door.state == DoorState::CLOSED) {
            SetCell(grid, x, y, NavCell::DoorClosed);
        } else if (door.state == DoorState::LOCKED) {
            if (door.ownerId == actorId) {
                SetCell(grid, x, y, NavCell::DoorLockedOwned);
            } else {
                SetCell(grid, x, y, NavCell::DoorLockedBlocked);
            }
        }
    }

    return grid;
}

bool CanMoveDiagonally(int currentX, int currentY, const Direction& direction, const NavigationGrid& grid) {
    if (direction.dx == 0 || direction.dy == 0) {
        return true;
    }

    const int sideAX = currentX + direction.dx;
    const int sideAY = currentY;

    const int sideBX = currentX;
    const int sideBY = currentY + direction.dy;

    if (!IsWalkable(sideAX, sideAY, grid)) {
        return false;
    }

    if (!IsWalkable(sideBX, sideBY, grid)) {
        return false;
    }

    return true;
}

std::vector<Vector2> ReconstructPath(Cell start, Cell end, const std::vector<Cell>& parents, int width) {
    std::vector<Vector2> path;

    Cell current = end;

    while (!(current.x == start.x && current.y == start.y)) {
        path.push_back(TileToWorldCenter(current.x, current.y));

        const int currentIndex = ToIndex(current.x, current.y, width);
        const Cell parent = parents[static_cast<size_t>(currentIndex)];

        if (parent.x < 0 || parent.y < 0) {
            return {};
        }

        current = parent;
    }

    std::reverse(path.begin(), path.end());
    return path;
}

float ComputePathCost(Vector2 start, const std::vector<Vector2>& path) {
    if (path.empty()) {
        return std::numeric_limits<float>::infinity();
    }

    float cost = 0.0f;
    Vector2 previous = start;

    for (const Vector2& point : path) {
        const float dx = point.x - previous.x;
        const float dy = point.y - previous.y;

        cost += std::sqrt(dx * dx + dy * dy);
        previous = point;
    }

    return cost;
}

std::vector<Vector2> FindPathInternal(Vector2 start, Vector2 end, const NavigationGrid& grid) {
    if (grid.width <= 0 || grid.height <= 0) {
        return {};
    }

    const Cell startCell = WorldToTile(start);
    const Cell endCell = WorldToTile(end);

    if (!IsInsideGrid(startCell.x, startCell.y, grid) || !IsInsideGrid(endCell.x, endCell.y, grid)) {
        return {};
    }

    if (startCell.x == endCell.x && startCell.y == endCell.y) {
        return {};
    }

    if (!IsWalkable(endCell.x, endCell.y, grid)) {
        return {};
    }

    const int tileCount = grid.width * grid.height;

    std::vector<float> gCosts(static_cast<size_t>(tileCount), std::numeric_limits<float>::infinity());

    std::vector<bool> closedSet(static_cast<size_t>(tileCount), false);

    std::vector<Cell> parents(static_cast<size_t>(tileCount), Cell{-1, -1});

    std::priority_queue<OpenNode, std::vector<OpenNode>, OpenNodeCompare> openSet;

    const int startIndex = ToIndex(startCell.x, startCell.y, grid.width);
    gCosts[static_cast<size_t>(startIndex)] = 0.0f;

    openSet.push({startCell.x, startCell.y, 0.0f, OctileDistance(startCell.x, startCell.y, endCell.x, endCell.y)});

    const Direction directions[8] = {{1, 0, 1.0f},   {-1, 0, 1.0f},   {0, 1, 1.0f},    {0, -1, 1.0f},

                                     {1, 1, SQRT_2}, {1, -1, SQRT_2}, {-1, 1, SQRT_2}, {-1, -1, SQRT_2}};

    while (!openSet.empty()) {
        const OpenNode current = openSet.top();
        openSet.pop();

        const int currentIndex = ToIndex(current.x, current.y, grid.width);

        if (closedSet[static_cast<size_t>(currentIndex)]) {
            continue;
        }

        closedSet[static_cast<size_t>(currentIndex)] = true;

        if (current.x == endCell.x && current.y == endCell.y) {
            return ReconstructPath(startCell, endCell, parents, grid.width);
        }

        for (const Direction& direction : directions) {
            const int neighborX = current.x + direction.dx;
            const int neighborY = current.y + direction.dy;

            if (!IsInsideGrid(neighborX, neighborY, grid)) {
                continue;
            }

            if (!IsWalkable(neighborX, neighborY, grid)) {
                continue;
            }

            const bool isDiagonal = direction.dx != 0 && direction.dy != 0;

            // A door must be entered orthogonally.
            // Otherwise the AI may try to open/pass through the door from a diagonal tile.
            if (isDiagonal) {
                const NavCell neighborCell = GetCell(grid, neighborX, neighborY);

                if (IsDoorCell(neighborCell)) {
                    continue;
                }
            }

            // Prevent diagonal movement through wall corners.
            if (!CanMoveDiagonally(current.x, current.y, direction, grid)) {
                continue;
            }

            const int neighborIndex = ToIndex(neighborX, neighborY, grid.width);

            if (closedSet[static_cast<size_t>(neighborIndex)]) {
                continue;
            }

            const float tentativeGCost = gCosts[static_cast<size_t>(currentIndex)] + direction.cost;

            if (tentativeGCost >= gCosts[static_cast<size_t>(neighborIndex)]) {
                continue;
            }

            parents[static_cast<size_t>(neighborIndex)] = {current.x, current.y};

            gCosts[static_cast<size_t>(neighborIndex)] = tentativeGCost;

            const float hCost = OctileDistance(neighborX, neighborY, endCell.x, endCell.y);

            openSet.push({neighborX, neighborY, tentativeGCost, hCost});
        }
    }

    return {};
}

} // namespace

std::vector<Vector2> Pathfinder::FindPath(Vector2 start, Vector2 end, const WorldMap& map, const TileRegistry& tileReg,
                                          const EntityManager& em, EntityID actorId) {
    if (map.GetWidth() <= 0 || map.GetHeight() <= 0) {
        return {};
    }

    const NavigationGrid grid = BuildNavigationGrid(map, tileReg, em, actorId);

    return FindPathInternal(start, end, grid);
}

std::vector<Vector2> Pathfinder::FindPathToAdjacentTile(Vector2 start, Vector2 target, const WorldMap& map, const TileRegistry& tileReg,
                                                        const EntityManager& em, EntityID actorId) {
    if (map.GetWidth() <= 0 || map.GetHeight() <= 0) {
        return {};
    }

    const NavigationGrid grid = BuildNavigationGrid(map, tileReg, em, actorId);

    const Cell startCell = WorldToTile(start);
    const Cell targetCell = WorldToTile(target);

    if (!IsInsideGrid(startCell.x, startCell.y, grid) || !IsInsideGrid(targetCell.x, targetCell.y, grid)) {
        return {};
    }

    // For build/dismantle jobs, the worker should stand on an orthogonal adjacent tile.
    // Diagonal adjacency is intentionally not used here.
    const Direction candidates[4] = {{1, 0, 1.0f}, {-1, 0, 1.0f}, {0, 1, 1.0f}, {0, -1, 1.0f}};

    std::vector<Vector2> bestPath;
    float bestCost = std::numeric_limits<float>::infinity();

    for (const Direction& candidate : candidates) {
        const int candidateX = targetCell.x + candidate.dx;
        const int candidateY = targetCell.y + candidate.dy;

        if (!IsInsideGrid(candidateX, candidateY, grid)) {
            continue;
        }

        if (!IsWalkable(candidateX, candidateY, grid)) {
            continue;
        }

        const Vector2 candidateWorld = TileToWorldCenter(candidateX, candidateY);

        // Already standing on a valid adjacent tile.
        if (startCell.x == candidateX && startCell.y == candidateY) {
            return {candidateWorld};
        }

        std::vector<Vector2> path = FindPathInternal(start, candidateWorld, grid);

        if (path.empty()) {
            continue;
        }

        const float pathCost = ComputePathCost(start, path);

        if (pathCost < bestCost) {
            bestCost = pathCost;
            bestPath = std::move(path);
        }
    }

    return bestPath;
}
