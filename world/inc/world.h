/**
 * @file world.h
 * @brief Core world representation, including tiles, objects, and buildings.
 *
 * This module defines the data structures that represent the game world,
 * including map tiles, interactive objects, and building detection rules.
 * It provides a foundation for world generation, room classification,
 * and gameplay logic related to structures and environment interaction.
 *
 * @date 2025-10-23
 * @author Hugo
 */

#ifndef WORLD_H
#define WORLD_H

#include "raylib.h"
#include <stdbool.h>
#include <stdint.h>
/**
 * @def MAP_WIDTH
 * @brief Width of the game map in tiles.
 */
#define MAP_WIDTH 200

/**
 * @def MAP_HEIGHT
 * @brief Height of the game map in tiles.
 */
#define MAP_HEIGHT 200

/**
 * @def TILE_SIZE
 * @brief Size of one tile in pixels (for rendering and placement).
 */
#define TILE_SIZE 64

/**
 * @def MAX_BUILDINGS
 * @brief Maximum number of buildings that can be tracked simultaneously.
 */
#define MAX_BUILDINGS 100

/** Maximum characters allowed for structure aura names. */
#define STRUCTURE_AURA_NAME_MAX 64
/** Maximum characters allowed for structure aura description text. */
#define STRUCTURE_AURA_DESC_MAX 160
/** Maximum characters allowed for structure occupant description text. */
#define STRUCTURE_OCCUPANT_DESC_MAX 128
/** Maximum characters allowed for structure trigger/action description text. */
#define STRUCTURE_TRIGGER_DESC_MAX 192
#define BUILDING_SPECIES_NAME_MAX 32
#define STRUCTURE_MAX_RESIDENT_ROLES 8
#define BUILDING_ROLE_NAME_MAX 32

// Tune for your target GPU. 128×128 balances rebuild cost vs draw calls.
// You can go 64×64 on very low-end GPUs or 256×256 for fewer textures.

#define CHUNK_W 32
#define CHUNK_H 32

/** Maximum number of explicit cluster members that can be attached to a structure definition. */
#define STRUCTURE_CLUSTER_MAX_MEMBERS 15

/** Maximum length for cluster identifiers used to group related blueprints. */
#define STRUCTURE_CLUSTER_NAME_MAX 32

/** Maximum number of object requirements used to identify a structure's interior. */
#define STRUCTURE_MAX_REQUIREMENTS 15

// -----------------------------------------------------------------------------
// ENUMERATIONS
// -----------------------------------------------------------------------------

/**
 * @enum ObjectTypeID
 * @brief Unique identifiers for all object types available in the world.
 */
typedef enum
{
    OBJ_NONE = 0, /**< Empty space / no object */

    // --- Furniture ---
    OBJ_BED_SMALL  = 1, /**< Small bed */
    OBJ_BED_LARGE  = 2, /**< Large bed */
    OBJ_TABLE_WOOD = 3, /**< Wooden table */
    OBJ_CHAIR_WOOD = 4, /**< Wooden chair */

    // --- Utility ---
    OBJ_TORCH_WALL = 5, /**< Wall-mounted torch */
    OBJ_WORKBENCH  = 6, /**< Workbench / crafting table */

    // --- Storage ---
    OBJ_CHEST_WOOD = 7, /**< Wooden chest */
    OBJ_CRATE      = 8, /**< Crate */

    // --- Structures ---
    OBJ_DOOR_WOOD  = 9,  /**< Wooden door */
    OBJ_WALL_STONE = 10, /**< Stone wall */
    OBJ_WALL_WOOD  = 11, /**< Wooden wall */

    // --- Decoration ---
    OBJ_DECOR_PLANT = 12, /**< Potted plant */
    OBJ_BONE_PILE   = 13, /**< Bones */

    // --- Resources / Nature ---
    OBJ_ROCK        = 14, /**< Rock */
    OBJ_TREE        = 15, /**< Tree */
    OBJ_DEAD_TREE   = 16, /**< Dead tree */
    OBJ_STDBUSH     = 17, /**< Bush */
    OBJ_STDBUSH_DRY = 18, /**< Dry bush */

    // --- Hazards / Special ---
    OBJ_SULFUR_VENT   = 19, /**< Sulfur vent */
    OBJ_FIREPIT       = 20, /**< Exterior fire pit */
    OBJ_ALTAR         = 21, /**< Altar */
    OBJ_CAULDRON      = 22, /**< Bubbling witch cauldron */
    OBJ_TOTEM_BLOOD   = 23, /**< Bloodied totem pole */
    OBJ_RITUAL_CIRCLE = 24, /**< Ritual circle marker */
    OBJ_GALLOW        = 25, /**< Execution gallows */
    OBJ_MEAT_HOOK     = 26, /**< Rusted meat hook rack */
    OBJ_VOID_OBELISK  = 27, /**< Void-touched obelisk */
    OBJ_PLAGUE_POD    = 28, /**< Bloated plague pod */

    OBJ_COUNT /**< Sentinel (number of object types) */
} ObjectTypeID;

typedef enum
{
    ROOM_NONE = 0,  /**< Unclassified or exterior space. */
    ROOM_BEDROOM,   /**< Legacy generic bedroom classification. */
    ROOM_KITCHEN,   /**< Legacy generic kitchen classification. */
    ROOM_HUT,       /**< Legacy generic hut classification. */
    ROOM_CRYPT,     /**< Legacy generic crypt classification. */
    ROOM_SANCTUARY, /**< Legacy generic sanctuary classification. */
    ROOM_HOUSE,     /**< Legacy generic house classification. */
    ROOM_LARGEROOM, /**< Legacy large-room classification. */

    // --- Structure specific identifiers ---
    ROOM_CANNIBAL_DEN,       /**< Cannibal den hut classification. */
    ROOM_CANNIBAL_LONGHOUSE, /**< Cannibal clan longhouse anchor. */
    ROOM_BUTCHER_TENT,       /**< Cannibal butcher / cook tent. */
    ROOM_SHAMAN_HUT,         /**< Cannibal shaman ritual hut. */
    ROOM_BONE_PIT,           /**< Trophy bone pit. */
    ROOM_WHISPERING_CRYPT,   /**< Whispering crypt interior. */
    ROOM_FORSAKEN_RUIN,      /**< Forsaken ruin remains. */
    ROOM_DESERTED_HOME,      /**< Deserted homestead. */
    ROOM_BLOODBOUND_TEMPLE,  /**< Bloodbound temple sanctum. */
    ROOM_HEXSPEAKER_HOVEL,   /**< Hexspeaker witch hovel. */
    ROOM_SORROW_GALLOWS,     /**< Sorrow gallows execution site. */
    ROOM_BLOODROSE_GARDEN,   /**< Bloodrose ritual garden. */
    ROOM_FLESH_PIT,          /**< Flesh pit butchery. */
    ROOM_VOID_OBELISK,       /**< Void obelisk chamber. */
    ROOM_PLAGUE_NURSERY,     /**< Plague nursery cocoon cluster. */

    ROOM_COUNT
} RoomTypeID;

/**
 * @enum TileTypeID
 * @brief Identifiers for basic terrain tile types.
 */
typedef enum
{
    TILE_GRASS = 0,     /**< Walkable grass tile */
    TILE_WATER,         /**< Non-walkable water tile */
    TILE_LAVA,          /**< Hazardous lava tile */
    TILE_FOREST,        /**< Dense forest (slower movement) */
    TILE_PLAIN,         /**< Open plain */
    TILE_SAVANNA,       /**< Dry savanna grassland */
    TILE_TUNDRA,        /**< Cold tundra (slow movement) */
    TILE_TUNDRA_2,      /**< Cold tundra (slow movement) */
    TILE_HELL,          /**< Infernal ground (extreme heat) */
    TILE_CURSED_FOREST, /**< Cursed: Rare Forest Variant */
    TILE_SWAMP,         /**< Swamp */
    TILE_DESERT,        /**< Desertt */
    TILE_MOUNTAIN,      /**< Mountain */
    TILE_POISON,        /**< Toxic liquid hazard */
    TILE_WOOD_FLOOR,    /**< Interior plank flooring */
    TILE_STRAW_FLOOR,   /**< Packed straw flooring */
    TILE_STONE_FLOOR,   /**< Cut stone flooring */
    TILE_MUD_ROAD,      /**< Mud road path */
    TILE_MAX,           /**< Total number of defined tile types */
} TileTypeID;

/**
 * @enum TileCategory
 * @brief High-level tile classification used for pathfinding and generation.
 */
typedef enum
{
    TILE_CATEGORY_GROUND,   /**< General ground or floor */
    TILE_CATEGORY_WATER,    /**< Water or liquid tile */
    TILE_CATEGORY_TREE,     /**< Tree or natural obstacle */
    TILE_CATEGORY_ROAD,     /**< Road or paved surface */
    TILE_CATEGORY_OBSTACLE, /**< Permanent/Hard obstacle (mountain, large rocks) */
    TILE_CATEGORY_HAZARD    /**< Hazardous terrain (lava, poison) */
} TileCategory;

/**
 * @brief Enumeration of all possible biome types in the game world.
 */
typedef enum
{
    BIO_FOREST,   ///< Forest biome.
    BIO_PLAIN,    ///< Plain/Grassland biome.
    BIO_SAVANNA,  ///< Savanna biome.
    BIO_TUNDRA,   ///< Tundra/Frozen biome.
    BIO_DESERT,   ///< Desert/Arid biome.
    BIO_SWAMP,    ///< Swamp/Marsh biome.
    BIO_MOUNTAIN, ///< Mountain/High altitude biome.
    BIO_CURSED,   ///< Cursed/Corrupted biome.
    BIO_HELL,     ///< Hell/Infernal biome.
    BIO_MAX       ///< Biome counter
} BiomeKind;

/**
 * @brief Enumeration of the different types of world structures.
 *
 * This list defines the specific structure blueprints available for generation.
 */
typedef enum
{
    STRUCT_HUT_CANNIBAL,        ///< A small, primitive hut, typically for cannibals.
    STRUCT_CANNIBAL_LONGHOUSE,  ///< Communal hall anchoring a cannibal camp.
    STRUCT_CANNIBAL_COOK_TENT,  ///< Butcher's tent with racks and fires.
    STRUCT_CANNIBAL_SHAMAN_HUT, ///< Fetish-lined sanctum for rituals.
    STRUCT_CANNIBAL_BONE_PIT,   ///< Pit of trophies and improvised cages.
    STRUCT_CRYPT,               ///< An underground tomb or burial chamber.
    STRUCT_RUIN,                ///< Remains of a destroyed building or wall.
    STRUCT_VILLAGE_HOUSE,       ///< A standard residential building in a village.
    STRUCT_TEMPLE,              ///< A large, religious building.
    STRUCT_WITCH_HOVEL,         ///< Fetid hut where occult rituals take place.
    STRUCT_GALLOWS,             ///< Execution site flanked by ominous effigies.
    STRUCT_BLOOD_GARDEN,        ///< Ritual garden drenched in blood offerings.
    STRUCT_FLESH_PIT,           ///< Grisly pit where butchers discard offerings.
    STRUCT_VOID_OBELISK,        ///< Obelisk radiating oppressive void energy.
    STRUCT_PLAGUE_NURSERY,      ///< Cocoon cluster breeding the cursed.
    STRUCT_COUNT                ///< The total number of defined structure kinds (must be the last entry).
} StructureKind;

typedef enum
{
    ENTITY_TYPE_INVALID = -1,

    ENTITY_TYPE_CURSED_ZOMBIE = 0,
    ENTITY_TYPE_CANNIBAL,
    ENTITY_TYPE_CANNIBAL_WOMAN,
    ENTITY_TYPE_CANNIBAL_CHILD,
    ENTITY_TYPE_CANNIBAL_SCOUT,
    ENTITY_TYPE_CANNIBAL_COOK,
    ENTITY_TYPE_CANNIBAL_SHAMAN,
    ENTITY_TYPE_CANNIBAL_CHIEFTAIN,
    ENTITY_TYPE_CANNIBAL_ZEALOT,
    ENTITY_TYPE_CANNIBAL_ELDER,
    ENTITY_TYPE_CANNIBAL_BERSERKER,
    ENTITY_TYPE_VILLAGER,
    ENTITY_TYPE_VILLAGER_CHILD,
    ENTITY_TYPE_GUARD,
    ENTITY_TYPE_GUARD_CAPTAIN,
    ENTITY_TYPE_FARMER,
    ENTITY_TYPE_FISHERMAN,
    ENTITY_TYPE_LUMBERJACK,
    ENTITY_TYPE_MINER,
    ENTITY_TYPE_BLACKSMITH,
    ENTITY_TYPE_BAKER,
    ENTITY_TYPE_APOTHECARY,
    ENTITY_TYPE_DOCTOR,
    ENTITY_TYPE_GRAVEDIGGER,
    ENTITY_TYPE_PRIEST,
    ENTITY_TYPE_ACOLYTE,
    ENTITY_TYPE_TEACHER,
    ENTITY_TYPE_TAVERNKEEPER,
    ENTITY_TYPE_MERCHANT,
    ENTITY_TYPE_WATCHDOG,
    ENTITY_TYPE_HORSE,
    ENTITY_TYPE_GOAT,
    ENTITY_TYPE_PIG,
    ENTITY_TYPE_CHICKEN,
    ENTITY_TYPE_CROW,
    ENTITY_TYPE_RAT,
    ENTITY_TYPE_DEER,
    ENTITY_TYPE_GHOULHOUND,
    ENTITY_TYPE_WENDIGO,
    ENTITY_TYPE_MARSH_HORROR,
    ENTITY_TYPE_BOG_FIEND,
    ENTITY_TYPE_CORPSE_BOAR,
    ENTITY_TYPE_BLIGHT_ELK,
    ENTITY_TYPE_DIRE_WOLF,
    ENTITY_TYPE_CAVE_SPIDER,
    ENTITY_TYPE_VAMPIRE_NOBLE,
    ENTITY_TYPE_VAMPIRE_THRALL,
    ENTITY_TYPE_NOSFERATU,
    ENTITY_TYPE_BLOOD_PRIEST,
    ENTITY_TYPE_CRIMSON_BAT,
    ENTITY_TYPE_CULTIST_INITIATE,
    ENTITY_TYPE_CULTIST_ADEPT,
    ENTITY_TYPE_CULT_LEADER,
    ENTITY_TYPE_OCCULT_SCHOLAR,
    ENTITY_TYPE_SHADOWBOUND_ACOLYTE,
    ENTITY_TYPE_POSSESSED_VILLAGER,
    ENTITY_TYPE_DOOMSPEAKER,
    ENTITY_TYPE_BUTCHER_PROPHET,
    ENTITY_TYPE_RESTLESS_SPIRIT,
    ENTITY_TYPE_GRAVEBOUND_CORPSE,
    ENTITY_TYPE_FALSE_RESURRECTION,
    ENTITY_TYPE_BONEWALKER,
    ENTITY_TYPE_PLAGUE_DEAD,
    ENTITY_TYPE_WRAITH,
    ENTITY_TYPE_HAUNTING_SHADE,
    ENTITY_TYPE_SPECTRAL_BRIDE,
    ENTITY_TYPE_CRYPT_LORD,
    ENTITY_TYPE_SHADOW_PHANTOM,
    ENTITY_TYPE_ECHOED_VOICE,
    ENTITY_TYPE_MIRROR_SHADE,
    ENTITY_TYPE_NAMELESS_MOURNER,
    ENTITY_TYPE_NIGHTMARE_APPARITION,
    ENTITY_TYPE_WELL_SPECTER,
    ENTITY_TYPE_ASH_WIDOW,
    ENTITY_TYPE_CLOCKWORK_WRAITH,
    ENTITY_TYPE_TRAVELING_MERCHANT,
    ENTITY_TYPE_PILGRIM_BAND,
    ENTITY_TYPE_INQUISITION_ENVOY,
    ENTITY_TYPE_NEIGHBOR_DELEGATION,
    ENTITY_TYPE_TRAVELING_PERFORMER,
    ENTITY_TYPE_WITCH_HUNTER,
    ENTITY_TYPE_MISSIONARY,
    ENTITY_TYPE_GRAVEDUST_PEDDLER,
    ENTITY_TYPE_WHISPERER_BENEATH,
    ENTITY_TYPE_PALE_CHILD,
    ENTITY_TYPE_BLOOD_MOON_APPARITION,
    ENTITY_TYPE_ARCHITECT_GHOST,
    ENTITY_TYPE_NAMELESS_ARCHIVIST,
    ENTITY_TYPE_THOUSAND_TEETH,
    ENTITY_TYPE_HOLLOW_WATCHER,
    ENTITY_TYPE_CANDLE_EATER,
    ENTITY_TYPE_FLESH_LIBRARIAN,
    ENTITY_TYPE_CRIMSON_CHOIR,
    ENTITY_TYPE_HEART_OF_TOWN,
    ENTITY_TYPE_DREAMING_GOD,
    ENTITY_TYPE_IMP,
    ENTITY_TYPE_LICH,
    ENTITY_TYPE_COUNT
} EntitiesTypeID;

typedef enum
{
    ENTITY_FLAG_HOSTILE     = 1u << 0,
    ENTITY_FLAG_MOBILE      = 1u << 1,
    ENTITY_FLAG_INTELLIGENT = 1u << 2,
    ENTITY_FLAG_UNDEAD      = 1u << 3,
    ENTITY_FLAG_MERCHANT    = 1u << 4,
    ENTITY_FLAG_ANIMAL      = 1u << 5
} EntityFlags;

// -----------------------------------------------------------------------------
// STRUCTURES
// -----------------------------------------------------------------------------

/**
 * @struct ObjectType
 * @brief Describes a type of placeable object in the world.
 *
 * This defines static data shared by all instances of that object,
 * such as its visual appearance, physical properties, and category.
 */
typedef struct
{
    ObjectTypeID id;          /**< Unique identifier for the object type */
    const char*  name;        /**< Internal identifier (e.g., "bed_small") */
    const char*  displayName; /**< Human-readable name for UI display */
    const char*  category;    /**< Category string (e.g., "furniture", "structure") */

    int         maxHP;       /**< Maximum hit points */
    int         comfort;     /**< Comfort rating contribution */
    int         warmth;      /**< Warmth rating contribution */
    int         lightLevel;  /**< Light emission level */
    int         lightRadius; /**< Light effect radius in tiles when active. */
    int         heatRadius;  /**< Heat effect radius in tiles when active. */
    int         width;       /**< Object width in tiles */
    int         height;      /**< Object height in tiles */
    bool        walkable;    /**< Whether the player can walk over it */
    bool        flammable;   /**< Whether it can catch fire */
    bool        isWall;
    bool        isDoor;
    Color       color;       /**< Default color (fallback if no texture) */
    const char* texturePath; /**< path to texture */
    Texture2D   texture;     /**< Texture used for rendering */

    // --- Activation & animation metadata ---
    bool activatable;             /**< Whether this object supports activation toggling. */
    bool activationDefaultActive; /**< Default activation state when instantiated. */
    bool activationWalkableOn;    /**< Walkable flag when the object is active. */
    bool activationWalkableOff;   /**< Walkable flag when the object is inactive. */

    int spriteFrameWidth;  /**< Width in pixels of a single frame inside the spritesheet (0 = auto). */
    int spriteFrameHeight; /**< Height in pixels of a single frame inside the spritesheet (0 = auto). */
    int spriteColumns;     /**< Number of columns in the spritesheet (0 = auto). */
    int spriteRows;        /**< Number of rows in the spritesheet (0 = auto). */
    int spriteFrameCount;  /**< Total number of frames available (0 = auto). */
    int spriteSpacingX;    /**< Horizontal spacing between frames, in pixels. */
    int spriteSpacingY;    /**< Vertical spacing between frames, in pixels. */

    int   activationFrameInactive; /**< Frame index (0-based) representing the inactive state. */
    int   activationFrameActive;   /**< Frame index (0-based) representing the active state. */
    float activationFrameTime;     /**< Time per animation frame when toggling states (seconds). */

    const char* activationSoundOnPath;  /**< Optional sound played when the object activates. */
    const char* activationSoundOffPath; /**< Optional sound played when the object deactivates. */
    Sound       activationSoundOn;      /**< Loaded activation sound asset (shared per type). */
    Sound       activationSoundOff;     /**< Loaded deactivation sound asset (shared per type). */
} ObjectType;

/**
 * @struct Object
 * @brief Represents a single instance of an object placed in the world.
 */
typedef struct Object
{
    const ObjectType* type;         /**< Pointer to its object type definition */
    Vector2           position;     /**< Position in tile coordinates */
    int               hp;           /**< Current health points */
    bool              isActive;     /**< Whether the object is currently active */
    int               variantFrame; /**< Selected static frame variation (-1 if unused). */

    struct
    {
        int   currentFrame; /**< Currently displayed animation frame. */
        int   targetFrame;  /**< Target frame during activation/deactivation transitions. */
        float accumulator;  /**< Accumulated time since the last frame advance. */
        bool  playing;      /**< True while the activation animation is running. */
        bool  forward;      /**< True if animating towards higher frame indices. */
    } animation;

    struct Object* nextDynamic; /**< Intrusive list pointer for dynamic rendering. */
} Object;

/**
 * @struct ObjectRequirement
 * @brief Describes a condition based on object presence within a room.
 *
 * Used by room detection to verify that a given area contains
 * the required minimum number of specific objects.
 */
typedef struct
{
    ObjectTypeID objectId; /**< Required object type ID */
    int          minCount; /**< Minimum count for the rule to be valid */
} ObjectRequirement;

/**
 * @struct TileType
 * @brief Defines a type of terrain tile with rendering and interaction properties.
 */
typedef struct
{
    const char*  name;                 /**< Internal tile name (e.g., "grass") */
    TileTypeID   id;                   /**< Unique tile identifier */
    TileCategory category;             /**< Tile classification (ground, wall, etc.) */
    bool         walkable;             /**< Whether entities can move through it */
    Color        color;                /**< Tile color (used when no texture is defined) */
    Texture2D    texture;              /**< Texture for rendering (optional) */
    const char*  texturePath;          /**< Path to the source texture (spritesheet or single tile). */
    int          textureVariations;    /**< Number of horizontal variations stored in the texture. */
    int          variationFrameWidth;  /**< Width in pixels for a single variation frame (derived). */
    int          variationFrameHeight; /**< Height in pixels for a single variation frame (derived). */
    int          variationColumns;     /**< Number of columns in the variation grid (optional). */
    int          variationRows;        /**< Number of rows in the variation grid (optional). */
    bool         isBreakable;          /**< Whether the tile can be terraformed */
    int          durability;           /**< Hit points before terraformation */
    float        movementCost;         /**< Relative movement cost (1.0 = normal) */
    float        darkness;             /**< Darkness factor [0.0 light .. 1.0 night]. */
    float        fertility;            /**< Fertility level (0.0 - 1.0). */
    float        humidity;             /**< Humidity level (0.0 dry to 1.0 wet). */
    float        temperature;          /**< Current temperature in °C. */
} TileType;

/**
 * @struct Map
 * @brief Represents the full world grid, including terrain and objects.
 *
 * Each tile contains both a terrain type and an optional object instance.
 */
typedef struct
{
    int        width;                             /**< Map width in tiles */
    int        height;                            /**< Map height in tiles */
    TileTypeID tiles[MAP_HEIGHT][MAP_WIDTH];      /**< 2D grid of terrain tiles */
    Object*    objects[MAP_HEIGHT][MAP_WIDTH];    /**< 2D grid of placed objects */
    float      lightField[MAP_HEIGHT][MAP_WIDTH]; /**< Accumulated light intensity per tile. */
    float      heatField[MAP_HEIGHT][MAP_WIDTH];  /**< Accumulated heat intensity per tile. */
} Map;

typedef struct StructureClusterMember
{
    StructureKind kind;     /**< Blueprint to instantiate as part of the cluster. */
    int           minCount; /**< Minimum amount of this member. */
    int           maxCount; /**< Maximum amount of this member. */
} StructureClusterMember;

/**
 * @brief Generic, data-driven descriptor for a world structure.
 *
 * This structure holds all the metadata required to define a type of structure,
 * including its size constraints, relative rarity for generation, and the
 * concrete function used to build it.
 */
typedef struct StructureDef
{
    char          name[64];             ///< Descriptive display name of the structure.
    StructureKind kind;                 ///< The type/classification of the structure.
    int           minWidth, maxWidth;   ///< Minimum and maximum width in map tiles.
    int           minHeight, maxHeight; ///< Minimum and maximum height in map tiles.
    float         rarity;               ///< Relative weight for drawing/picking this structure (higher = more common).
    /**
     * @brief Concrete construction callback.
     *
     * This function is responsible for placing walls, doors, objects, and
     * other details that constitute the structure on the map.
     * @param map The map where the structure will be built.
     * @param x The top-left X coordinate of the structure's bounding box.
     * @param y The top-left Y coordinate of the structure's bounding box.
     * @param rng Pointer to the random number generator state.
     */
    void (*build)(Map* map, int x, int y, uint64_t* rng);
    int      minInstances;      ///< Guaranteed minimum number of instances to spawn per world.
    int      maxInstances;      ///< Maximum number of instances to spawn per world (0 = unlimited).
    uint32_t allowedBiomesMask; ///< Bitmask restricting which biomes may host this structure (0 = any biome).

    char  auraName[STRUCTURE_AURA_NAME_MAX];        ///< Short aura label.
    char  auraDescription[STRUCTURE_AURA_DESC_MAX]; ///< Long form aura description.
    float auraRadius;                               ///< Aura influence radius (in tiles).
    float auraIntensity;                            ///< Aura intensity score (arbitrary units).

    EntitiesTypeID occupantType;                                                ///< Default resident type.
    int            occupantMin;                                                 ///< Minimum number of residents.
    int            occupantMax;                                                 ///< Maximum number of residents.
    char           occupantDescription[STRUCTURE_OCCUPANT_DESC_MAX];            ///< Resident label/description.
    char           triggerDescription[STRUCTURE_TRIGGER_DESC_MAX];              ///< Description of the structure's triggered action/effect.
    char           species[BUILDING_SPECIES_NAME_MAX];                          ///< Normalised species owning the structure.
    int            speciesId;                                                   ///< Stable species identifier for the owning faction.
    bool           hasPantry;                                                   ///< True if the structure exposes a pantry inventory.
    int            pantryCapacity;                                              ///< Maximum number of food stacks stored in the pantry.
    int            roleCount;                                                   ///< Number of role labels bound to resident slots.
    char           roles[STRUCTURE_MAX_RESIDENT_ROLES][BUILDING_ROLE_NAME_MAX]; ///< Role labels for residents.

    char                   clusterGroup[STRUCTURE_CLUSTER_NAME_MAX];      ///< Optional identifier grouping related structures.
    bool                   clusterAnchor;                                 ///< Whether this blueprint seeds a cluster.
    int                    clusterMinMembers;                             ///< Minimum number of additional members to attempt.
    int                    clusterMaxMembers;                             ///< Maximum number of additional members to attempt.
    float                  clusterRadiusMin;                              ///< Minimum scatter radius (tiles).
    float                  clusterRadiusMax;                              ///< Maximum scatter radius (tiles).
    int                    clusterMemberCount;                            ///< Number of explicit cluster member descriptors.
    StructureClusterMember clusterMembers[STRUCTURE_CLUSTER_MAX_MEMBERS]; ///< Member descriptors.

    RoomTypeID        roomId;                                   ///< Identifier matching RoomTypeID for classification.
    int               minArea;                                  ///< Minimum interior area required to match.
    int               maxArea;                                  ///< Maximum interior area (0 = unlimited).
    ObjectRequirement requirements[STRUCTURE_MAX_REQUIREMENTS]; ///< List of object presence requirements.
    int               requirementCount;                         ///< Number of active requirements.
} StructureDef;

/**
 * @struct Building
 * @brief Represents a detected building or enclosed room within the world.
 *
 * Contains metadata such as its bounding box, contained objects,
 * computed center, and classification according to room rules.
 */
typedef struct Building
{
    int                        id;            /**< Unique building identifier */
    Rectangle                  bounds;        /**< Bounding box (in tile coordinates) */
    Vector2                    center;        /**< Geometric center (in tile coordinates) */
    int                        area;          /**< Interior area in tiles */
    char                       name[64];      /**< Inferred or generic building name */
    int                        objectCount;   /**< Number of objects inside */
    Object**                   objects;       /**< Pointer to a dynamic list of object instances */
    RoomTypeID                 roomTypeId;    /**< Detected room category (optional) */
    StructureKind              structureKind; /**< Optional originating structure blueprint. */
    const struct StructureDef* structureDef;  /**< Back-reference to immutable structure definition. */
    char                       auraName[STRUCTURE_AURA_NAME_MAX];
    char                       auraDescription[STRUCTURE_AURA_DESC_MAX];
    float                      auraRadius;
    float                      auraIntensity;
    EntitiesTypeID             occupantType;    /**< Default resident type linked to this structure. */
    int                        occupantMin;     /**< Minimum intended number of occupants. */
    int                        occupantMax;     /**< Maximum intended number of occupants. */
    int                        occupantCurrent; /**< Deterministic resident count used for spawning. */
    int                        occupantActive;  /**< Currently instantiated resident count. */
    char                       occupantDescription[STRUCTURE_OCCUPANT_DESC_MAX];
    char                       triggerDescription[STRUCTURE_TRIGGER_DESC_MAX];              /**< Narrative of the structure's special action. */
    bool                       isGenerated;                                                 /**< True if this entry originates from world generation. */
    int                        speciesId;                                                   /**< Owning species identifier (0 = none). */
    char                       species[BUILDING_SPECIES_NAME_MAX];                          /**< Normalised owning species label. */
    int                        villageId;                                                   /**< Village/colony grouping identifier. */
    bool                       hasPantry;                                                   /**< True when a pantry inventory is available. */
    int                        pantryCapacity;                                              /**< Maximum pantry storage capacity. */
    int                        pantryId;                                                    /**< Index into pantry system (-1 if none). */
    int                        roleCount;                                                   /**< Number of resident role labels. */
    char                       roles[STRUCTURE_MAX_RESIDENT_ROLES][BUILDING_ROLE_NAME_MAX]; /**< Resident role labels. */
    uint16_t*                  residents;                                                   /**< Dynamic list of resident entity identifiers. */
    int                        residentCount;                                               /**< Current number of registered residents. */
    int                        residentCapacity;                                            /**< Allocated capacity for resident ids. */
} Building;

/**
 * @brief Parameters for world generation.
 *
 * This structure holds all the necessary configuration variables
 * to define the size, biome distribution, and feature density
 * of a generated game world.
 */
typedef struct
{
    int min_biome_radius; /**< Minimum radius of biome cells, measured in map tiles. */

    // --- Relative Biome Weights ---

    float weight_forest;   /**< Relative weight for Forest biome generation (0.0 to 1.0+). */
    float weight_plain;    /**< Relative weight for Plain biome generation (0.0 to 1.0+). */
    float weight_savanna;  /**< Relative weight for Savanna biome generation (0.0 to 1.0+). */
    float weight_tundra;   /**< Relative weight for Tundra biome generation (0.0 to 1.0+). */
    float weight_desert;   /**< Relative weight for Desert biome generation (0.0 to 1.0+). */
    float weight_swamp;    /**< Relative weight for Swamp biome generation (0.0 to 1.0+). */
    float weight_mountain; /**< Relative weight for Mountain biome generation (0.0 to 1.0+). */
    float weight_cursed;   /**< Relative weight for Cursed biome generation (0.0 to 1.0+). */
    float weight_hell;     /**< Relative weight for Hell biome generation (0.0 to 1.0+). */

    // --- Feature and Structure Density ---

    /**
     * @brief Density of decorative features (e.g., trees, rocks). Expected range: ~0.0 to 0.2.
     */
    float feature_density;

    /**
     * @brief Probability of generating a major structure (e.g., dungeon, village) per map tile. Expected range: ~0.0 to 0.01.
     */
    float structure_chance;

    /**
     * @brief espace between structures
     */
    int structure_min_spacing;

    float biome_struct_mult_forest;   ///< Structure chance multiplier for the Forest biome.
    float biome_struct_mult_plain;    ///< Structure chance multiplier for the Plain biome.
    float biome_struct_mult_savanna;  ///< Structure chance multiplier for the Savanna biome.
    float biome_struct_mult_tundra;   ///< Structure chance multiplier for the Tundra biome.
    float biome_struct_mult_desert;   ///< Structure chance multiplier for the Desert biome.
    float biome_struct_mult_swamp;    ///< Structure chance multiplier for the Swamp biome.
    float biome_struct_mult_mountain; ///< Structure chance multiplier for the Mountain biome.
    float biome_struct_mult_cursed;   ///< Structure chance multiplier for the Cursed biome.
    float biome_struct_mult_hell;     ///< Structure chance multiplier for the Hell biome.
} WorldGenParams;

/**
 * @brief Defines the properties of a single biome cell or center point.
 *
 * Biomes are typically defined by their centers, from which their influence spreads.
 */
typedef struct
{
    int        x, y;      ///< Coordinates of the biome center on the map.
    BiomeKind  kind;      ///< The type of biome (e.g., Forest, Desert).
    TileTypeID primary;   /**< The primary tile type of the biome (e.g., main grass, main sand). */
    TileTypeID secondary; /**< A close variant tile type (used for texture variation or subtiles). */
} BiomeCenter;

typedef struct
{
    StructureKind kind;
    float         weight;
} BiomeStructureEntry;

typedef struct
{
    BiomeKind            kind;
    TileTypeID           primary, secondary;
    float                tempMin, tempMax;
    float                humidMin, humidMax;
    float                heightMin, heightMax;
    float                treeMul, bushMul, rockMul, structMul;
    int                  maxInstances;
    int                  minInstances;
    BiomeStructureEntry* structures;
    int                  structureCount;
} BiomeDef;

typedef struct
{
    int           x;
    int           y;
    StructureKind kind;
    int           doorX;
    int           doorY;
    int           boundsX;
    int           boundsY;
    int           boundsW;
    int           boundsH;
    int           speciesId;
    int           villageId;
} PlacedStructure;

typedef struct MapChunk
{
    int             cx, cy;      // Chunk coordinates (in chunk units, not tiles)
    RenderTexture2D rt;          // Cached render of this chunk
    RenderTexture2D rt_prev;     // Previous render of this chunk
    RenderTexture2D rt_next;     // Next render of this chunk
    bool            dirty;       // Needs rebuild before being drawn
    float           buildTimer;  //
    bool            pendingSwap; //
} MapChunk;

typedef struct ChunkGrid
{
    int       chunksX, chunksY;
    MapChunk* chunks; // [chunksY * chunksX]
} ChunkGrid;
#endif /* WORLD_H */
