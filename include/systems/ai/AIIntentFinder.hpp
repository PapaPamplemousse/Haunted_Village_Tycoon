/**
 * @file AIIntentFinder.hpp
 * @brief Routes AI task types to intent finders.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "data/ResourceRegistry.hpp"
#include "data/TileRegistry.hpp"
#include "data/WeaponRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "systems/ai/AIIntent.hpp"
#include "systems/ai/AITaskType.hpp"
#include "world/EntitySpatialGrid.hpp"
#include "world/WorldMap.hpp"

#include <optional>

namespace AIIntentFinder {

std::optional<AIIntent> FindIntentForTask(EntityID entity, AITaskType taskType, EntityManager& em, const WorldMap& map,
                                          const TileRegistry& tileReg, const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                          const EntitySpatialGrid& spatialGrid);

} // namespace AIIntentFinder
