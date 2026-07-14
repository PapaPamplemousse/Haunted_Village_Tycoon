/**
 * @file AIReligionIntents.cpp
 * @brief Religion AI intents.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Config.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/ai/AIIntentFinders.hpp"

#include <limits>
#include <math.h>

namespace {

constexpr float RELIGION_RADIUS_TILES = 18.0f;

bool IsValidHuman(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasTag[entity] && em.hasTransform[entity] &&
           em.tags[entity].species == "human";
}

bool IsOldFaithShrine(EntityID entity, const EntityManager& em) {
    return entity < em.active.size() && em.active[entity] && em.hasTag[entity] && em.hasTransform[entity] &&
           em.tags[entity].prefabId == "OLD_FAITH_SHRINE" && !(em.hasBlueprint[entity] && !em.blueprints[entity].isFinished);
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

EntityID FindNearestShrine(EntityID entity, const EntityManager& em, const EntitySpatialGrid& spatialGrid) {
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

bool IsPriest(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasProfession[entity] &&
           em.professions[entity].currentProfession == "priest";
}

bool AreSameVillage(const EntityManager& em, EntityID a, EntityID b) {
    return a < em.active.size() && b < em.active.size() && em.active[a] && em.active[b] && em.hasVillageMember[a] &&
           em.hasVillageMember[b] && em.villageMembers[a].villageId == em.villageMembers[b].villageId;
}

EntityID FindPreachTarget(EntityID priest, EntityManager& em, const EntitySpatialGrid& spatialGrid) {
    constexpr float PREACH_RADIUS_TILES = 10.0f;

    const float radius = PREACH_RADIUS_TILES * Config::TILE_SIZE;

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[priest].position, radius, em);

    EntityID best = static_cast<EntityID>(-1);
    float bestScore = -std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (candidate == priest || !IsValidHuman(em, candidate) || !AreSameVillage(em, priest, candidate) || !em.hasFaction[candidate]) {
            continue;
        }

        const FactionComponent& faction = em.factions[candidate];

        if (faction.factionId == "CULT_OF_THE_HOLLOW") {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[priest].position, em.transforms[candidate].position);

        const float distancePenalty = std::sqrt(distanceSq) / Config::TILE_SIZE;

        float score = 100.0f - faction.conviction - distancePenalty;

        if (faction.factionId == "COMMON_FOLK") {
            score += 80.0f;
        } else if (faction.factionId == "OLD_FAITH") {
            score += 25.0f;
        }

        if (score > bestScore) {
            bestScore = score;
            best = candidate;
        }
    }

    return best;
}

EntityID FindFrightenedTarget(EntityID priest, EntityManager& em, const EntitySpatialGrid& spatialGrid) {
    constexpr float COMFORT_RADIUS_TILES = 14.0f;

    const float radius = COMFORT_RADIUS_TILES * Config::TILE_SIZE;

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[priest].position, radius, em);

    EntityID best = static_cast<EntityID>(-1);
    float bestFear = 0.0f;

    for (EntityID candidate : candidates) {
        if (candidate == priest || !IsValidHuman(em, candidate) || !AreSameVillage(em, priest, candidate)) {
            continue;
        }

        const float fear = ComputeFearPressure(em, candidate);

        if (fear >= 35.0f && fear > bestFear) {
            bestFear = fear;
            best = candidate;
        }
    }

    return best;
}

} // namespace

namespace ai::intents {

std::optional<AIIntent> FindPrayIntent(EntityID entity, EntityManager& em, const WorldMap&, const TileRegistry&, const ResourceRegistry&,
                                       const WeaponRegistry&, const EntitySpatialGrid& spatialGrid) {
    if (!IsValidHuman(em, entity) || !em.hasFaction[entity]) {
        return std::nullopt;
    }

    const bool canPray = em.factions[entity].factionId == "OLD_FAITH" ||
                         (em.factions[entity].factionId == "COMMON_FOLK" && ComputeFearPressure(em, entity) >= 40.0f);

    if (!canPray) {
        return std::nullopt;
    }

    const EntityID shrine = FindNearestShrine(entity, em, spatialGrid);

    if (shrine == static_cast<EntityID>(-1)) {
        return std::nullopt;
    }

    AIIntent intent;
    intent.kind = AIIntentKind::MoveAdjacentToEntity;
    intent.targetEntity = shrine;
    intent.moveTask = "moving_to_pray";
    intent.actionTask = "praying";
    intent.actionDuration = 3.0f;

    return intent;
}

std::optional<AIIntent> FindPreachIntent(EntityID entity, EntityManager& em, const WorldMap&, const TileRegistry&, const ResourceRegistry&,
                                         const WeaponRegistry&, const EntitySpatialGrid& spatialGrid) {
    if (!IsValidHuman(em, entity) || !IsPriest(em, entity)) {
        return std::nullopt;
    }

    const EntityID target = FindPreachTarget(entity, em, spatialGrid);

    if (target == static_cast<EntityID>(-1)) {
        return std::nullopt;
    }

    AIIntent intent;
    intent.kind = AIIntentKind::MoveAdjacentToEntity;
    intent.targetEntity = target;
    intent.moveTask = "moving_to_preach";
    intent.actionTask = "preaching";
    intent.actionDuration = 2.5f;

    return intent;
}

std::optional<AIIntent> FindHoldRitualIntent(EntityID entity, EntityManager& em, const WorldMap&, const TileRegistry&,
                                             const ResourceRegistry&, const WeaponRegistry&, const EntitySpatialGrid& spatialGrid) {
    if (!IsValidHuman(em, entity) || !IsPriest(em, entity)) {
        return std::nullopt;
    }

    const EntityID shrine = FindNearestShrine(entity, em, spatialGrid);

    if (shrine == static_cast<EntityID>(-1)) {
        return std::nullopt;
    }

    AIIntent intent;
    intent.kind = AIIntentKind::MoveAdjacentToEntity;
    intent.targetEntity = shrine;
    intent.moveTask = "moving_to_ritual";
    intent.actionTask = "holding_ritual";
    intent.actionDuration = 5.0f;

    return intent;
}

std::optional<AIIntent> FindComfortFrightenedIntent(EntityID entity, EntityManager& em, const WorldMap&, const TileRegistry&,
                                                    const ResourceRegistry&, const WeaponRegistry&, const EntitySpatialGrid& spatialGrid) {
    if (!IsValidHuman(em, entity) || !IsPriest(em, entity)) {
        return std::nullopt;
    }

    const EntityID target = FindFrightenedTarget(entity, em, spatialGrid);

    if (target == static_cast<EntityID>(-1)) {
        return std::nullopt;
    }

    AIIntent intent;
    intent.kind = AIIntentKind::MoveAdjacentToEntity;
    intent.targetEntity = target;
    intent.moveTask = "moving_to_comfort";
    intent.actionTask = "comforting_frightened";
    intent.actionDuration = 2.0f;

    return intent;
}

} // namespace ai::intents
