/**
 * @file AISystemDecision.cpp
 * @brief AI decision helpers for interruption, threat response and short-term context.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Config.hpp"
#include "systems/AISystem.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/Pathfinder.hpp"
#include "systems/ai/AIDecisionContext.hpp"
#include "systems/ai/AIDecisionScoring.hpp"
#include "systems/ai/AIIntent.hpp"
#include "systems/ai/AIIntentFinder.hpp"
#include "systems/ai/AIPriority.hpp"
#include "systems/ai/AITaskCandidate.hpp"
#include "systems/ai/AITaskExecutor.hpp"
#include "systems/ai/AITaskProviders.hpp"
#include "systems/ai/AITaskType.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr float THREAT_MEMORY_DURATION = 8.0f;
constexpr float FLEE_DISTANCE_TILES = 8.0f;
constexpr int FLEE_PATH_ATTEMPTS = 10;

float Clamp01(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }

    if (value > 1.0f) {
        return 1.0f;
    }

    return value;
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

bool CandidateSortPredicate(const AITaskCandidate& a, const AITaskCandidate& b) {
    if (a.priority != b.priority) {
        return a.priority > b.priority;
    }

    return a.score > b.score;
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

float DistanceScore(float distanceSq) {
    const float distance = std::sqrt(distanceSq);
    const float distanceTiles = distance / Config::TILE_SIZE;

    return std::max(0.0f, 100.0f - distanceTiles * 4.0f);
}

const BehaviorRule* FindRule(const BehaviorComponent& behavior, const std::string& ruleName) {
    for (const BehaviorRule& rule : behavior.innateBehaviorRules) {
        if (rule.name == ruleName) {
            return &rule;
        }
    }

    return nullptr;
}

bool RuleTargetsPrefab(const BehaviorRule& rule, const std::string& prefabId) {
    for (const std::string& arg : rule.arguments) {
        if (arg == prefabId) {
            return true;
        }
    }

    return false;
}

float GetDropUsefulness(const DropEntry& drop, const ResourceRegistry& resourceReg) {
    const ResourceDef* resource = resourceReg.GetResourceDef(drop.itemId);

    if (resource != nullptr && resource->isConsumable && resource->nutrition > 0.0f) {
        return 80.0f;
    }

    if (drop.itemId == "WOOD") {
        return 60.0f;
    }

    if (drop.itemId == "STONE" || drop.itemId == "ROPE") {
        return 45.0f;
    }

    return 20.0f;
}

bool IsDawnReturnWindow(float hour) {
    return hour >= 5.0f && hour < 7.0f;
}

float EstimateSocializeUtility(EntityID entity, const EntityManager& em) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasPersonality[entity]) {
        return 20.0f;
    }

    const PersonalityComponent& personality = em.personalities[entity];

    return 20.0f + personality.sociability * 80.0f;
}

bool IsSocialTimeWindow(float hour) {
    // Dawn gathering / morning village life.
    if (hour >= 5.0f && hour < 8.0f) {
        return true;
    }

    // Midday short break.
    if (hour >= 12.0f && hour < 14.0f) {
        return true;
    }

    // Evening social life before sleep.
    if (hour >= 18.0f && hour < 23.0f) {
        return true;
    }

    return false;
}

bool IsCarryingItems(EntityID entity, const EntityManager& em) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasInventory[entity]) {
        return false;
    }

    for (const auto& item : em.inventories[entity].items) {
        if (item.second > 0) {
            return true;
        }
    }

    return false;
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

            if (context.threatMemoryTimer <= 0.0f) {
                context.threatMemoryTimer = 0.0f;
                context.lastThreatId = static_cast<EntityID>(-1);
            }
        }

        if (context.socialActionCooldownTimer > 0.0f) {
            context.socialActionCooldownTimer -= deltaTime;

            if (context.socialActionCooldownTimer < 0.0f) {
                context.socialActionCooldownTimer = 0.0f;
            }
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

bool AISystem::TryInterruptCurrentTask(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                       const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                       const EntitySpatialGrid& spatialGrid) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasBehavior[entity] || !em.hasAIContext[entity]) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[entity];
    AIContextComponent& context = em.aiContexts[entity];

    if (!context.currentTaskInterruptible || !IsInterruptibleTask(behavior)) {
        return false;
    }

    auto tryApplyInterruptIntent = [&](AITaskType taskType, float priority) -> bool {
        const std::optional<AIIntent> intent =
            AIIntentFinder::FindIntentForTask(entity, taskType, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        if (!intent.has_value()) {
            return false;
        }

        CancelCurrentTask(entity, em);

        if (!AITaskExecutor::ApplyIntent(entity, em, map, tileReg, intent.value())) {
            return false;
        }

        if (em.hasAIContext[entity]) {
            em.aiContexts[entity].currentTaskPriority = priority;
            em.aiContexts[entity].currentTaskInterruptible = true;
        }

        return true;
    };
    // =========================================================
    // 1. Threat response has top priority.
    // =========================================================
    const EntityID threat = context.lastThreatId;

    const bool hasValidThreat = threat != static_cast<EntityID>(-1) && context.threatMemoryTimer > 0.0f && threat < em.active.size() &&
                                em.active[threat] && em.hasTransform[threat] && em.hasHealth[threat] && em.healths[threat].current > 0.0f;

    if (hasValidThreat && !ai::decision::IsCurrentThreatResponseTask(behavior, threat) &&
        context.currentTaskPriority < AIPriority::Threat) {
        if (ai::decision::HasCapability(behavior, "flee")) {
            if (tryApplyInterruptIntent(AITaskType::Flee, AIPriority::Threat)) {
                return true;
            }
        }

        if (ai::decision::HasCapability(behavior, "defend") || ai::decision::HasCapability(behavior, "hunt")) {
            if (tryApplyInterruptIntent(AITaskType::Defend, AIPriority::Threat - 20.0f)) {
                return true;
            }
        }
    }

    // =========================================================
    // 2. Critical hunger can interrupt productive / long tasks.
    // =========================================================
    const float hungerRatio = ai::decision::GetHungerRatio(entity, em);

    if (hungerRatio <= 0.15f && ai::decision::HasCapability(behavior, "seek_food") &&
        context.currentTaskPriority < AIPriority::CriticalNeed) {
        if (tryApplyInterruptIntent(AITaskType::SeekFood, AIPriority::CriticalNeed)) {
            return true;
        }
    }

    // =========================================================
    // 3. Extreme fatigue can interrupt work.
    // =========================================================
    const float fatigueRatio = ai::decision::GetFatigueRatio(entity, em);

    if (fatigueRatio >= 0.95f && ai::decision::HasCapability(behavior, "rest") && context.currentTaskPriority < AIPriority::Rest) {
        if (tryApplyInterruptIntent(AITaskType::Rest, AIPriority::Rest)) {
            return true;
        }
    }

    return false;
}

void AISystem::BuildTaskCandidates(EntityID entity, EntityManager& em, const ResourceRegistry& resourceReg,
                                   const EntitySpatialGrid& spatialGrid, float currentHour, std::vector<AITaskCandidate>& candidates) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasBehavior[entity] || !em.hasTransform[entity]) {
        return;
    }

    AIDecisionContext ctx{entity, em, resourceReg, spatialGrid, currentHour};

    ai::providers::AppendThreatCandidates(ctx, candidates);
    ai::providers::AppendNeedCandidates(ctx, candidates);
    ai::providers::AppendDefenseCandidates(ctx, candidates);
    ai::providers::AppendVillageCandidates(ctx, candidates);
    ai::providers::AppendSocialReactionCandidates(ctx, candidates);
    ai::providers::AppendFamilyCandidates(ctx, candidates);
    ai::providers::AppendLogisticsCandidates(ctx, candidates);
    ai::providers::AppendWorkCandidates(ctx, candidates);
    ai::providers::AppendReligionCandidates(ctx, candidates);
    ai::providers::AppendEquipmentCandidates(ctx, candidates);
    ai::providers::AppendProfessionSupportCandidates(ctx, candidates);
    ai::providers::AppendHostilityCandidates(ctx, candidates);
}

bool AISystem::TryStartTaskCandidate(EntityID entity, const AITaskCandidate& candidate, EntityManager& em, const WorldMap& map,
                                     const TileRegistry& tileReg, const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                     const EntitySpatialGrid& spatialGrid) {
    const std::optional<AIIntent> intent =
        AIIntentFinder::FindIntentForTask(entity, candidate.type, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

    if (!intent.has_value()) {
        return false;
    }

    return AITaskExecutor::ApplyIntent(entity, em, map, tileReg, intent.value());
}

bool AISystem::TryStartFallbackTask(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                    const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                    const EntitySpatialGrid& spatialGrid) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasBehavior[entity]) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[entity];

    const bool canPray = ai::decision::HasCapability(behavior, "pray");
    const bool canSocialize = ai::decision::HasCapability(behavior, "socialize");
    const bool canWander = ai::decision::HasCapability(behavior, "wander");

    if (canPray && !ai::decision::HasUrgentPersonalNeed(entity, em)) {
        const std::optional<AIIntent> intent =
            AIIntentFinder::FindIntentForTask(entity, AITaskType::Pray, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        if (intent.has_value() && AITaskExecutor::ApplyIntent(entity, em, map, tileReg, intent.value())) {
            if (em.hasAIContext[entity]) {
                em.aiContexts[entity].currentTaskPriority = AIPriority::Pray;
                em.aiContexts[entity].currentTaskInterruptible = true;
            }

            return true;
        }
    }

    if (canSocialize && !ai::decision::HasUrgentPersonalNeed(entity, em)) {
        const std::optional<AIIntent> intent =
            AIIntentFinder::FindIntentForTask(entity, AITaskType::Socialize, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        if (intent.has_value() && AITaskExecutor::ApplyIntent(entity, em, map, tileReg, intent.value())) {
            if (em.hasAIContext[entity]) {
                em.aiContexts[entity].currentTaskPriority = AIPriority::Socialize;
                em.aiContexts[entity].currentTaskInterruptible = true;
            }

            return true;
        }
    }

    if (canWander) {
        const std::optional<AIIntent> intent =
            AIIntentFinder::FindIntentForTask(entity, AITaskType::Wander, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        if (intent.has_value() && AITaskExecutor::ApplyIntent(entity, em, map, tileReg, intent.value())) {
            if (em.hasAIContext[entity]) {
                em.aiContexts[entity].currentTaskPriority = AIPriority::Idle;
                em.aiContexts[entity].currentTaskInterruptible = true;
            }

            return true;
        }
    }

    return false;
}

bool AISystem::SelectAndStartBestTask(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                      const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                      const EntitySpatialGrid& spatialGrid, float currentHour) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasBehavior[entity] || !em.hasTransform[entity]) {
        return false;
    }

    std::vector<AITaskCandidate> candidates;

    BuildTaskCandidates(entity, em, resourceReg, spatialGrid, currentHour, candidates);

    std::sort(candidates.begin(), candidates.end(), CandidateSortPredicate);

    for (const AITaskCandidate& candidate : candidates) {
        if (!TryStartTaskCandidate(entity, candidate, em, map, tileReg, resourceReg, weaponReg, spatialGrid)) {
            continue;
        }

        if (em.hasAIContext[entity]) {
            em.aiContexts[entity].currentTaskPriority = candidate.priority;
            em.aiContexts[entity].currentTaskInterruptible = true;
        }

        return true;
    }

    return TryStartFallbackTask(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);
}
