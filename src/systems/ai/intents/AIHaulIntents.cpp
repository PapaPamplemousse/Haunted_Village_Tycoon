/**
 * @file AIHaulIntents.cpp
 * @brief Hauling AI intents.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/AISystemUtils.hpp"
#include "systems/ai/AIIntentFinders.hpp"

#include <algorithm>
#include <limits>
#include <math.h>
#include <string>
#include <vector>

namespace {

bool IsValidActor(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasTransform[entity] && em.hasBehavior[entity] && em.hasAIContext[entity];
}

bool IsUsableStorage(EntityID entity, const EntityManager& em) {
    return entity < em.active.size() && em.active[entity] && em.hasTransform[entity] && em.hasInventory[entity] && em.hasStorage[entity] &&
           !(em.hasBlueprint[entity] && !em.blueprints[entity].isFinished);
}

EntityID FindDestinationForItem(EntityID worker, EntityID source, const std::string& itemId, const EntityManager& em,
                                const EntitySpatialGrid& spatialGrid) {
    const float radius = AISystemUtils::GetActionRadiusWorld(worker, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[worker].position, radius, em);

    EntityID bestDestination = static_cast<EntityID>(-1);
    float bestDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID destination : candidates) {
        if (destination == source || !IsUsableStorage(destination, em)) {
            continue;
        }

        if (!AISystemUtils::StorageAcceptsItem(em.storages[destination], itemId)) {
            continue;
        }

        if (!AISystemUtils::HasAvailableStorageCapacity(destination, em)) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[worker].position, em.transforms[destination].position);

        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestDestination = destination;
        }
    }

    return bestDestination;
}

bool FindHaulPair(EntityID worker, EntityManager& em, const EntitySpatialGrid& spatialGrid, EntityID& outSource, EntityID& outDestination,
                  std::string& outItemId, int& outAmount) {
    outSource = static_cast<EntityID>(-1);
    outDestination = static_cast<EntityID>(-1);
    outItemId.clear();
    outAmount = 0;

    const float radius = AISystemUtils::GetActionRadiusWorld(worker, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[worker].position, radius, em);

    float bestScore = -1.0f;

    for (EntityID source : candidates) {
        if (!IsUsableStorage(source, em)) {
            continue;
        }

        for (const auto& item : em.inventories[source].items) {
            const std::string& itemId = item.first;
            const int count = item.second;

            if (count <= 0) {
                continue;
            }

            const EntityID destination = FindDestinationForItem(worker, source, itemId, em, spatialGrid);

            if (destination == static_cast<EntityID>(-1)) {
                continue;
            }

            const float sourceDistanceSq = AISystemUtils::SquaredDistance(em.transforms[worker].position, em.transforms[source].position);

            const float score = static_cast<float>(count) * 10.0f - std::sqrt(sourceDistanceSq) * 0.01f;

            if (score > bestScore) {
                bestScore = score;
                outSource = source;
                outDestination = destination;
                outItemId = itemId;
                outAmount = std::min(count, 5);
            }
        }
    }

    return outSource != static_cast<EntityID>(-1) && outDestination != static_cast<EntityID>(-1) && !outItemId.empty() && outAmount > 0;
}

} // namespace

namespace ai::intents {

std::optional<AIIntent> FindHaulIntent(EntityID entity, EntityManager& em, const WorldMap&, const TileRegistry&, const ResourceRegistry&,
                                       const WeaponRegistry&, const EntitySpatialGrid& spatialGrid) {
    if (!IsValidActor(em, entity)) {
        return std::nullopt;
    }

    EntityID source = static_cast<EntityID>(-1);
    EntityID destination = static_cast<EntityID>(-1);
    std::string itemId;
    int amount = 0;

    if (!FindHaulPair(entity, em, spatialGrid, source, destination, itemId, amount)) {
        return std::nullopt;
    }

    AIContextComponent& context = em.aiContexts[entity];
    context.haulSourceId = source;
    context.haulDestinationId = destination;
    context.haulItemId = itemId;
    context.haulAmount = amount;

    AIIntent intent;
    intent.kind = AIIntentKind::MoveAdjacentToEntity;
    intent.targetEntity = source;
    intent.moveTask = "moving_to_haul_source";
    intent.actionTask = "hauling_pickup";
    intent.actionDuration = AISystemUtils::DEPOSIT_DURATION;
    intent.itemTarget = itemId;

    return intent;
}

} // namespace ai::intents
