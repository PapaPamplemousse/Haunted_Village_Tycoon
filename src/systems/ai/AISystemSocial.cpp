/**
 * @file AISystemSocial.cpp
 * @brief AI social actions: socialize, avoid disliked people, and confront hostile relationships.
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

constexpr float SOCIALIZE_RADIUS_TILES = 12.0f;
constexpr float AVOID_DISTANCE_TILES = 8.0f;
constexpr float CONFRONT_MIN_RESENTMENT = 65.0f;
constexpr float CONFRONT_MAX_FRIENDSHIP = 35.0f;
constexpr float AVOID_MIN_FEAR = 55.0f;
constexpr float AVOID_MIN_RESENTMENT = 70.0f;

constexpr int AVOID_PATH_ATTEMPTS = 10;

bool IsValidHuman(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasTag[entity] && em.hasTransform[entity] && em.hasSocial[entity] &&
           em.tags[entity].species == "human";
}

bool AreSameVillage(const EntityManager& em, EntityID a, EntityID b) {
    if (a >= em.active.size() || b >= em.active.size() || !em.active[a] || !em.active[b] || !em.hasVillageMember[a] ||
        !em.hasVillageMember[b]) {
        return false;
    }

    return em.villageMembers[a].villageId == em.villageMembers[b].villageId;
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

RelationshipEntry& GetOrCreateRelationship(EntityManager& em, EntityID owner, EntityID other) {
    SocialComponent& social = em.socials[owner];

    for (RelationshipEntry& relationship : social.relationships) {
        if (relationship.otherId == other) {
            return relationship;
        }
    }

    social.relationships.push_back({other});
    return social.relationships.back();
}

float ClampSocial(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }

    if (value > 100.0f) {
        return 100.0f;
    }

    return value;
}

float GetSociability(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasPersonality[entity]) {
        return 0.5f;
    }

    return em.personalities[entity].sociability;
}

float GetAggression(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasPersonality[entity]) {
        return 0.0f;
    }

    return em.personalities[entity].aggression;
}

float GetKindness(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasPersonality[entity]) {
        return 0.5f;
    }

    return em.personalities[entity].kindness;
}

float GetBravery(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasPersonality[entity]) {
        return 0.5f;
    }

    return em.personalities[entity].bravery;
}

bool ShouldAvoidRelationship(const RelationshipEntry& relationship) {
    return relationship.fear >= AVOID_MIN_FEAR || relationship.resentment >= AVOID_MIN_RESENTMENT;
}

bool ShouldConfrontRelationship(const RelationshipEntry& relationship, const EntityManager& em, EntityID owner) {
    const float aggression = GetAggression(em, owner);
    const float bravery = GetBravery(em, owner);

    if (relationship.resentment < CONFRONT_MIN_RESENTMENT) {
        return false;
    }

    if (relationship.friendship > CONFRONT_MAX_FRIENDSHIP) {
        return false;
    }

    // V1: confrontation requires at least some assertiveness.
    return aggression >= 0.35f || bravery >= 0.65f;
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

} // namespace

bool AISystem::TryFindSocializeJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                   const EntitySpatialGrid& spatialGrid) {
    if (!IsValidHuman(em, entity) || !em.hasBehavior[entity]) {
        return false;
    }

    const float sociability = GetSociability(em, entity);

    // Solitary villagers rarely initiate social interactions.
    if (sociability < 0.35f && GetRandomValue(0, 100) > 20) {
        return false;
    }

    const float radius = SOCIALIZE_RADIUS_TILES * Config::TILE_SIZE;

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, radius, em);

    EntityID bestTarget = static_cast<EntityID>(-1);
    float bestScore = -std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (candidate == entity || !IsValidHuman(em, candidate) || !AreSameVillage(em, entity, candidate)) {
            continue;
        }

        RelationshipEntry* relationship = FindRelationship(em, entity, candidate);

        float friendship = 0.0f;
        float resentment = 0.0f;
        float fear = 0.0f;

        if (relationship != nullptr) {
            friendship = relationship->friendship;
            resentment = relationship->resentment;
            fear = relationship->fear;
        }

        // Do not socialize with people currently feared or hated.
        if (resentment >= 60.0f || fear >= 50.0f) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        const float distancePenalty = std::sqrt(distanceSq) / Config::TILE_SIZE;

        // Prefer existing friends, but also allow neutral social discovery.
        const float score = friendship * 0.8f + sociability * 40.0f - distancePenalty;

        if (score > bestScore) {
            bestScore = score;
            bestTarget = candidate;
        }
    }

    if (bestTarget == static_cast<EntityID>(-1)) {
        return false;
    }

    return AITaskExecutor::StartMoveAdjacentToEntity(entity, bestTarget, em, map, tileReg, "moving_to_socialize", "socializing", 1.5f);
}

bool AISystem::TryFindAvoidPersonJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg) {
    if (!IsValidHuman(em, entity) || !em.hasBehavior[entity]) {
        return false;
    }

    EntityID avoidedTarget = static_cast<EntityID>(-1);
    float highestPressure = -1.0f;

    for (RelationshipEntry& relationship : em.socials[entity].relationships) {
        const EntityID other = relationship.otherId;

        if (!IsValidHuman(em, other) || !AreSameVillage(em, entity, other)) {
            continue;
        }

        if (!ShouldAvoidRelationship(relationship)) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[other].position);

        const float distanceTiles = std::sqrt(distanceSq) / Config::TILE_SIZE;

        // Avoid only if the person is actually close enough to matter.
        if (distanceTiles > 10.0f) {
            continue;
        }

        const float pressure = relationship.fear + relationship.resentment - distanceTiles * 5.0f;

        if (pressure > highestPressure) {
            highestPressure = pressure;
            avoidedTarget = other;
        }
    }

    if (avoidedTarget == static_cast<EntityID>(-1)) {
        return false;
    }

    const Vector2 selfPos = em.transforms[entity].position;
    const Vector2 otherPos = em.transforms[avoidedTarget].position;

    Vector2 away = {selfPos.x - otherPos.x, selfPos.y - otherPos.y};

    away = NormalizeSafe(away);

    const float fleeDistance = AVOID_DISTANCE_TILES * Config::TILE_SIZE;

    for (int attempt = 0; attempt < AVOID_PATH_ATTEMPTS; ++attempt) {
        const float angleOffset = static_cast<float>(attempt - AVOID_PATH_ATTEMPTS / 2) * 0.35f;
        const Vector2 direction = Rotate(away, angleOffset);

        const Vector2 target = {selfPos.x + direction.x * fleeDistance, selfPos.y + direction.y * fleeDistance};

        std::vector<Vector2> path = Pathfinder::FindPath(selfPos, target, map, tileReg, em, entity);

        if (path.empty()) {
            continue;
        }

        BehaviorComponent& behavior = em.behaviors[entity];

        behavior.currentTask = "avoiding_person";
        behavior.currentJobTarget = avoidedTarget;
        behavior.hasJob = true;
        behavior.currentPath = std::move(path);
        behavior.currentPathIndex = 0;
        behavior.currentTarget = behavior.currentPath[0];
        behavior.isMoving = true;
        behavior.stateTimer = 0.0f;

        return true;
    }

    return false;
}

bool AISystem::TryFindConfrontPersonJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                        const EntitySpatialGrid& spatialGrid) {
    if (!IsValidHuman(em, entity) || !em.hasBehavior[entity]) {
        return false;
    }

    const float radius = SOCIALIZE_RADIUS_TILES * Config::TILE_SIZE;

    const std::vector<EntityID> nearby = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, radius, em);

    EntityID bestTarget = static_cast<EntityID>(-1);
    float bestScore = -std::numeric_limits<float>::infinity();

    for (EntityID candidate : nearby) {
        if (candidate == entity || !IsValidHuman(em, candidate) || !AreSameVillage(em, entity, candidate)) {
            continue;
        }

        RelationshipEntry* relationship = FindRelationship(em, entity, candidate);

        if (relationship == nullptr) {
            continue;
        }

        if (!ShouldConfrontRelationship(*relationship, em, entity)) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        const float distancePenalty = std::sqrt(distanceSq) / Config::TILE_SIZE;

        const float score = relationship->resentment + GetAggression(em, entity) * 30.0f + GetBravery(em, entity) * 20.0f - distancePenalty;

        if (score > bestScore) {
            bestScore = score;
            bestTarget = candidate;
        }
    }

    if (bestTarget == static_cast<EntityID>(-1)) {
        return false;
    }

    return AITaskExecutor::StartMoveAdjacentToEntity(entity, bestTarget, em, map, tileReg, "moving_to_confront", "confronting_person",
                                                     1.2f);
}
