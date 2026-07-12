/**
 * @file AISystemNeeds.cpp
 * @brief Evaluates and delegates tasks for satisfying vital needs (eating, resting).
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/AISystem.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/Pathfinder.hpp"

#include <limits>
#include <utility>
#include <vector>

bool AISystem::TryFindSeekFoodJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                  const ResourceRegistry& resourceReg, const EntitySpatialGrid& spatialGrid) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasNeeds[entity] || !em.hasInventory[entity] || !em.hasBehavior[entity] ||
        !em.hasTransform[entity]) {
        return false;
    }

    auto& needs = em.needs[entity];

    const float hungerThreshold = needs.maxHunger * AISystemUtils::SEEK_FOOD_THRESHOLD_RATIO;

    if (needs.hunger >= hungerThreshold) {
        return false;
    }

    auto& inventory = em.inventories[entity];
    auto& behavior = em.behaviors[entity];

    // =========================================================
    // 1. Eat from own inventory.
    // =========================================================
    const std::string ownFood = AISystemUtils::FindFirstFoodItemInInventory(inventory, resourceReg);

    if (!ownFood.empty()) {
        behavior.currentTask = "eating";
        behavior.currentItemTarget = ownFood;
        behavior.hasJob = true;
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        behavior.stateTimer = AISystemUtils::EAT_DURATION;

        return true;
    }

    const float searchRadius = AISystemUtils::GetActionRadiusWorld(entity, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, searchRadius, em);

    // =========================================================
    // 2. Search nearby completed storage with food.
    // =========================================================
    EntityID bestStorage = static_cast<EntityID>(-1);
    std::string bestStorageFood;
    float bestStorageDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (candidate == entity || candidate >= em.active.size() || !em.active[candidate] || !em.hasTransform[candidate] ||
            !em.hasInventory[candidate]) {
            continue;
        }

        // Do not eat from unfinished blueprints.
        if (em.hasBlueprint[candidate] && !em.blueprints[candidate].isFinished) {
            continue;
        }

        // Avoid stealing from other creatures for now.
        // Storage furniture has inventory but no behavior.
        if (em.hasBehavior[candidate]) {
            continue;
        }

        const std::string foodItem = AISystemUtils::FindFirstFoodItemInInventory(em.inventories[candidate], resourceReg);

        if (foodItem.empty()) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        if (distanceSq < bestStorageDistanceSq) {
            bestStorageDistanceSq = distanceSq;
            bestStorage = candidate;
            bestStorageFood = foodItem;
        }
    }

    if (bestStorage != static_cast<EntityID>(-1)) {
        if (AreEntitiesAdjacent(entity, bestStorage, em)) {
            behavior.currentTask = "eating_from_storage";
            behavior.currentJobTarget = bestStorage;
            behavior.currentItemTarget = bestStorageFood;
            behavior.hasJob = true;
            behavior.isMoving = false;
            behavior.currentPath.clear();
            behavior.currentPathIndex = 0;
            behavior.stateTimer = AISystemUtils::EAT_DURATION;

            return true;
        }

        std::vector<Vector2> path = Pathfinder::FindPathToAdjacentTile(em.transforms[entity].position, em.transforms[bestStorage].position,
                                                                       map, tileReg, em, entity);

        if (!path.empty()) {
            behavior.currentTask = "moving_to_food_storage";
            behavior.currentJobTarget = bestStorage;
            behavior.currentItemTarget = bestStorageFood;
            behavior.hasJob = true;
            behavior.currentPath = std::move(path);
            behavior.currentPathIndex = 0;
            behavior.currentTarget = behavior.currentPath[0];
            behavior.isMoving = true;

            return true;
        }
    }

    // =========================================================
    // 3. Search nearby harvestable food source.
    // Example: BUSH_BERRY with consumable BUSH_BERRY drop.
    // =========================================================
    EntityID bestFoodSource = static_cast<EntityID>(-1);
    float bestFoodSourceDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (candidate == entity || candidate >= em.active.size() || !em.active[candidate] || !em.hasTransform[candidate] ||
            !em.hasHarvestable[candidate] || !em.hasHealth[candidate]) {
            continue;
        }

        const HarvestableComponent& harvestable = em.harvestables[candidate];

        if (!AISystemUtils::HarvestableHasFoodDrop(harvestable, resourceReg)) {
            continue;
        }

        if (!AISystemUtils::HasRequiredHarvestTool(entity, harvestable, em)) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        if (distanceSq < bestFoodSourceDistanceSq) {
            bestFoodSourceDistanceSq = distanceSq;
            bestFoodSource = candidate;
        }
    }

    if (bestFoodSource == static_cast<EntityID>(-1)) {
        return false;
    }

    if (AreEntitiesAdjacent(entity, bestFoodSource, em)) {
        behavior.currentTask = "harvesting";
        behavior.currentJobTarget = bestFoodSource;
        behavior.hasJob = true;
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        behavior.stateTimer = 1.0f;
        behavior.actionAccumulator = 0.0f;

        return true;
    }

    std::vector<Vector2> path = Pathfinder::FindPathToAdjacentTile(em.transforms[entity].position, em.transforms[bestFoodSource].position,
                                                                   map, tileReg, em, entity);

    if (path.empty()) {
        return false;
    }

    behavior.currentTask = "moving_to_harvest";
    behavior.currentJobTarget = bestFoodSource;
    behavior.hasJob = true;
    behavior.currentPath = std::move(path);
    behavior.currentPathIndex = 0;
    behavior.currentTarget = behavior.currentPath[0];
    behavior.isMoving = true;

    return true;
}

bool AISystem::TryFindRestJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                              const EntitySpatialGrid& spatialGrid) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasTransform[entity] || !em.hasBehavior[entity]) {
        return false;
    }

    const float searchRadius = AISystemUtils::GetActionRadiusWorld(entity, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, searchRadius, em);

    EntityID bestRestSpot = static_cast<EntityID>(-1);
    float bestDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (candidate >= em.active.size() || !em.active[candidate] || !em.hasTransform[candidate] || !em.hasRestSpot[candidate]) {
            continue;
        }

        if (em.hasBlueprint[candidate] && !em.blueprints[candidate].isFinished) {
            continue;
        }

        // Future private property hook:
        // if em.restSpots[candidate].isPrivate, check family/village ownership here.

        if (!AISystemUtils::RestSpotHasCapacity(candidate, em)) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestRestSpot = candidate;
        }
    }

    if (bestRestSpot == static_cast<EntityID>(-1)) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[entity];

    if (AreEntitiesAdjacent(entity, bestRestSpot, em)) {
        if (!AISystemUtils::ReserveRestSpot(bestRestSpot, entity, em)) {
            return false;
        }

        behavior.currentTask = "resting";
        behavior.currentJobTarget = bestRestSpot;
        behavior.hasJob = true;
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        behavior.stateTimer = 1.0f;

        return true;
    }

    std::vector<Vector2> path =
        Pathfinder::FindPathToAdjacentTile(em.transforms[entity].position, em.transforms[bestRestSpot].position, map, tileReg, em, entity);

    if (path.empty()) {
        return false;
    }

    if (!AISystemUtils::ReserveRestSpot(bestRestSpot, entity, em)) {
        return false;
    }

    behavior.currentTask = "moving_to_rest";
    behavior.currentJobTarget = bestRestSpot;
    behavior.hasJob = true;
    behavior.currentPath = std::move(path);
    behavior.currentPathIndex = 0;
    behavior.currentTarget = behavior.currentPath[0];
    behavior.isMoving = true;

    return true;
}
