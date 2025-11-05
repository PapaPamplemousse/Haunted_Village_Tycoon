#ifndef BEHAVIOR_H
#define BEHAVIOR_H

#include <stdbool.h>
#include <stdint.h>

#include "entity.h"

typedef struct EntitySystem EntityList;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enumerates higher level competences (abilities) an entity may possess.
 */
typedef enum EntityCompetence
{
    ENTITY_COMPETENCE_NONE                 = 0u,
    ENTITY_COMPETENCE_OPEN_DOORS           = 1u << 0,
    ENTITY_COMPETENCE_SEEK_SHELTER_AT_NIGHT = 1u << 1,
    ENTITY_COMPETENCE_LIGHT_AT_NIGHT       = 1u << 2,
} EntityCompetence;

/**
 * @brief Returns the current global darkness factor (0.0 = day, 1.0 = deep night).
 */
float behavior_darkness_factor(void);

/**
 * @brief Convenience helper returning true when darkness exceeds the threshold.
 */
bool behavior_is_night(float threshold);

/**
 * @brief Checks if the given entity type owns the requested competence bitmask.
 */
bool behavior_type_has_competence(const EntityType* type, EntityCompetence competence);

/**
 * @brief Checks if the given entity instance owns the requested competence bitmask.
 */
bool behavior_entity_has_competence(const Entity* entity, EntityCompetence competence);

/**
 * @brief Attempts to open doors blocking the desired movement corridor.
 *
 * @param entity Entity attempting to traverse the door.
 * @param map    World map (mutable) to toggle door objects.
 * @param desiredPosition Target position the entity would like to reach.
 * @return true if at least one door was opened.
 */
bool behavior_try_open_doors(Entity* entity, Map* map, Vector2 desiredPosition);

/**
 * @brief Synchronises nearby light sources with the desired active state.
 *
 * @param entity Entity performing the action.
 * @param map    World map (mutable) containing objects.
 * @param shouldBeActive Desired activation state (true = on).
 * @param radiusTiles Search radius expressed in tiles.
 * @return true if at least one light source changed state.
 */
bool behavior_sync_nearby_lights(Entity* entity, Map* map, bool shouldBeActive, int radiusTiles);

/**
 * @brief Updates the hunger meter of a living entity and applies starvation effects.
 */
void behavior_hunger_update(EntitySystem* sys, Entity* entity, Map* map);

/**
 * @brief Handles the death of an entity, spawning remains and rewarding the killer.
 */
void behavior_handle_entity_death(EntitySystem* sys, Map* map, Entity* victim, Entity* killer);

/**
 * @brief Attempts to trigger a reproduction interaction with a compatible partner.
 */
void behavior_try_reproduce(Entity* entity, EntityList* entities);

/**
 * @brief Daytime hunting routine used by carnivorous entities.
 */
void behavior_hunt(Entity* entity, EntityList* entities, Map* map);
bool behavior_force_open_doors(Entity* entity, Map* map, Vector2 desiredPosition, float radiusOverride);

/**
 * @brief Daytime gathering routine used by herbivorous or civilised entities.
 */
void behavior_gather(Entity* entity, Map* map);

/**
 * @brief Attempts to consume rations from the entity's pantry when hungry.
 */
void behavior_eat_if_hungry(Entity* entity);

#ifdef __cplusplus
}
#endif

#endif /* BEHAVIOR_H */
