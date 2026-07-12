/**
 * @file AISystemStorage.cpp
 * @brief AI logic for managing inventory limits and finding valid storage deposits.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/AISystem.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/Pathfinder.hpp"

#include <limits>
#include <utility>
#include <vector>

bool AISystem::TryFindStoreJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                               const EntitySpatialGrid& spatialGrid) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasInventory[entity] || !em.hasBehavior[entity] ||
        !em.hasTransform[entity]) {
        return false;
    }

    InventoryComponent& inventory = em.inventories[entity];

    if (!AISystemUtils::HasAnyInventoryItem(inventory)) {
        return false;
    }

    const float searchRadius = AISystemUtils::GetActionRadiusWorld(entity, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, searchRadius, em);

    EntityID bestStorage = static_cast<EntityID>(-1);
    float bestDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (candidate == entity || candidate >= em.active.size() || !em.active[candidate] || !em.hasTransform[candidate] ||
            !em.hasInventory[candidate] || !em.hasStorage[candidate]) {
            continue;
        }

        // Do not store into unfinished blueprints.
        if (em.hasBlueprint[candidate] && !em.blueprints[candidate].isFinished) {
            continue;
        }

        // Avoid depositing into another creature inventory.
        if (em.hasBehavior[candidate]) {
            continue;
        }

        if (!AISystemUtils::StorageCanAcceptFromInventory(candidate, inventory, em)) {
            continue;
        }
        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestStorage = candidate;
        }
    }

    if (bestStorage == static_cast<EntityID>(-1)) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[entity];

    if (AreEntitiesAdjacent(entity, bestStorage, em)) {
        behavior.currentTask = "depositing";
        behavior.currentJobTarget = bestStorage;
        behavior.hasJob = true;
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        behavior.stateTimer = 0.8f;

        return true;
    }

    std::vector<Vector2> path =
        Pathfinder::FindPathToAdjacentTile(em.transforms[entity].position, em.transforms[bestStorage].position, map, tileReg, em, entity);

    if (path.empty()) {
        return false;
    }

    behavior.currentTask = "moving_to_storage";
    behavior.currentJobTarget = bestStorage;
    behavior.hasJob = true;
    behavior.currentPath = std::move(path);
    behavior.currentPathIndex = 0;
    behavior.currentTarget = behavior.currentPath[0];
    behavior.isMoving = true;

    return true;
}
