/**
 * @file AIStorageIntents.cpp
 * @brief Storage-related AI intents.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/AISystemUtils.hpp"
#include "systems/ai/AIIntentFinders.hpp"

#include <limits>
#include <string>
#include <vector>

namespace {

bool IsValidActor(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasTransform[entity] && em.hasBehavior[entity];
}

EntityID FindBestStorageForInventory(EntityID entity, const EntityManager& em, const EntitySpatialGrid& spatialGrid) {
    if (!IsValidActor(em, entity) || !em.hasInventory[entity]) {
        return static_cast<EntityID>(-1);
    }

    const float radius = AISystemUtils::GetActionRadiusWorld(entity, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, radius, em);

    EntityID bestStorage = static_cast<EntityID>(-1);
    float bestDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (candidate >= em.active.size() || !em.active[candidate] || !em.hasTransform[candidate] || !em.hasInventory[candidate] ||
            !em.hasStorage[candidate]) {
            continue;
        }

        if (candidate == entity) {
            continue;
        }

        if (em.hasBlueprint[candidate] && !em.blueprints[candidate].isFinished) {
            continue;
        }

        if (!AISystemUtils::HasAvailableStorageCapacity(candidate, em)) {
            continue;
        }

        if (!AISystemUtils::StorageCanAcceptFromInventory(candidate, em.inventories[entity], em)) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestStorage = candidate;
        }
    }

    return bestStorage;
}

} // namespace

namespace ai::intents {

std::optional<AIIntent> FindStoreIntent(EntityID entity, EntityManager& em, const WorldMap&, const TileRegistry&, const ResourceRegistry&,
                                        const WeaponRegistry&, const EntitySpatialGrid& spatialGrid) {
    if (!IsValidActor(em, entity) || !em.hasInventory[entity]) {
        return std::nullopt;
    }

    if (!AISystemUtils::HasAnyInventoryItem(em.inventories[entity])) {
        return std::nullopt;
    }

    const EntityID storage = FindBestStorageForInventory(entity, em, spatialGrid);

    if (storage == static_cast<EntityID>(-1)) {
        return std::nullopt;
    }

    AIIntent intent;
    intent.kind = AIIntentKind::MoveAdjacentToEntity;
    intent.targetEntity = storage;
    intent.moveTask = "moving_to_storage";
    intent.actionTask = "depositing";
    intent.actionDuration = AISystemUtils::DEPOSIT_DURATION;

    return intent;
}

} // namespace ai::intents
