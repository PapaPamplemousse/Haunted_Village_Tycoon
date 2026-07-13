/**
 * @file AISystemEquipment.cpp
 * @brief AI routines for finding and equipping weapons.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/AISystem.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/Pathfinder.hpp"
#include "systems/VillageRequestSystem.hpp"

#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

bool IsWeaponItem(const std::string& itemId) {
    return itemId == "SPEAR" || itemId == "IRON_AXE" || itemId == "WOOD_BOW";
}

std::string FindFirstWeaponInInventory(const InventoryComponent& inventory, const WeaponRegistry& weaponReg) {
    for (const auto& item : inventory.items) {
        if (item.second <= 0) {
            continue;
        }

        const WeaponDef* weapon = weaponReg.GetWeaponDef(item.first);

        if (weapon != nullptr && weapon->damage > 0.0f) {
            return item.first;
        }
    }

    return "";
}

float GetWeaponDamage(const std::string& itemId) {
    if (itemId == "IRON_AXE") {
        return 12.0f;
    }

    if (itemId == "WOOD_BOW") {
        return 8.0f;
    }

    if (itemId == "SPEAR") {
        return 10.0f;
    }

    return 1.0f;
}

std::string GetWeaponToolType(const std::string& itemId) {
    if (itemId == "IRON_AXE") {
        return "axe";
    }

    return "weapon";
}

bool HasWeaponEquipped(EntityID entity, const EntityManager& em) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasEquipment[entity]) {
        return false;
    }

    const EquipmentComponent& equipment = em.equipments[entity];

    return !equipment.rightHandItemId.empty() && equipment.rightHandDamage > 0.0f;
}

} // namespace

bool AISystem::TryFindEquipWeaponJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                     const WeaponRegistry& weaponReg, const EntitySpatialGrid& spatialGrid) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasBehavior[entity] || !em.hasTransform[entity]) {
        return false;
    }

    if (HasWeaponEquipped(entity, em)) {
        return false;
    }

    const float searchRadius = AISystemUtils::GetActionRadiusWorld(entity, em);
    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, searchRadius, em);

    EntityID bestStorage = static_cast<EntityID>(-1);
    std::string bestWeapon;
    float bestDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID storage : candidates) {
        if (storage >= em.active.size() || !em.active[storage] || !em.hasTransform[storage] || !em.hasInventory[storage] ||
            !em.hasStorage[storage]) {
            continue;
        }

        if (em.hasBlueprint[storage] && !em.blueprints[storage].isFinished) {
            continue;
        }

        if (em.hasBehavior[storage]) {
            continue;
        }

        const std::string weapon = FindFirstWeaponInInventory(em.inventories[storage], weaponReg);

        if (weapon.empty()) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[storage].position);

        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestStorage = storage;
            bestWeapon = weapon;
        }
    }

    if (bestStorage == static_cast<EntityID>(-1) || bestWeapon.empty()) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[entity];

    if (AreEntitiesAdjacent(entity, bestStorage, em)) {
        behavior.currentTask = "equipping_weapon";
        behavior.currentJobTarget = bestStorage;
        behavior.currentItemTarget = bestWeapon;
        behavior.hasJob = true;
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        behavior.stateTimer = 0.4f;
        return true;
    }

    std::vector<Vector2> path =
        Pathfinder::FindPathToAdjacentTile(em.transforms[entity].position, em.transforms[bestStorage].position, map, tileReg, em, entity);

    if (path.empty()) {
        return false;
    }

    behavior.currentTask = "moving_to_weapon_storage";
    behavior.currentJobTarget = bestStorage;
    behavior.currentItemTarget = bestWeapon;
    behavior.hasJob = true;
    behavior.currentPath = std::move(path);
    behavior.currentPathIndex = 0;
    behavior.currentTarget = behavior.currentPath[0];
    behavior.isMoving = true;

    return true;
}

bool AISystem::TryFindRequestWeaponJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                       const EntitySpatialGrid& spatialGrid) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasVillageMember[entity] || !em.hasBehavior[entity] ||
        !em.hasTransform[entity]) {
        return false;
    }

    if (HasWeaponEquipped(entity, em)) {
        return false;
    }

    const EntityID villageId = em.villageMembers[entity].villageId;
    const std::string requestedWeapon = "SPEAR";

    if (VillageRequestSystem::HasOpenRequestByRequester(em, VillageRequestType::WeaponNeeded, entity, requestedWeapon)) {
        em.behaviors[entity].currentTask = "waiting_for_weapon";
        em.behaviors[entity].stateTimer = 2.0f;
        return true;
    }

    const float searchRadius = AISystemUtils::GetActionRadiusWorld(entity, em);
    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, searchRadius, em);

    EntityID bestBlacksmith = static_cast<EntityID>(-1);
    float bestDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (candidate == entity || candidate >= em.active.size() || !em.active[candidate] || !em.hasTransform[candidate] ||
            !em.hasProfession[candidate] || !em.hasVillageMember[candidate]) {
            continue;
        }

        if (em.villageMembers[candidate].villageId != villageId) {
            continue;
        }

        if (em.professions[candidate].currentProfession != "blacksmith") {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestBlacksmith = candidate;
        }
    }

    if (bestBlacksmith == static_cast<EntityID>(-1)) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[entity];

    if (AreEntitiesAdjacent(entity, bestBlacksmith, em)) {
        behavior.currentTask = "requesting_weapon";
        behavior.currentJobTarget = bestBlacksmith;
        behavior.currentItemTarget = requestedWeapon;
        behavior.hasJob = true;
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        behavior.stateTimer = 1.2f;
        return true;
    }

    std::vector<Vector2> path = Pathfinder::FindPathToAdjacentTile(em.transforms[entity].position, em.transforms[bestBlacksmith].position,
                                                                   map, tileReg, em, entity);

    if (path.empty()) {
        return false;
    }

    behavior.currentTask = "moving_to_request_weapon";
    behavior.currentJobTarget = bestBlacksmith;
    behavior.currentItemTarget = requestedWeapon;
    behavior.hasJob = true;
    behavior.currentPath = std::move(path);
    behavior.currentPathIndex = 0;
    behavior.currentTarget = behavior.currentPath[0];
    behavior.isMoving = true;
    behavior.stateTimer = 0.0f;

    return true;
}
