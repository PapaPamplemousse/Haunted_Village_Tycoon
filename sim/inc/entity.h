/**
 * @file entity.h
 * @brief Generic, extensible entity system for living actors (mobs & NPCs).
 *
 * The entity system exposes a light-weight pool of living actors that are
 * distinct from map `Object` instances. Definitions for entity species are
 * data-driven (loaded from `.stv` files) and can be extended with dedicated AI
 * behaviours, texture-based rendering, and biome aware spawn tables.
 */
#ifndef ENTITY_H
#define ENTITY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "raylib.h"
#include "world.h"

// -----------------------------------------------------------------------------
// CONSTANTS
// -----------------------------------------------------------------------------

/** Maximum number of simultaneously active entities (pooled storage). */
#define MAX_ENTITIES 1024

/** Maximum amount of entity types that can be loaded from configuration. */
#define ENTITY_MAX_TYPES 128

/** Maximum number of custom personality traits per entity type. */
#define ENTITY_MAX_TRAITS 8

/** Maximum length (including null terminator) of a single trait name. */
#define ENTITY_TRAIT_NAME_MAX 32

/** Maximum length (including null terminator) of the entity category label. */
#define ENTITY_CATEGORY_NAME_MAX 32

/** Maximum amount of spawn rules that can be defined across all types. */
#define ENTITY_MAX_SPAWN_RULES 256

/** Maximum length (including null terminator) for entity identifiers. */
#define ENTITY_TYPE_NAME_MAX 32

/** Maximum path length (including null terminator) for sprite textures. */
#define ENTITY_TEXTURE_PATH_MAX 128

/** Number of bytes reserved as an inline behaviour blackboard per entity. */
#define ENTITY_BRAIN_BYTES 64

/** Value used to mark invalid entity identifiers. */
#define ENTITY_ID_INVALID ((uint16_t)0xFFFF)

/** Maximum number of persistent entity reservations used for streaming. */
#define ENTITY_MAX_RESERVATIONS 1024

// -----------------------------------------------------------------------------
// ENUMS & FLAGS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// FORWARD DECLARATIONS
// -----------------------------------------------------------------------------

struct Entity;
struct EntitySystem;

// -----------------------------------------------------------------------------
// BEHAVIOUR INTERFACES
// -----------------------------------------------------------------------------

typedef void (*EntityBehaviourSpawnFn)(struct EntitySystem*, struct Entity*);
typedef void (*EntityBehaviourUpdateFn)(struct EntitySystem*, struct Entity*, const Map*, float dt);
typedef void (*EntityBehaviourDespawnFn)(struct EntitySystem*, struct Entity*);

typedef struct EntityBehavior
{
    EntityBehaviourSpawnFn   onSpawn;
    EntityBehaviourUpdateFn  onUpdate;
    EntityBehaviourDespawnFn onDespawn;
    size_t                   brainSize; /**< Required blackboard bytes (<= ENTITY_BRAIN_BYTES). */
} EntityBehavior;

// -----------------------------------------------------------------------------
// DATA STRUCTURES
// -----------------------------------------------------------------------------

typedef struct EntitySprite
{
    Texture2D texture;                              /**< Loaded texture atlas (optional). */
    char      texturePath[ENTITY_TEXTURE_PATH_MAX]; /**< Source PNG path, for reload/debug. */
    int       frameWidth;                           /**< Width of a single animation frame. */
    int       frameHeight;                          /**< Height of a single animation frame. */
    int       frameCount;                           /**< Total number of frames in the strip. */
    float     frameDuration;                        /**< Duration of one frame (seconds). */
    Vector2   origin;                               /**< Pivot for rendering (pixels). */
} EntitySprite;

typedef struct EntityType
{
    EntitiesTypeID        id;                                               /**< Unique numeric identifier. */
    char                  identifier[ENTITY_TYPE_NAME_MAX];                 /**< Internal identifier used in debug logs. */
    char                  displayName[ENTITY_TYPE_NAME_MAX];                /**< Optional human readable name. */
    EntityFlags           flags;                                            /**< Capability & faction tags. */
    uint32_t              competences;                                      /**< Bitmask of special competences/abilities. */
    char                  category[ENTITY_CATEGORY_NAME_MAX];               /**< Normalised faction/category label. */
    int                   traitCount;                                       /**< Number of active trait labels. */
    char                  traits[ENTITY_MAX_TRAITS][ENTITY_TRAIT_NAME_MAX]; /**< Normalised trait labels. */
    float                 maxSpeed;                                         /**< Maximum locomotion speed (px/s). */
    float                 radius;                                           /**< Collision/render radius (px). */
    int                   maxHP;                                            /**< Maximum hit points. */
    Color                 tint;                                             /**< Fallback render colour (no texture). */
    EntitySprite          sprite;                                           /**< Sprite description (texture/animation). */
    const EntityBehavior* behavior;                                         /**< Optional default behaviour. */
    StructureKind         referredStructure;                                /**< Optional structure affinity. */
} EntityType;

typedef struct Entity
{
    uint16_t              id;
    bool                  active;
    Vector2               position;                  /**< Position in world pixels. */
    Vector2               velocity;                  /**< Velocity expressed in px/s (for animation). */
    float                 orientation;               /**< Facing angle in radians. */
    int                   hp;                        /**< Current hit points. */
    float                 animTime;                  /**< Accumulated animation timer. */
    int                   animFrame;                 /**< Current animation frame index. */
    const EntityType*     type;                      /**< Pointer to immutable type definition. */
    const EntityBehavior* behavior;                  /**< Behaviour handlers (AI/state). */
    uint8_t               brain[ENTITY_BRAIN_BYTES]; /**< Inline behaviour state storage. */
    Vector2               home;                      /**< Preferred anchor position in world space. */
    StructureKind         homeStructure;             /**< Structure affinity used for behaviour. */
    int                   reservationIndex;          /**< Index into reservation array or -1 if none. */
} Entity;

typedef struct EntitySpawnRule
{
    EntitiesTypeID    id; /**< Unique numeric identifier. */
    const EntityType* type;
    BiomeKind         biome;    /**< BIO_MAX indicates "any". */
    TileTypeID        tile;     /**< TILE_MAX indicates "any". */
    float             density;  /**< Spawn probability per matching tile (0..1). */
    int               groupMin; /**< Minimum number of entities per spawn. */
    int               groupMax; /**< Maximum number of entities per spawn. */
} EntitySpawnRule;

typedef struct EntityReservation
{
    bool           used;               /**< Slot is populated with reservation data. */
    bool           active;             /**< Reservation currently has a live entity instance. */
    EntitiesTypeID typeId;             /**< Entity type identifier. */
    uint16_t       entityId;           /**< Runtime id when active, ENTITY_ID_INVALID otherwise. */
    Vector2        position;           /**< Persisted world position (pixels). */
    Vector2        velocity;           /**< Persisted velocity vector. */
    float          orientation;        /**< Persisted facing angle. */
    int            hp;                 /**< Persisted hit points. */
    Vector2        home;               /**< Home position anchor. */
    StructureKind  homeStructure;      /**< Optional affiliated structure. */
    int            buildingId;         /**< Owning building id or -1 if free roaming. */
    float          activationRadius;   /**< Distance from focus required to instantiate. */
    float          deactivationRadius; /**< Distance from focus required to despawn. */
} EntityReservation;

typedef struct EntitySystem
{
    Entity       entities[MAX_ENTITIES];
    int          activeCount;  /**< Number of active entities in the pool. */
    int          highestIndex; /**< Highest slot index currently in use. */
    unsigned int rngState;     /**< RNG state (XorShift). */

    EntityType types[ENTITY_MAX_TYPES];
    int        typeCount;

    EntitySpawnRule spawnRules[ENTITY_MAX_SPAWN_RULES];
    int             spawnRuleCount;

    EntityReservation reservations[ENTITY_MAX_RESERVATIONS];
    int               reservationCount;
    float             streamActivationPadding;   /**< Additional radius around viewport for activation. */
    float             streamDeactivationPadding; /**< Hysteresis radius for deactivation. */
} EntitySystem;

// -----------------------------------------------------------------------------
// API
// -----------------------------------------------------------------------------

/**
 * @brief Initializes the entity system and loads type definitions.
 *
 * @param sys Entity system to initialize.
 * @param map Map used for initial spawn context.
 * @param seed Seed used for deterministic RNG.
 * @param definitionsPath Path to the entity definition STV file.
 * @return true if initialization succeeded.
 */
bool entity_system_init(EntitySystem* sys, const Map* map, unsigned int seed, const char* definitionsPath);

/**
 * @brief Releases resources associated with the entity system.
 */
void entity_system_shutdown(EntitySystem* sys);

/**
 * @brief Advances entity logic by one frame.
 *
 * @param sys Entity system to update.
 * @param map Current map used for collision and spawning context.
 * @param camera Active camera used to determine streaming focus.
 * @param dt Delta time in seconds.
 */
void entity_system_update(EntitySystem* sys, const Map* map, const Camera2D* camera, float dt);

/**
 * @brief Renders all active entities.
 *
 * @param sys Entity system to draw.
 */
void entity_system_draw(const EntitySystem* sys);

/**
 * @brief Spawns a new entity of the specified type.
 *
 * @param sys Entity system that owns the pool.
 * @param typeId Numeric identifier of the entity type to create (EntitiesTypeID).
 * @param position World position for the spawn.
 * @return Runtime identifier of the new entity, or ENTITY_ID_INVALID on failure.
 */
uint16_t entity_spawn(EntitySystem* sys, EntitiesTypeID typeId, Vector2 position);

/**
 * @brief Removes an entity from the simulation.
 *
 * @param sys Entity system that owns the pool.
 * @param id Identifier returned by @ref entity_spawn.
 */
void entity_despawn(EntitySystem* sys, uint16_t id);

/**
 * @brief Provides mutable access to an entity by id.
 *
 * @param sys Entity system owning the entity.
 * @param id Identifier returned by @ref entity_spawn.
 * @return Pointer to the entity or NULL if not active.
 */
Entity* entity_acquire(EntitySystem* sys, uint16_t id);

/**
 * @brief Provides read-only access to an entity by id.
 *
 * @param sys Entity system owning the entity.
 * @param id Identifier returned by @ref entity_spawn.
 * @return Pointer to the entity or NULL if not active.
 */
const Entity* entity_get(const EntitySystem* sys, uint16_t id);

/**
 * @brief Searches for an entity type definition by identifier.
 *
 * @param sys Entity system containing the registered types.
 * @param typeId Identifier to search.
 * @return Pointer to the type definition or NULL if not found.
 */
const EntityType* entity_find_type(const EntitySystem* sys, EntitiesTypeID typeId);

/**
 * @brief Initializes a spawn rule structure with default values.
 */
void entity_spawn_rule_init(EntitySpawnRule* rule);

/**
 * @brief Registers a new entity type within the system (used by loaders).
 */
bool entity_system_register_type(EntitySystem* sys, const EntityType* def, const EntitySpawnRule* spawn);

/**
 * @brief Generates deterministic random values tied to the entity system.
 */
unsigned int entity_random(EntitySystem* sys);
float        entity_randomf(EntitySystem* sys, float min, float max);
int          entity_randomi(EntitySystem* sys, int min, int max);

/**
 * @brief Queries whether an entity type declares a specific trait.
 */
bool entity_type_has_trait(const EntityType* type, const char* trait);

/**
 * @brief Checks whether the given entity type owns the provided competence mask.
 */
bool entity_type_has_competence(const EntityType* type, uint32_t competenceMask);

/**
 * @brief Checks if an entity type belongs to the specified category label.
 */
bool entity_type_is_category(const EntityType* type, const char* category);

/**
 * @brief Convenience wrapper for checking traits on a runtime entity instance.
 */
bool entity_has_trait(const Entity* entity, const char* trait);

/**
 * @brief Convenience wrapper for category comparisons on a runtime entity instance.
 */
bool entity_is_category(const Entity* entity, const char* category);

/**
 * @brief Returns the number of registered entity types.
 */
int entity_system_type_count(const EntitySystem* sys);

/**
 * @brief Provides indexed access to the registered entity type definitions.
 */
const EntityType* entity_system_type_at(const EntitySystem* sys, int index);

/**
 * @brief Tests whether a position is traversable for an entity with the provided radius.
 */
bool entity_position_is_walkable(const Map* map, Vector2 position, float radius);

#endif /* ENTITY_H */
