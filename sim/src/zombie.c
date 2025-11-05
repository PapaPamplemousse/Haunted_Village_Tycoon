#include "zombie.h"

#include <math.h>
#include <string.h>

#include "behavior.h"
#include "tile.h"

#ifndef PI
#define PI 3.14159265358979323846f
#endif

typedef struct ZombieBrain
{
    float    wanderTimer;
    uint16_t targetId;
    float    attackCooldown;
} ZombieBrain;

static bool zombie_is_valid_target(const Entity* self, const Entity* other)
{
    if (!other || other == self || !other->active || !other->type)
        return false;

    if (entity_type_is_category(other->type, "undead") || entity_type_is_category(other->type, "undead"))
        return false;
    if (entity_type_is_category(other->type, "demon") || entity_type_is_category(other->type, "démon"))
        return false;
    if (entity_type_has_trait(other->type, "undead") || entity_type_has_trait(other->type, "demon") || entity_type_has_trait(other->type, "démon"))
        return false;
    return true;
}

static uint16_t zombie_pick_target(EntitySystem* sys, Entity* self)
{
    if (!sys || !self)
        return ENTITY_ID_INVALID;
    const float detection    = 4.0f * TILE_SIZE;
    const float detectionSq  = detection * detection;
    float       bestDistance = detectionSq;
    uint16_t    bestId       = ENTITY_ID_INVALID;

    for (int i = 0; i <= sys->highestIndex; ++i)
    {
        Entity* other = &sys->entities[i];
        if (!zombie_is_valid_target(self, other))
            continue;

        float dx     = other->position.x - self->position.x;
        float dy     = other->position.y - self->position.y;
        float distSq = dx * dx + dy * dy;
        if (distSq <= bestDistance)
        {
            bestDistance = distSq;
            bestId       = other->id;
        }
    }
    return bestId;
}
static void zombie_pick_direction(EntitySystem* sys, Entity* e, ZombieBrain* brain)
{
    if (!sys || !e || !e->type || !brain)
        return;

    float angle = entity_randomf(sys, 0.0f, 2.0f * PI);
    float speed = e->type->maxSpeed * entity_randomf(sys, 0.45f, 1.0f);

    e->velocity.x      = cosf(angle) * speed;
    e->velocity.y      = sinf(angle) * speed;
    e->orientation     = angle;
    brain->wanderTimer = entity_randomf(sys, 1.2f, 3.6f);
}

static void zombie_on_spawn(EntitySystem* sys, Entity* e)
{
    (void)sys;
    if (!e)
        return;

    memset(e->brain, 0, ENTITY_BRAIN_BYTES);
    e->hp              = e->type ? e->type->maxHP : 10;
    e->animFrame       = 0;
    e->animTime        = 0.0f;
    ZombieBrain* brain = (ZombieBrain*)e->brain;
    if (brain)
    {
        brain->wanderTimer    = 0.0f;
        brain->targetId       = ENTITY_ID_INVALID;
        brain->attackCooldown = 0.0f;
    }
}

static void zombie_on_update(EntitySystem* sys, Entity* e, const Map* map, float dt)
{
    if (!sys || !e || !map || !e->type)
        return;
    Map*         mutableMap = (Map*)map;
    ZombieBrain* brain      = (ZombieBrain*)e->brain;
    if (sizeof(ZombieBrain) > ENTITY_BRAIN_BYTES)
        return;

    if (brain->attackCooldown > 0.0f)
    {
        brain->attackCooldown -= dt;
        if (brain->attackCooldown < 0.0f)
            brain->attackCooldown = 0.0f;
    }

    Entity* target = NULL;
    if (brain->targetId != ENTITY_ID_INVALID)
    {
        target = entity_acquire(sys, brain->targetId);
        if (!zombie_is_valid_target(e, target))
        {
            target             = NULL;
            brain->targetId    = ENTITY_ID_INVALID;
            brain->wanderTimer = 0.0f;
        }
    }

    if (!target)
    {
        uint16_t id = zombie_pick_target(sys, e);
        if (id != ENTITY_ID_INVALID)
        {
            brain->targetId = id;
            target          = entity_acquire(sys, id);
        }
    }

    if (target)
    {
        Vector2 toTarget = {target->position.x - e->position.x, target->position.y - e->position.y};
        float   distance = sqrtf(toTarget.x * toTarget.x + toTarget.y * toTarget.y);
        if (distance > 1e-3f)
        {
            float inv      = 1.0f / distance;
            e->velocity.x  = toTarget.x * inv * e->type->maxSpeed;
            e->velocity.y  = toTarget.y * inv * e->type->maxSpeed;
            e->orientation = atan2f(e->velocity.y, e->velocity.x);
        }
        brain->wanderTimer = 0.0f;
    }
    else if (brain->wanderTimer <= 0.0f || (fabsf(e->velocity.x) < 0.1f && fabsf(e->velocity.y) < 0.1f))
    {
        zombie_pick_direction(sys, e, brain);
    }
    else
    {
        brain->wanderTimer -= dt;
    }

    Vector2 next = {
        e->position.x + e->velocity.x * dt,
        e->position.y + e->velocity.y * dt,
    };

    if (!entity_position_is_walkable(map, next, e->type->radius))
    {
        bool openedDoor = behavior_try_open_doors(e, mutableMap, next);
        if ((!openedDoor || !entity_position_is_walkable(map, next, e->type->radius)) && e->behaviorTargetId != ENTITY_ID_INVALID)
        {
            float doorRadius = e->type ? fmaxf(e->type->radius, TILE_SIZE * 0.6f) : TILE_SIZE * 0.6f;
            if (!behavior_force_open_doors(e, mutableMap, next, doorRadius) || !entity_position_is_walkable(map, next, e->type->radius))
            {
                e->velocity.x      = -e->velocity.x * 0.3f;
                e->velocity.y      = -e->velocity.y * 0.3f;
                brain->wanderTimer = 0.0f;
                return;
            }
        }
        else if (!openedDoor || !entity_position_is_walkable(map, next, e->type->radius))
        {
            e->velocity.x      = -e->velocity.x * 0.3f;
            e->velocity.y      = -e->velocity.y * 0.3f;
            brain->wanderTimer = 0.0f;
            return;
        }
    }

    e->position = next;
    if (fabsf(e->velocity.x) > 1e-3f || fabsf(e->velocity.y) > 1e-3f)
        e->orientation = atan2f(e->velocity.y, e->velocity.x);
    if (target && target->active && target->type)
    {
        float dx          = target->position.x - e->position.x;
        float dy          = target->position.y - e->position.y;
        float distSq      = dx * dx + dy * dy;
        float attackRange = (e->type->radius + target->type->radius) + 12.0f;
        if (distSq <= attackRange * attackRange && brain->attackCooldown <= 0.0f)
        {
            target->hp -= 12;
            if (target->hp <= 0)
            {
                behavior_handle_entity_death(sys, mutableMap, target, e);
                e->behaviorTargetId = ENTITY_ID_INVALID;
                e->behaviorTimer    = 1.2f;
                // zombie_reward_bloodrage(e);
            }
            brain->attackCooldown = 1.2f;
        }
    }
}

static const EntityBehavior G_ZOMBIE_BEHAVIOR = {
    .onSpawn   = zombie_on_spawn,
    .onUpdate  = zombie_on_update,
    .onDespawn = NULL,
    .brainSize = sizeof(ZombieBrain),
};

const EntityBehavior* entity_zombie_behavior(void)
{
    return &G_ZOMBIE_BEHAVIOR;
}
