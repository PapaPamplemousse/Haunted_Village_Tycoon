/**
 * @file AINeedIntents.cpp
 * @brief Need-related AI intents: food, rest, child feeding.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Config.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/Pathfinder.hpp"
#include "systems/ai/AIIntentFinders.hpp"

#include <limits>
#include <string>
#include <vector>

namespace {

bool IsValidActor(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasTransform[entity] && em.hasBehavior[entity];
}

bool IsValidFoodStorage(EntityID storage, const EntityManager& em, const ResourceRegistry& resourceReg, std::string& outFoodItem) {
    if (storage >= em.active.size() || !em.active[storage] || !em.hasTransform[storage] || !em.hasInventory[storage] ||
        !em.hasStorage[storage]) {
        return false;
    }

    if (em.hasBlueprint[storage] && !em.blueprints[storage].isFinished) {
        return false;
    }

    for (const auto& item : em.inventories[storage].items) {
        if (item.second <= 0) {
            continue;
        }

        if (!AISystemUtils::IsConsumableFoodItem(resourceReg, item.first)) {
            continue;
        }

        outFoodItem = item.first;
        return true;
    }

    return false;
}

EntityID FindNearestFoodStorage(EntityID entity, const EntityManager& em, const ResourceRegistry& resourceReg,
                                const EntitySpatialGrid& spatialGrid, std::string& outFoodItem) {
    if (!IsValidActor(em, entity)) {
        return static_cast<EntityID>(-1);
    }

    const float radius = AISystemUtils::GetActionRadiusWorld(entity, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, radius, em);

    EntityID bestStorage = static_cast<EntityID>(-1);
    float bestDistanceSq = std::numeric_limits<float>::infinity();
    std::string bestFood;

    for (EntityID candidate : candidates) {
        std::string foodItem;

        if (!IsValidFoodStorage(candidate, em, resourceReg, foodItem)) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestStorage = candidate;
            bestFood = foodItem;
        }
    }

    outFoodItem = bestFood;
    return bestStorage;
}

bool IsHungryChildOfSameVillage(EntityID child, EntityID caretaker, const EntityManager& em) {
    if (child >= em.active.size() || !em.active[child] || !em.hasTag[child] || !em.hasNeeds[child] || !em.hasTransform[child] ||
        !em.hasVillageMember[child] || !em.hasVillageMember[caretaker]) {
        return false;
    }

    if (em.villageMembers[child].villageId != em.villageMembers[caretaker].villageId) {
        return false;
    }

    if (em.tags[child].species != "human") {
        return false;
    }

    if (em.tags[child].age >= Config::ADULT_AGE) {
        return false;
    }

    const NeedsComponent& needs = em.needs[child];

    if (needs.maxHunger <= 0.0f) {
        return false;
    }

    const float hungerRatio = needs.hunger / needs.maxHunger;
    return hungerRatio <= 0.55f;
}

EntityID FindHungryChildTarget(EntityID entity, const EntityManager& em, const EntitySpatialGrid& spatialGrid) {
    if (!IsValidActor(em, entity) || !em.hasVillageMember[entity]) {
        return static_cast<EntityID>(-1);
    }

    const float radius = AISystemUtils::GetActionRadiusWorld(entity, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, radius, em);

    EntityID bestChild = static_cast<EntityID>(-1);
    float bestNeed = -1.0f;

    for (EntityID candidate : candidates) {
        if (!IsHungryChildOfSameVillage(candidate, entity, em)) {
            continue;
        }

        const NeedsComponent& needs = em.needs[candidate];
        const float hungerRatio = needs.maxHunger > 0.0f ? needs.hunger / needs.maxHunger : 1.0f;
        const float needScore = 1.0f - hungerRatio;

        if (needScore > bestNeed) {
            bestNeed = needScore;
            bestChild = candidate;
        }
    }

    return bestChild;
}

EntityID FindBestRestSpot(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                          const EntitySpatialGrid& spatialGrid) {
    if (!IsValidActor(em, entity)) {
        return static_cast<EntityID>(-1);
    }

    const float radius = AISystemUtils::GetActionRadiusWorld(entity, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, radius, em);

    EntityID bestRestSpot = static_cast<EntityID>(-1);
    int bestPriority = -1;
    float bestDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (candidate >= em.active.size() || !em.active[candidate] || !em.hasRestSpot[candidate] || !em.hasTransform[candidate]) {
            continue;
        }

        if (em.hasBlueprint[candidate] && !em.blueprints[candidate].isFinished) {
            continue;
        }

        AISystemUtils::CleanRestSpotOccupants(em.restSpots[candidate], em);

        if (!AISystemUtils::RestSpotHasCapacity(candidate, em)) {
            continue;
        }

        if (!AISystemUtils::CanEntityUseRestSpot(entity, candidate, em)) {
            continue;
        }

        // Avoid reserving before checking at least one path exists.
        const std::vector<Vector2> path =
            Pathfinder::FindPathToAdjacentTile(em.transforms[entity].position, em.transforms[candidate].position, map, tileReg, em, entity);

        if (path.empty()) {
            continue;
        }

        const int accessPriority = AISystemUtils::GetRestSpotAccessPriority(entity, candidate, em);

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        if (accessPriority > bestPriority || (accessPriority == bestPriority && distanceSq < bestDistanceSq)) {
            bestPriority = accessPriority;
            bestDistanceSq = distanceSq;
            bestRestSpot = candidate;
        }
    }

    return bestRestSpot;
}

} // namespace

namespace ai::intents {

std::optional<AIIntent> FindSeekFoodIntent(EntityID entity, EntityManager& em, const WorldMap&, const TileRegistry&,
                                           const ResourceRegistry& resourceReg, const WeaponRegistry&,
                                           const EntitySpatialGrid& spatialGrid) {
    if (!IsValidActor(em, entity) || !em.hasNeeds[entity]) {
        return std::nullopt;
    }

    if (em.hasInventory[entity]) {
        const std::string itemId = AISystemUtils::FindFirstFoodItemInInventory(em.inventories[entity], resourceReg);

        if (!itemId.empty()) {
            AIIntent intent;
            intent.kind = AIIntentKind::StartAction;
            intent.targetEntity = entity;
            intent.actionTask = "eating";
            intent.actionDuration = AISystemUtils::EAT_DURATION;
            intent.itemTarget = itemId;
            return intent;
        }
    }

    std::string foodItem;
    const EntityID storage = FindNearestFoodStorage(entity, em, resourceReg, spatialGrid, foodItem);

    if (storage == static_cast<EntityID>(-1) || foodItem.empty()) {
        return std::nullopt;
    }

    AIIntent intent;
    intent.kind = AIIntentKind::MoveAdjacentToEntity;
    intent.targetEntity = storage;
    intent.moveTask = "moving_to_food_storage";
    intent.actionTask = "eating_from_storage";
    intent.actionDuration = AISystemUtils::EAT_DURATION;
    intent.itemTarget = foodItem;

    return intent;
}

std::optional<AIIntent> FindRestIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                       const ResourceRegistry&, const WeaponRegistry&, const EntitySpatialGrid& spatialGrid) {
    if (!IsValidActor(em, entity)) {
        return std::nullopt;
    }

    const EntityID restSpot = FindBestRestSpot(entity, em, map, tileReg, spatialGrid);

    if (restSpot == static_cast<EntityID>(-1)) {
        return std::nullopt;
    }

    if (!AISystemUtils::ReserveRestSpot(restSpot, entity, em)) {
        return std::nullopt;
    }

    AIIntent intent;
    intent.kind = AIIntentKind::MoveAdjacentToEntity;
    intent.targetEntity = restSpot;
    intent.moveTask = "moving_to_rest";
    intent.actionTask = "resting";
    intent.actionDuration = 1.0f;

    return intent;
}

std::optional<AIIntent> FindCareChildFoodIntent(EntityID entity, EntityManager& em, const WorldMap&, const TileRegistry&,
                                                const ResourceRegistry& resourceReg, const WeaponRegistry&,
                                                const EntitySpatialGrid& spatialGrid) {
    if (!IsValidActor(em, entity) || !em.hasInventory[entity]) {
        return std::nullopt;
    }

    const std::string foodItem = AISystemUtils::FindFirstFoodItemInInventory(em.inventories[entity], resourceReg);

    if (foodItem.empty()) {
        return std::nullopt;
    }

    const EntityID child = FindHungryChildTarget(entity, em, spatialGrid);

    if (child == static_cast<EntityID>(-1)) {
        return std::nullopt;
    }

    if (em.hasAIContext[entity]) {
        em.aiContexts[entity].careTargetId = child;
    }

    AIIntent intent;
    intent.kind = AIIntentKind::MoveAdjacentToEntity;
    intent.targetEntity = child;
    intent.moveTask = "moving_to_feed_child";
    intent.actionTask = "feeding_child";
    intent.actionDuration = AISystemUtils::EAT_DURATION;
    intent.itemTarget = foodItem;

    return intent;
}

} // namespace ai::intents
