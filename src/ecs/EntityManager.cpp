/**
 * @file EntityManager.cpp
 * @brief Implementation of the ECS EntityManager.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "ecs/EntityManager.hpp"

#include <initializer_list>
#include <vector>

namespace {

void ReserveFlagArrays(std::size_t capacity, std::initializer_list<std::vector<bool>*> flags) {
    for (std::vector<bool>* flagArray : flags) {
        flagArray->reserve(capacity);
    }
}

void PushInactiveFlags(std::initializer_list<std::vector<bool>*> flags) {
    for (std::vector<bool>* flagArray : flags) {
        flagArray->push_back(false);
    }
}

void ResetFlags(EntityID id, std::initializer_list<std::vector<bool>*> flags) {
    for (std::vector<bool>* flagArray : flags) {
        (*flagArray)[id] = false;
    }
}

template <typename... Vectors> void ReserveComponentArrays(std::size_t capacity, Vectors&... vectors) {
    (vectors.reserve(capacity), ...);
}

template <typename... Vectors> void PushDefaultComponents(Vectors&... vectors) {
    (vectors.push_back({}), ...);
}

} // namespace

EntityManager::EntityManager() {
    active.reserve(INITIAL_CAPACITY);
    ReserveComponentStorage(INITIAL_CAPACITY);
}

EntityID EntityManager::FindReusableSlot() const {
    for (EntityID id = 0; id < active.size(); ++id) {
        if (!active[id]) {
            return id;
        }
    }

    return static_cast<EntityID>(-1);
}

void EntityManager::ReserveComponentStorage(std::size_t capacity) {
    ReserveFlagArrays(capacity,
                      {&hasTag,          &hasTransform,     &hasSprite,    &hasInventory,   &hasStorage,   &hasRestSpot,    &hasBlueprint,
                       &hasConstruction, &hasRoom,          &hasCost,      &hasDeconstruct, &hasDoor,      &hasHarvestable, &hasLoot,
                       &hasVillage,      &hasVillageMember, &hasFamily,    &hasSocial,      &hasWorkplace, &hasHealth,      &hasNeeds,
                       &hasProfession,   &hasBehavior,      &hasAIContext, &hasStats,       &hasEquipment});

    ReserveComponentArrays(capacity, tags, transforms, sprites, inventories, storages, restSpots, blueprints, constructions, rooms, costs,
                           deconstructs, doors, harvestables, loots, villages, villageMembers, families, socials, workplaces, healths,
                           needs, professions, behaviors, aiContexts, stats, equipments);
}

void EntityManager::AppendEntitySlot() {
    active.push_back(false);

    PushInactiveFlags({&hasTag,          &hasTransform,     &hasSprite,    &hasInventory,   &hasStorage,   &hasRestSpot,    &hasBlueprint,
                       &hasConstruction, &hasRoom,          &hasCost,      &hasDeconstruct, &hasDoor,      &hasHarvestable, &hasLoot,
                       &hasVillage,      &hasVillageMember, &hasFamily,    &hasSocial,      &hasWorkplace, &hasHealth,      &hasNeeds,
                       &hasProfession,   &hasBehavior,      &hasAIContext, &hasStats,       &hasEquipment});

    PushDefaultComponents(tags, transforms, sprites, inventories, storages, restSpots, blueprints, constructions, rooms, costs,
                          deconstructs, doors, harvestables, loots, villages, villageMembers, families, socials, workplaces, healths, needs,
                          professions, behaviors, aiContexts, stats, equipments);
}

void EntityManager::ResetComponentFlags(EntityID id) {
    ResetFlags(id, {&hasTag,          &hasTransform,     &hasSprite,    &hasInventory,   &hasStorage,   &hasRestSpot,    &hasBlueprint,
                    &hasConstruction, &hasRoom,          &hasCost,      &hasDeconstruct, &hasDoor,      &hasHarvestable, &hasLoot,
                    &hasVillage,      &hasVillageMember, &hasFamily,    &hasSocial,      &hasWorkplace, &hasHealth,      &hasNeeds,
                    &hasProfession,   &hasBehavior,      &hasAIContext, &hasStats,       &hasEquipment});
}

EntityID EntityManager::CreateEntity() {
    EntityID id = FindReusableSlot();

    if (id == static_cast<EntityID>(-1)) {
        id = active.size();
        AppendEntitySlot();
    }

    active[id] = true;
    ResetComponentFlags(id);

    return id;
}

void EntityManager::DestroyEntity(EntityID id) {
    if (id >= active.size()) {
        return;
    }

    active[id] = false;
}
