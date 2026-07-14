/**
 * @file AIReligionIntents.cpp
 * @brief Religion AI intents.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Config.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/ai/AIIntentFinders.hpp"

#include <limits>

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

} // namespace ai::intents
