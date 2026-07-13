/**
 * @file AISystemDecision.cpp
 * @brief AI decision helpers for interruption, threat response and short-term context.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Config.hpp"
#include "systems/AISystem.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/Pathfinder.hpp"

#include <cmath>
#include <utility>
#include <vector>

namespace {

constexpr float THREAT_RESPONSE_PRIORITY = 1000.0f;
constexpr float THREAT_MEMORY_DURATION = 8.0f;
constexpr float FLEE_DISTANCE_TILES = 8.0f;
constexpr int FLEE_PATH_ATTEMPTS = 10;

bool IsValidThreat(EntityID threat, const EntityManager& em) {
    return threat < em.active.size() && em.active[threat] && em.hasTransform[threat] && em.hasHealth[threat] &&
           em.healths[threat].current > 0.0f;
}

bool IsCurrentThreatResponseTask(const BehaviorComponent& behavior, EntityID threat) {
    if (behavior.currentJobTarget != threat) {
        return false;
    }

    return behavior.currentTask == "moving_to_flee" || behavior.currentTask == "fleeing" || behavior.currentTask == "moving_to_hunt" ||
           behavior.currentTask == "attacking";
}

bool IsInterruptibleTask(const BehaviorComponent& behavior) {
    // For V1, most tasks can be interrupted.
    // Later, some tasks could become atomic / non-interruptible.
    if (behavior.currentTask == "attacking") {
        return false;
    }

    return true;
}

Vector2 NormalizeSafe(Vector2 v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y);

    if (len <= 0.001f) {
        return {1.0f, 0.0f};
    }

    return {v.x / len, v.y / len};
}

Vector2 Rotate(Vector2 v, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);

    return {v.x * c - v.y * s, v.x * s + v.y * c};
}

} // namespace

void AISystem::UpdateAIContextTimers(float deltaTime, EntityManager& em) {
    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!em.active[entity] || !em.hasAIContext[entity]) {
            continue;
        }

        AIContextComponent& context = em.aiContexts[entity];

        if (context.threatMemoryTimer > 0.0f) {
            context.threatMemoryTimer -= deltaTime;
        }

        if (context.threatMemoryTimer <= 0.0f || !IsValidThreat(context.lastThreatId, em)) {
            context.lastThreatId = static_cast<EntityID>(-1);
            context.threatMemoryTimer = 0.0f;
        }
    }
}

void AISystem::CancelCurrentTask(EntityID entity, EntityManager& em) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasBehavior[entity]) {
        return;
    }

    BehaviorComponent& behavior = em.behaviors[entity];

    if (behavior.currentTask == "moving_to_rest" || behavior.currentTask == "resting" ||
        behavior.reservedRestSpot != static_cast<EntityID>(-1)) {
        AISystemUtils::ReleaseRestSpotReservation(entity, em);
    }

    ResetBehaviorState(behavior);

    if (em.hasAIContext[entity]) {
        em.aiContexts[entity].currentTaskPriority = 0.0f;
        em.aiContexts[entity].currentTaskInterruptible = true;
    }
}

bool AISystem::TryStartFleeFromThreat(EntityID entity, EntityID threat, EntityManager& em, const WorldMap& map,
                                      const TileRegistry& tileReg) {
    if (entity >= em.active.size() || threat >= em.active.size() || !em.active[entity] || !em.active[threat] || !em.hasTransform[entity] ||
        !em.hasTransform[threat] || !em.hasBehavior[entity]) {
        return false;
    }

    const Vector2 entityPos = em.transforms[entity].position;
    const Vector2 threatPos = em.transforms[threat].position;

    Vector2 away = {entityPos.x - threatPos.x, entityPos.y - threatPos.y};

    away = NormalizeSafe(away);

    const float fleeDistance = FLEE_DISTANCE_TILES * Config::TILE_SIZE;

    for (int attempt = 0; attempt < FLEE_PATH_ATTEMPTS; ++attempt) {
        const float angleOffset = static_cast<float>(attempt - FLEE_PATH_ATTEMPTS / 2) * 0.35f;
        const Vector2 direction = Rotate(away, angleOffset);

        const Vector2 target = {entityPos.x + direction.x * fleeDistance, entityPos.y + direction.y * fleeDistance};

        std::vector<Vector2> path = Pathfinder::FindPath(entityPos, target, map, tileReg, em, entity);

        if (path.empty()) {
            continue;
        }

        BehaviorComponent& behavior = em.behaviors[entity];

        behavior.currentTask = "moving_to_flee";
        behavior.currentJobTarget = threat;
        behavior.hasJob = true;
        behavior.currentPath = std::move(path);
        behavior.currentPathIndex = 0;
        behavior.currentTarget = behavior.currentPath[0];
        behavior.isMoving = true;
        behavior.stateTimer = 0.0f;

        if (em.hasAIContext[entity]) {
            em.aiContexts[entity].currentTaskPriority = THREAT_RESPONSE_PRIORITY;
            em.aiContexts[entity].currentTaskInterruptible = true;
        }

        return true;
    }

    return false;
}

bool AISystem::TryStartDefendAgainstThreat(EntityID entity, EntityID threat, EntityManager& em, const WorldMap& map,
                                           const TileRegistry& tileReg) {
    if (entity >= em.active.size() || threat >= em.active.size() || !em.active[entity] || !em.active[threat] || !em.hasTransform[entity] ||
        !em.hasTransform[threat] || !em.hasBehavior[entity] || !em.hasHealth[threat]) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[entity];

    if (AreEntitiesAdjacent(entity, threat, em)) {
        behavior.currentTask = "attacking";
        behavior.currentJobTarget = threat;
        behavior.hasJob = true;
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        behavior.stateTimer = AISystemUtils::ATTACK_DURATION;

        if (em.hasAIContext[entity]) {
            em.aiContexts[entity].currentTaskPriority = THREAT_RESPONSE_PRIORITY;
            em.aiContexts[entity].currentTaskInterruptible = false;
        }

        return true;
    }

    std::vector<Vector2> path =
        Pathfinder::FindPathToAdjacentTile(em.transforms[entity].position, em.transforms[threat].position, map, tileReg, em, entity);

    if (path.empty()) {
        return false;
    }

    behavior.currentTask = "moving_to_hunt";
    behavior.currentJobTarget = threat;
    behavior.hasJob = true;
    behavior.currentPath = std::move(path);
    behavior.currentPathIndex = 0;
    behavior.currentTarget = behavior.currentPath[0];
    behavior.isMoving = true;
    behavior.stateTimer = 0.0f;

    if (em.hasAIContext[entity]) {
        em.aiContexts[entity].currentTaskPriority = THREAT_RESPONSE_PRIORITY;
        em.aiContexts[entity].currentTaskInterruptible = true;
    }

    return true;
}

bool AISystem::TryInterruptCurrentTask(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                       const EntitySpatialGrid&) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasBehavior[entity] || !em.hasAIContext[entity]) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[entity];
    AIContextComponent& context = em.aiContexts[entity];

    const EntityID threat = context.lastThreatId;

    if (threat == static_cast<EntityID>(-1) || context.threatMemoryTimer <= 0.0f || !IsValidThreat(threat, em)) {
        return false;
    }

    if (IsCurrentThreatResponseTask(behavior, threat)) {
        return false;
    }

    if (!context.currentTaskInterruptible || !IsInterruptibleTask(behavior)) {
        return false;
    }

    if (context.currentTaskPriority >= THREAT_RESPONSE_PRIORITY) {
        return false;
    }

    const bool canFlee = HasCapability(behavior, "flee");
    const bool canDefend = HasCapability(behavior, "defend") || HasCapability(behavior, "hunt");

    if (!canFlee && !canDefend) {
        return false;
    }

    CancelCurrentTask(entity, em);

    if (canFlee && TryStartFleeFromThreat(entity, threat, em, map, tileReg)) {
        return true;
    }

    if (canDefend && TryStartDefendAgainstThreat(entity, threat, em, map, tileReg)) {
        return true;
    }

    return false;
}
