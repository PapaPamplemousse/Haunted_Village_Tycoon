#pragma once
#include "data/TileRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "systems/RoomSystem.hpp"
#include "world/WorldMap.hpp"

/**
 * @class AISystem
 * @brief Processes AI decision making, job searching, and movement using a State Machine approach.
 */
class AISystem {
public:
    AISystem() = default;

    void Update(float deltaTime, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg, RoomSystem& roomSys);

private:
    // --- State Handlers ---
    void HandleIdleState(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg);
    void HandleMovingState(EntityID entity, float deltaTime, EntityManager& em);
    void HandleTaskCompletion(EntityID entity, EntityManager& em, RoomSystem& roomSys);

    // --- Job Searchers ---
    bool TryFindBuildJob(EntityID entity, EntityManager& em);
    bool TryFindDismantleJob(EntityID entity, EntityManager& em);
    bool TryFindWanderJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg);
};
