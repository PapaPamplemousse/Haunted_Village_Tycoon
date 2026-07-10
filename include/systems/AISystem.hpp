#pragma once
#include "data/TileRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "world/WorldMap.hpp"

/**
 * @class AISystem
 * @brief Processes AI decision making and movement.
 */
class AISystem {
public:
    AISystem() = default;

    void Update(float deltaTime, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg);
};
