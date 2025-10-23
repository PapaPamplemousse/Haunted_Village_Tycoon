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

// -----------------------------------------------------------------------------
// ENUMERATIONS
// -----------------------------------------------------------------------------

/**
 * @enum ObjectTypeID
 * @brief Unique identifiers for all object types available in the world.
 */
typedef enum
{
    OBJ_NONE = 0,    /**< Empty space / no object */
    OBJ_BED_SMALL,   /**< Small bed */
    OBJ_BED_LARGE,   /**< Large bed */
    OBJ_TABLE_WOOD,  /**< Wooden table */
    OBJ_CHAIR_WOOD,  /**< Wooden chair */
    OBJ_TORCH_WALL,  /**< Wall-mounted torch */
    OBJ_FIRE_PIT,    /**< Fire pit */
    OBJ_CHEST_WOOD,  /**< Wooden chest */
    OBJ_WORKBENCH,   /**< Workbench or crafting table */
    OBJ_DOOR_WOOD,   /**< Wooden door */
    OBJ_WINDOW_WOOD, /**< Wooden window */
    OBJ_WALL_STONE,  /**< Stone wall segment */
    OBJ_DECOR_PLANT, /**< Decorative plant */
    OBJ_MAX          /**< Sentinel value (number of object types) */
} ObjectTypeID;

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
    TILE_HELL,          /**< Infernal ground (extreme heat) */
    TILE_CURSED_FOREST, /**< Cursed: Rare Forest Variant */
    TILE_SWAMP,         /**< Swamp */
    TILE_DESERT,        /**< Desertt */
    TILE_MOUNTAIN,      /**< Mountain */
    TILE_COUNT          /**< Total number of defined tile types */
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

    int       maxHP;      /**< Maximum hit points */
    int       comfort;    /**< Comfort rating contribution */
    int       warmth;     /**< Warmth rating contribution */
    int       lightLevel; /**< Light emission level */
    int       width;      /**< Object width in tiles */
    int       height;     /**< Object height in tiles */
    bool      walkable;   /**< Whether the player can walk over it */
    bool      flammable;  /**< Whether it can catch fire */
    Color     color;      /**< Default color (fallback if no texture) */
    Texture2D texture;    /**< Texture used for rendering */
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

#endif /* WORLD_H */
