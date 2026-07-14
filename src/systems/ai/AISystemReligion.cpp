/**
 * @file AISystemReligion.cpp
 * @brief AI routines for religion-related village actions.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Config.hpp"
#include "systems/AISystem.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/Pathfinder.hpp"
#include "systems/ai/AITaskExecutor.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr float RELIGION_RADIUS_TILES = 18.0f;
constexpr float PREACH_RADIUS_TILES = 10.0f;
constexpr float COMFORT_RADIUS_TILES = 14.0f;

bool IsValidHuman(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasTag[entity] && em.hasTransform[entity] &&
           em.tags[entity].species == "human";
}

bool IsPriest(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasProfession[entity] &&
           em.professions[entity].currentProfession == "priest";
}

bool AreSameVillage(const EntityManager& em, EntityID a, EntityID b) {
    if (a >= em.active.size() || b >= em.active.size() || !em.active[a] || !em.active[b] || !em.hasVillageMember[a] ||
        !em.hasVillageMember[b]) {
        return false;
    }

    return em.villageMembers[a].villageId == em.villageMembers[b].villageId;
}

bool IsOldFaithFollower(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasFaction[entity] && em.factions[entity].factionId == "OLD_FAITH";
}

bool IsCultFollower(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasFaction[entity] && em.factions[entity].factionId == "CULT_OF_THE_HOLLOW";
}

bool IsOldFaithShrine(EntityID entity, const EntityManager& em) {
    return entity < em.active.size() && em.active[entity] && em.hasTag[entity] && em.hasTransform[entity] &&
           em.tags[entity].prefabId == "OLD_FAITH_SHRINE" && !(em.hasBlueprint[entity] && !em.blueprints[entity].isFinished);
}

EntityID FindNearestShrine(EntityID entity, const EntityManager& em, const EntitySpatialGrid& spatialGrid) {
    if (!IsValidHuman(em, entity)) {
        return static_cast<EntityID>(-1);
    }

    const float radius = RELIGION_RADIUS_TILES * Config::TILE_SIZE;

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, radius, em);

    EntityID best = static_cast<EntityID>(-1);
    float bestDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (!IsOldFaithShrine(candidate, em)) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            best = candidate;
        }
    }

    return best;
}

RelationshipEntry* FindRelationship(EntityManager& em, EntityID owner, EntityID other) {
    if (owner >= em.active.size() || !em.active[owner] || !em.hasSocial[owner]) {
        return nullptr;
    }

    for (RelationshipEntry& relationship : em.socials[owner].relationships) {
        if (relationship.otherId == other) {
            return &relationship;
        }
    }

    return nullptr;
}

float ComputeFearPressure(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasSocial[entity]) {
        return 0.0f;
    }

    float maxFear = 0.0f;

    for (const RelationshipEntry& relationship : em.socials[entity].relationships) {
        maxFear = std::max(maxFear, relationship.fear);
    }

    return maxFear;
}

EntityID FindPreachTarget(EntityID priest, EntityManager& em, const EntitySpatialGrid& spatialGrid) {
    const float radius = PREACH_RADIUS_TILES * Config::TILE_SIZE;

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[priest].position, radius, em);

    EntityID best = static_cast<EntityID>(-1);
    float bestScore = -std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (candidate == priest || !IsValidHuman(em, candidate) || !AreSameVillage(em, priest, candidate)) {
            continue;
        }

        if (!em.hasFaction[candidate]) {
            continue;
        }

        const FactionComponent& faction = em.factions[candidate];

        // Do not preach aggressively to cultists in V1.
        if (faction.factionId == "CULT_OF_THE_HOLLOW") {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[priest].position, em.transforms[candidate].position);

        const float distancePenalty = std::sqrt(distanceSq) / Config::TILE_SIZE;

        // Prefer common folk and low conviction villagers.
        float score = 0.0f;

        if (faction.factionId == "COMMON_FOLK") {
            score += 80.0f;
        }

        if (faction.factionId == "OLD_FAITH") {
            score += 25.0f;
        }

        score += 100.0f - faction.conviction;
        score -= distancePenalty;

        if (score > bestScore) {
            bestScore = score;
            best = candidate;
        }
    }

    return best;
}

EntityID FindFrightenedTarget(EntityID priest, EntityManager& em, const EntitySpatialGrid& spatialGrid) {
    const float radius = COMFORT_RADIUS_TILES * Config::TILE_SIZE;

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[priest].position, radius, em);

    EntityID best = static_cast<EntityID>(-1);
    float bestFear = 0.0f;

    for (EntityID candidate : candidates) {
        if (candidate == priest || !IsValidHuman(em, candidate) || !AreSameVillage(em, priest, candidate)) {
            continue;
        }

        const float fear = ComputeFearPressure(em, candidate);

        if (fear < 35.0f) {
            continue;
        }

        if (fear > bestFear) {
            bestFear = fear;
            best = candidate;
        }
    }

    return best;
}

} // namespace

bool AISystem::TryFindPrayJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                              const EntitySpatialGrid& spatialGrid) {
    if (!IsValidHuman(em, entity) || !em.hasBehavior[entity] || !em.hasFaction[entity]) {
        return false;
    }

    // V1: Old Faith followers pray. Common folk may pray if fear pressure is high.
    const bool canPray = em.factions[entity].factionId == "OLD_FAITH" ||
                         (em.factions[entity].factionId == "COMMON_FOLK" && ComputeFearPressure(em, entity) >= 40.0f);

    if (!canPray) {
        return false;
    }

    const EntityID shrine = FindNearestShrine(entity, em, spatialGrid);

    if (shrine == static_cast<EntityID>(-1)) {
        return false;
    }

    return AITaskExecutor::StartMoveAdjacentToEntity(entity, shrine, em, map, tileReg, "moving_to_pray", "praying", 3.0f);
}

bool AISystem::TryFindPreachJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                const EntitySpatialGrid& spatialGrid) {
    if (!IsValidHuman(em, entity) || !IsPriest(em, entity) || !em.hasBehavior[entity]) {
        return false;
    }

    const EntityID target = FindPreachTarget(entity, em, spatialGrid);

    if (target == static_cast<EntityID>(-1)) {
        return false;
    }

    return AITaskExecutor::StartMoveAdjacentToEntity(entity, target, em, map, tileReg, "moving_to_preach", "preaching", 2.5f);
}

bool AISystem::TryFindHoldRitualJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                    const EntitySpatialGrid& spatialGrid) {
    if (!IsValidHuman(em, entity) || !IsPriest(em, entity) || !em.hasBehavior[entity]) {
        return false;
    }

    const EntityID shrine = FindNearestShrine(entity, em, spatialGrid);

    if (shrine == static_cast<EntityID>(-1)) {
        return false;
    }

    return AITaskExecutor::StartMoveAdjacentToEntity(entity, shrine, em, map, tileReg, "moving_to_ritual", "holding_ritual", 5.0f);
}

bool AISystem::TryFindComfortFrightenedJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                           const EntitySpatialGrid& spatialGrid) {
    if (!IsValidHuman(em, entity) || !IsPriest(em, entity) || !em.hasBehavior[entity]) {
        return false;
    }

    const EntityID target = FindFrightenedTarget(entity, em, spatialGrid);

    if (target == static_cast<EntityID>(-1)) {
        return false;
    }

    return AITaskExecutor::StartMoveAdjacentToEntity(entity, target, em, map, tileReg, "moving_to_comfort", "comforting_frightened", 2.0f);
}
