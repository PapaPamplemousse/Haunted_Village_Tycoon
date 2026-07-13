/**
 * @file AISystem.hpp
 * @brief Main AI system responsible for entity state machines and task delegation.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "data/ResourceRegistry.hpp"
#include "data/TileRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "systems/RoomSystem.hpp"
#include "world/EntitySpatialGrid.hpp"
#include "world/WorldMap.hpp"

#include <raylib.h>
#include <string>

class AISystem {
public:
    AISystem() = default;

    void Update(float deltaTime, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg, const ResourceRegistry& resourceReg,
                const EntitySpatialGrid& spatialGrid, const Vector2& simulationCenter, float activeRadiusTiles, float currentHour,
                RoomSystem& roomSys);

private:
    // --- State Handlers ---
    void HandleIdleState(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                         const ResourceRegistry& resourceReg, const EntitySpatialGrid& spatialGrid, float currentHour);

    void HandleMovingState(EntityID entity, float deltaTime, EntityManager& em);

    void HandleTaskCompletion(EntityID entity, EntityManager& em, const ResourceRegistry& resourceReg, const EntitySpatialGrid& spatialGrid,
                              RoomSystem& roomSys);

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
                                 const EntitySpatialGrid& spatialGrid);

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
};
