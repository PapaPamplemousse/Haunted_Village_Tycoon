/**
 * @file EntitySpatialGrid.hpp
 * @brief Lightweight spatial index for fast entity lookups by world position.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "ecs/EntityManager.hpp"

#include <cstdint>
#include <raylib.h>
#include <unordered_map>
#include <vector>

/**
 * @class EntitySpatialGrid
 * @brief Lightweight spatial index for querying entities by world position.
 *
 * This is not a full chunk streaming system yet.
 * It is a fast lookup structure rebuilt from the ECS to avoid scanning all entities
 * for rendering, inspection, and later AI queries.
 */
class EntitySpatialGrid {
public:
    EntitySpatialGrid() = default;

    void Rebuild(const EntityManager& em);

    std::vector<EntityID> GetEntitiesInRect(const Rectangle& worldRect, const EntityManager& em) const;

    std::vector<EntityID> GetEntitiesInRadius(const Vector2& center, float radiusWorld, const EntityManager& em) const;

    std::vector<EntityID> GetEntitiesAtTile(int tileX, int tileY, const EntityManager& em) const;

    void Clear();

private:
    std::unordered_map<std::int64_t, std::vector<EntityID>> m_cells;

    int WorldToCell(float worldCoord) const;
    std::int64_t MakeKey(int cellX, int cellY) const;
};
