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

/**
 * @def MAP_WIDTH
 * @brief Width of the game map in tiles.
 */
#define MAP_WIDTH 100

/**
 * @def MAP_HEIGHT
 * @brief Height of the game map in tiles.
 */
#define MAP_HEIGHT 100

/**
 * @def TILE_SIZE
 * @brief Size of one tile in pixels (for rendering and placement).
 */
#define TILE_SIZE 32

/**
 * @def MAX_BUILDINGS
 * @brief Maximum number of buildings that can be tracked simultaneously.
 */
#define MAX_BUILDINGS 100

// Tune for your target GPU. 128×128 balances rebuild cost vs draw calls.
// You can go 64×64 on very low-end GPUs or 256×256 for fewer textures.

#define CHUNK_W 32
#define CHUNK_H 32

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
    OBJ_SULFUR_VENT = 19, /**< Sulfur vent */
    OBJ_FIREPIT     = 20, /**< Exterior fire pit */
    OBJ_ALTAR       = 21, /**< Altar */

    OBJ_COUNT /**< Sentinel (number of object types) */
} ObjectTypeID;

typedef enum
{
    ROOM_NONE = 0,
    ROOM_BEDROOM,
    ROOM_KITCHEN,
    ROOM_HUT,
    ROOM_CRYPT,
    ROOM_SANCTUARY,
    ROOM_HOUSE,
    ROOM_LARGEROOM,
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

    int         maxHP;      /**< Maximum hit points */
    int         comfort;    /**< Comfort rating contribution */
    int         warmth;     /**< Warmth rating contribution */
    int         lightLevel; /**< Light emission level */
    int         width;      /**< Object width in tiles */
    int         height;     /**< Object height in tiles */
    bool        walkable;   /**< Whether the player can walk over it */
    bool        flammable;  /**< Whether it can catch fire */
    bool        isWall;
    bool        isDoor;
    Color       color;       /**< Default color (fallback if no texture) */
    const char* texturePath; /**< path to texture */
    Texture2D   texture;     /**< Texture used for rendering */
} ObjectType;

/**
 * @struct Object
 * @brief Represents a single instance of an object placed in the world.
 */
typedef struct Object
{
    const ObjectType* type;     /**< Pointer to its object type definition */
    Vector2           position; /**< Position in tile coordinates */
    int               hp;       /**< Current health points */
    bool              isActive; /**< Whether the object is active or disabled */
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
 * @struct RoomTypeRule
 * @brief Defines criteria for identifying a specific room type.
 *
 * Each rule defines size constraints and object requirements
 * to classify a detected enclosed space.
 */
typedef struct
{
    RoomTypeID               id;
    const char*              name;             /**< Room type name (e.g., "Bedroom") */
    int                      minArea;          /**< Minimum area (in tiles) */
    int                      maxArea;          /**< Maximum area (in tiles) */
    const ObjectRequirement* requirements;     /**< List of object requirements */
    int                      requirementCount; /**< Number of requirements in the list */
} RoomTypeRule;

/**
 * @struct Building
 * @brief Represents a detected building or enclosed room within the world.
 *
 * Contains metadata such as its bounding box, contained objects,
 * computed center, and classification according to room rules.
 */
typedef struct Building
{
    int                 id;          /**< Unique building identifier */
    Rectangle           bounds;      /**< Bounding box (in tile coordinates) */
    Vector2             center;      /**< Geometric center (in tile coordinates) */
    int                 area;        /**< Interior area in tiles */
    char                name[64];    /**< Inferred or generic building name */
    int                 objectCount; /**< Number of objects inside */
    Object**            objects;     /**< Pointer to a dynamic list of object instances */
    const RoomTypeRule* roomType;    /**< Detected room type (optional) */
} Building;

/**
 * @struct TileType
 * @brief Defines a type of terrain tile with rendering and interaction properties.
 */
typedef struct
{
    const char*  name;         /**< Internal tile name (e.g., "grass") */
    TileTypeID   id;           /**< Unique tile identifier */
    TileCategory category;     /**< Tile classification (ground, wall, etc.) */
    bool         walkable;     /**< Whether entities can move through it */
    Color        color;        /**< Tile color (used when no texture is defined) */
    Texture2D    texture;      /**< Texture for rendering (optional) */
    const char*  texturePath;  /**< path to texture */
    bool         isBreakable;  /**< Whether the tile can be terraformed */
    int          durability;   /**< Hit points before terraformation */
    float        movementCost; /**< Relative movement cost (1.0 = normal) */
    float        humidity;     /**< Humidity level (0.0 dry to 1.0 wet) */
    float        fertility;    /**< Fertility level (0.0 - 1.0) */
    int          temperature;  /**<  Current Temperature ( °C )*/
} TileType;

/**
 * @struct Map
 * @brief Represents the full world grid, including terrain and objects.
 *
 * Each tile contains both a terrain type and an optional object instance.
 */
typedef struct
{
    int        width;                          /**< Map width in tiles */
    int        height;                         /**< Map height in tiles */
    TileTypeID tiles[MAP_HEIGHT][MAP_WIDTH];   /**< 2D grid of terrain tiles */
    Object*    objects[MAP_HEIGHT][MAP_WIDTH]; /**< 2D grid of placed objects */
} Map;

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
    STRUCT_HUT_CANNIBAL,  ///< A small, primitive hut, typically for cannibals.
    STRUCT_CRYPT,         ///< An underground tomb or burial chamber.
    STRUCT_RUIN,          ///< Remains of a destroyed building or wall.
    STRUCT_VILLAGE_HOUSE, ///< A standard residential building in a village.
    STRUCT_TEMPLE,        ///< A large, religious building.
    STRUCT_COUNT          ///< The total number of defined structure kinds (must be the last entry).
} StructureKind;

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
