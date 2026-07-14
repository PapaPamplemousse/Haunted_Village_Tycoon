/**
 * @file AIIntentFinders.hpp
 * @brief Domain-specific AI intent finder declarations.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "data/ResourceRegistry.hpp"
#include "data/TileRegistry.hpp"
#include "data/WeaponRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "systems/ai/AIIntent.hpp"
#include "world/EntitySpatialGrid.hpp"
#include "world/WorldMap.hpp"

#include <optional>

namespace ai::intents {

std::optional<AIIntent> FindWanderIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                         const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                         const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindReturnToVillageCoreIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                                      const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                                      const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindSocializeIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                            const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                            const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindPrayIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                       const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                       const EntitySpatialGrid& spatialGrid);

} // namespace ai::intents
