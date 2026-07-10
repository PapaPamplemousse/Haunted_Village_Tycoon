#pragma once

#include "data/ResourceRegistry.hpp"
#include "data/TileRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "systems/RoomSystem.hpp"
#include "world/WorldMap.hpp"

#include <string>

/**
 * @class AISystem
 * @brief Processes AI decision making, job searching, and movement using a State Machine approach.
 */
class AISystem {
public:
    AISystem() = default;

    void Update(float deltaTime, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg, const ResourceRegistry& resourceReg,
                RoomSystem& roomSys);

private:
    // --- State Handlers ---
    void HandleIdleState(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                         const ResourceRegistry& resourceReg);

    void HandleMovingState(EntityID entity, float deltaTime, EntityManager& em);

    void HandleTaskCompletion(EntityID entity, EntityManager& em, const ResourceRegistry& resourceReg, RoomSystem& roomSys);

    // --- Helper Functions ---
    void InteractWithDoorIfPresent(EntityID entity, int targetX, int targetY, EntityManager& em);

    bool HasCapability(const BehaviorComponent& behavior, const std::string& capability) const;

    const BehaviorRule* FindBehaviorRule(const BehaviorComponent& behavior, const std::string& ruleName) const;

    bool IsSpeciesTargetedByRule(const BehaviorRule& rule, const std::string& species) const;

    bool AreEntitiesAdjacent(EntityID a, EntityID b, const EntityManager& em) const;

    void ResetBehaviorState(BehaviorComponent& behavior);

    // --- Job Searchers ---
    bool TryFindHuntJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg);

    bool TryFindBuildJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg);

    bool TryFindDismantleJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg);

    bool TryFindWanderJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg);

    bool TryFindHarvestJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg);

    bool TryFindSeekFoodJob(EntityID entity, EntityManager& em, const ResourceRegistry& resourceReg);
};
