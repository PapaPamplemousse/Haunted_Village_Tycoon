/**
 * @file AISystem.cpp
 * @brief Implementation of core AI updates and state management.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/AISystem.hpp"

#include "core/Config.hpp"
#include "systems/AISystemUtils.hpp"

#include <algorithm>
#include <iostream>

void AISystem::Update(float deltaTime, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                      const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg, const EntitySpatialGrid& spatialGrid,
                      const Vector2& simulationCenter, float activeRadiusTiles, float currentHour, RoomSystem& roomSys) {
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

        if (HandleFatigueCollapse(i, deltaTime, em)) {
            continue;
        }

        if (TryInterruptCurrentTask(i, em, map, tileReg, resourceReg, weaponReg, spatialGrid)) {
            continue;
        }

        if (behavior.stateTimer > 0.0f) {
            behavior.stateTimer -= deltaTime;
            continue;
        }

        if (behavior.currentTask == "idle") {
            HandleIdleState(i, em, map, tileReg, resourceReg, weaponReg, spatialGrid, currentHour);
        } else if (behavior.isMoving) {
            HandleMovingState(i, deltaTime, em, map, tileReg);
        } else {
            HandleTaskCompletion(i, em, map, tileReg, resourceReg, weaponReg, spatialGrid, roomSys);
        }
    }
}
void AISystem::HandleIdleState(EntityID i, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                               const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg, const EntitySpatialGrid& spatialGrid,
                               float currentHour) {
    if (SelectAndStartBestTask(i, em, map, tileReg, resourceReg, weaponReg, spatialGrid, currentHour)) {
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

bool AISystem::HandleFatigueCollapse(EntityID entity, float deltaTime, EntityManager& em) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasNeeds[entity] || !em.hasBehavior[entity]) {
        return false;
    }

    NeedsComponent& needs = em.needs[entity];
    BehaviorComponent& behavior = em.behaviors[entity];

    const bool shouldCollapse = needs.collapsedFromFatigue || (needs.maxFatigue > 0.0f && needs.fatigue >= needs.maxFatigue);

    if (!shouldCollapse) {
        return false;
    }

    if (!needs.collapsedFromFatigue) {
        needs.collapsedFromFatigue = true;

        if (behavior.currentTask == "moving_to_rest" || behavior.currentTask == "resting" ||
            behavior.reservedRestSpot != static_cast<EntityID>(-1)) {
            AISystemUtils::ReleaseRestSpotReservation(entity, em);
        }

        behavior.currentTask = "collapsed_sleep";
        behavior.currentJobTarget = 0;
        behavior.currentItemTarget.clear();
        behavior.hasJob = true;
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        behavior.stateTimer = 0.0f;

        if (em.hasAIContext[entity]) {
            em.aiContexts[entity].currentTaskPriority = 2000.0f;
            em.aiContexts[entity].currentTaskInterruptible = false;
        }

        std::cout << "[AI] Entity #" << entity << " collapsed from fatigue." << std::endl;
    }

    needs.fatigue -= Config::FATIGUE_REST_RECOVERY_PER_SECOND * deltaTime;

    if (needs.fatigue < 0.0f) {
        needs.fatigue = 0.0f;
    }

    if (needs.fatigue <= 0.0f) {
        needs.collapsedFromFatigue = false;

        ResetBehaviorState(behavior);
        behavior.stateTimer = 1.0f;

        if (em.hasAIContext[entity]) {
            em.aiContexts[entity].currentTaskPriority = 0.0f;
            em.aiContexts[entity].currentTaskInterruptible = true;
        }

        std::cout << "[AI] Entity #" << entity << " woke up from fatigue collapse." << std::endl;
    }

    return true;
}
