/**
 * @file EntityManager.hpp
 * @brief Manages entity creation, destruction, and Structure of Arrays (SoA) component storage.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "ecs/Components.hpp"

#include <cstddef>
#include <vector>

/**
 * @class EntityManager
 * @brief Manages entity creation, destruction, and component storage.
 *
 * The manager uses a Structure of Arrays layout:
 * - one "hasX" flag array per optional component;
 * - one component data array per optional component.
 *
 * Component arrays are intentionally public for now because systems directly
 * operate on them. Internal creation/reset logic is centralized in private
 * helpers to avoid repetitive boilerplate.
 */
class EntityManager {
public:
    EntityManager();
    ~EntityManager() = default;

    /**
     * @brief Creates a new entity or recycles an inactive slot.
     * @return EntityID The unique ID of the allocated entity.
     */
    EntityID CreateEntity();

    /**
     * @brief Marks an entity as inactive.
     *
     * Components are not immediately cleared. They are reset when the slot is
     * reused by CreateEntity().
     */
    void DestroyEntity(EntityID id);

    // =========================================================
    // ENTITY STATE
    // =========================================================

    std::vector<bool> active;

    // =========================================================
    // IDENTITY & RENDERING
    // =========================================================

    std::vector<bool> hasTag;
    std::vector<TagComponent> tags;

    std::vector<bool> hasTransform;
    std::vector<TransformComponent> transforms;

    std::vector<bool> hasSprite;
    std::vector<SpriteComponent> sprites;

    // =========================================================
    // LOGISTICS, STORAGE & CONSTRUCTION
    // =========================================================

    std::vector<bool> hasInventory;
    std::vector<InventoryComponent> inventories;

    std::vector<bool> hasStorage;
    std::vector<StorageComponent> storages;

    std::vector<bool> hasRestSpot;
    std::vector<RestSpotComponent> restSpots;

    std::vector<bool> hasBlueprint;
    std::vector<BlueprintComponent> blueprints;

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

    std::vector<bool> hasLoot;
    std::vector<LootComponent> loots;

    // =========================================================
    // SOCIAL / SETTLEMENT
    // =========================================================

    std::vector<bool> hasVillage;
    std::vector<VillageComponent> villages;

    std::vector<bool> hasVillageMember;
    std::vector<VillageMemberComponent> villageMembers;

    std::vector<bool> hasFamily;
    std::vector<FamilyComponent> families;

    std::vector<bool> hasSocial;
    std::vector<SocialComponent> socials;

    std::vector<bool> hasFaction;
    std::vector<FactionComponent> factions;

    std::vector<bool> hasPersonality;
    std::vector<PersonalityComponent> personalities;

    std::vector<bool> hasWorkplace;
    std::vector<WorkplaceComponent> workplaces;

    // =========================================================
    // LIFE, AI & JOBS
    // =========================================================

    std::vector<bool> hasHealth;
    std::vector<HealthComponent> healths;

    std::vector<bool> hasNeeds;
    std::vector<NeedsComponent> needs;

    std::vector<bool> hasProfession;
    std::vector<ProfessionComponent> professions;

    std::vector<bool> hasBehavior;
    std::vector<BehaviorComponent> behaviors;

    std::vector<bool> hasVillageRequest;
    std::vector<VillageRequestComponent> villageRequests;

    std::vector<bool> hasAIContext;
    std::vector<AIContextComponent> aiContexts;

    std::vector<bool> hasStats;
    std::vector<StatsComponent> stats;

    std::vector<bool> hasEquipment;
    std::vector<EquipmentComponent> equipments;

private:
    static constexpr std::size_t INITIAL_CAPACITY = 10000;

    EntityID FindReusableSlot() const;

    void ReserveComponentStorage(std::size_t capacity);
    void AppendEntitySlot();
    void ResetComponentFlags(EntityID id);
};
