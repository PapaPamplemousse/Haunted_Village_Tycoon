#include "cannibal.h"

#include <math.h>
#include <string.h>

#include "tile.h"

#ifndef PI
#define PI 3.14159265358979323846f
#endif

typedef struct CannibalBrain
{
    float wanderTimer;
} CannibalBrain;

static TileTypeID clamp_tile_index(const Map* map, int x, int y, bool* inside)
{
    if (!map)
    {
        if (inside)
            *inside = false;
        return TILE_GRASS;
    }

    if (x < 0 || y < 0 || x >= map->width || y >= map->height)
    {
        if (inside)
            *inside = false;
        return TILE_GRASS;
    }

    if (inside)
        *inside = true;

    return map->tiles[y][x];
}

static void cannibal_pick_direction(EntitySystem* sys, Entity* e, CannibalBrain* brain)
{
    if (!sys || !e || !e->type || !brain)
        return;

    float angle = entity_randomf(sys, 0.0f, 2.0f * PI);
    float speed = e->type->maxSpeed * entity_randomf(sys, 0.65f, 1.1f);

    e->velocity.x      = cosf(angle) * speed;
    e->velocity.y      = sinf(angle) * speed;
    e->orientation     = angle;
    brain->wanderTimer = entity_randomf(sys, 0.6f, 2.2f);
}

static void cannibal_on_spawn(EntitySystem* sys, Entity* e)
{
    (void)sys;
    if (!e)
        return;

    memset(e->brain, 0, ENTITY_BRAIN_BYTES);
    e->hp        = e->type ? e->type->maxHP : 10;
    e->animFrame = 0;
    e->animTime  = 0.0f;
}

static void cannibal_on_update(EntitySystem* sys, Entity* e, const Map* map, float dt)
{
    if (!sys || !e || !map || !e->type)
        return;

    CannibalBrain* brain = (CannibalBrain*)e->brain;
    if (sizeof(CannibalBrain) > ENTITY_BRAIN_BYTES)
        return;

    if (brain->wanderTimer <= 0.0f)
        cannibal_pick_direction(sys, e, brain);
    else
        brain->wanderTimer -= dt;

    Vector2 next = {
        e->position.x + e->velocity.x * dt,
        e->position.y + e->velocity.y * dt,
    };

    int tileX = (int)floorf(next.x / TILE_SIZE);
    int tileY = (int)floorf(next.y / TILE_SIZE);

    bool       inside = false;
    TileTypeID tid    = clamp_tile_index(map, tileX, tileY, &inside);

    if (!inside)
    {
        e->velocity.x      = -e->velocity.x;
        e->velocity.y      = -e->velocity.y;
        brain->wanderTimer = 0.0f;
        return;
    }

    TileType* tt = get_tile_type(tid);
    if (!tt || !tt->walkable)
    {
        e->velocity.x      = -e->velocity.x;
        e->velocity.y      = -e->velocity.y;
        brain->wanderTimer = 0.0f;
        return;
    }

    e->position = next;
    if (fabsf(e->velocity.x) > 1e-3f || fabsf(e->velocity.y) > 1e-3f)
        e->orientation = atan2f(e->velocity.y, e->velocity.x);
}

static const EntityBehavior G_CANNIBAL_BEHAVIOR = {
    .onSpawn   = cannibal_on_spawn,
    .onUpdate  = cannibal_on_update,
    .onDespawn = NULL,
    .brainSize = sizeof(CannibalBrain),
};

const EntityBehavior* entity_cannibal_behavior(void)
{
    return &G_CANNIBAL_BEHAVIOR;
}
