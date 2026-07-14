/**
 * @file AISystemProfessionJobs.cpp
 * @brief AI jobs for profession-specific village work: repair, guard and patrol.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Config.hpp"
#include "systems/AISystem.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/Pathfinder.hpp"
#include "systems/ai/AITaskExecutor.hpp"

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

    return AITaskExecutor::StartMoveAdjacentToEntity(entity, bestTarget, em, map, tileReg, "moving_to_repair", "repairing", 1.0f);
}

bool AISystem::TryFindGuardJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                               const EntitySpatialGrid& spatialGrid) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasTransform[entity] || !em.hasBehavior[entity] ||
        !em.hasVillageMember[entity]) {
        return false;
    }

    const EntityID villageId = em.villageMembers[entity].villageId;

    if (villageId >= em.active.size() || !em.active[villageId] || !em.hasTransform[villageId]) {
        return false;
    }

    constexpr float GUARD_RESPONSE_RADIUS_TILES = 90.0f;
    constexpr float THREAT_TO_VILLAGER_RADIUS_TILES = 12.0f;

    const float villageSearchRadius = GUARD_RESPONSE_RADIUS_TILES * Config::TILE_SIZE;

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[villageId].position, villageSearchRadius, em);

    EntityID bestThreat = static_cast<EntityID>(-1);
    float bestScore = -std::numeric_limits<float>::infinity();

    for (EntityID threat : candidates) {
        if (threat == entity || threat >= em.active.size() || !em.active[threat] || !em.hasTag[threat] || !em.hasTransform[threat] ||
            !em.hasHealth[threat]) {
            continue;
        }

        const std::string& species = em.tags[threat].species;

        const bool hostile = species == "cannibal" || species == "zombie" || species == "eldritch";

        if (!hostile) {
            continue;
        }

        if (em.healths[threat].current <= 0.0f) {
            continue;
        }

        float threatenedVillagerBonus = 0.0f;

        for (EntityID villager = 0; villager < em.active.size(); ++villager) {
            if (!em.active[villager] || !em.hasVillageMember[villager] || !em.hasTransform[villager] || !em.hasTag[villager]) {
                continue;
            }

            if (em.villageMembers[villager].villageId != villageId) {
                continue;
            }

            if (em.tags[villager].species != "human") {
                continue;
            }

            const float threatToVillagerSq =
                AISystemUtils::SquaredDistance(em.transforms[threat].position, em.transforms[villager].position);

            const float threatToVillagerTiles = std::sqrt(threatToVillagerSq) / Config::TILE_SIZE;

            if (threatToVillagerTiles <= THREAT_TO_VILLAGER_RADIUS_TILES) {
                threatenedVillagerBonus = std::max(threatenedVillagerBonus, 200.0f - threatToVillagerTiles * 10.0f);
            }
        }

        const float guardToThreatSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[threat].position);

        const float villageToThreatSq = AISystemUtils::SquaredDistance(em.transforms[villageId].position, em.transforms[threat].position);

        const float guardDistancePenalty = std::sqrt(guardToThreatSq) / Config::TILE_SIZE;

        const float villageDistancePenalty = std::sqrt(villageToThreatSq) / Config::TILE_SIZE * 0.5f;

        const float score = 300.0f + threatenedVillagerBonus - guardDistancePenalty - villageDistancePenalty;

        if (score > bestScore) {
            bestScore = score;
            bestThreat = threat;
        }
    }

    if (bestThreat == static_cast<EntityID>(-1)) {
        return false;
    }

    return AITaskExecutor::StartMoveAdjacentToEntity(entity, bestThreat, em, map, tileReg, "moving_to_hunt", "attacking",
                                                     AISystemUtils::ATTACK_DURATION);
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
