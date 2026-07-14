/**
 * @file AIEquipmentIntents.cpp
 * @brief Equipment and weapon request AI intents.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Config.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/ai/AIIntentFinders.hpp"

#include <limits>
#include <string>
#include <vector>

namespace {

bool IsValidActor(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasTransform[entity] && em.hasBehavior[entity];
}

bool HasUsableWeaponEquipped(const EntityManager& em, EntityID entity) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasEquipment[entity]) {
        return false;
    }

    return em.equipments[entity].rightHandDamage > 0.0f;
}

bool IsWeaponItem(const WeaponRegistry& weaponReg, const std::string& itemId) {
    return weaponReg.GetWeaponDef(itemId) != nullptr;
}

EntityID FindNearestWeaponStorage(EntityID entity, const EntityManager& em, const WeaponRegistry& weaponReg,
                                  const EntitySpatialGrid& spatialGrid, std::string& outWeaponItem) {
    if (!IsValidActor(em, entity)) {
        return static_cast<EntityID>(-1);
    }

    const float radius = AISystemUtils::GetActionRadiusWorld(entity, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, radius, em);

    EntityID bestStorage = static_cast<EntityID>(-1);
    float bestDistanceSq = std::numeric_limits<float>::infinity();
    std::string bestWeapon;

    for (EntityID candidate : candidates) {
        if (candidate >= em.active.size() || !em.active[candidate] || !em.hasTransform[candidate] || !em.hasInventory[candidate] ||
            !em.hasStorage[candidate]) {
            continue;
        }

        if (em.hasBlueprint[candidate] && !em.blueprints[candidate].isFinished) {
            continue;
        }

        for (const auto& item : em.inventories[candidate].items) {
            if (item.second <= 0) {
                continue;
            }

            if (!IsWeaponItem(weaponReg, item.first)) {
                continue;
            }

            const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

            if (distanceSq < bestDistanceSq) {
                bestDistanceSq = distanceSq;
                bestStorage = candidate;
                bestWeapon = item.first;
            }
        }
    }

    outWeaponItem = bestWeapon;
    return bestStorage;
}

bool IsBlacksmith(EntityID candidate, const EntityManager& em) {
    return candidate < em.active.size() && em.active[candidate] && em.hasTransform[candidate] && em.hasProfession[candidate] &&
           em.professions[candidate].currentProfession == "blacksmith";
}

bool SameVillage(EntityID a, EntityID b, const EntityManager& em) {
    return a < em.active.size() && b < em.active.size() && em.active[a] && em.active[b] && em.hasVillageMember[a] &&
           em.hasVillageMember[b] && em.villageMembers[a].villageId == em.villageMembers[b].villageId;
}

EntityID FindNearestBlacksmith(EntityID entity, const EntityManager& em, const EntitySpatialGrid& spatialGrid) {
    if (!IsValidActor(em, entity)) {
        return static_cast<EntityID>(-1);
    }

    const float radius = AISystemUtils::GetActionRadiusWorld(entity, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, radius, em);

    EntityID best = static_cast<EntityID>(-1);
    float bestDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (candidate == entity || !IsBlacksmith(candidate, em) || !SameVillage(entity, candidate, em)) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            best = candidate;
        }
    }

    return best;
}

} // namespace

namespace ai::intents {

std::optional<AIIntent> FindEquipWeaponIntent(EntityID entity, EntityManager& em, const WorldMap&, const TileRegistry&,
                                              const ResourceRegistry&, const WeaponRegistry& weaponReg,
                                              const EntitySpatialGrid& spatialGrid) {
    if (!IsValidActor(em, entity)) {
        return std::nullopt;
    }

    if (HasUsableWeaponEquipped(em, entity)) {
        return std::nullopt;
    }

    std::string weaponItem;
    const EntityID storage = FindNearestWeaponStorage(entity, em, weaponReg, spatialGrid, weaponItem);

    if (storage == static_cast<EntityID>(-1) || weaponItem.empty()) {
        return std::nullopt;
    }

    AIIntent intent;
    intent.kind = AIIntentKind::MoveAdjacentToEntity;
    intent.targetEntity = storage;
    intent.moveTask = "moving_to_equip_weapon";
    intent.actionTask = "equipping_weapon";
    intent.actionDuration = 0.8f;
    intent.itemTarget = weaponItem;

    return intent;
}

std::optional<AIIntent> FindRequestWeaponIntent(EntityID entity, EntityManager& em, const WorldMap&, const TileRegistry&,
                                                const ResourceRegistry&, const WeaponRegistry&, const EntitySpatialGrid& spatialGrid) {
    if (!IsValidActor(em, entity)) {
        return std::nullopt;
    }

    if (HasUsableWeaponEquipped(em, entity)) {
        return std::nullopt;
    }

    const EntityID blacksmith = FindNearestBlacksmith(entity, em, spatialGrid);

    if (blacksmith == static_cast<EntityID>(-1)) {
        return std::nullopt;
    }

    AIIntent intent;
    intent.kind = AIIntentKind::MoveAdjacentToEntity;
    intent.targetEntity = blacksmith;
    intent.moveTask = "moving_to_request_weapon";
    intent.actionTask = "requesting_weapon";
    intent.actionDuration = 1.0f;

    // V1 default request.
    // Later, this can be selected by profession / threat level.
    intent.itemTarget = "SPEAR";

    return intent;
}

} // namespace ai::intents
