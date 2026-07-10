#pragma once

#include "data/TileRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "world/WorldMap.hpp"

#include <raylib.h>
#include <vector>

class Pathfinder {
public:
    static std::vector<Vector2> FindPath(Vector2 start, Vector2 end, const WorldMap& map, const TileRegistry& tileReg,
                                         const EntityManager& em, EntityID actorId);
    static std::vector<Vector2> FindPathToAdjacentTile(Vector2 start, Vector2 target, const WorldMap& map, const TileRegistry& tileReg,
                                                       const EntityManager& em, EntityID actorId);
};
