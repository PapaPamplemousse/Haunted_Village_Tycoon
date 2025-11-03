#include "behavior.h"
#include <math.h>
#include <string.h>

#include "map.h"
#include "object.h"
#include "tile.h"
#include "world_time.h"

static bool behavior_entity_can_interact_with_tile(const Entity* entity, int tileX, int tileY)
{
    if (!entity)
        return false;

    const float radius = entity->type ? entity->type->radius : 0.0f;
    const float reach  = radius + (TILE_SIZE * 0.8f);

    const float tileCenterX = (tileX + 0.5f) * TILE_SIZE;
    const float tileCenterY = (tileY + 0.5f) * TILE_SIZE;

    const float dx = entity->position.x - tileCenterX;
    const float dy = entity->position.y - tileCenterY;

    return (dx * dx + dy * dy) <= (reach * reach);
}

static bool behavior_object_is_light(const Object* obj)
{
    if (!obj || !obj->type)
        return false;
    return obj->type->lightLevel > 0 || obj->type->lightRadius > 0;
}

float behavior_darkness_factor(void)
{
    return world_time_get_darkness();
}

bool behavior_is_night(float threshold)
{
    return behavior_darkness_factor() >= threshold;
}

bool behavior_type_has_competence(const EntityType* type, EntityCompetence competence)
{
    if (competence == ENTITY_COMPETENCE_NONE)
        return false;
    return entity_type_has_competence(type, (uint32_t)competence);
}

bool behavior_entity_has_competence(const Entity* entity, EntityCompetence competence)
{
    return entity && behavior_type_has_competence(entity->type, competence);
}

bool behavior_try_open_doors(Entity* entity, Map* map, Vector2 desiredPosition)
{
    if (!entity || !map)
        return false;

    if (!behavior_entity_has_competence(entity, ENTITY_COMPETENCE_OPEN_DOORS))
        return false;

    const EntityType* type = entity->type;
    float             radius = type ? type->radius : 0.0f;

    float minWorldX = fminf(entity->position.x, desiredPosition.x) - radius;
    float maxWorldX = fmaxf(entity->position.x, desiredPosition.x) + radius;
    float minWorldY = fminf(entity->position.y, desiredPosition.y) - radius;
    float maxWorldY = fmaxf(entity->position.y, desiredPosition.y) + radius;

    int minTileX = (int)floorf(minWorldX / TILE_SIZE);
    int maxTileX = (int)floorf(maxWorldX / TILE_SIZE);
    int minTileY = (int)floorf(minWorldY / TILE_SIZE);
    int maxTileY = (int)floorf(maxWorldY / TILE_SIZE);

    bool openedDoor = false;
    for (int ty = minTileY; ty <= maxTileY; ++ty)
    {
        if (ty < 0 || ty >= map->height)
            continue;

        for (int tx = minTileX; tx <= maxTileX; ++tx)
        {
            if (tx < 0 || tx >= map->width)
                continue;

            Object* obj = map->objects[ty][tx];
            if (!obj || !obj->type || !obj->type->isDoor)
                continue;

            if (object_is_walkable(obj))
                continue;

            if (!behavior_entity_can_interact_with_tile(entity, tx, ty))
                continue;

            if (map_toggle_door(map, tx, ty, true))
                openedDoor = true;
        }
    }

    return openedDoor;
}

bool behavior_sync_nearby_lights(Entity* entity, Map* map, bool shouldBeActive, int radiusTiles)
{
    if (!entity || !map)
        return false;

    if (!behavior_entity_has_competence(entity, ENTITY_COMPETENCE_LIGHT_AT_NIGHT))
        return false;

    if (radiusTiles < 1)
        radiusTiles = 1;

    int centerX = (int)floorf(entity->position.x / TILE_SIZE);
    int centerY = (int)floorf(entity->position.y / TILE_SIZE);

    bool changed = false;
    for (int dy = -radiusTiles; dy <= radiusTiles; ++dy)
    {
        int ty = centerY + dy;
        if (ty < 0 || ty >= map->height)
            continue;

        for (int dx = -radiusTiles; dx <= radiusTiles; ++dx)
        {
            int tx = centerX + dx;
            if (tx < 0 || tx >= map->width)
                continue;

            Object* obj = map->objects[ty][tx];
            if (!behavior_object_is_light(obj))
                continue;

            if (!object_has_activation(obj))
                continue;

            if (!behavior_entity_can_interact_with_tile(entity, tx, ty))
                continue;

            if (obj->isActive != shouldBeActive)
            {
                if (object_set_active(obj, shouldBeActive))
                    changed = true;
            }
        }
    }

    return changed;
}
