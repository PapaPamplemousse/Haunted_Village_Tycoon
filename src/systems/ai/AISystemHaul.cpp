/**
 * @file AISystemHaul.cpp
 * @brief AI logic for carrier / hauler resource transport.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/AISystem.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/Pathfinder.hpp"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int HAUL_BATCH_SIZE = 10;

bool IsValidStorage(EntityID entity, const EntityManager& em) {
    if (entity >= em.active.size() || !em.active[entity]) {
        return false;
    }

    if (!em.hasTransform[entity] || !em.hasInventory[entity] || !em.hasStorage[entity]) {
        return false;
    }

    if (em.hasBlueprint[entity] && !em.blueprints[entity].isFinished) {
        return false;
    }

    // Do not use creature inventories as storage nodes.
    if (em.hasBehavior[entity]) {
        return false;
    }

    return true;
}

bool StorageHasAnyItem(const InventoryComponent& inventory) {
    for (const auto& item : inventory.items) {
        if (item.second > 0) {
            return true;
        }
    }

    return false;
}

int GetStorageRemainingCapacity(EntityID storageEntity, const EntityManager& em) {
    if (!IsValidStorage(storageEntity, em)) {
        return 0;
    }

    const int used = AISystemUtils::GetInventoryItemCount(em.inventories[storageEntity]);
    const int capacity = em.storages[storageEntity].capacity;

    return std::max(0, capacity - used);
}

bool IsSpecializedStorage(const StorageComponent& storage) {
    return !storage.acceptedItems.empty();
}

/**
 * @brief Prevent obvious storage loops.
 *
 * V1 rule:
 * - moving from generic storage to specialized storage is good;
 * - moving from specialized storage to generic storage is avoided;
 * - moving between two generic storages is avoided;
 * - moving between specialized storages is allowed only if destination accepts the item
 *   and source does not have a stricter reason to keep it.
 */
bool IsAllowedHaulRoute(EntityID source, EntityID destination, const std::string& itemId, const EntityManager& em) {
    if (!IsValidStorage(source, em) || !IsValidStorage(destination, em) || source == destination) {
        return false;
    }

    const StorageComponent& sourceStorage = em.storages[source];
    const StorageComponent& destStorage = em.storages[destination];

    if (!AISystemUtils::StorageAcceptsItem(destStorage, itemId)) {
        return false;
    }

    const bool sourceSpecialized = IsSpecializedStorage(sourceStorage);
    const bool destSpecialized = IsSpecializedStorage(destStorage);

    // Avoid draining specialized storage into generic storage.
    if (sourceSpecialized && !destSpecialized) {
        return false;
    }

    // Avoid moving generic -> generic for now; it creates noise.
    if (!sourceSpecialized && !destSpecialized) {
        return false;
    }

    return true;
}

bool FindBestHaulItemAndDestination(EntityID carrier, EntityID source, const std::vector<EntityID>& storageCandidates,
                                    const EntityManager& em, std::string& outItemId, EntityID& outDestination, int& outAmount,
                                    float& outScore) {
    outItemId.clear();
    outDestination = static_cast<EntityID>(-1);
    outAmount = 0;
    outScore = -std::numeric_limits<float>::infinity();

    if (!IsValidStorage(source, em) || !em.hasTransform[carrier]) {
        return false;
    }

    const InventoryComponent& sourceInventory = em.inventories[source];

    for (const auto& item : sourceInventory.items) {
        const std::string& itemId = item.first;
        const int sourceCount = item.second;

        if (sourceCount <= 0) {
            continue;
        }

        for (EntityID destination : storageCandidates) {
            if (!IsValidStorage(destination, em) || destination == source) {
                continue;
            }

            if (!IsAllowedHaulRoute(source, destination, itemId, em)) {
                continue;
            }

            const int remainingCapacity = GetStorageRemainingCapacity(destination, em);

            if (remainingCapacity <= 0) {
                continue;
            }

            const int amount = std::min({HAUL_BATCH_SIZE, sourceCount, remainingCapacity});

            if (amount <= 0) {
                continue;
            }

            const float sourceDistanceSq = AISystemUtils::SquaredDistance(em.transforms[carrier].position, em.transforms[source].position);

            const float sourceToDestDistanceSq =
                AISystemUtils::SquaredDistance(em.transforms[source].position, em.transforms[destination].position);

            const bool destSpecialized = IsSpecializedStorage(em.storages[destination]);

            float score = 0.0f;
            score += static_cast<float>(amount) * 5.0f;
            score += destSpecialized ? 80.0f : 0.0f;
            score -= sourceDistanceSq / 10000.0f;
            score -= sourceToDestDistanceSq / 20000.0f;

            if (score > outScore) {
                outScore = score;
                outItemId = itemId;
                outDestination = destination;
                outAmount = amount;
            }
        }
    }

    return outDestination != static_cast<EntityID>(-1) && !outItemId.empty() && outAmount > 0;
}

} // namespace

bool AISystem::TryFindHaulJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg, const ResourceRegistry&,
                              const EntitySpatialGrid& spatialGrid) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasBehavior[entity] || !em.hasTransform[entity] ||
        !em.hasInventory[entity] || !em.hasAIContext[entity]) {
        return false;
    }

    // If the carrier already carries something, normal store job should handle it first.
    if (AISystemUtils::HasAnyInventoryItem(em.inventories[entity])) {
        return false;
    }

    const float searchRadius = AISystemUtils::GetActionRadiusWorld(entity, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, searchRadius, em);

    std::vector<EntityID> storages;

    for (EntityID candidate : candidates) {
        if (IsValidStorage(candidate, em)) {
            storages.push_back(candidate);
        }
    }

    if (storages.size() < 2) {
        return false;
    }

    EntityID bestSource = static_cast<EntityID>(-1);
    EntityID bestDestination = static_cast<EntityID>(-1);
    std::string bestItemId;
    int bestAmount = 0;
    float bestScore = -std::numeric_limits<float>::infinity();

    for (EntityID source : storages) {
        if (!StorageHasAnyItem(em.inventories[source])) {
            continue;
        }

        std::string itemId;
        EntityID destination = static_cast<EntityID>(-1);
        int amount = 0;
        float score = -std::numeric_limits<float>::infinity();

        if (!FindBestHaulItemAndDestination(entity, source, storages, em, itemId, destination, amount, score)) {
            continue;
        }

        if (score > bestScore) {
            bestScore = score;
            bestSource = source;
            bestDestination = destination;
            bestItemId = itemId;
            bestAmount = amount;
        }
    }

    if (bestSource == static_cast<EntityID>(-1) || bestDestination == static_cast<EntityID>(-1) || bestItemId.empty() || bestAmount <= 0) {
        return false;
    }

    AIContextComponent& context = em.aiContexts[entity];
    context.haulSourceId = bestSource;
    context.haulDestinationId = bestDestination;
    context.haulItemId = bestItemId;
    context.haulAmount = bestAmount;

    BehaviorComponent& behavior = em.behaviors[entity];

    if (AreEntitiesAdjacent(entity, bestSource, em)) {
        const int removed = AISystemUtils::RemoveItemFromInventory(em.inventories[bestSource], bestItemId, bestAmount);

        if (removed <= 0) {
            return false;
        }

        em.inventories[entity].items[bestItemId] += removed;
        context.haulAmount = removed;

        std::vector<Vector2> path = Pathfinder::FindPathToAdjacentTile(em.transforms[entity].position,
                                                                       em.transforms[bestDestination].position, map, tileReg, em, entity);

        if (path.empty()) {
            return false;
        }

        behavior.currentTask = "moving_to_haul_destination";
        behavior.currentJobTarget = bestDestination;
        behavior.currentItemTarget = bestItemId;
        behavior.hasJob = true;
        behavior.currentPath = std::move(path);
        behavior.currentPathIndex = 0;
        behavior.currentTarget = behavior.currentPath[0];
        behavior.isMoving = true;
        behavior.stateTimer = 0.0f;

        return true;
    }

    std::vector<Vector2> path =
        Pathfinder::FindPathToAdjacentTile(em.transforms[entity].position, em.transforms[bestSource].position, map, tileReg, em, entity);

    if (path.empty()) {
        return false;
    }

    behavior.currentTask = "moving_to_haul_source";
    behavior.currentJobTarget = bestSource;
    behavior.currentItemTarget = bestItemId;
    behavior.hasJob = true;
    behavior.currentPath = std::move(path);
    behavior.currentPathIndex = 0;
    behavior.currentTarget = behavior.currentPath[0];
    behavior.isMoving = true;
    behavior.stateTimer = 0.0f;

    return true;
}
