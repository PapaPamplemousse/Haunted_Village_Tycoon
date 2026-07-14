/**
 * @file AISocialIntents.cpp
 * @brief Social AI intents.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Config.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/ai/AIIntentFinders.hpp"

#include <cmath>
#include <limits>

namespace {

bool IsValidHuman(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasTag[entity] && em.hasTransform[entity] && em.hasVillageMember[entity] &&
           em.hasSocial[entity] && em.tags[entity].species == "human";
}

bool AreSameVillage(const EntityManager& em, EntityID a, EntityID b) {
    return em.hasVillageMember[a] && em.hasVillageMember[b] && em.villageMembers[a].villageId == em.villageMembers[b].villageId;
}

float GetSociability(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasPersonality[entity]) {
        return 0.5f;
    }

    return em.personalities[entity].sociability;
}

const RelationshipEntry* FindRelationship(const EntityManager& em, EntityID owner, EntityID other) {
    if (owner >= em.active.size() || !em.active[owner] || !em.hasSocial[owner]) {
        return nullptr;
    }

    for (const RelationshipEntry& relationship : em.socials[owner].relationships) {
        if (relationship.otherId == other) {
            return &relationship;
        }
    }

    return nullptr;
}

} // namespace

namespace ai::intents {

std::optional<AIIntent> FindSocializeIntent(EntityID entity, EntityManager& em, const WorldMap&, const TileRegistry&,
                                            const ResourceRegistry&, const WeaponRegistry&, const EntitySpatialGrid& spatialGrid) {
    if (!IsValidHuman(em, entity)) {
        return std::nullopt;
    }

    const float sociability = GetSociability(em, entity);
    const float radius = Config::SOCIAL_RADIUS_TILES * Config::TILE_SIZE;

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, radius, em);

    EntityID bestTarget = static_cast<EntityID>(-1);
    float bestScore = -std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (candidate == entity || !IsValidHuman(em, candidate) || !AreSameVillage(em, entity, candidate)) {
            continue;
        }

        const RelationshipEntry* relationship = FindRelationship(em, entity, candidate);

        float friendship = 0.0f;
        float resentment = 0.0f;
        float fear = 0.0f;

        if (relationship != nullptr) {
            friendship = relationship->friendship;
            resentment = relationship->resentment;
            fear = relationship->fear;
        }

        if (resentment >= 60.0f || fear >= 50.0f) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        const float distancePenalty = std::sqrt(distanceSq) / Config::TILE_SIZE;
        const float score = friendship * 0.8f + sociability * 40.0f - distancePenalty;

        if (score > bestScore) {
            bestScore = score;
            bestTarget = candidate;
        }
    }

    if (bestTarget == static_cast<EntityID>(-1)) {
        return std::nullopt;
    }

    AIIntent intent;
    intent.kind = AIIntentKind::MoveAdjacentToEntity;
    intent.targetEntity = bestTarget;
    intent.moveTask = "moving_to_socialize";
    intent.actionTask = "socializing";
    intent.actionDuration = 1.5f;

    return intent;
}

} // namespace ai::intents
