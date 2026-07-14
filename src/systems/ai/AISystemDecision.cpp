/**
 * @file AISystemDecision.cpp
 * @brief AI decision helpers for interruption, threat response and short-term context.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Config.hpp"
#include "systems/AISystem.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/Pathfinder.hpp"
#include "systems/ai/AIPriority.hpp"
#include "systems/ai/AITaskCandidate.hpp"
#include "systems/ai/AITaskType.hpp"

#include <algorithm>
#include <cmath>
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

float EstimateStoreUtility(EntityID entity, const EntityManager& em, const EntitySpatialGrid& spatialGrid) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasInventory[entity] || !em.hasTransform[entity]) {
        return -1.0f;
    }

    const InventoryComponent& inventory = em.inventories[entity];

    if (!AISystemUtils::HasAnyInventoryItem(inventory)) {
        return -1.0f;
    }

    const float searchRadius = AISystemUtils::GetActionRadiusWorld(entity, em);
    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, searchRadius, em);

    float bestScore = -1.0f;

    for (EntityID candidate : candidates) {
        if (candidate == entity || candidate >= em.active.size() || !em.active[candidate] || !em.hasTransform[candidate] ||
            !em.hasInventory[candidate] || !em.hasStorage[candidate]) {
            continue;
        }

        if (em.hasBlueprint[candidate] && !em.blueprints[candidate].isFinished) {
            continue;
        }

        if (em.hasBehavior[candidate]) {
            continue;
        }

        if (!AISystemUtils::StorageCanAcceptFromInventory(candidate, inventory, em)) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        const float carriedScore = static_cast<float>(AISystemUtils::GetInventoryItemCount(inventory)) * 3.0f;

        const float score = carriedScore + DistanceScore(distanceSq);

        bestScore = std::max(bestScore, score);
    }

    return bestScore;
}

float EstimateBuildUtility(EntityID entity, const EntityManager& em, const EntitySpatialGrid& spatialGrid) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasTransform[entity]) {
        return -1.0f;
    }

    const float searchRadius = AISystemUtils::GetActionRadiusWorld(entity, em);
    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, searchRadius, em);

    float bestScore = -1.0f;

    for (EntityID candidate : candidates) {
        if (candidate >= em.active.size() || !em.active[candidate] || !em.hasBlueprint[candidate] || em.blueprints[candidate].isFinished ||
            !em.hasTransform[candidate]) {
            continue;
        }

        const auto& required = em.blueprints[candidate].requiredMaterials;

        if (!AISystemUtils::HasAccessibleMaterials(entity, em, spatialGrid, required)) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        // Full material readiness matters more than distance.
        const float score = 120.0f + DistanceScore(distanceSq);

        bestScore = std::max(bestScore, score);
    }

    return bestScore;
}

float EstimateDismantleUtility(EntityID entity, const EntityManager& em, const EntitySpatialGrid& spatialGrid) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasTransform[entity]) {
        return -1.0f;
    }

    const float searchRadius = AISystemUtils::GetActionRadiusWorld(entity, em);
    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, searchRadius, em);

    float bestScore = -1.0f;

    for (EntityID candidate : candidates) {
        if (candidate >= em.active.size() || !em.active[candidate] || !em.hasDeconstruct[candidate] || !em.hasTransform[candidate]) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        bestScore = std::max(bestScore, DistanceScore(distanceSq));
    }

    return bestScore;
}

float EstimateHuntUtility(EntityID entity, const EntityManager& em, const EntitySpatialGrid& spatialGrid) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasBehavior[entity] || !em.hasTransform[entity]) {
        return -1.0f;
    }

    const BehaviorComponent& behavior = em.behaviors[entity];
    const BehaviorRule* huntRule = FindRule(behavior, "hunt");

    if (huntRule == nullptr || huntRule->arguments.empty()) {
        return -1.0f;
    }

    const float searchRadius = AISystemUtils::GetActionRadiusWorld(entity, em);
    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, searchRadius, em);

    float bestScore = -1.0f;

    for (EntityID candidate : candidates) {
        if (candidate == entity || candidate >= em.active.size() || !em.active[candidate] || !em.hasTag[candidate] ||
            !em.hasTransform[candidate] || !em.hasHealth[candidate] || em.healths[candidate].current <= 0.0f) {
            continue;
        }

        bool targeted = false;

        for (const std::string& species : huntRule->arguments) {
            if (species == em.tags[candidate].species) {
                targeted = true;
                break;
            }
        }

        if (!targeted) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        bestScore = std::max(bestScore, 80.0f + DistanceScore(distanceSq));
    }

    return bestScore;
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

float EstimateHarvestUtility(EntityID entity, const EntityManager& em, const ResourceRegistry& resourceReg,
                             const EntitySpatialGrid& spatialGrid) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasBehavior[entity] || !em.hasTransform[entity]) {
        return -1.0f;
    }

    const BehaviorComponent& behavior = em.behaviors[entity];
    const BehaviorRule* harvestRule = FindRule(behavior, "harvest");

    if (harvestRule == nullptr || harvestRule->arguments.empty()) {
        return -1.0f;
    }

    const float searchRadius = AISystemUtils::GetActionRadiusWorld(entity, em);
    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, searchRadius, em);

    float bestScore = -1.0f;

    for (EntityID candidate : candidates) {
        if (candidate == entity || candidate >= em.active.size() || !em.active[candidate] || !em.hasTag[candidate] ||
            !em.hasTransform[candidate] || !em.hasHarvestable[candidate]) {
            continue;
        }

        if (!RuleTargetsPrefab(*harvestRule, em.tags[candidate].prefabId)) {
            continue;
        }

        const HarvestableComponent& harvestable = em.harvestables[candidate];

        if (!AISystemUtils::HasRequiredHarvestTool(entity, harvestable, em)) {
            continue;
        }

        float resourceScore = 0.0f;

        for (const DropEntry& drop : harvestable.drops) {
            if (drop.amount <= 0 || drop.itemId.empty()) {
                continue;
            }

            resourceScore = std::max(resourceScore, GetDropUsefulness(drop, resourceReg));
        }

        const float hungerRatio = GetHungerRatio(entity, em);

        // If hungry, food harvest becomes even more useful.
        if (hungerRatio < 0.6f && AISystemUtils::HarvestableHasFoodDrop(harvestable, resourceReg)) {
            resourceScore += 60.0f;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        const float score = resourceScore + DistanceScore(distanceSq);

        bestScore = std::max(bestScore, score);
    }

    return bestScore;
}

float EstimateCareChildFoodUtility(EntityID entity, const EntityManager& em, const ResourceRegistry& resourceReg) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasFamily[entity] || !em.hasInventory[entity] || !em.hasTransform[entity]) {
        return -1.0f;
    }

    const std::string foodItem = AISystemUtils::FindFirstFoodItemInInventory(em.inventories[entity], resourceReg);

    if (foodItem.empty()) {
        return -1.0f;
    }

    const FamilyComponent& family = em.families[entity];

    float bestScore = -1.0f;

    for (EntityID child : family.children) {
        if (child >= em.active.size() || !em.active[child] || !em.hasNeeds[child] || !em.hasTransform[child]) {
            continue;
        }

        const NeedsComponent& needs = em.needs[child];

        if (needs.maxHunger <= 0.0f) {
            continue;
        }

        const float hungerRatio = needs.hunger / needs.maxHunger;

        if (hungerRatio >= 0.5f) {
            continue;
        }

        const float urgencyScore = (1.0f - hungerRatio) * 160.0f;

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[child].position);

        const float score = urgencyScore + DistanceScore(distanceSq);

        bestScore = std::max(bestScore, score);
    }

    return bestScore;
}

bool IsDawnReturnWindow(float hour) {
    return hour >= 5.0f && hour < 7.0f;
}

float EstimateReturnToVillageCoreUtility(EntityID entity, const EntityManager& em, float currentHour) {
    if (!IsDawnReturnWindow(currentHour)) {
        return -1.0f;
    }

    if (entity >= em.active.size() || !em.active[entity] || !em.hasVillageMember[entity] || !em.hasTransform[entity]) {
        return -1.0f;
    }

    const EntityID villageId = em.villageMembers[entity].villageId;

    if (villageId >= em.active.size() || !em.active[villageId] || !em.hasVillage[villageId] || !em.hasTransform[villageId]) {
        return -1.0f;
    }

    const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[villageId].position);

    const float distanceTiles = std::sqrt(distanceSq) / Config::TILE_SIZE;

    if (distanceTiles < 6.0f) {
        return -1.0f;
    }

    return distanceTiles * 10.0f;
}

float EstimateHaulUtility(EntityID entity, const EntityManager& em, const EntitySpatialGrid& spatialGrid) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasTransform[entity] || !em.hasInventory[entity]) {
        return -1.0f;
    }

    // If this carrier already carries items, store should run first.
    if (AISystemUtils::HasAnyInventoryItem(em.inventories[entity])) {
        return -1.0f;
    }

    const float searchRadius = AISystemUtils::GetActionRadiusWorld(entity, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, searchRadius, em);

    int storagesWithItems = 0;
    int specializedStoragesWithCapacity = 0;

    for (EntityID candidate : candidates) {
        if (candidate >= em.active.size() || !em.active[candidate] || !em.hasInventory[candidate] || !em.hasStorage[candidate]) {
            continue;
        }

        if (em.hasBlueprint[candidate] && !em.blueprints[candidate].isFinished) {
            continue;
        }

        if (em.hasBehavior[candidate]) {
            continue;
        }

        if (AISystemUtils::HasAnyInventoryItem(em.inventories[candidate])) {
            storagesWithItems++;
        }

        const bool specialized = !em.storages[candidate].acceptedItems.empty();
        const bool hasCapacity = AISystemUtils::HasAvailableStorageCapacity(candidate, em);

        if (specialized && hasCapacity) {
            specializedStoragesWithCapacity++;
        }
    }

    if (storagesWithItems <= 0 || specializedStoragesWithCapacity <= 0) {
        return -1.0f;
    }

    return static_cast<float>(storagesWithItems * 30 + specializedStoragesWithCapacity * 50);
}

float EstimateSocializeUtility(EntityID entity, const EntityManager& em) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasPersonality[entity]) {
        return 20.0f;
    }

    const PersonalityComponent& personality = em.personalities[entity];

    return 20.0f + personality.sociability * 80.0f;
}

float EstimateAvoidPersonUtility(EntityID entity, const EntityManager& em) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasSocial[entity]) {
        return -1.0f;
    }

    float best = -1.0f;

    for (const RelationshipEntry& relationship : em.socials[entity].relationships) {
        const float pressure = relationship.fear + relationship.resentment;

        if (pressure > best) {
            best = pressure;
        }
    }

    if (best < 70.0f) {
        return -1.0f;
    }

    return best;
}

float EstimateConfrontPersonUtility(EntityID entity, const EntityManager& em) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasSocial[entity]) {
        return -1.0f;
    }

    const float aggression = em.hasPersonality[entity] ? em.personalities[entity].aggression : 0.0f;

    const float bravery = em.hasPersonality[entity] ? em.personalities[entity].bravery : 0.5f;

    if (aggression < 0.35f && bravery < 0.65f) {
        return -1.0f;
    }

    float best = -1.0f;

    for (const RelationshipEntry& relationship : em.socials[entity].relationships) {
        if (relationship.resentment < 65.0f || relationship.friendship > 35.0f) {
            continue;
        }

        const float score = relationship.resentment + aggression * 30.0f + bravery * 20.0f;

        best = std::max(best, score);
    }

    return best;
}

float EstimateHighestHostility(EntityID entity, const EntityManager& em) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasSocial[entity]) {
        return -1.0f;
    }

    float best = -1.0f;

    for (const RelationshipEntry& relationship : em.socials[entity].relationships) {
        const float score = relationship.resentment - relationship.friendship * 0.5f + relationship.fear * 0.2f;

        best = std::max(best, score);
    }

    return best;
}

float EstimateMurderUtility(EntityID entity, const EntityManager& em) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasSocial[entity] || !em.hasPersonality[entity]) {
        return -1.0f;
    }

    const PersonalityComponent& personality = em.personalities[entity];

    const bool darkTrait = std::find(personality.traits.begin(), personality.traits.end(), "VIOLENT") != personality.traits.end() ||
                           std::find(personality.traits.begin(), personality.traits.end(), "VENGEFUL") != personality.traits.end();

    if (!darkTrait || personality.aggression < 0.85f || personality.patience > 0.35f) {
        return -1.0f;
    }

    float best = -1.0f;

    for (const RelationshipEntry& relationship : em.socials[entity].relationships) {
        if (relationship.resentment < 95.0f || relationship.friendship > 5.0f) {
            continue;
        }

        const float score = relationship.resentment + personality.aggression * 50.0f - relationship.friendship;

        best = std::max(best, score);
    }

    return best;
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

bool HasUrgentPersonalNeed(EntityID entity, const EntityManager& em) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasNeeds[entity]) {
        return false;
    }

    const NeedsComponent& needs = em.needs[entity];

    const float hungerRatio = needs.maxHunger > 0.0f ? needs.hunger / needs.maxHunger : 1.0f;

    const float fatigueRatio = needs.maxFatigue > 0.0f ? needs.fatigue / needs.maxFatigue : 0.0f;

    return hungerRatio <= 0.35f || fatigueRatio >= 0.85f || needs.collapsedFromFatigue;
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
            em.aiContexts[entity].currentTaskPriority = AIPriority::Threat;
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

    if (hasValidThreat && !IsCurrentThreatResponseTask(behavior, threat) && context.currentTaskPriority < AIPriority::Threat) {
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
    const float fatigueRatio = GetFatigueRatio(entity, em);

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

    BehaviorComponent& behavior = em.behaviors[entity];

    const bool canFlee = HasCapability(behavior, "flee");
    const bool canDefend = HasCapability(behavior, "defend") || HasCapability(behavior, "hunt");

    const bool canSeekFood = HasCapability(behavior, "seek_food");
    const bool canRest = HasCapability(behavior, "rest");
    const bool canCareChild = HasCapability(behavior, "care_child");

    const bool canReturnToVillageCore = HasCapability(behavior, "return_village_core");

    const bool canEquipWeapon = HasCapability(behavior, "equip_weapon");
    const bool canRequestWeapon = HasCapability(behavior, "request_weapon");
    const bool canFulfillWeaponRequest = HasCapability(behavior, "fulfill_weapon_request");

    const bool canStore = HasCapability(behavior, "store");
    const bool canHaul = HasCapability(behavior, "haul");

    const bool canGuard = HasCapability(behavior, "guard");
    const bool canRepair = HasCapability(behavior, "repair");
    const bool canPatrol = HasCapability(behavior, "patrol");

    const bool canHunt = HasCapability(behavior, "hunt");
    const bool canBuild = HasCapability(behavior, "build");
    const bool canDismantle = HasCapability(behavior, "dismantle");
    const bool canHarvest = HasCapability(behavior, "harvest");

    const bool canAvoidPerson = HasCapability(behavior, "avoid_person");
    const bool canConfrontPerson = HasCapability(behavior, "confront_person");

    const bool canPreach = HasCapability(behavior, "preach");
    const bool canHoldRitual = HasCapability(behavior, "hold_ritual");
    const bool canComfortFrightened = HasCapability(behavior, "comfort_frightened");

    const bool canIntimidate = HasCapability(behavior, "intimidate");
    const bool canFightNonLethal = HasCapability(behavior, "fight_non_lethal");
    const bool canMurder = HasCapability(behavior, "murder");

    const bool canStartWork = AISystemUtils::CanStartWorkNow(behavior, currentHour);

    // =========================================================
    // Threat memory
    // =========================================================
    if (HasThreatMemory(entity, em)) {
        const EntityID threat = em.aiContexts[entity].lastThreatId;

        if (canFlee) {
            candidates.push_back({AITaskType::Flee, AIPriority::Threat, 100.0f});
        }

        if (canDefend && !IsCurrentThreatResponseTask(behavior, threat)) {
            candidates.push_back({AITaskType::Defend, AIPriority::Threat - 20.0f, 90.0f});
        }
    }

    // =========================================================
    // Needs
    // =========================================================
    if (canSeekFood) {
        const float hungerRatio = GetHungerRatio(entity, em);

        if (hungerRatio <= AISystemUtils::SEEK_FOOD_THRESHOLD_RATIO) {
            const float priority = hungerRatio <= 0.25f ? AIPriority::CriticalNeed : AIPriority::HighNeed;
            const float score = (1.0f - hungerRatio) * 100.0f;

            candidates.push_back({AITaskType::SeekFood, priority, score});
        }
    }

    if (canRest && AISystemUtils::ShouldRest(entity, em, behavior, currentHour)) {
        const float fatigueRatio = GetFatigueRatio(entity, em);
        const float priority = fatigueRatio >= 0.90f ? AIPriority::CriticalNeed : AIPriority::Rest;
        const float score = fatigueRatio * 100.0f;

        candidates.push_back({AITaskType::Rest, priority, score});
    }

    // =========================================================
    // Defense
    // =========================================================
    if (canGuard) {
        float guardScore = 100.0f;

        if (currentHour >= 20.0f || currentHour < 6.0f) {
            guardScore += 80.0f;
        }

        candidates.push_back({AITaskType::Guard, AIPriority::Guard, guardScore});
    }

    // =========================================================
    // Village anchoring
    // =========================================================
    if (canReturnToVillageCore) {
        const float returnScore = EstimateReturnToVillageCoreUtility(entity, em, currentHour);

        if (returnScore > 0.0f) {
            candidates.push_back({AITaskType::ReturnToVillageCore, AIPriority::ReturnToVillage, returnScore});
        }
    }

    // =========================================================
    // Defensive social reaction
    // =========================================================
    if (canAvoidPerson) {
        const float avoidScore = EstimateAvoidPersonUtility(entity, em);

        if (avoidScore > 0.0f) {
            candidates.push_back({AITaskType::AvoidPerson, AIPriority::AvoidPerson, avoidScore});
        }
    }

    // =========================================================
    // Family care
    // =========================================================
    if (canCareChild) {
        const float careScore = EstimateCareChildFoodUtility(entity, em, resourceReg);

        if (careScore > 0.0f) {
            candidates.push_back({AITaskType::CareChildFood, AIPriority::Care, careScore});
        }
    }

    // =========================================================
    // Logistics
    // =========================================================
    if (canStore && AISystemUtils::ShouldDepositInventory(entity, em, behavior, currentHour)) {
        const float storeScore = EstimateStoreUtility(entity, em, spatialGrid);

        if (storeScore > 0.0f) {
            candidates.push_back({AITaskType::Store, AIPriority::Logistics, storeScore});
        }
    }

    if (canHaul) {
        const float haulScore = EstimateHaulUtility(entity, em, spatialGrid);

        if (haulScore > 0.0f) {
            candidates.push_back({AITaskType::Haul, AIPriority::Haul, haulScore});
        }
    }

    // =========================================================
    // Profession work
    // =========================================================
    if (canStartWork) {
        if (canHunt) {
            const float huntScore = EstimateHuntUtility(entity, em, spatialGrid);

            if (huntScore > 0.0f) {
                candidates.push_back({AITaskType::Hunt, AIPriority::Work + 40.0f, huntScore});
            }
        }

        if (canBuild) {
            const float buildScore = EstimateBuildUtility(entity, em, spatialGrid);

            if (buildScore > 0.0f) {
                candidates.push_back({AITaskType::Build, AIPriority::Work + 30.0f, buildScore});
            }
        }

        if (canDismantle) {
            const float dismantleScore = EstimateDismantleUtility(entity, em, spatialGrid);

            if (dismantleScore > 0.0f) {
                candidates.push_back({AITaskType::Dismantle, AIPriority::Work + 20.0f, dismantleScore});
            }
        }

        if (canHarvest) {
            const float harvestScore = EstimateHarvestUtility(entity, em, resourceReg, spatialGrid);

            if (harvestScore > 0.0f) {
                candidates.push_back({AITaskType::Harvest, AIPriority::Work + 10.0f, harvestScore});
            }
        }
    }

    // =========================================================
    // Religion / support
    // =========================================================
    if (canComfortFrightened) {
        candidates.push_back({AITaskType::ComfortFrightened, AIPriority::ComfortFrightened, 100.0f});
    }

    if (canPreach) {
        candidates.push_back({AITaskType::Preach, AIPriority::Preach, 70.0f});
    }

    if (canHoldRitual) {
        candidates.push_back({AITaskType::HoldRitual, AIPriority::HoldRitual, 60.0f});
    }

    // =========================================================
    // Equipment / requests
    // =========================================================
    if (canEquipWeapon) {
        candidates.push_back({AITaskType::EquipWeapon, AIPriority::EquipWeapon, 100.0f});
    }

    if (canRequestWeapon) {
        candidates.push_back({AITaskType::RequestWeapon, AIPriority::RequestWeapon, 80.0f});
    }

    if (canFulfillWeaponRequest) {
        candidates.push_back({AITaskType::FulfillWeaponRequest, AIPriority::FulfillRequest, 100.0f});
    }

    // =========================================================
    // Profession support
    // =========================================================
    if (canRepair) {
        candidates.push_back({AITaskType::Repair, AIPriority::Repair, 80.0f});
    }

    if (canPatrol) {
        candidates.push_back({AITaskType::Patrol, AIPriority::Patrol, 10.0f});
    }

    // =========================================================
    // Hostile social actions
    // =========================================================
    const bool socialCooldownReady = !em.hasAIContext[entity] || em.aiContexts[entity].socialActionCooldownTimer <= 0.0f;

    const bool canConsiderHostility = socialCooldownReady && !HasUrgentPersonalNeed(entity, em);

    if (canConsiderHostility && canConfrontPerson) {
        const float confrontScore = EstimateConfrontPersonUtility(entity, em);

        if (confrontScore > 0.0f) {
            candidates.push_back({AITaskType::ConfrontPerson, AIPriority::ConfrontPerson, confrontScore});
        }
    }

    if (canConsiderHostility && canMurder) {
        const float murderScore = EstimateMurderUtility(entity, em);

        if (murderScore > 0.0f) {
            candidates.push_back({AITaskType::Murder, AIPriority::Murder, murderScore});
        }
    }

    if (canConsiderHostility && canFightNonLethal) {
        const float hostilityScore = EstimateHighestHostility(entity, em);

        if (hostilityScore >= 75.0f) {
            candidates.push_back({AITaskType::FightNonLethal, AIPriority::FightNonLethal, hostilityScore});
        }
    }

    if (canConsiderHostility && canIntimidate) {
        const float hostilityScore = EstimateHighestHostility(entity, em);

        if (hostilityScore >= 55.0f) {
            candidates.push_back({AITaskType::Intimidate, AIPriority::Intimidate, hostilityScore});
        }
    }
}

bool AISystem::TryStartTaskCandidate(EntityID entity, const AITaskCandidate& candidate, EntityManager& em, const WorldMap& map,
                                     const TileRegistry& tileReg, const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                     const EntitySpatialGrid& spatialGrid) {
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
                                    const EntitySpatialGrid& spatialGrid) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasBehavior[entity]) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[entity];

    const bool canPray = HasCapability(behavior, "pray");
    const bool canSocialize = HasCapability(behavior, "socialize");
    const bool canWander = HasCapability(behavior, "wander");

    if (canPray && !HasUrgentPersonalNeed(entity, em)) {
        if (TryFindPrayJob(entity, em, map, tileReg, spatialGrid)) {
            if (em.hasAIContext[entity]) {
                em.aiContexts[entity].currentTaskPriority = AIPriority::Pray;
                em.aiContexts[entity].currentTaskInterruptible = true;
            }

            return true;
        }
    }

    if (canSocialize && !HasUrgentPersonalNeed(entity, em)) {
        if (TryFindSocializeJob(entity, em, map, tileReg, spatialGrid)) {
            if (em.hasAIContext[entity]) {
                em.aiContexts[entity].currentTaskPriority = AIPriority::Socialize;
                em.aiContexts[entity].currentTaskInterruptible = true;
            }

            return true;
        }
    }

    if (canWander) {
        if (TryFindWanderJob(entity, em, map, tileReg)) {
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

    return TryStartFallbackTask(entity, em, map, tileReg, spatialGrid);
}
