/**
 * @file AISystemProfessionJobs.cpp
 * @brief AI jobs for profession-specific village work: repair, guard and patrol.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Config.hpp"
#include "systems/AISystem.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/Pathfinder.hpp"

#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr float REPAIR_AMOUNT_PER_STEP = 25.0f;
constexpr float PATROL_MIN_RADIUS_TILES = 4.0f;
constexpr float PATROL_MAX_RADIUS_TILES = 10.0f;

bool IsValidEntity(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity];
}

bool IsHostileSpecies(const std::string& species) {
    return species == "cannibal" || species == "zombie" || species == "eldritch";
}

bool IsRepairableTarget(EntityID target, const EntityManager& em) {
    if (!IsValidEntity(em, target) || !em.hasHealth[target] || !em.hasTransform[target]) {
        return false;
    }

    if (em.healths[target].current >= em.healths[target].max) {
        return false;
    }

    if (em.hasBehavior[target]) {
        return false;
    }

    if (em.hasBlueprint[target] && !em.blueprints[target].isFinished) {
        return false;
    }

    return em.hasConstruction[target] || em.hasStorage[target] || em.hasRestSpot[target] || em.hasDoor[target] ||
           (em.hasTag[target] && !em.hasHarvestable[target]);
}

Vector2 FindPatrolPointAroundVillage(EntityID entity, const EntityManager& em) {
    const EntityID villageId = em.villageMembers[entity].villageId;
    const Vector2 center = em.transforms[villageId].position;

    const float angle = static_cast<float>(GetRandomValue(0, 360)) * DEG2RAD;

    const float minDist = PATROL_MIN_RADIUS_TILES * Config::TILE_SIZE;
    const float maxDist = PATROL_MAX_RADIUS_TILES * Config::TILE_SIZE;

    const float distance = static_cast<float>(GetRandomValue(static_cast<int>(minDist), static_cast<int>(maxDist)));

    return {center.x + std::cos(angle) * distance, center.y + std::sin(angle) * distance};
}

} // namespace

bool AISystem::TryFindRepairJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                const EntitySpatialGrid& spatialGrid) {
    if (!IsValidEntity(em, entity) || !em.hasTransform[entity] || !em.hasBehavior[entity]) {
        return false;
    }

    const float searchRadius = AISystemUtils::GetActionRadiusWorld(entity, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, searchRadius, em);

    EntityID bestTarget = static_cast<EntityID>(-1);
    float bestScore = -std::numeric_limits<float>::infinity();

    for (EntityID target : candidates) {
        if (!IsRepairableTarget(target, em)) {
            continue;
        }

        const float missingHp = em.healths[target].max - em.healths[target].current;
        const float missingRatio = missingHp / std::max(1.0f, em.healths[target].max);

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[target].position);

        const float distancePenalty = std::sqrt(distanceSq) / Config::TILE_SIZE;

        const float score = missingRatio * 200.0f - distancePenalty;

        if (score > bestScore) {
            bestScore = score;
            bestTarget = target;
        }
    }

    if (bestTarget == static_cast<EntityID>(-1)) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[entity];

    if (AreEntitiesAdjacent(entity, bestTarget, em)) {
        behavior.currentTask = "repairing";
        behavior.currentJobTarget = bestTarget;
        behavior.hasJob = true;
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        behavior.stateTimer = 1.0f;
        return true;
    }

    std::vector<Vector2> path =
        Pathfinder::FindPathToAdjacentTile(em.transforms[entity].position, em.transforms[bestTarget].position, map, tileReg, em, entity);

    if (path.empty()) {
        return false;
    }

    behavior.currentTask = "moving_to_repair";
    behavior.currentJobTarget = bestTarget;
    behavior.hasJob = true;
    behavior.currentPath = std::move(path);
    behavior.currentPathIndex = 0;
    behavior.currentTarget = behavior.currentPath[0];
    behavior.isMoving = true;
    behavior.stateTimer = 0.0f;

    return true;
}

bool AISystem::TryFindGuardJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                               const EntitySpatialGrid& spatialGrid) {
    if (!IsValidEntity(em, entity) || !em.hasTransform[entity] || !em.hasBehavior[entity]) {
        return false;
    }

    const float searchRadius = AISystemUtils::GetActionRadiusWorld(entity, em);
    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, searchRadius, em);

    EntityID bestTarget = static_cast<EntityID>(-1);
    float bestDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID target : candidates) {
        if (target == entity || !IsValidEntity(em, target) || !em.hasTag[target] || !em.hasTransform[target] || !em.hasHealth[target]) {
            continue;
        }

        if (!IsHostileSpecies(em.tags[target].species)) {
            continue;
        }

        if (em.healths[target].current <= 0.0f) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[target].position);

        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestTarget = target;
        }
    }

    if (bestTarget == static_cast<EntityID>(-1)) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[entity];

    if (AreEntitiesAdjacent(entity, bestTarget, em)) {
        behavior.currentTask = "attacking";
        behavior.currentJobTarget = bestTarget;
        behavior.hasJob = true;
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        behavior.stateTimer = AISystemUtils::ATTACK_DURATION;
        return true;
    }

    std::vector<Vector2> path =
        Pathfinder::FindPathToAdjacentTile(em.transforms[entity].position, em.transforms[bestTarget].position, map, tileReg, em, entity);

    if (path.empty()) {
        return false;
    }

    behavior.currentTask = "moving_to_hunt";
    behavior.currentJobTarget = bestTarget;
    behavior.hasJob = true;
    behavior.currentPath = std::move(path);
    behavior.currentPathIndex = 0;
    behavior.currentTarget = behavior.currentPath[0];
    behavior.isMoving = true;

    return true;
}

bool AISystem::TryFindPatrolJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg) {
    if (!IsValidEntity(em, entity) || !em.hasBehavior[entity] || !em.hasTransform[entity] || !em.hasVillageMember[entity]) {
        return false;
    }

    const EntityID villageId = em.villageMembers[entity].villageId;

    if (!IsValidEntity(em, villageId) || !em.hasVillage[villageId] || !em.hasTransform[villageId]) {
        return false;
    }

    constexpr int MAX_ATTEMPTS = 8;

    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        const Vector2 target = FindPatrolPointAroundVillage(entity, em);

        std::vector<Vector2> path = Pathfinder::FindPath(em.transforms[entity].position, target, map, tileReg, em, entity);

        if (path.empty()) {
            continue;
        }

        BehaviorComponent& behavior = em.behaviors[entity];

        behavior.currentTask = "patrolling";
        behavior.currentJobTarget = villageId;
        behavior.hasJob = true;
        behavior.currentPath = std::move(path);
        behavior.currentPathIndex = 0;
        behavior.currentTarget = behavior.currentPath[0];
        behavior.isMoving = true;

        return true;
    }

    return false;
}
