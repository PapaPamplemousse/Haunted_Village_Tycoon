#include "cannibal.h"

#include <math.h>
#include <string.h>

#include "behavior.h"
#include "map.h"
#include "pathfinding.h"
#include "tile.h"

#ifndef PI
#define PI 3.14159265358979323846f
#endif

typedef struct CannibalBrain
{
    float    wanderTimer;
    uint16_t targetId;
    uint8_t  waypointValid;
    uint8_t  padding;
    float    attackCooldown;
    int      lastHP;
    float    repathTimer;
    Vector2  pathGoal;
    Vector2  waypoint;
} CannibalBrain;

static bool cannibal_is_friendly(const Entity* other)
{
    if (!other || !other->type)
        return false;
    bool hasCannibalTrait = entity_type_has_trait(other->type, "cannibal");
    bool humanoidCategory = entity_type_is_category(other->type, "humanoid");
    return hasCannibalTrait && humanoidCategory;
}

static bool cannibal_is_valid_target(const Entity* self, const Entity* other)
{
    if (!other || other == self || !other->active || !other->type)
        return false;
    if (cannibal_is_friendly(other))
        return false;
    return true;
}

static uint16_t cannibal_pick_target(EntitySystem* sys, Entity* self)
{
    if (!sys || !self)
        return ENTITY_ID_INVALID;
    const float detection    = 4.5f * TILE_SIZE;
    const float detectionSq  = detection * detection;
    float       bestDistance = detectionSq;
    uint16_t    bestId       = ENTITY_ID_INVALID;

    for (int i = 0; i <= sys->highestIndex; ++i)
    {
        Entity* other = &sys->entities[i];
        if (!cannibal_is_valid_target(self, other))
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
    e->hp                = e->type ? e->type->maxHP : 10;
    e->animFrame         = 0;
    e->animTime          = 0.0f;
    CannibalBrain* brain = (CannibalBrain*)e->brain;
    if (brain)
    {
        brain->wanderTimer    = 0.0f;
        brain->targetId       = ENTITY_ID_INVALID;
        brain->attackCooldown = 0.0f;
        brain->lastHP         = e->hp;
        brain->repathTimer    = 0.0f;
        brain->waypoint       = e->position;
        brain->pathGoal       = e->home;
        brain->waypointValid  = 0;
    }
}

static void cannibal_on_update(EntitySystem* sys, Entity* e, const Map* map, float dt)
{
    if (!sys || !e || !map || !e->type)
        return;

    Map*           mutableMap = (Map*)map;
    CannibalBrain* brain      = (CannibalBrain*)e->brain;
    if (sizeof(CannibalBrain) > ENTITY_BRAIN_BYTES)
        return;

    if (brain->attackCooldown > 0.0f)
    {
        brain->attackCooldown -= dt;
        if (brain->attackCooldown < 0.0f)
            brain->attackCooldown = 0.0f;
    }

    bool       wasHit         = (brain->lastHP > e->hp);
    Entity*    target         = NULL;
    const bool isNight        = behavior_is_night(0.55f);
    const bool canShelter     = behavior_entity_has_competence(e, ENTITY_COMPETENCE_SEEK_SHELTER_AT_NIGHT);
    bool       seekingShelter = false;
    Vector2    desiredGoal    = e->position;
    bool       haveGoal       = false;

    if (behavior_entity_has_competence(e, ENTITY_COMPETENCE_LIGHT_AT_NIGHT))
        behavior_sync_nearby_lights(e, mutableMap, isNight, 1);

    if (brain->targetId != ENTITY_ID_INVALID)
    {
        target = entity_acquire(sys, brain->targetId);
        if (!cannibal_is_valid_target(e, target))
        {
            target          = NULL;
            brain->targetId = ENTITY_ID_INVALID;
        }
    }

    if (!target)
    {
        uint16_t id = cannibal_pick_target(sys, e);
        if (id != ENTITY_ID_INVALID)
        {
            brain->targetId = id;
            target          = entity_acquire(sys, id);
        }
    }

    const float maxRange   = 8.0f * TILE_SIZE;
    const float maxRangeSq = maxRange * maxRange;
    Vector2     toHome     = {e->home.x - e->position.x, e->home.y - e->position.y};
    float       homeDistSq = toHome.x * toHome.x + toHome.y * toHome.y;

    if (!target && canShelter && isNight)
    {
        float dist = sqrtf(homeDistSq);
        if (dist > e->type->radius * 1.5f)
        {
            desiredGoal    = e->home;
            haveGoal       = true;
            seekingShelter = true;
        }
    }

    if (!haveGoal)
    {
        if (target)
        {
            desiredGoal = target->position;
            haveGoal    = true;
        }
        else if (homeDistSq > maxRangeSq)
        {
            desiredGoal = e->home;
            haveGoal    = true;
        }
    }

    if (haveGoal)
    {
        float goalDistSq = (desiredGoal.x - e->position.x) * (desiredGoal.x - e->position.x) + (desiredGoal.y - e->position.y) * (desiredGoal.y - e->position.y);

        bool usedPath = false;
        if (goalDistSq > 64.0f)
        {
            brain->repathTimer -= dt;
            bool needNewWaypoint = !brain->waypointValid;
            if (!needNewWaypoint)
            {
                float goalDelta = (brain->pathGoal.x - desiredGoal.x) * (brain->pathGoal.x - desiredGoal.x) + (brain->pathGoal.y - desiredGoal.y) * (brain->pathGoal.y - desiredGoal.y);
                if (goalDelta > TILE_SIZE * TILE_SIZE)
                    needNewWaypoint = true;
            }

            if (needNewWaypoint || brain->repathTimer <= 0.0f)
            {
                PathfindingOptions options = {
                    .allowDiagonal = true,
                    .canOpenDoors  = behavior_entity_has_competence(e, ENTITY_COMPETENCE_OPEN_DOORS),
                    .agentRadius   = e->type->radius,
                };

                PathfindingPath path;
                if (pathfinding_find_path(map, e->position, desiredGoal, &options, &path) && path.count > 0)
                {
                    Vector2 nextPoint = path.points[0];
                    if (path.count >= 2)
                        nextPoint = path.points[1];

                    brain->waypoint      = nextPoint;
                    brain->pathGoal      = desiredGoal;
                    brain->waypointValid = 1;
                    brain->repathTimer   = 0.6f;
                }
                else
                {
                    brain->waypointValid = 0;
                    brain->repathTimer   = 0.3f;
                }
            }

            if (brain->waypointValid)
            {
                Vector2 toWaypoint = {brain->waypoint.x - e->position.x, brain->waypoint.y - e->position.y};
                float   distance   = sqrtf(toWaypoint.x * toWaypoint.x + toWaypoint.y * toWaypoint.y);
                if (distance > 1e-3f)
                {
                    float inv      = 1.0f / distance;
                    e->velocity.x  = toWaypoint.x * inv * e->type->maxSpeed;
                    e->velocity.y  = toWaypoint.y * inv * e->type->maxSpeed;
                    e->orientation = atan2f(e->velocity.y, e->velocity.x);
                    usedPath       = true;
                }

                if (distance < TILE_SIZE * 0.2f)
                    brain->waypointValid = 0;
            }
        }

        if (!usedPath)
        {
            Vector2 toGoal   = {desiredGoal.x - e->position.x, desiredGoal.y - e->position.y};
            float   distance = sqrtf(toGoal.x * toGoal.x + toGoal.y * toGoal.y);
            if (distance > 1e-3f)
            {
                float inv      = 1.0f / distance;
                e->velocity.x  = toGoal.x * inv * ((seekingShelter ? 0.9f : 1.0f) * e->type->maxSpeed);
                e->velocity.y  = toGoal.y * inv * ((seekingShelter ? 0.9f : 1.0f) * e->type->maxSpeed);
                e->orientation = atan2f(e->velocity.y, e->velocity.x);
            }
            brain->waypointValid = 0;
        }

        brain->wanderTimer = 0.0f;
    }
    else if (homeDistSq > maxRangeSq)
    {
        float dist = sqrtf(homeDistSq);
        if (dist > 1e-3f)
        {
            float inv      = 1.0f / dist;
            e->velocity.x  = toHome.x * inv * (e->type->maxSpeed * 0.9f);
            e->velocity.y  = toHome.y * inv * (e->type->maxSpeed * 0.9f);
            e->orientation = atan2f(e->velocity.y, e->velocity.x);
        }
        brain->wanderTimer = entity_randomf(sys, 0.2f, 0.8f);
    }
    else if (brain->wanderTimer <= 0.0f)
    {
        cannibal_pick_direction(sys, e, brain);
    }
    else
    {
        brain->wanderTimer -= dt;
    }

    Vector2 next = {
        e->position.x + e->velocity.x * dt,
        e->position.y + e->velocity.y * dt,
    };

    float nextHomeDx = next.x - e->home.x;
    float nextHomeDy = next.y - e->home.y;
    float nextHomeSq = nextHomeDx * nextHomeDx + nextHomeDy * nextHomeDy;
    if (nextHomeSq > maxRangeSq)
    {
        float dist = sqrtf(toHome.x * toHome.x + toHome.y * toHome.y);
        if (dist > 1e-3f)
        {
            float inv      = 1.0f / dist;
            e->velocity.x  = toHome.x * inv * e->type->maxSpeed;
            e->velocity.y  = toHome.y * inv * e->type->maxSpeed;
            e->orientation = atan2f(e->velocity.y, e->velocity.x);
            next.x         = e->position.x + e->velocity.x * dt;
            next.y         = e->position.y + e->velocity.y * dt;
        }
    }

    if (!entity_position_is_walkable(map, next, e->type->radius))
    {
        bool openedDoor = behavior_try_open_doors(e, mutableMap, next);
        if (!openedDoor || !entity_position_is_walkable(map, next, e->type->radius))
        {
            e->velocity.x        = -e->velocity.x * 0.3f;
            e->velocity.y        = -e->velocity.y * 0.3f;
            brain->wanderTimer   = 0.0f;
            brain->lastHP        = e->hp;
            brain->waypointValid = 0;
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
        float attackRange = (e->type->radius + target->type->radius) + 10.0f;
        if ((distSq <= attackRange * attackRange || wasHit) && brain->attackCooldown <= 0.0f)
        {
            target->hp -= 18;
            if (target->hp <= 0)
                entity_despawn(sys, target->id);
            brain->attackCooldown = 0.9f;
        }
    }

    brain->lastHP = e->hp;
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
