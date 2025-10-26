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
#define MAX_ENTITIES 256

/** Maximum amount of entity types that can be loaded from configuration. */
#define ENTITY_MAX_TYPES 32

/** Maximum amount of spawn rules that can be defined across all types. */
#define ENTITY_MAX_SPAWN_RULES 64

/** Maximum length (including null terminator) for entity identifiers. */
#define ENTITY_TYPE_NAME_MAX 32

/** Maximum path length (including null terminator) for sprite textures. */
#define ENTITY_TEXTURE_PATH_MAX 128

/** Number of bytes reserved as an inline behaviour blackboard per entity. */
#define ENTITY_BRAIN_BYTES 64

/** Value used to mark invalid entity identifiers. */
#define ENTITY_ID_INVALID ((uint16_t)0xFFFF)

// -----------------------------------------------------------------------------
// ENUMS & FLAGS
// -----------------------------------------------------------------------------

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
    char                  id[ENTITY_TYPE_NAME_MAX];          /**< Internal identifier used in code & config. */
    char                  displayName[ENTITY_TYPE_NAME_MAX]; /**< Optional human readable name. */
    EntityFlags           flags;                             /**< Capability & faction tags. */
    float                 maxSpeed;                          /**< Maximum locomotion speed (px/s). */
    float                 radius;                            /**< Collision/render radius (px). */
    int                   maxHP;                             /**< Maximum hit points. */
    Color                 tint;                              /**< Fallback render colour (no texture). */
    EntitySprite          sprite;                            /**< Sprite description (texture/animation). */
    const EntityBehavior* behavior;                          /**< Optional default behaviour. */
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
} Entity;

typedef struct EntitySpawnRule
{
    char              typeId[ENTITY_TYPE_NAME_MAX];
    const EntityType* type;
    BiomeKind         biome;    /**< BIO_MAX indicates "any". */
    TileTypeID        tile;     /**< TILE_MAX indicates "any". */
    float             density;  /**< Spawn probability per matching tile (0..1). */
    int               groupMin; /**< Minimum number of entities per spawn. */
    int               groupMax; /**< Maximum number of entities per spawn. */
} EntitySpawnRule;

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
} EntitySystem;

// -----------------------------------------------------------------------------
// API
// -----------------------------------------------------------------------------

bool entity_system_init(EntitySystem* sys, const Map* map, unsigned int seed, const char* definitionsPath);
void entity_system_shutdown(EntitySystem* sys);
void entity_system_update(EntitySystem* sys, const Map* map, float dt);
void entity_system_draw(const EntitySystem* sys);

uint16_t      entity_spawn(EntitySystem* sys, const char* typeId, Vector2 position);
void          entity_despawn(EntitySystem* sys, uint16_t id);
Entity*       entity_acquire(EntitySystem* sys, uint16_t id);
const Entity* entity_get(const EntitySystem* sys, uint16_t id);

const EntityType* entity_find_type(const EntitySystem* sys, const char* typeId);

#endif /* ENTITY_H */
