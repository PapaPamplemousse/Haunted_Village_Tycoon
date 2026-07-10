#include "ecs/EntityManager.hpp"

EntityManager::EntityManager() {
    // Pre-allocate memory to prevent runtime reallocations (performance boost)
    active.reserve(INITIAL_CAPACITY);

    hasTag.reserve(INITIAL_CAPACITY);
    tags.reserve(INITIAL_CAPACITY);

    hasTransform.reserve(INITIAL_CAPACITY);
    transforms.reserve(INITIAL_CAPACITY);

    hasInventory.reserve(INITIAL_CAPACITY);
    inventories.reserve(INITIAL_CAPACITY);

    hasBlueprint.reserve(INITIAL_CAPACITY);
    blueprints.reserve(INITIAL_CAPACITY);

    hasHealth.reserve(INITIAL_CAPACITY);
    healths.reserve(INITIAL_CAPACITY);

    hasNeeds.reserve(INITIAL_CAPACITY);
    needs.reserve(INITIAL_CAPACITY);

    hasProfession.reserve(INITIAL_CAPACITY);
    professions.reserve(INITIAL_CAPACITY);

    hasBehavior.reserve(INITIAL_CAPACITY);
    behaviors.reserve(INITIAL_CAPACITY);

    hasSprite.reserve(INITIAL_CAPACITY);
    sprites.reserve(INITIAL_CAPACITY);

    hasStats.reserve(INITIAL_CAPACITY);
    stats.reserve(INITIAL_CAPACITY);

    hasConstruction.reserve(INITIAL_CAPACITY);
    constructions.reserve(INITIAL_CAPACITY);

    hasRoom.reserve(INITIAL_CAPACITY);
    rooms.reserve(INITIAL_CAPACITY);

    hasCost.reserve(INITIAL_CAPACITY);
    costs.reserve(INITIAL_CAPACITY);

    hasDeconstruct.reserve(INITIAL_CAPACITY);
    deconstructs.reserve(INITIAL_CAPACITY);

    hasDoor.reserve(INITIAL_CAPACITY);
    doors.reserve(INITIAL_CAPACITY);

    hasHarvestable.reserve(INITIAL_CAPACITY);
    harvestables.reserve(INITIAL_CAPACITY);

    hasEquipment.reserve(INITIAL_CAPACITY);
    equipments.reserve(INITIAL_CAPACITY);
}

EntityID EntityManager::CreateEntity() {
    EntityID id = 0;
    bool foundDead = false;

    // 1. Try to find a dead entity slot to recycle (keeps arrays small)
    for (size_t i = 0; i < active.size(); ++i) {
        if (!active[i]) {
            id = i;
            foundDead = true;
            break;
        }
    }

    // 2. If no dead slot is found, expand the arrays
    if (!foundDead) {
        id = active.size();

        active.push_back(false);
        hasTag.push_back(false);
        tags.push_back({});
        hasTransform.push_back(false);
        transforms.push_back({});
        hasInventory.push_back(false);
        inventories.push_back({});
        hasBlueprint.push_back(false);
        blueprints.push_back({});
        hasHealth.push_back(false);
        healths.push_back({});
        hasNeeds.push_back(false);
        needs.push_back({});
        hasProfession.push_back(false);
        professions.push_back({});
        hasBehavior.push_back(false);
        behaviors.push_back({});
        hasSprite.push_back(false);
        sprites.push_back({});
        hasStats.push_back(false);
        stats.push_back({});
        hasConstruction.push_back(false);
        constructions.push_back({});
        hasRoom.push_back(false);
        rooms.push_back({});
        hasCost.push_back(false);
        costs.push_back({});
        hasDeconstruct.push_back(false);
        deconstructs.push_back({});
        hasDoor.push_back(false);
        doors.push_back({});
        hasHarvestable.push_back(false);
        harvestables.push_back({});
        hasEquipment.push_back(false);
        equipments.push_back({});
    }

    // 3. Initialize the new entity
    active[id] = true;

    // Reset all "hasComponent" flags to false
    hasTag[id] = false;
    hasTransform[id] = false;
    hasInventory[id] = false;
    hasBlueprint[id] = false;
    hasHealth[id] = false;
    hasNeeds[id] = false;
    hasProfession[id] = false;
    hasBehavior[id] = false;
    hasSprite[id] = false;
    hasStats[id] = false;
    hasConstruction[id] = false;
    hasRoom[id] = false;
    hasCost[id] = false;
    hasDeconstruct[id] = false;
    hasDoor[id] = false;
    hasHarvestable[id] = false;
    hasEquipment[id] = false;

    return id;
}

void EntityManager::DestroyEntity(EntityID id) {
    if (id < active.size()) {
        active[id] = false; // The slot is now available for recycling
    }
}
