#include "world/EntitySpatialGrid.hpp"

#include "core/Config.hpp"

#include <algorithm>
#include <cmath>

void EntitySpatialGrid::Clear() {
    m_cells.clear();
}

int EntitySpatialGrid::WorldToCell(float worldCoord) const {
    const float cellSizeWorld = static_cast<float>(Config::SPATIAL_CELL_SIZE_TILES) * Config::TILE_SIZE;
    return static_cast<int>(std::floor(worldCoord / cellSizeWorld));
}

std::int64_t EntitySpatialGrid::MakeKey(int cellX, int cellY) const {
    return (static_cast<std::int64_t>(cellX) << 32) ^ static_cast<std::uint32_t>(cellY);
}

void EntitySpatialGrid::Rebuild(const EntityManager& em) {
    m_cells.clear();
    for (EntityID id = 0; id < em.active.size(); ++id) {
        if (!em.active[id] || !em.hasTransform[id]) {
            continue;
        }

        const Vector2 position = em.transforms[id].position;
        const int cellX = WorldToCell(position.x);
        const int cellY = WorldToCell(position.y);
        m_cells[MakeKey(cellX, cellY)].push_back(id);
    }
}

std::vector<EntityID> EntitySpatialGrid::GetEntitiesInRect(const Rectangle& worldRect, const EntityManager& em) const {
    std::vector<EntityID> result;

    const int minCellX = WorldToCell(worldRect.x);
    const int minCellY = WorldToCell(worldRect.y);
    const int maxCellX = WorldToCell(worldRect.x + worldRect.width);
    const int maxCellY = WorldToCell(worldRect.y + worldRect.height);

    for (int cellY = minCellY; cellY <= maxCellY; ++cellY) {
        for (int cellX = minCellX; cellX <= maxCellX; ++cellX) {
            const auto it = m_cells.find(MakeKey(cellX, cellY));
            if (it == m_cells.end()) {
                continue;
            }

            for (EntityID id : it->second) {
                if (id >= em.active.size() || !em.active[id] || !em.hasTransform[id]) {
                    continue;
                }

                const Vector2 position = em.transforms[id].position;
                if (position.x < worldRect.x || position.x > worldRect.x + worldRect.width || position.y < worldRect.y ||
                    position.y > worldRect.y + worldRect.height) {
                    continue;
                }

                result.push_back(id);
            }
        }
    }

    return result;
}

std::vector<EntityID> EntitySpatialGrid::GetEntitiesInRadius(const Vector2& center, float radiusWorld, const EntityManager& em) const {
    Rectangle bounds = {center.x - radiusWorld, center.y - radiusWorld, radiusWorld * 2.0f, radiusWorld * 2.0f};

    std::vector<EntityID> candidates = GetEntitiesInRect(bounds, em);
    std::vector<EntityID> result;

    const float radiusSq = radiusWorld * radiusWorld;

    for (EntityID id : candidates) {
        if (id >= em.active.size() || !em.active[id] || !em.hasTransform[id]) {
            continue;
        }

        const Vector2 position = em.transforms[id].position;
        const float dx = position.x - center.x;
        const float dy = position.y - center.y;
        if (dx * dx + dy * dy <= radiusSq) {
            result.push_back(id);
        }
    }

    return result;
}

std::vector<EntityID> EntitySpatialGrid::GetEntitiesAtTile(int tileX, int tileY, const EntityManager& em) const {
    Rectangle tileRect = {static_cast<float>(tileX) * Config::TILE_SIZE, static_cast<float>(tileY) * Config::TILE_SIZE, Config::TILE_SIZE,
                          Config::TILE_SIZE};

    return GetEntitiesInRect(tileRect, em);
}
