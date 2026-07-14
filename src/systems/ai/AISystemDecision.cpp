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

        if (AITaskExecutor::StartMoveToPosition(entity, threat, target, em, map, tileReg, "moving_to_flee")) {
            if (em.hasAIContext[entity]) {
                em.aiContexts[entity].currentTaskPriority = AIPriority::Threat;
                em.aiContexts[entity].currentTaskInterruptible = true;
            }

            return true;
        }
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
            em.aiContexts[entity].currentTaskPriority = AIPriority::Threat;
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
        em.aiContexts[entity].currentTaskPriority = AIPriority::Threat;
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

    if (hasValidThreat && !ai::decision::IsCurrentThreatResponseTask(behavior, threat) &&
        context.currentTaskPriority < AIPriority::Threat) {
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
    const float hungerRatio = ai::decision::GetHungerRatio(entity, em);

    if (hungerRatio <= 0.15f && HasCapability(behavior, "seek_food") && context.currentTaskPriority < AIPriority::CriticalNeed) {
        CancelCurrentTask(entity, em);

        if (TryFindSeekFoodJob(entity, em, map, tileReg, resourceReg, spatialGrid)) {
            if (em.hasAIContext[entity]) {
                em.aiContexts[entity].currentTaskPriority = AIPriority::CriticalNeed;
                em.aiContexts[entity].currentTaskInterruptible = true;
            }

            return true;
        }
    }

    // =========================================================
    // 3. Extreme fatigue can interrupt work, but stays below
    //    threat and critical hunger.
    // =========================================================
    const float fatigueRatio = ai::decision::GetFatigueRatio(entity, em);

    if (fatigueRatio >= 0.95f && HasCapability(behavior, "rest") && context.currentTaskPriority < AIPriority::Rest) {
        CancelCurrentTask(entity, em);

        if (TryFindRestJob(entity, em, map, tileReg, spatialGrid)) {
            if (em.hasAIContext[entity]) {
                em.aiContexts[entity].currentTaskPriority = AIPriority::Rest;
                em.aiContexts[entity].currentTaskInterruptible = true;
            }

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

    if (intent.has_value()) {
        return AITaskExecutor::ApplyIntent(entity, em, map, tileReg, intent.value());
    }

    switch (candidate.type) {
        case AITaskType::Flee:
            if (em.hasAIContext[entity]) {
                return TryStartFleeFromThreat(entity, em.aiContexts[entity].lastThreatId, em, map, tileReg);
            }
            return false;

        case AITaskType::Defend:
            if (em.hasAIContext[entity]) {
                return TryStartDefendAgainstThreat(entity, em.aiContexts[entity].lastThreatId, em, map, tileReg);
            }
            return false;

        case AITaskType::SeekFood:
            return TryFindSeekFoodJob(entity, em, map, tileReg, resourceReg, spatialGrid);

        case AITaskType::Rest:
            return TryFindRestJob(entity, em, map, tileReg, spatialGrid);

        case AITaskType::CareChildFood:
            return TryFindCareChildFoodJob(entity, em, map, tileReg, resourceReg);

        case AITaskType::ReturnToVillageCore:
            return TryFindReturnToVillageCoreJob(entity, em, map, tileReg);

        case AITaskType::EquipWeapon:
            return TryFindEquipWeaponJob(entity, em, map, tileReg, weaponReg, spatialGrid);

        case AITaskType::RequestWeapon:
            return TryFindRequestWeaponJob(entity, em, map, tileReg, spatialGrid);

        case AITaskType::FulfillWeaponRequest:
            return TryFindFulfillWeaponRequestJob(entity, em, map, tileReg, resourceReg, weaponReg, spatialGrid);

        case AITaskType::Store:
            return TryFindStoreJob(entity, em, map, tileReg, spatialGrid);

        case AITaskType::Haul:
            return TryFindHaulJob(entity, em, map, tileReg, resourceReg, spatialGrid);

        case AITaskType::Guard:
            return TryFindGuardJob(entity, em, map, tileReg, spatialGrid);

        case AITaskType::Repair:
            return TryFindRepairJob(entity, em, map, tileReg, spatialGrid);

        case AITaskType::Patrol:
            return TryFindPatrolJob(entity, em, map, tileReg);

        case AITaskType::Hunt:
            return TryFindHuntJob(entity, em, map, tileReg, spatialGrid);

        case AITaskType::Build:
            return TryFindBuildJob(entity, em, map, tileReg, spatialGrid);

        case AITaskType::Dismantle:
            return TryFindDismantleJob(entity, em, map, tileReg, spatialGrid);

        case AITaskType::Harvest:
            return TryFindHarvestJob(entity, em, map, tileReg, spatialGrid);

        case AITaskType::AvoidPerson:
            return TryFindAvoidPersonJob(entity, em, map, tileReg);

        case AITaskType::ConfrontPerson:
            return TryFindConfrontPersonJob(entity, em, map, tileReg, spatialGrid);

        case AITaskType::ComfortFrightened:
            return TryFindComfortFrightenedJob(entity, em, map, tileReg, spatialGrid);

        case AITaskType::Preach:
            return TryFindPreachJob(entity, em, map, tileReg, spatialGrid);

        case AITaskType::HoldRitual:
            return TryFindHoldRitualJob(entity, em, map, tileReg, spatialGrid);

        case AITaskType::Intimidate:
            return TryFindIntimidateJob(entity, em, map, tileReg, spatialGrid);

        case AITaskType::FightNonLethal:
            return TryFindFightNonLethalJob(entity, em, map, tileReg, spatialGrid);

        case AITaskType::Murder:
            return TryFindMurderJob(entity, em, map, tileReg, spatialGrid);

        case AITaskType::Pray:
        case AITaskType::Socialize:
        case AITaskType::Wander:
        case AITaskType::None:
            return false;
    }

    return false;
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
