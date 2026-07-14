/**
 * @file AITaskExecutor.hpp
 * @brief Applies AI task intents to entity behavior state.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "data/TileRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "systems/ai/AIIntent.hpp"
#include "world/WorldMap.hpp"

#include <string>

class AITaskExecutor {
public:
    static bool StartAction(EntityID actor, EntityID target, EntityManager& em, const std::string& actionTask, float actionDuration);

    static bool StartMoveAdjacentToEntity(EntityID actor, EntityID target, EntityManager& em, const WorldMap& map,
                                          const TileRegistry& tileReg, const std::string& moveTask, const std::string& actionTask,
                                          float actionDuration);

    static bool ApplyIntent(EntityID actor, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg, const AIIntent& intent);

    static bool AreEntitiesAdjacent(EntityID a, EntityID b, const EntityManager& em);

    static bool StartMoveToPosition(EntityID actor, EntityID taskTarget, Vector2 targetPosition, EntityManager& em, const WorldMap& map,
                                    const TileRegistry& tileReg, const std::string& moveTask);
};
