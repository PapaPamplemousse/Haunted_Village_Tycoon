/**
 * @file AISystemCrafting.cpp
 * @brief AI routines for blacksmith-style crafting and village request fulfillment.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/AISystem.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/Pathfinder.hpp"
#include "systems/VillageRequestSystem.hpp"

#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

bool IsForgeAnvil(EntityID entity, const EntityManager& em) {
    return entity < em.active.size() && em.active[entity] && em.hasTag[entity] && em.tags[entity].prefabId == "FORGE_ANVIL" &&
           em.hasTransform[entity] && !(em.hasBlueprint[entity] && !em.blueprints[entity].isFinished);
}

EntityID FindNearestForgeAnvil(EntityID worker, const EntityManager& em, const EntitySpatialGrid& spatialGrid) {
    if (worker >= em.active.size() || !em.active[worker] || !em.hasTransform[worker]) {
        return static_cast<EntityID>(-1);
    }

    const float searchRadius = AISystemUtils::GetActionRadiusWorld(worker, em);
    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[worker].position, searchRadius, em);

    EntityID best = static_cast<EntityID>(-1);
    float bestDist = std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (!IsForgeAnvil(candidate, em)) {
            continue;
        }

        const float d = AISystemUtils::SquaredDistance(em.transforms[worker].position, em.transforms[candidate].position);

        if (d < bestDist) {
            bestDist = d;
            best = candidate;
        }
    }

    return best;
}

EntityID FindWeaponStorage(EntityID worker, const EntityManager& em, const EntitySpatialGrid& spatialGrid, const std::string& itemId) {
    if (worker >= em.active.size() || !em.active[worker] || !em.hasTransform[worker]) {
        return static_cast<EntityID>(-1);
    }

    const float searchRadius = AISystemUtils::GetActionRadiusWorld(worker, em);
    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[worker].position, searchRadius, em);

    EntityID best = static_cast<EntityID>(-1);
    float bestDist = std::numeric_limits<float>::infinity();

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

        const float d = AISystemUtils::SquaredDistance(em.transforms[worker].position, em.transforms[candidate].position);

        if (d < bestDist) {
            bestDist = d;
            best = candidate;
        }
    }

    return best;
}

} // namespace

bool AISystem::TryFindFulfillWeaponRequestJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                              const ResourceRegistry&, const WeaponRegistry& weaponReg,
                                              const EntitySpatialGrid& spatialGrid) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasVillageMember[entity] || !em.hasBehavior[entity] ||
        !em.hasTransform[entity] || !em.hasAIContext[entity]) {
        return false;
    }

    const EntityID villageId = em.villageMembers[entity].villageId;

    EntityID requestId = VillageRequestSystem::FindAssignedRequestForAssignee(em, VillageRequestType::WeaponNeeded, entity);

    if (requestId == static_cast<EntityID>(-1)) {
        requestId = VillageRequestSystem::FindOpenRequest(em, VillageRequestType::WeaponNeeded, villageId);

        if (requestId == static_cast<EntityID>(-1)) {
            return false;
        }

        if (!VillageRequestSystem::AssignRequest(em, requestId, entity)) {
            return false;
        }
    }

    const VillageRequestComponent& request = em.villageRequests[requestId];

    const std::string itemId = request.requestedItemId.empty() ? "SPEAR" : request.requestedItemId;

    const WeaponDef* weaponDef = weaponReg.GetWeaponDef(itemId);

    if (weaponDef == nullptr || weaponDef->requirements.empty()) {
        return false;
    }

    const std::unordered_map<std::string, int>& requirements = weaponDef->requirements;

    if (requirements.empty()) {
        return false;
    }

    if (!AISystemUtils::HasAccessibleMaterials(entity, em, spatialGrid, requirements)) {
        return false;
    }

    const EntityID forge = FindNearestForgeAnvil(entity, em, spatialGrid);

    if (forge == static_cast<EntityID>(-1)) {
        return false;
    }

    AIContextComponent& context = em.aiContexts[entity];
    context.activeRequestId = requestId;
    context.requestedCraftItemId = itemId;

    BehaviorComponent& behavior = em.behaviors[entity];

    if (AreEntitiesAdjacent(entity, forge, em)) {
        behavior.currentTask = "crafting_weapon";
        behavior.currentJobTarget = forge;
        behavior.currentItemTarget = itemId;
        behavior.hasJob = true;
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        behavior.stateTimer = 8.0f;
        return true;
    }

    std::vector<Vector2> path =
        Pathfinder::FindPathToAdjacentTile(em.transforms[entity].position, em.transforms[forge].position, map, tileReg, em, entity);

    if (path.empty()) {
        return false;
    }

    behavior.currentTask = "moving_to_forge";
    behavior.currentJobTarget = forge;
    behavior.currentItemTarget = itemId;
    behavior.hasJob = true;
    behavior.currentPath = std::move(path);
    behavior.currentPathIndex = 0;
    behavior.currentTarget = behavior.currentPath[0];
    behavior.isMoving = true;
    behavior.stateTimer = 0.0f;

    return true;
}
