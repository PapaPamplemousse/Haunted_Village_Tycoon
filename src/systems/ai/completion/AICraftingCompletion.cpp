/**
 * @file AICraftingCompletion.cpp
 * @brief Completion handlers for weapon crafting and crafted item delivery.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/AISystemUtils.hpp"
#include "systems/VillageRequestSystem.hpp"
#include "systems/ai/AICompletion.hpp"
#include "systems/ai/AITaskExecutor.hpp"

#include <algorithm>
#include <limits>
#include <unordered_map>

namespace {

std::unordered_map<std::string, int> GetCraftRequirementsForTask(const std::string& itemId) {
    if (itemId == "SPEAR") {
        return {{"WOOD", 4}, {"ROPE", 1}};
    }

    if (itemId == "IRON_AXE") {
        return {{"WOOD", 2}, {"IRON_INGOT", 1}};
    }

    if (itemId == "WOOD_BOW") {
        return {{"WOOD", 6}, {"ROPE", 2}};
    }

    return {};
}

EntityID FindNearestCompatibleStorage(EntityID worker, const std::string& itemId, const EntityManager& em,
                                      const EntitySpatialGrid& spatialGrid) {
    if (worker >= em.active.size() || !em.active[worker] || !em.hasTransform[worker]) {
        return static_cast<EntityID>(-1);
    }

    const float searchRadius = AISystemUtils::GetActionRadiusWorld(worker, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[worker].position, searchRadius, em);

    EntityID bestStorage = static_cast<EntityID>(-1);
    float bestDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (candidate >= em.active.size() || !em.active[candidate] || !em.hasTransform[candidate] || !em.hasInventory[candidate] ||
            !em.hasStorage[candidate]) {
            continue;
        }

        if (em.hasBlueprint[candidate] && !em.blueprints[candidate].isFinished) {
            continue;
        }

        if (em.hasBehavior[candidate]) {
            continue;
        }

        if (!AISystemUtils::StorageAcceptsItem(em.storages[candidate], itemId)) {
            continue;
        }

        if (!AISystemUtils::HasAvailableStorageCapacity(candidate, em)) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[worker].position, em.transforms[candidate].position);

        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestStorage = candidate;
        }
    }

    return bestStorage;
}

} // namespace

namespace ai::completion {

bool TryCompleteCraftingTask(const AICompletionContext& ctx) {
    EntityManager& em = ctx.em;
    const EntityID entity = ctx.entity;
    BehaviorComponent& behavior = ctx.behavior();

    if (behavior.currentTask == "crafting_weapon") {
        if (!em.hasAIContext[entity] || !em.hasInventory[entity]) {
            return true;
        }

        AIContextComponent& context = em.aiContexts[entity];

        const EntityID requestId = context.activeRequestId;
        const std::string itemId = behavior.currentItemTarget.empty() ? context.requestedCraftItemId : behavior.currentItemTarget;

        if (itemId.empty()) {
            if (requestId < em.active.size() && em.active[requestId] && em.hasVillageRequest[requestId]) {
                VillageRequestSystem::CancelRequest(em, requestId);
            }

            context.activeRequestId = static_cast<EntityID>(-1);
            context.requestedCraftItemId.clear();
            return true;
        }

        const std::unordered_map<std::string, int> requirements = GetCraftRequirementsForTask(itemId);

        bool crafted = false;

        if (!requirements.empty() && AISystemUtils::HasAccessibleMaterials(entity, em, ctx.spatialGrid, requirements) &&
            AISystemUtils::ConsumeAccessibleMaterials(entity, em, ctx.spatialGrid, requirements)) {
            em.inventories[entity].items[itemId] += 1;
            crafted = true;
        }

        if (!crafted) {
            // If the blacksmith cannot craft after accepting the request,
            // cancel the request to avoid a stuck active request.
            if (requestId < em.active.size() && em.active[requestId] && em.hasVillageRequest[requestId]) {
                VillageRequestSystem::CancelRequest(em, requestId);
            }

            context.activeRequestId = static_cast<EntityID>(-1);
            context.requestedCraftItemId.clear();
            return true;
        }

        const EntityID storage = FindNearestCompatibleStorage(entity, itemId, em, ctx.spatialGrid);

        if (storage == static_cast<EntityID>(-1)) {
            if (requestId < em.active.size() && em.active[requestId] && em.hasVillageRequest[requestId]) {
                VillageRequestSystem::CancelRequest(em, requestId);
            }

            context.activeRequestId = static_cast<EntityID>(-1);
            context.requestedCraftItemId.clear();
            return true;
        }

        if (AITaskExecutor::AreEntitiesAdjacent(entity, storage, em)) {
            if (AITaskExecutor::StartAction(entity, storage, em, "depositing_crafted_weapon", AISystemUtils::DEPOSIT_DURATION)) {
                behavior.currentItemTarget = itemId;
                return true;
            }
        }

        if (AITaskExecutor::StartMoveAdjacentToEntity(entity, storage, em, ctx.map, ctx.tileReg, "moving_to_crafted_weapon_storage",
                                                      "depositing_crafted_weapon", AISystemUtils::DEPOSIT_DURATION)) {
            behavior.currentItemTarget = itemId;
            return true;
        }

        if (requestId < em.active.size() && em.active[requestId] && em.hasVillageRequest[requestId]) {
            VillageRequestSystem::CancelRequest(em, requestId);
        }

        context.activeRequestId = static_cast<EntityID>(-1);
        context.requestedCraftItemId.clear();
        return true;
    }

    if (behavior.currentTask == "depositing_crafted_weapon") {
        if (!em.hasAIContext[entity] || !em.hasInventory[entity]) {
            return true;
        }

        AIContextComponent& context = em.aiContexts[entity];

        const EntityID requestId = context.activeRequestId;
        const EntityID destination = behavior.currentJobTarget;
        const std::string itemId = behavior.currentItemTarget.empty() ? context.requestedCraftItemId : behavior.currentItemTarget;

        bool deposited = false;

        if (destination < em.active.size() && em.active[destination] && em.hasInventory[destination] && em.hasStorage[destination] &&
            !itemId.empty() && AISystemUtils::StorageAcceptsItem(em.storages[destination], itemId) &&
            AISystemUtils::HasAvailableStorageCapacity(destination, em)) {
            const int removed = AISystemUtils::RemoveItemFromInventory(em.inventories[entity], itemId, 1);

            if (removed > 0) {
                em.inventories[destination].items[itemId] += removed;
                deposited = true;
            }
        }

        if (requestId < em.active.size() && em.active[requestId] && em.hasVillageRequest[requestId]) {
            if (deposited) {
                VillageRequestSystem::CompleteRequest(em, requestId);
            } else {
                VillageRequestSystem::CancelRequest(em, requestId);
            }
        }

        context.activeRequestId = static_cast<EntityID>(-1);
        context.requestedCraftItemId.clear();

        return true;
    }

    return false;
}

} // namespace ai::completion
