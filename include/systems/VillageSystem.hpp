#pragma once

#include "data/BehaviorRegistry.hpp"
#include "data/EntityRegistry.hpp"
#include "data/FurnitureRegistry.hpp"
#include "data/NameRegistry.hpp"
#include "data/ResourceRegistry.hpp"
#include "data/TileRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "systems/TimeSystem.hpp"
#include "world/WorldMap.hpp"

#include <raylib.h>

/**
 * @class VillageSystem
 * @brief Handles village initialization, population tracking and simple reproduction.
 */
class VillageSystem {
public:
    VillageSystem() = default;

    EntityID InitializeStartingVillage(EntityManager& em, EntityRegistry& entityReg, FurnitureRegistry& furnitureReg,
                                       const NameRegistry& nameReg, const BehaviorRegistry& behaviorReg, const WorldMap& map,
                                       const TileRegistry& tileReg, Vector2 preferredCenter);

    void Update(float deltaTime, EntityManager& em, EntityRegistry& entityReg, const NameRegistry& nameReg,
                const BehaviorRegistry& behaviorReg, const WorldMap& map, const TileRegistry& tileReg, const ResourceRegistry& resourceReg,
                const TimeSystem& timeSystem);

private:
    int m_lastProcessedSeasonNumber = 0;
};
