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
                                 const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                 const EntitySpatialGrid& spatialGrid);

    void BuildTaskCandidates(EntityID entity, EntityManager& em, const ResourceRegistry& resourceReg, const EntitySpatialGrid& spatialGrid,
                             float currentHour, std::vector<AITaskCandidate>& candidates);

    bool TryStartTaskCandidate(EntityID entity, const AITaskCandidate& candidate, EntityManager& em, const WorldMap& map,
                               const TileRegistry& tileReg, const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                               const EntitySpatialGrid& spatialGrid);

    bool TryStartFallbackTask(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                              const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg, const EntitySpatialGrid& spatialGrid);

    bool SelectAndStartBestTask(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg, const EntitySpatialGrid& spatialGrid,
                                float currentHour);

    void CancelCurrentTask(EntityID entity, EntityManager& em);
};
