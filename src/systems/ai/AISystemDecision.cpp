/**
 * @file AISystemDecision.cpp
 * @brief AI decision helpers for interruption, threat response and short-term context.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Config.hpp"
#include "systems/AISystem.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/Pathfinder.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr float THREAT_MEMORY_DURATION = 8.0f;
constexpr float FLEE_DISTANCE_TILES = 8.0f;
constexpr int FLEE_PATH_ATTEMPTS = 10;
constexpr float PRIORITY_THREAT = 1000.0f;
constexpr float PRIORITY_CRITICAL_NEED = 900.0f;
constexpr float PRIORITY_HIGH_NEED = 700.0f;
constexpr float PRIORITY_REST = 520.0f;
constexpr float PRIORITY_LOGISTICS = 430.0f;
constexpr float PRIORITY_WORK = 250.0f;
constexpr float PRIORITY_IDLE = 10.0f;

enum class AIDecisionTaskType { Flee, Defend, SeekFood, Rest, Store, Hunt, Build, Dismantle, Harvest, Wander };

struct AITaskCandidate {
    AIDecisionTaskType type = AIDecisionTaskType::Wander;
    float priority = 0.0f;
    float score = 0.0f;
};

float Clamp01(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }

    if (value > 1.0f) {
        return 1.0f;
    }

    return value;
}

float GetHungerRatio(EntityID entity, const EntityManager& em) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasNeeds[entity]) {
        return 1.0f;
    }

    const NeedsComponent& needs = em.needs[entity];

    if (needs.maxHunger <= 0.0f) {
        return 1.0f;
    }

    return Clamp01(needs.hunger / needs.maxHunger);
}

float GetFatigueRatio(EntityID entity, const EntityManager& em) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasNeeds[entity]) {
        return 0.0f;
    }

    const NeedsComponent& needs = em.needs[entity];

    if (needs.maxFatigue <= 0.0f) {
        return 0.0f;
    }

    return Clamp01(needs.fatigue / needs.maxFatigue);
}

int GetInventoryItemCountSafe(EntityID entity, const EntityManager& em) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasInventory[entity]) {
        return 0;
    }

    int total = 0;

    for (const auto& item : em.inventories[entity].items) {
        if (item.second > 0) {
            total += item.second;
        }
    }

    return total;
}

bool IsValidThreat(EntityID threat, const EntityManager& em) {
    return threat < em.active.size() && em.active[threat] && em.hasTransform[threat] && em.hasHealth[threat] &&
           em.healths[threat].current > 0.0f;
}

bool HasThreatMemory(EntityID entity, const EntityManager& em) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasAIContext[entity]) {
        return false;
    }

    const AIContextComponent& context = em.aiContexts[entity];

    return context.lastThreatId != static_cast<EntityID>(-1) && context.threatMemoryTimer > 0.0f && IsValidThreat(context.lastThreatId, em);
}

bool CandidateSortPredicate(const AITaskCandidate& a, const AITaskCandidate& b) {
    if (a.priority != b.priority) {
        return a.priority > b.priority;
    }

    return a.score > b.score;
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
            em.aiContexts[entity].currentTaskPriority = PRIORITY_THREAT;
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
            em.aiContexts[entity].currentTaskPriority = PRIORITY_THREAT;
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
        em.aiContexts[entity].currentTaskPriority = PRIORITY_THREAT;
        em.aiContexts[entity].currentTaskInterruptible = true;
    }

    return true;
}

bool AISystem::TryInterruptCurrentTask(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                       const ResourceRegistry& resourceReg, const EntitySpatialGrid& spatialGrid) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasBehavior[entity] || !em.hasAIContext[entity]) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[entity];
    AIContextComponent& context = em.aiContexts[entity];

    if (!context.currentTaskInterruptible || !IsInterruptibleTask(behavior)) {
        return false;
    }

    // =========================================================
    // 1. Threat response has absolute priority.
    // =========================================================
    const EntityID threat = context.lastThreatId;

    const bool hasValidThreat = threat != static_cast<EntityID>(-1) && context.threatMemoryTimer > 0.0f && IsValidThreat(threat, em);

    if (hasValidThreat && !IsCurrentThreatResponseTask(behavior, threat) && context.currentTaskPriority < PRIORITY_THREAT) {
        const bool canFlee = HasCapability(behavior, "flee");
        const bool canDefend = HasCapability(behavior, "defend") || HasCapability(behavior, "hunt");

        if (canFlee || canDefend) {
            CancelCurrentTask(entity, em);

            if (canFlee && TryStartFleeFromThreat(entity, threat, em, map, tileReg)) {
                return true;
            }

            if (canDefend && TryStartDefendAgainstThreat(entity, threat, em, map, tileReg)) {
                return true;
            }
        }
    }

    // =========================================================
    // 2. Critical hunger can interrupt productive / long tasks.
    // =========================================================
    const float hungerRatio = GetHungerRatio(entity, em);

    if (hungerRatio <= 0.15f && HasCapability(behavior, "seek_food") && context.currentTaskPriority < PRIORITY_CRITICAL_NEED) {
        CancelCurrentTask(entity, em);

        if (TryFindSeekFoodJob(entity, em, map, tileReg, resourceReg, spatialGrid)) {
            if (em.hasAIContext[entity]) {
                em.aiContexts[entity].currentTaskPriority = PRIORITY_CRITICAL_NEED;
                em.aiContexts[entity].currentTaskInterruptible = true;
            }

            return true;
        }
    }

    // =========================================================
    // 3. Extreme fatigue can interrupt work, but stays below
    //    threat and critical hunger.
    // =========================================================
    const float fatigueRatio = GetFatigueRatio(entity, em);

    if (fatigueRatio >= 0.95f && HasCapability(behavior, "rest") && context.currentTaskPriority < PRIORITY_REST) {
        CancelCurrentTask(entity, em);

        if (TryFindRestJob(entity, em, map, tileReg, spatialGrid)) {
            if (em.hasAIContext[entity]) {
                em.aiContexts[entity].currentTaskPriority = PRIORITY_REST;
                em.aiContexts[entity].currentTaskInterruptible = true;
            }

            return true;
        }
    }

    return false;
}

bool AISystem::SelectAndStartBestTask(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                      const ResourceRegistry& resourceReg, const EntitySpatialGrid& spatialGrid, float currentHour) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasBehavior[entity] || !em.hasTransform[entity]) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[entity];

    std::vector<AITaskCandidate> candidates;

    const bool canFlee = HasCapability(behavior, "flee");
    const bool canDefend = HasCapability(behavior, "defend") || HasCapability(behavior, "hunt");
    const bool canSeekFood = HasCapability(behavior, "seek_food");
    const bool canRest = HasCapability(behavior, "rest");
    const bool canStore = HasCapability(behavior, "store");
    const bool canHunt = HasCapability(behavior, "hunt");
    const bool canBuild = HasCapability(behavior, "build");
    const bool canDismantle = HasCapability(behavior, "dismantle");
    const bool canHarvest = HasCapability(behavior, "harvest");
    const bool canWander = HasCapability(behavior, "wander");

    const bool canStartWork = AISystemUtils::CanStartWorkNow(behavior, currentHour);

    // =========================================================
    // Threat response
    // =========================================================
    if (HasThreatMemory(entity, em)) {
        if (canFlee) {
            candidates.push_back({AIDecisionTaskType::Flee, PRIORITY_THREAT, 100.0f});
        }

        if (canDefend) {
            candidates.push_back({AIDecisionTaskType::Defend, PRIORITY_THREAT - 20.0f, 90.0f});
        }
    }

    // =========================================================
    // Survival needs
    // =========================================================
    if (canSeekFood && em.hasNeeds[entity]) {
        const float hungerRatio = GetHungerRatio(entity, em);

        if (hungerRatio < AISystemUtils::SEEK_FOOD_THRESHOLD_RATIO) {
            float priority = PRIORITY_HIGH_NEED;
            float score = (1.0f - hungerRatio) * 100.0f;

            if (hungerRatio <= 0.15f) {
                priority = PRIORITY_CRITICAL_NEED;
                score += 100.0f;
            }

            candidates.push_back({AIDecisionTaskType::SeekFood, priority, score});
        }
    }

    if (canRest && AISystemUtils::ShouldRest(entity, em, behavior, currentHour)) {
        const float fatigueRatio = GetFatigueRatio(entity, em);

        float priority = PRIORITY_REST;
        float score = fatigueRatio * 100.0f;

        if (!canStartWork) {
            score += 25.0f;
        }

        if (fatigueRatio >= 0.95f) {
            priority = PRIORITY_HIGH_NEED;
            score += 75.0f;
        }

        candidates.push_back({AIDecisionTaskType::Rest, priority, score});
    }

    // =========================================================
    // Logistics
    // =========================================================
    if (canStore && AISystemUtils::ShouldDepositInventory(entity, em, behavior, currentHour)) {
        const int itemCount = GetInventoryItemCountSafe(entity, em);
        const float score = static_cast<float>(itemCount);

        candidates.push_back({AIDecisionTaskType::Store, PRIORITY_LOGISTICS, score});
    }

    // =========================================================
    // Productive work
    // =========================================================
    if (canStartWork) {
        if (canHunt) {
            candidates.push_back({AIDecisionTaskType::Hunt, PRIORITY_WORK + 40.0f, 40.0f});
        }

        if (canBuild) {
            candidates.push_back({AIDecisionTaskType::Build, PRIORITY_WORK + 30.0f, 30.0f});
        }

        if (canDismantle) {
            candidates.push_back({AIDecisionTaskType::Dismantle, PRIORITY_WORK + 20.0f, 20.0f});
        }

        if (canHarvest) {
            candidates.push_back({AIDecisionTaskType::Harvest, PRIORITY_WORK + 10.0f, 10.0f});
        }
    }

    // =========================================================
    // Idle fallback
    // =========================================================
    if (canWander) {
        candidates.push_back({AIDecisionTaskType::Wander, PRIORITY_IDLE, 0.0f});
    }

    if (candidates.empty()) {
        return false;
    }

    std::sort(candidates.begin(), candidates.end(), CandidateSortPredicate);

    for (const AITaskCandidate& candidate : candidates) {
        bool started = false;

        switch (candidate.type) {
            case AIDecisionTaskType::Flee:
                if (em.hasAIContext[entity]) {
                    started = TryStartFleeFromThreat(entity, em.aiContexts[entity].lastThreatId, em, map, tileReg);
                }
                break;

            case AIDecisionTaskType::Defend:
                if (em.hasAIContext[entity]) {
                    started = TryStartDefendAgainstThreat(entity, em.aiContexts[entity].lastThreatId, em, map, tileReg);
                }
                break;

            case AIDecisionTaskType::SeekFood:
                started = TryFindSeekFoodJob(entity, em, map, tileReg, resourceReg, spatialGrid);
                break;

            case AIDecisionTaskType::Rest:
                started = TryFindRestJob(entity, em, map, tileReg, spatialGrid);
                break;

            case AIDecisionTaskType::Store:
                started = TryFindStoreJob(entity, em, map, tileReg, spatialGrid);
                break;

            case AIDecisionTaskType::Hunt:
                started = TryFindHuntJob(entity, em, map, tileReg, spatialGrid);
                break;

            case AIDecisionTaskType::Build:
                started = TryFindBuildJob(entity, em, map, tileReg, spatialGrid);
                break;

            case AIDecisionTaskType::Dismantle:
                started = TryFindDismantleJob(entity, em, map, tileReg, spatialGrid);
                break;

            case AIDecisionTaskType::Harvest:
                started = TryFindHarvestJob(entity, em, map, tileReg, spatialGrid);
                break;

            case AIDecisionTaskType::Wander:
                started = TryFindWanderJob(entity, em, map, tileReg);
                break;
        }

        if (!started) {
            continue;
        }

        if (em.hasAIContext[entity]) {
            em.aiContexts[entity].currentTaskPriority = candidate.priority;
            em.aiContexts[entity].currentTaskInterruptible = true;
        }

        return true;
    }

    return false;
}
