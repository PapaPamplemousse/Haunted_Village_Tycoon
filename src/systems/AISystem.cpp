/**
 * @file AISystem.cpp
 * @brief Implementation of core AI updates and state management.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/AISystem.hpp"

#include "systems/AISystemUtils.hpp"

#include <algorithm>

void AISystem::Update(float deltaTime, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                      const ResourceRegistry& resourceReg, const EntitySpatialGrid& spatialGrid, const Vector2& simulationCenter,
                      float activeRadiusTiles, float currentHour, RoomSystem& roomSys) {
    UpdateAIContextTimers(deltaTime, em);

    for (size_t i = 0; i < em.active.size(); ++i) {
        if (!em.active[i]) {
            continue;
        }

        if (!em.hasBehavior[i] || !em.hasTransform[i] || !em.hasStats[i]) {
            continue;
        }

        if (!AISystemUtils::IsWithinActiveSimulationRadius(em.transforms[i].position, simulationCenter, activeRadiusTiles)) {
            continue;
        }

        auto& behavior = em.behaviors[i];

        if (TryInterruptCurrentTask(i, em, map, tileReg, resourceReg, spatialGrid)) {
            continue;
        }

        if (behavior.stateTimer > 0.0f) {
            behavior.stateTimer -= deltaTime;
            continue;
        }

        if (behavior.currentTask == "idle") {
            HandleIdleState(i, em, map, tileReg, resourceReg, spatialGrid, currentHour);
        } else if (behavior.isMoving) {
            HandleMovingState(i, deltaTime, em);
        } else {
            HandleTaskCompletion(i, em, resourceReg, spatialGrid, roomSys);
        }
    }
}

void AISystem::HandleIdleState(EntityID i, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                               const ResourceRegistry& resourceReg, const EntitySpatialGrid& spatialGrid, float currentHour) {
    if (SelectAndStartBestTask(i, em, map, tileReg, resourceReg, spatialGrid, currentHour)) {
        return;
    }

    if (em.hasBehavior[i]) {
        em.behaviors[i].stateTimer = 1.0f;
    }
}

bool AISystem::HasCapability(const BehaviorComponent& behavior, const std::string& capability) const {
    return std::find(behavior.innateCapabilities.begin(), behavior.innateCapabilities.end(), capability) !=
           behavior.innateCapabilities.end();
}

const BehaviorRule* AISystem::FindBehaviorRule(const BehaviorComponent& behavior, const std::string& ruleName) const {
    for (const BehaviorRule& rule : behavior.innateBehaviorRules) {
        if (rule.name == ruleName) {
            return &rule;
        }
    }

    return nullptr;
}

bool AISystem::IsSpeciesTargetedByRule(const BehaviorRule& rule, const std::string& species) const {
    for (const std::string& targetSpecies : rule.arguments) {
        if (targetSpecies == species) {
            return true;
        }
    }

    return false;
}

bool AISystem::AreEntitiesAdjacent(EntityID a, EntityID b, const EntityManager& em) const {
    if (a >= em.active.size() || b >= em.active.size() || !em.active[a] || !em.active[b] || !em.hasTransform[a] || !em.hasTransform[b]) {
        return false;
    }

    const int ax = AISystemUtils::ToTileCoord(em.transforms[a].position.x);
    const int ay = AISystemUtils::ToTileCoord(em.transforms[a].position.y);

    const int bx = AISystemUtils::ToTileCoord(em.transforms[b].position.x);
    const int by = AISystemUtils::ToTileCoord(em.transforms[b].position.y);

    const int dx = std::abs(ax - bx);
    const int dy = std::abs(ay - by);

    return std::max(dx, dy) <= 1;
}

void AISystem::ResetBehaviorState(BehaviorComponent& behavior) {
    behavior.currentTask = "idle";
    behavior.hasJob = false;
    behavior.currentJobTarget = 0;
    behavior.currentItemTarget.clear();
    behavior.currentPath.clear();
    behavior.currentPathIndex = 0;
    behavior.isMoving = false;
    behavior.reservedRestSpot = static_cast<EntityID>(-1);
}
