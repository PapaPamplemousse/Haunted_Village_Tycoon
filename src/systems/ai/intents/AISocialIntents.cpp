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

Vector2 NormalizeSafe(Vector2 value) {
    const float len = std::sqrt(value.x * value.x + value.y * value.y);

    if (len <= 0.001f) {
        return {1.0f, 0.0f};
    }

    return {value.x / len, value.y / len};
}

Vector2 Rotate(Vector2 value, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);

    return {value.x * c - value.y * s, value.x * s + value.y * c};
}

EntityID FindWorstFearTarget(EntityID entity, const EntityManager& em) {
    if (!IsValidHuman(em, entity)) {
        return static_cast<EntityID>(-1);
    }

    EntityID best = static_cast<EntityID>(-1);
    float bestScore = 0.0f;

    for (const RelationshipEntry& relationship : em.socials[entity].relationships) {
        const EntityID other = relationship.otherId;

        if (!IsValidHuman(em, other) || !AreSameVillage(em, entity, other)) {
            continue;
        }

        const float score = relationship.fear + relationship.resentment * 0.25f;

        if (score >= 45.0f && score > bestScore) {
            bestScore = score;
            best = other;
        }
    }

    return best;
}

EntityID FindBestConfrontTarget(EntityID entity, const EntityManager& em, const EntitySpatialGrid& spatialGrid) {
    if (!IsValidHuman(em, entity)) {
        return static_cast<EntityID>(-1);
    }

    const float radius = Config::SOCIAL_RADIUS_TILES * Config::TILE_SIZE;

    const std::vector<EntityID> nearby = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, radius, em);

    EntityID bestTarget = static_cast<EntityID>(-1);
    float bestScore = -std::numeric_limits<float>::infinity();

    for (EntityID candidate : nearby) {
        if (candidate == entity || !IsValidHuman(em, candidate) || !AreSameVillage(em, entity, candidate)) {
            continue;
        }

        const RelationshipEntry* relationship = FindRelationship(em, entity, candidate);

        if (relationship == nullptr) {
            continue;
        }

        if (relationship->resentment < 35.0f) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        const float distancePenalty = std::sqrt(distanceSq) / Config::TILE_SIZE;
        const float score = relationship->resentment - relationship->friendship * 0.4f - distancePenalty;

        if (score > bestScore) {
            bestScore = score;
            bestTarget = candidate;
        }
    }

    return bestTarget;
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

std::optional<AIIntent> FindAvoidPersonIntent(EntityID entity, EntityManager& em, const WorldMap&, const TileRegistry&,
                                              const ResourceRegistry&, const WeaponRegistry&, const EntitySpatialGrid&) {
    if (!IsValidHuman(em, entity)) {
        return std::nullopt;
    }

    const EntityID fearedTarget = FindWorstFearTarget(entity, em);

    if (fearedTarget == static_cast<EntityID>(-1) || !em.hasTransform[fearedTarget]) {
        return std::nullopt;
    }

    const Vector2 selfPos = em.transforms[entity].position;
    const Vector2 threatPos = em.transforms[fearedTarget].position;

    Vector2 away = {selfPos.x - threatPos.x, selfPos.y - threatPos.y};

    away = NormalizeSafe(away);

    constexpr int MAX_ATTEMPTS = 8;
    constexpr float AVOID_DISTANCE_TILES = 8.0f;

    const float distance = AVOID_DISTANCE_TILES * Config::TILE_SIZE;

    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        const float angleOffset = static_cast<float>(attempt - MAX_ATTEMPTS / 2) * 0.35f;
        const Vector2 direction = Rotate(away, angleOffset);

        AIIntent intent;
        intent.kind = AIIntentKind::MoveToPosition;
        intent.targetEntity = fearedTarget;
        intent.targetPosition = {selfPos.x + direction.x * distance, selfPos.y + direction.y * distance};
        intent.moveTask = "moving_to_avoid";

        return intent;
    }

    return std::nullopt;
}

std::optional<AIIntent> FindConfrontPersonIntent(EntityID entity, EntityManager& em, const WorldMap&, const TileRegistry&,
                                                 const ResourceRegistry&, const WeaponRegistry&, const EntitySpatialGrid& spatialGrid) {
    const EntityID target = FindBestConfrontTarget(entity, em, spatialGrid);

    if (target == static_cast<EntityID>(-1)) {
        return std::nullopt;
    }

    AIIntent intent;
    intent.kind = AIIntentKind::MoveAdjacentToEntity;
    intent.targetEntity = target;
    intent.moveTask = "moving_to_confront";
    intent.actionTask = "confronting_person";
    intent.actionDuration = 1.2f;

    return intent;
}

} // namespace ai::intents
