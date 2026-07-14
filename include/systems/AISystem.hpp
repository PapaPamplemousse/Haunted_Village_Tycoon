/**
 * @file AISystem.hpp
 * @brief Main AI system responsible for entity state machines and task delegation.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "data/ResourceRegistry.hpp"
#include "data/TileRegistry.hpp"
#include "data/WeaponRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "systems/RoomSystem.hpp"
#include "systems/ai/AITaskCandidate.hpp"
#include "world/EntitySpatialGrid.hpp"
#include "world/WorldMap.hpp"

#include <raylib.h>
#include <string>
#include <vector>

class AISystem {
public:
    AISystem() = default;

    void Update(float deltaTime, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg, const ResourceRegistry& resourceReg,
                const WeaponRegistry& weaponReg, const EntitySpatialGrid& spatialGrid, const Vector2& simulationCenter,
                float activeRadiusTiles, float currentHour, RoomSystem& roomSys);

private:
    // --- State Handlers ---
    void HandleIdleState(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                         const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg, const EntitySpatialGrid& spatialGrid,
                         float currentHour);

    void HandleMovingState(EntityID entity, float deltaTime, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg);

    void HandleTaskCompletion(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                              const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg, const EntitySpatialGrid& spatialGrid,
                              RoomSystem& roomSys);

    bool HandleFatigueCollapse(EntityID entity, float deltaTime, EntityManager& em);

    // --- Helpers ---
    void InteractWithDoorIfPresent(EntityID entity, int targetX, int targetY, EntityManager& em);

    bool HasCapability(const BehaviorComponent& behavior, const std::string& capability) const;

    const BehaviorRule* FindBehaviorRule(const BehaviorComponent& behavior, const std::string& ruleName) const;

    bool IsSpeciesTargetedByRule(const BehaviorRule& rule, const std::string& species) const;

    bool AreEntitiesAdjacent(EntityID a, EntityID b, const EntityManager& em) const;

    void ResetBehaviorState(BehaviorComponent& behavior);

    // --- Decision / Interruption ---
    void UpdateAIContextTimers(float deltaTime, EntityManager& em);

    bool TryInterruptCurrentTask(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                 const ResourceRegistry& resourceReg, const EntitySpatialGrid& spatialGrid);

    void BuildTaskCandidates(EntityID entity, EntityManager& em, const ResourceRegistry& resourceReg, const EntitySpatialGrid& spatialGrid,
                             float currentHour, std::vector<AITaskCandidate>& candidates);

    bool TryStartTaskCandidate(EntityID entity, const AITaskCandidate& candidate, EntityManager& em, const WorldMap& map,
                               const TileRegistry& tileReg, const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                               const EntitySpatialGrid& spatialGrid);

    bool TryStartFallbackTask(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                              const EntitySpatialGrid& spatialGrid);

    bool SelectAndStartBestTask(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg, const EntitySpatialGrid& spatialGrid,
                                float currentHour);

    void CancelCurrentTask(EntityID entity, EntityManager& em);

    bool TryStartFleeFromThreat(EntityID entity, EntityID threat, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg);

    bool TryStartDefendAgainstThreat(EntityID entity, EntityID threat, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg);

    // --- Jobs ---
    bool TryFindHuntJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                        const EntitySpatialGrid& spatialGrid);

    bool TryFindBuildJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                         const EntitySpatialGrid& spatialGrid);

    bool TryFindDismantleJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                             const EntitySpatialGrid& spatialGrid);

    bool TryFindWanderJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg);

    bool TryFindHarvestJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                           const EntitySpatialGrid& spatialGrid);

    bool TryFindStoreJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                         const EntitySpatialGrid& spatialGrid);

    bool TryFindSeekFoodJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                            const ResourceRegistry& resourceReg, const EntitySpatialGrid& spatialGrid);

    bool TryFindRestJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                        const EntitySpatialGrid& spatialGrid);

    bool TryFindCareChildFoodJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                 const ResourceRegistry& resourceReg);

    bool TryFindReturnToVillageCoreJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg);

    bool TryFindRepairJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                          const EntitySpatialGrid& spatialGrid);

    bool TryFindGuardJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                         const EntitySpatialGrid& spatialGrid);

    bool TryFindPatrolJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg);

    bool TryFindHaulJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                        const ResourceRegistry& resourceReg, const EntitySpatialGrid& spatialGrid);

    bool TryFindRequestWeaponJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                 const EntitySpatialGrid& spatialGrid);

    bool TryFindEquipWeaponJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                               const WeaponRegistry& weaponReg, const EntitySpatialGrid& spatialGrid);

    bool TryFindFulfillWeaponRequestJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                        const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                        const EntitySpatialGrid& spatialGrid);

    bool TryFindSocializeJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                             const EntitySpatialGrid& spatialGrid);

    bool TryFindAvoidPersonJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg);

    bool TryFindConfrontPersonJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                  const EntitySpatialGrid& spatialGrid);

    bool TryFindIntimidateJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                              const EntitySpatialGrid& spatialGrid);

    bool TryFindFightNonLethalJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                  const EntitySpatialGrid& spatialGrid);

    bool TryFindMurderJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                          const EntitySpatialGrid& spatialGrid);

    bool TryFindPrayJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                        const EntitySpatialGrid& spatialGrid);

    bool TryFindPreachJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                          const EntitySpatialGrid& spatialGrid);

    bool TryFindHoldRitualJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                              const EntitySpatialGrid& spatialGrid);

    bool TryFindComfortFrightenedJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                     const EntitySpatialGrid& spatialGrid);
};
