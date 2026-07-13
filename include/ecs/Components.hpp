/**
 * @file Components.hpp
 * @brief Definitions for all ECS components (Tag, Transform, Logistics, AI, Needs, etc.).
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once
#include <raylib.h>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @typedef EntityID
 * @brief Represents a unique identifier for an entity in the game world.
 */
using EntityID = size_t;

// =========================================================
// IDENTITY & RENDERING
// =========================================================

/**
 * @struct TagComponent
 * @brief Stores the readable name and the prefab ID (from the .stv files).
 */
struct TagComponent {
    std::string name;
    std::string prefabId;
    std::string firstName = "";
    std::string species = "";
    std::string category = "";
    std::string genderModel = "undefined";
    std::string gender = "undefined";
    int age = 0;
};

/**
 * @struct TransformComponent
 * @brief Represents the physical location in the 2D world.
 */
struct TransformComponent {
    Vector2 position = {0.0f, 0.0f};
};

enum class SpriteFacing { Down, Up, Right, Left };

enum class SpritePose { Normal, Action };

enum class SpriteSheetMode { None, DirectionalAction, FurnitureState, Seasonal };

/**
 * @struct SpriteComponent
 * @brief Holds visual data for rendering. Supports primitive shapes and spritesheets.
 */
struct SpriteComponent {
    std::string texturePath = "square";

    Color tint = WHITE;

    // Rendered size in world pixels.
    float width = 32.0f;
    float height = 32.0f;

    bool isAnimated = false;

    // Spritesheet mode.
    bool useSpriteSheet = false;
    SpriteSheetMode sheetMode = SpriteSheetMode::None;

    int sheetColumns = 1;
    int sheetRows = 1;

    float frameWidth = 0.0f;
    float frameHeight = 0.0f;

    // Optional gaps in source pixels.
    float columnGap = 0.0f;
    float rowGap = 0.0f;

    // Entity directional/action state.
    SpriteFacing facing = SpriteFacing::Down;
    SpritePose pose = SpritePose::Normal;

    // Generic state for furniture/light/etc.
    // This can be toggled later by LightSystem, AI, interaction, etc.
    bool isInUse = false;
};

// =========================================================
// LOGISTICS & CONSTRUCTION
// =========================================================

/**
 * @struct InventoryComponent
 * @brief Stores items held by an entity (NPC backpack, chest, or tree drops).
 */
struct InventoryComponent {
    std::unordered_map<std::string, int> items;
};

/**
 * @struct StorageComponent
 * @brief Marks an entity as a storage container.
 *
 * If acceptedItems is empty, the storage accepts every item.
 * Otherwise, only listed item IDs are accepted.
 */
struct StorageComponent {
    int capacity = 0;
    std::vector<std::string> acceptedItems;
};

/**
 * @struct RestSpotComponent
 * @brief Marks an entity as a usable rest spot such as a bed, straw bed or double bed.
 *
 * Ownership fields are reserved for future private property / family ownership.
 */
struct RestSpotComponent {
    int capacity = 1;
    std::vector<EntityID> occupants;

    bool isPrivate = false;
    EntityID ownerVillageId = static_cast<EntityID>(-1);
    EntityID ownerFamilyId = static_cast<EntityID>(-1);
};

/**
 * @struct VillageComponent
 * @brief Marks an entity as the center of a village/settlement.
 */
struct VillageComponent {
    std::string name = "Unnamed Village";
    Vector2 rallyPoint = {0.0f, 0.0f};

    int populationLimit = 10;
    int currentPopulation = 0;
    int adultPopulation = 0;
    int childPopulation = 0;
};

/**
 * @struct VillageMemberComponent
 * @brief Associates an entity with a village.
 */
struct VillageMemberComponent {
    EntityID villageId = static_cast<EntityID>(-1);
};

/**
 * @struct FamilyComponent
 * @brief Stores lightweight family links.
 *
 * First version:
 * - partnerId can be used later for stable couples.
 * - parentA / parentB are mainly used for children.
 * - children is useful for debug and future family logic.
 */
struct FamilyComponent {
    EntityID partnerId = static_cast<EntityID>(-1);
    EntityID parentA = static_cast<EntityID>(-1);
    EntityID parentB = static_cast<EntityID>(-1);
    std::vector<EntityID> children;
};

/**
 * @struct RelationshipEntry
 * @brief Stores the relationship state between two entities.
 */
struct RelationshipEntry {
    EntityID otherId = static_cast<EntityID>(-1);

    float friendship = 0.0f;
    float romance = 0.0f;

    // Social V2.
    float trust = 0.0f;
    float respect = 0.0f;
    float resentment = 0.0f;
    float fear = 0.0f;

    bool friendshipAnnounced = false;
    bool romanceAnnounced = false;
    bool hatredAnnounced = false;
};

/**
 * @struct PersonalityComponent
 * @brief Stores personality traits and derived social modifiers for an entity.
 */
struct PersonalityComponent {
    std::vector<std::string> traits;

    // Normalized personality dimensions.
    // 0.0 = very low, 1.0 = very high.
    float sociability = 0.5f;
    float bravery = 0.5f;
    float kindness = 0.5f;
    float patience = 0.5f;
    float aggression = 0.0f;
    float loyalty = 0.5f;

    // Multipliers / modifiers used by systems.
    float resentmentGainMultiplier = 1.0f;
    float resentmentDecayMultiplier = 1.0f;
    float workScoreModifier = 0.0f;
};

/**
 * @struct SocialComponent
 * @brief Stores lightweight social relationships for an entity.
 */
struct SocialComponent {
    std::vector<RelationshipEntry> relationships;
};

/**
 * @struct BlueprintComponent
 * @brief Represents an unfinished structure. Requires materials to become active.
 */
struct BlueprintComponent {
    std::unordered_map<std::string, int> requiredMaterials;
    bool isFinished = false;
};

struct DropEntry {
    std::string itemId = "";
    int amount = 1;
    float chance = 1.0f;
};

/**
 * @struct HarvestableComponent
 * @brief Identifies natural resources that can be chopped, mined, or gathered.
 */
struct HarvestableComponent {
    std::string requiredTool = "none";
    std::vector<DropEntry> drops;
};

/**
 * @struct ConstructionComponent
 * @brief Identifies walls, doors, and fences. Used for room detection and pathfinding.
 */
struct ConstructionComponent {
    bool isWall = false;
    bool isDoor = false;
};

/**
 * @struct RoomComponent
 * @brief Represents an enclosed area generated by the RoomSystem.
 */
struct RoomComponent {
    std::string structureId;
    std::string name;
    int area = 0;
    std::vector<Vector2> floorTiles;

    // Semantic tags copied from StructureDef.
    bool isHousing = false;
    bool isBedroom = false;

    // Ownership fields for private bedrooms / family rooms.
    bool isPrivate = false;
    EntityID ownerVillageId = static_cast<EntityID>(-1);
    EntityID ownerFamilyId = static_cast<EntityID>(-1);
};

/**
 * @struct CostComponent
 * @brief Stores the original blueprint cost of the entity to calculate refunds.
 */
struct CostComponent {
    std::unordered_map<std::string, int> materials;
};

/**
 * @struct DeconstructComponent
 * @brief Marks an entity to be dismantled by a worker.
 */
struct DeconstructComponent {
    bool marked = true;
};

enum class DoorState { OPEN, CLOSED, LOCKED };

struct DoorComponent {
    DoorState state = DoorState::CLOSED;
    EntityID ownerId = 0; // ID du PNJ qui possède la clé/est propriétaire

    bool CanPass(EntityID actorId) {
        if (state == DoorState::OPEN)
            return true;
        if (state == DoorState::LOCKED)
            return (ownerId == actorId);
        return false; // CLOSED (il faut l'ouvrir avant)
    }
};

struct LootComponent {
    std::vector<DropEntry> drops;
};
// =========================================================
// LIFE, AI & JOBS
// =========================================================

/**
 * @struct StatsComponent
 * @brief Holds fixed RPG statistics.
 */
struct StatsComponent {
    float maxSpeed = 30.0f;
    float baseAttack = 1.0f;
    // Maximum range, in tiles, where this entity can look for jobs/targets.
    float actionRadiusTiles = 40.0f;
};

/**
 * @struct HealthComponent
 * @brief Tracks physical damage and life status.
 */
struct HealthComponent {
    float current = 100.0f;
    float max = 100.0f;
};

struct EquipmentComponent {
    std::string rightHandItemId = "";
    std::string rightHandToolType = "none";
    float rightHandDamage = 0.0f;

    std::string equipmentSlot = "";
};

struct NeedsComponent {
    float hunger = 100.0f;
    float maxHunger = 100.0f;

    // Fatigue model:
    // 0   = fully rested
    // max = exhausted
    float fatigue = 0.0f;
    float maxFatigue = 100.0f;

    bool collapsedFromFatigue = false;
};

enum class ProfessionAssignmentMode { Auto, Manual };

/**
 * @struct ProfessionComponent
 * @brief Determines what kind of jobs this entity is allowed to take.
 *
 * Auto:
 *   The ProfessionSystem may automatically assign this entity to available slots.
 *
 * Manual:
 *   The player controls the assignment.
 *   If currentProfession == "none", the entity intentionally remains unemployed.
 */
struct ProfessionComponent {
    std::string currentProfession = "none"; // e.g., "builder", "lumberjack"
    ProfessionAssignmentMode assignmentMode = ProfessionAssignmentMode::Auto;
};

/**170 * @struct BehaviorRule171 * @brief Parsed AI behavior rule.172 *173 * Examples:174 *   "wander"             -> name="wander",
 * arguments={}175 *   "hunt(human,rabbit)" -> name="hunt", arguments={"human", "rabbit"}176 */
struct BehaviorRule {
    std::string name;
    std::vector<std::string> arguments;
};

/**
 * @struct BehaviorComponent
 * @brief Stores innate capabilities for the AI (e.g., "hunt", "flee", "wander").
 */
struct BehaviorComponent {
    std::vector<std::string> innateCapabilities;
    std::vector<BehaviorRule> innateBehaviorRules;

    Vector2 currentTarget = {0.0f, 0.0f};
    bool isMoving = false;
    float stateTimer = 0.0f;
    float actionAccumulator = 0.0f;

    std::string currentTask = "idle";
    EntityID currentJobTarget = 0;
    bool hasJob = false;

    // Used by simple item-based AI actions such as eating.
    std::string currentItemTarget = "";

    EntityID reservedRestSpot = static_cast<EntityID>(-1);

    // Daily activity rules.
    // activityPeriod: "any", "diurnal", "nocturnal"
    std::string activityPeriod = "any";
    float workStartHour = 0.0f;
    float workEndHour = 24.0f;

    // Minimum number of carried items before trying to deposit into storage.
    int storeThreshold = 10;

    std::vector<Vector2> currentPath;
    size_t currentPathIndex = 0;
};

/**
 * @struct AIContextComponent
 * @brief Stores short-term AI memory and interruption state.
 *
 * This component is intentionally separated from BehaviorComponent:
 * - BehaviorComponent describes what the entity is doing.
 * - AIContextComponent stores context used to decide whether the entity should interrupt its current task.
 */
struct AIContextComponent {
    EntityID lastThreatId = static_cast<EntityID>(-1);
    float threatMemoryTimer = 0.0f;

    float currentTaskPriority = 0.0f;
    bool currentTaskInterruptible = true;

    EntityID careTargetId = static_cast<EntityID>(-1);

    // Hauling context.
    EntityID haulSourceId = static_cast<EntityID>(-1);
    EntityID haulDestinationId = static_cast<EntityID>(-1);
    std::string haulItemId = "";
    int haulAmount = 0;

    // Social economy context
    EntityID activeRequestId = static_cast<EntityID>(-1);
    std::string requestedCraftItemId = "";
};

enum class VillageRequestType { WeaponNeeded, ToolNeeded, FoodNeeded, ChildFoodNeeded, RepairNeeded, MedicineNeeded, FuelNeeded };

enum class VillageRequestStatus { Open, Assigned, Completed, Cancelled };

struct VillageRequestComponent {
    VillageRequestType type = VillageRequestType::WeaponNeeded;
    VillageRequestStatus status = VillageRequestStatus::Open;

    EntityID requesterId = static_cast<EntityID>(-1);
    EntityID assigneeId = static_cast<EntityID>(-1);
    EntityID targetEntityId = static_cast<EntityID>(-1);
    EntityID villageId = static_cast<EntityID>(-1);

    std::string requestedItemId = "";
    int amount = 1;

    float priority = 0.0f;
    bool socialImpactApplied = false;
};

struct JobSlot {
    std::string profession;
    EntityID workerId = static_cast<EntityID>(-1); // -1 signifie "Poste vacant"
};

struct WorkplaceComponent {
    std::vector<JobSlot> slots;
};
