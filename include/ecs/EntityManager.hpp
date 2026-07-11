#pragma once
#include "ecs/Components.hpp"

#include <vector>

/**
 * @class EntityManager
 * @brief Manages the creation, destruction, and memory allocation of all entities and their components.
 * Employs a Structure of Arrays (SoA) approach for maximum CPU cache coherence.
 */
class EntityManager {
public:
    EntityManager();
    ~EntityManager() = default;

    /**
     * @brief Creates a new empty entity.
     * @return EntityID The unique ID of the newly created entity.
     */
    EntityID CreateEntity();

    /**
     * @brief Marks an entity and its components for destruction.
     * @param id The ID of the entity to destroy.
     */
    void DestroyEntity(EntityID id);

    // =========================================================
    // COMPONENT ARRAYS (Structure of Arrays)
    // =========================================================

    std::vector<bool> active; // True if the entity is currently alive in the world

    std::vector<bool> hasTag;
    std::vector<TagComponent> tags;

    std::vector<bool> hasTransform;
    std::vector<TransformComponent> transforms;

    std::vector<bool> hasInventory;
    std::vector<InventoryComponent> inventories;

    std::vector<bool> hasStorage;
    std::vector<StorageComponent> storages;

    std::vector<bool> hasBlueprint;
    std::vector<BlueprintComponent> blueprints;

    std::vector<bool> hasHealth;
    std::vector<HealthComponent> healths;

    std::vector<bool> hasNeeds;
    std::vector<NeedsComponent> needs;

    std::vector<bool> hasProfession;
    std::vector<ProfessionComponent> professions;

    std::vector<bool> hasBehavior;
    std::vector<BehaviorComponent> behaviors;

    std::vector<bool> hasSprite;
    std::vector<SpriteComponent> sprites;

    std::vector<bool> hasStats;
    std::vector<StatsComponent> stats;

    std::vector<bool> hasConstruction;
    std::vector<ConstructionComponent> constructions;

    std::vector<bool> hasRoom;
    std::vector<RoomComponent> rooms;

    std::vector<bool> hasCost;
    std::vector<CostComponent> costs;

    std::vector<bool> hasDeconstruct;
    std::vector<DeconstructComponent> deconstructs;

    std::vector<bool> hasDoor;
    std::vector<DoorComponent> doors;

    std::vector<bool> hasHarvestable;
    std::vector<HarvestableComponent> harvestables;

    std::vector<bool> hasEquipment;
    std::vector<EquipmentComponent> equipments;

    std::vector<bool> hasWorkplace;
    std::vector<WorkplaceComponent> workplaces;

    std::vector<bool> hasLoot;
    std::vector<LootComponent> loots;

private:
    const size_t INITIAL_CAPACITY = 10000; // Pre-allocate memory for 10,000 entities
};
