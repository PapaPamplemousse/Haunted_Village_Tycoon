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

std::optional<AIIntent> FindAvoidPersonIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                              const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                              const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindConfrontPersonIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                                 const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                                 const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindPreachIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                         const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                         const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindHoldRitualIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                             const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                             const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindComfortFrightenedIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                                    const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                                    const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindIntimidateIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                             const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                             const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindFightNonLethalIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                                 const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                                 const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindMurderIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                         const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                         const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindSeekFoodIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                           const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                           const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindRestIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                       const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                       const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindCareChildFoodIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                                const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                                const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindStoreIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                        const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                        const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindEquipWeaponIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                              const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                              const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindRequestWeaponIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                                const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                                const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindFulfillWeaponRequestIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                                       const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                                       const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindHaulIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                       const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                       const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindFleeIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                       const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                       const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindDefendIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                         const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                         const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindHuntIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                       const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                       const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindGuardIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                        const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                        const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindPatrolIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                         const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                         const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindBuildIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                        const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                        const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindDismantleIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                            const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                            const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindRepairIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                         const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                         const EntitySpatialGrid& spatialGrid);

std::optional<AIIntent> FindHarvestIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                          const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                          const EntitySpatialGrid& spatialGrid);
} // namespace ai::intents
