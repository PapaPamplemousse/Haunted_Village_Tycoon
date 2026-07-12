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
    auto& behavior = em.behaviors[i];

    const bool canHunt = HasCapability(behavior, "hunt");
    const bool canHarvest = HasCapability(behavior, "harvest");
    const bool canBuild = HasCapability(behavior, "build");
    const bool canDismantle = HasCapability(behavior, "dismantle");
    const bool canWander = HasCapability(behavior, "wander");
    const bool canSeekFood = HasCapability(behavior, "seek_food");
    const bool canStore = HasCapability(behavior, "store");
    const bool canRest = HasCapability(behavior, "rest");

    const bool canStartWork = AISystemUtils::CanStartWorkNow(behavior, currentHour);

    // Survival has priority and is always allowed.
    if (canSeekFood && TryFindSeekFoodJob(i, em, map, tileReg, resourceReg, spatialGrid)) {
        return;
    }

    // Deposit if threshold reached or work day is over.
    if (canStore && AISystemUtils::ShouldDepositInventory(i, em, behavior, currentHour)) {
        if (TryFindStoreJob(i, em, map, tileReg, spatialGrid)) {
            return;
        }
    }

    if (canRest && AISystemUtils::ShouldRest(i, em, behavior, currentHour)) {
        if (TryFindRestJob(i, em, map, tileReg, spatialGrid)) {
            return;
        }
    }

    // Outside work hours: do not start productive jobs.
    if (!canStartWork) {
        if (canWander && TryFindWanderJob(i, em, map, tileReg)) {
            return;
        }

        behavior.stateTimer = 1.0f;
        return;
    }
    if (canHunt && TryFindHuntJob(i, em, map, tileReg, spatialGrid)) {
        return;
    }

    if (canBuild && TryFindBuildJob(i, em, map, tileReg, spatialGrid)) {
        return;
    }

    if (canDismantle && TryFindDismantleJob(i, em, map, tileReg, spatialGrid)) {
        return;
    }

    if (canHarvest && TryFindHarvestJob(i, em, map, tileReg, spatialGrid)) {
        return;
    }

    if (canWander && TryFindWanderJob(i, em, map, tileReg)) {
        return;
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
