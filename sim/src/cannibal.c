#include "cannibal.h"

#include <math.h>
#include <string.h>

#include "behavior.h"
#include "building.h"
#include "map.h"
#include "pathfinding.h"
#include "tile.h"
#include "world_time.h"

#ifndef PI
#define PI 3.14159265358979323846f
#endif

typedef struct CannibalBrain
{
    float   wanderTimer;
    float   attackCooldown;
    float   repathTimer;
    float   romanceTimer;
    float   romanceCooldownDays;
    float   juvenileAgeDays;
    float   heartPhase;
    int     lastHP;
    int     targetId;
    int     romancePartnerId;
    int     romanceBuildingId;
    Vector2 pathGoal;
    Vector2 waypoint;
    uint8_t waypointValid;
    uint8_t romanceActive;
    uint8_t pendingBaby;
    uint8_t padding;
} CannibalBrain;

static void cannibal_on_spawn(EntitySystem* sys, Entity* e);

static bool cannibal_is_child(const Entity* e)
{
    return e && e->type && e->type->id == ENTITY_TYPE_CANNIBAL_CHILD;
}

static bool cannibal_is_male(const Entity* e)
{
    return e && e->type && e->type->id == ENTITY_TYPE_CANNIBAL;
}

static bool cannibal_is_female(const Entity* e)
{
    return e && e->type && e->type->id == ENTITY_TYPE_CANNIBAL_WOMAN;
}

static bool cannibal_is_adult(const Entity* e)
{
    if (!e || !e->type)
        return false;
    EntitiesTypeID id = e->type->id;
    return id == ENTITY_TYPE_CANNIBAL || id == ENTITY_TYPE_CANNIBAL_WOMAN;
}

static float cannibal_sim_days_step(void)
{
    float secondsPerDay = world_time_get_seconds_per_day();
    if (secondsPerDay <= 0.0f)
        return 0.0f;
    return world_time_get_last_step_seconds() / secondsPerDay;
}

static bool cannibal_is_idle_candidate(const Entity* e, const CannibalBrain* brain)
{
    if (!e || !brain)
        return false;
    if (brain->targetId != ENTITY_ID_INVALID)
        return false;
    if (brain->romanceActive)
        return false;
    if (brain->wanderTimer > 0.0f)
        return false;
    float speedSq = e->velocity.x * e->velocity.x + e->velocity.y * e->velocity.y;
    return speedSq < (TILE_SIZE * 0.5f) * (TILE_SIZE * 0.5f);
}

static int cannibal_current_building(const EntitySystem* sys, const Entity* e)
{
    if (!sys || !e)
        return -1;
    int idx = e->reservationIndex;
    if (idx < 0 || idx >= sys->reservationCount)
        return -1;
    const EntityReservation* res = &sys->reservations[idx];
    if (!res->used)
        return res->buildingId;
    return res->buildingId;
}

static void cannibal_spawn_baby(EntitySystem* sys, Entity* parent, int partnerId, int buildingId)
{
    if (!sys || !parent)
        return;

    Vector2 spawnPos = parent->position;
    if (buildingId >= 0)
    {
        const Building* building = building_get(buildingId);
        if (building)
        {
            spawnPos.x = building->center.x * (float)TILE_SIZE;
            spawnPos.y = building->center.y * (float)TILE_SIZE;
        }
    }

    float jitter = TILE_SIZE * 0.35f;
    spawnPos.x += entity_randomf(sys, -jitter, jitter);
    spawnPos.y += entity_randomf(sys, -jitter, jitter);

    uint16_t childId = entity_spawn(sys, ENTITY_TYPE_CANNIBAL_CHILD, spawnPos);
    if (childId == ENTITY_ID_INVALID)
        return;

    Entity* child = entity_acquire(sys, childId);
    if (!child)
        return;

    child->home = parent->home;
    child->orientation = parent->orientation;
}

static void cannibal_end_romance(EntitySystem* sys, Entity* e, CannibalBrain* brain)
{
    if (!sys || !e || !brain)
        return;

    int partnerId  = brain->romancePartnerId;
    int buildingId = brain->romanceBuildingId;
    bool spawnBaby = brain->pendingBaby && cannibal_is_male(e) && !cannibal_is_child(e);

    brain->romanceActive      = 0;
    brain->romanceTimer       = 0.0f;
    brain->heartPhase         = 0.0f;
    brain->romancePartnerId   = ENTITY_ID_INVALID;
    brain->romanceBuildingId  = -1;
    brain->pendingBaby        = 0;

    if (partnerId != ENTITY_ID_INVALID)
    {
        Entity* partner = entity_acquire(sys, (uint16_t)partnerId);
        if (partner)
        {
            CannibalBrain* other = (CannibalBrain*)partner->brain;
            if (other)
            {
                other->romanceActive     = 0;
                other->romanceTimer      = 0.0f;
                other->heartPhase        = 0.0f;
                if (other->romancePartnerId == e->id)
                    other->romancePartnerId = ENTITY_ID_INVALID;
                other->romanceBuildingId = -1;
                other->pendingBaby       = 0;
            }
        }
    }

    if (spawnBaby)
        cannibal_spawn_baby(sys, e, partnerId, buildingId);
}

static void cannibal_update_romance_animation(EntitySystem* sys, Entity* e, CannibalBrain* brain, float dt)
{
    if (!sys || !e || !brain)
        return;

    e->velocity = (Vector2){0.0f, 0.0f};
    if (brain->romancePartnerId != ENTITY_ID_INVALID)
    {
        Entity* partner = entity_acquire(sys, (uint16_t)brain->romancePartnerId);
        if (partner)
        {
            float angle = atan2f(partner->position.y - e->position.y, partner->position.x - e->position.x);
            e->orientation = angle;
        }
    }

    brain->heartPhase += dt * 4.0f;
    if (brain->heartPhase > (float)(2.0f * PI))
        brain->heartPhase -= (float)(2.0f * PI);

    brain->romanceTimer -= dt;
    if (brain->romanceTimer <= 0.0f)
        cannibal_end_romance(sys, e, brain);
}

static void cannibal_promote_child(EntitySystem* sys, Entity* e)
{
    if (!sys || !e || !cannibal_is_child(e))
        return;

    float roll = entity_randomf(sys, 0.0f, 1.0f);
    EntitiesTypeID newTypeId = (roll < 0.5f) ? ENTITY_TYPE_CANNIBAL : ENTITY_TYPE_CANNIBAL_WOMAN;
    const EntityType* newType = entity_find_type(sys, newTypeId);
    if (!newType)
        return;

    e->type     = newType;
    e->behavior = newType->behavior;
    e->hp       = newType->maxHP;

    cannibal_on_spawn(sys, e);

    CannibalBrain* brain = (CannibalBrain*)e->brain;
    if (brain)
    {
        brain->romanceCooldownDays = 1.0f;
        brain->juvenileAgeDays     = 0.0f;
    }
}

static Entity* cannibal_find_partner(EntitySystem* sys, Entity* self, int buildingId)
{
    if (!sys || !self)
        return NULL;

    float maxDistance = TILE_SIZE * 1.6f;
    float maxDistanceSq = maxDistance * maxDistance;

    for (int i = 0; i <= sys->highestIndex; ++i)
    {
        Entity* candidate = &sys->entities[i];
        if (!candidate->active || candidate == self)
            continue;
        if (!cannibal_is_female(candidate) || !cannibal_is_adult(candidate) || cannibal_is_child(candidate))
            continue;

        CannibalBrain* otherBrain = (CannibalBrain*)candidate->brain;
        if (!otherBrain || otherBrain->romanceActive)
            continue;

        if (cannibal_current_building(sys, candidate) != buildingId)
            continue;

        if (!cannibal_is_idle_candidate(candidate, otherBrain))
            continue;

        float dx = candidate->position.x - self->position.x;
        float dy = candidate->position.y - self->position.y;
        float distSq = dx * dx + dy * dy;
        if (distSq > maxDistanceSq)
            continue;

        return candidate;
    }

    return NULL;
}

static bool cannibal_try_begin_romance(EntitySystem* sys,
                                        Entity*        male,
                                        CannibalBrain* maleBrain,
                                        bool           isNight,
                                        bool           maleIdle)
{
    if (!sys || !male || !maleBrain)
        return false;
    if (!isNight || !maleIdle)
        return false;
    if (!cannibal_is_male(male) || !cannibal_is_adult(male))
        return false;
    if (maleBrain->romanceActive || maleBrain->romanceCooldownDays > 0.0f)
        return false;

    int buildingId = cannibal_current_building(sys, male);
    if (buildingId < 0)
        return false;

    Entity* partner = cannibal_find_partner(sys, male, buildingId);
    if (!partner)
        return false;

    CannibalBrain* partnerBrain = (CannibalBrain*)partner->brain;
    if (!partnerBrain || partnerBrain->romanceCooldownDays > 0.0f)
        return false;

    float chance = entity_randomf(sys, 0.0f, 1.0f);
    if (chance > 0.20f)
    {
        maleBrain->romanceCooldownDays = fmaxf(maleBrain->romanceCooldownDays, 0.15f);
        return false;
    }

    maleBrain->romanceActive      = 1;
    partnerBrain->romanceActive   = 1;
    maleBrain->romancePartnerId   = partner->id;
    partnerBrain->romancePartnerId = male->id;
    maleBrain->romanceBuildingId  = buildingId;
    partnerBrain->romanceBuildingId = buildingId;
    maleBrain->romanceTimer       = partnerBrain->romanceTimer = 3.2f;
    maleBrain->heartPhase         = 0.0f;
    partnerBrain->heartPhase      = 0.0f;
    maleBrain->pendingBaby        = 1;
    partnerBrain->pendingBaby     = 0;
    maleBrain->wanderTimer        = 0.0f;
    partnerBrain->wanderTimer     = 0.0f;
    maleBrain->romanceCooldownDays = fmaxf(maleBrain->romanceCooldownDays, 1.0f);
    partnerBrain->romanceCooldownDays = fmaxf(partnerBrain->romanceCooldownDays, 1.0f);
    return true;
}

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
        brain->attackCooldown = 0.0f;
        brain->lastHP         = e->hp;
        brain->repathTimer    = 0.0f;
        brain->waypoint       = e->position;
        brain->pathGoal       = e->home;
        brain->waypointValid  = 0;
        brain->targetId            = ENTITY_ID_INVALID;
        brain->romancePartnerId    = ENTITY_ID_INVALID;
        brain->romanceBuildingId   = -1;
        brain->romanceTimer        = 0.0f;
        brain->romanceCooldownDays = 0.0f;
        brain->juvenileAgeDays     = 0.0f;
        brain->heartPhase          = 0.0f;
        brain->romanceActive       = 0;
        brain->pendingBaby         = 0;
        brain->padding             = 0;
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

    float simDayStep = cannibal_sim_days_step();
    if (brain->romanceCooldownDays > 0.0f)
    {
        brain->romanceCooldownDays -= simDayStep;
        if (brain->romanceCooldownDays < 0.0f)
            brain->romanceCooldownDays = 0.0f;
    }

    if (cannibal_is_child(e))
    {
        brain->juvenileAgeDays += simDayStep;
        if (brain->juvenileAgeDays >= 10.0f)
        {
            cannibal_promote_child(sys, e);
            brain = (CannibalBrain*)e->brain;
        }
    }

    if (brain->romanceActive)
    {
        cannibal_update_romance_animation(sys, e, brain, dt);
        brain->lastHP = e->hp;
        return;
    }

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
        target = entity_acquire(sys, (uint16_t)brain->targetId);
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
            brain->targetId = (int)id;
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
                float speedMul = seekingShelter ? 0.9f : 1.0f;
                e->velocity.x  = toGoal.x * inv * (e->type->maxSpeed * speedMul);
                e->velocity.y  = toGoal.y * inv * (e->type->maxSpeed * speedMul);
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
        if (brain->wanderTimer < 0.0f)
            brain->wanderTimer = 0.0f;
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

    bool idleForRomance = (!target && !seekingShelter && cannibal_is_idle_candidate(e, brain));
    if (idleForRomance && cannibal_is_male(e) && isNight)
    {
        if (cannibal_try_begin_romance(sys, e, brain, isNight, idleForRomance))
        {
            cannibal_update_romance_animation(sys, e, brain, dt);
            brain->lastHP = e->hp;
            return;
        }
    }

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

void cannibal_draw_overlay(const Entity* e)
{
    if (!e || !e->type)
        return;

    const CannibalBrain* brain = (const CannibalBrain*)e->brain;
    if (!brain || !brain->romanceActive)
        return;

    float radius = (e->type->radius > 0.0f) ? e->type->radius : 16.0f;
    float bob    = sinf(brain->heartPhase) * 4.0f;
    float baseY  = e->position.y - radius - 18.0f + bob;
    float centerX = e->position.x;

    unsigned char alpha = (unsigned char)fminf(255.0f, 180.0f + fabsf(sinf(brain->heartPhase * 0.5f)) * 60.0f);
    Color heartColor    = (Color){220, 40, 70, alpha};

    Vector2 left    = {centerX - 6.0f, baseY};
    Vector2 right   = {centerX + 6.0f, baseY};
    Vector2 bottomA = {centerX - 10.0f, baseY + 8.0f};
    Vector2 bottomB = {centerX, baseY + 16.0f};
    Vector2 bottomC = {centerX + 10.0f, baseY + 8.0f};

    DrawCircleV(left, 4.0f, heartColor);
    DrawCircleV(right, 4.0f, heartColor);
    DrawTriangle(bottomA, bottomB, bottomC, heartColor);
}
