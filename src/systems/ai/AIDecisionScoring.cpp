/**
 * @file AIDecisionScoring.cpp
 * @brief Shared scoring helpers for AI decision providers.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/ai/AIDecisionScoring.hpp"

#include "core/Config.hpp"
#include "systems/AISystemUtils.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace ai::decision {
namespace {

float Clamp01(float value) {
    if (value < 0.0f) {
        return 0.0f;
    }

    if (value > 1.0f) {
        return 1.0f;
    }

    return value;
}

bool IsValidThreat(EntityID threat, const EntityManager& em) {
    return threat < em.active.size() && em.active[threat] && em.hasTransform[threat] && em.hasHealth[threat] &&
           em.healths[threat].current > 0.0f;
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

    if (resource == nullptr) {
        return 1.0f;
    }

    if (resource->isConsumable && resource->nutrition > 0.0f) {
        return 80.0f + resource->nutrition;
    }

    return 20.0f;
}

bool IsDawnReturnWindow(float hour) {
    return hour >= 5.0f && hour < 7.0f;
}

} // namespace

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

bool HasThreatMemory(EntityID entity, const EntityManager& em) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasAIContext[entity]) {
        return false;
    }

    const AIContextComponent& context = em.aiContexts[entity];

    return context.lastThreatId != static_cast<EntityID>(-1) && context.threatMemoryTimer > 0.0f && IsValidThreat(context.lastThreatId, em);
}

bool IsCurrentThreatResponseTask(const BehaviorComponent& behavior, EntityID threat) {
    if (behavior.currentJobTarget != threat) {
        return false;
    }

    return behavior.currentTask == "moving_to_flee" || behavior.currentTask == "fleeing" || behavior.currentTask == "moving_to_hunt" ||
           behavior.currentTask == "attacking";
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

        const float hungerRatio = ai::decision::GetHungerRatio(entity, em);

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

bool HasUrgentPersonalNeed(EntityID entity, const EntityManager& em) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasNeeds[entity]) {
        return false;
    }

    const NeedsComponent& needs = em.needs[entity];

    const float hungerRatio = needs.maxHunger > 0.0f ? needs.hunger / needs.maxHunger : 1.0f;

    const float fatigueRatio = needs.maxFatigue > 0.0f ? needs.fatigue / needs.maxFatigue : 0.0f;

    return hungerRatio <= 0.35f || fatigueRatio >= 0.85f || needs.collapsedFromFatigue;
}

bool HasCapability(const BehaviorComponent& behavior, const std::string& capability) {
    return std::find(behavior.innateCapabilities.begin(), behavior.innateCapabilities.end(), capability) !=
           behavior.innateCapabilities.end();
}

} // namespace ai::decision
