#include "behavior.h"
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "map.h"
#include "object.h"
#include "tile.h"
#include "world_time.h"
#include "building.h"
#include "pantry.h"

// -----------------------------------------------------------------------------
// Behaviour tuning constants
// -----------------------------------------------------------------------------

#define HUNGER_DECAY_UNDEAD_PER_SECOND 0.12f
#define HUNGER_STARVATION_DAYS 5.0f
#define HUNGER_ALERT_THRESHOLD 40.0f
#define HUNGER_STARVATION_THRESHOLD 0.5f
#define HUNGER_FEAST_AMOUNT 45.0f
#define GATHER_FEAST_AMOUNT 30.0f
#define REPRODUCTION_DISTANCE (TILE_SIZE * 2.0f)
#define REPRODUCTION_COOLDOWN_SECONDS 65.0f
#define REPRODUCTION_ANIMATION_SECONDS 5.0f
#define HUNT_SEARCH_RADIUS_TILES 12
#define HUNT_ENRAGED_BONUS_TILES 4
#define GATHER_SEARCH_RADIUS_TILES 8

static bool behavior_object_is_gatherable(const Object* obj);

static void behavior_reward_nutrition(Entity* entity, float amount)
{
    if (!entity || !entity->active)
        return;
    entity->hunger += amount;
    if (entity->hunger > entity->maxHunger)
        entity->hunger = entity->maxHunger;
    entity->isHungry = (entity->hunger <= HUNGER_ALERT_THRESHOLD);
}

static bool behavior_deposit_food(Entity* entity, PantryItemType item, int quantity)
{
    if (!entity || quantity <= 0)
        return false;

    Building* home = entity_get_home(entity);
    if (!home || !home->hasPantry)
        return false;

    Pantry* pantry = pantry_get_for_building(home->id);
    if (!pantry)
        pantry = pantry_create_or_get(home->id, home->pantryCapacity);
    if (!pantry)
        return false;

    return pantry_deposit(pantry, item, quantity);
}

static bool behavior_withdraw_food(Entity* entity, PantryItemType item, int quantity)
{
    if (!entity || quantity <= 0)
        return false;

    Building* home = entity_get_home(entity);
    if (!home || !home->hasPantry)
        return false;

    Pantry* pantry = pantry_get_for_building(home->id);
    if (!pantry)
        pantry = pantry_create_or_get(home->id, home->pantryCapacity);
    if (!pantry)
        return false;

    int withdrawn = pantry_withdraw(pantry, item, quantity);
    if (withdrawn <= 0)
        return false;

    behavior_reward_nutrition(entity, HUNGER_FEAST_AMOUNT * (float)withdrawn);
    return true;
}

static void behavior_normalize_token(const char* src, char* dst, size_t cap)
{
    if (!dst || cap == 0)
        return;
    size_t len = 0;
    while (src && *src && len + 1 < cap)
    {
        unsigned char c = (unsigned char)*src++;
        if (c == ' ' || c == '-' || c == '\t')
            c = '_';
        dst[len++] = (char)tolower(c);
    }
    dst[len] = '\0';
}

static bool behavior_object_matches_descriptor(const Object* obj, const char* descriptor)
{
    if (!obj || !obj->type || !descriptor || descriptor[0] == '\0')
        return false;

    char needle[ENTITY_TARGET_TAG_MAX];
    behavior_normalize_token(descriptor, needle, sizeof(needle));
    if (needle[0] == '\0')
        return false;

    char              buffer[ENTITY_TARGET_TAG_MAX];
    const ObjectType* type = obj->type;
    if (type->category)
    {
        behavior_normalize_token(type->category, buffer, sizeof(buffer));
        if (strstr(buffer, needle))
            return true;
    }
    if (type->name)
    {
        behavior_normalize_token(type->name, buffer, sizeof(buffer));
        if (strstr(buffer, needle))
            return true;
    }
    if (type->displayName)
    {
        behavior_normalize_token(type->displayName, buffer, sizeof(buffer));
        if (strstr(buffer, needle))
            return true;
    }
    return false;
}

static bool behavior_can_gather_object(const Entity* entity, const Object* obj)
{
    if (!obj)
        return false;
    const EntityType* type = entity ? entity->type : NULL;
    if (type && type->gatherTargetCount > 0)
    {
        for (int i = 0; i < type->gatherTargetCount; ++i)
        {
            if (behavior_object_matches_descriptor(obj, type->gatherTargets[i]))
                return true;
        }
        return false;
    }
    return behavior_object_is_gatherable(obj);
}

static bool behavior_entity_matches_target(const Entity* candidate, const char* descriptor)
{
    if (!candidate || !candidate->type || !descriptor || descriptor[0] == '\0')
        return false;

    if (strcmp(descriptor, "any") == 0)
        return true;
    if (strcmp(descriptor, "living") == 0)
        return !candidate->isUndead;
    if (strcmp(descriptor, "undead") == 0)
        return candidate->isUndead;
    if (entity_type_has_trait(candidate->type, descriptor))
        return true;
    if (entity_type_is_category(candidate->type, descriptor))
        return true;
    if (candidate->type->identifier[0] != '\0' && strstr(candidate->type->identifier, descriptor))
        return true;
    return false;
}

static Building* behavior_select_home_building(const Entity* a, const Entity* b)
{
    Building* homeA = entity_get_home(a);
    if (homeA)
        return homeA;
    Building* homeB = entity_get_home(b);
    if (homeB)
        return homeB;

    const char* species = NULL;
    int         village = -1;
    if (a && a->type && a->type->species[0] != '\0')
    {
        species = a->type->species;
        village = a->villageId;
    }
    else if (b && b->type && b->type->species[0] != '\0')
    {
        species = b->type->species;
        village = b->villageId;
    }

    if (species)
        return building_get_for_species(species, village);
    return NULL;
}

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

    return behavior_force_open_doors(entity, map, desiredPosition, -1.0f);
}

bool behavior_force_open_doors(Entity* entity, Map* map, Vector2 desiredPosition, float radiusOverride)
{
    if (!entity || !map)
        return false;

    const EntityType* type   = entity->type;
    float             radius = (radiusOverride >= 0.0f) ? radiusOverride : (type ? type->radius : 0.0f);

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

static float behavior_last_step_seconds(void)
{
    float dt = world_time_get_last_step_seconds();
    if (dt <= 0.0f)
        dt = 1.0f / 60.0f;
    return dt;
}

static EntitySystem* behavior_get_system(Entity* e, EntityList* list)
{
    if (list)
        return list;
    return e ? e->system : NULL;
}

static void behavior_species_label(const EntityType* type, char* out, size_t cap)
{
    if (!out || cap == 0)
        return;
    if (!type)
    {
        out[0] = '\0';
        return;
    }

    snprintf(out, cap, "%s", type->identifier);
    char* cut = strchr(out, '_');
    if (cut)
        *cut = '\0';
}

static const EntityType* behavior_pick_offspring_type(const EntitySystem* sys, const EntityType* parentType)
{
    if (!sys || !parentType)
        return NULL;

    if (parentType->offspringTypeId > ENTITY_TYPE_INVALID)
    {
        const EntityType* explicitType = entity_find_type(sys, parentType->offspringTypeId);
        if (explicitType)
            return explicitType;
    }

    char parentBase[ENTITY_TYPE_NAME_MAX];
    behavior_species_label(parentType, parentBase, sizeof(parentBase));

    int typeCount = entity_system_type_count(sys);
    for (int i = 0; i < typeCount; ++i)
    {
        const EntityType* candidate = entity_system_type_at(sys, i);
        if (!candidate || candidate == parentType)
            continue;

        char candidateBase[ENTITY_TYPE_NAME_MAX];
        behavior_species_label(candidate, candidateBase, sizeof(candidateBase));

        if (strcmp(candidateBase, parentBase) != 0)
            continue;

        if (strstr(candidate->identifier, "child") || entity_type_has_trait(candidate, "child") || entity_type_has_trait(candidate, "juvenile"))
        {
            return candidate;
        }
    }

    return parentType;
}

static bool behavior_can_mate(const Entity* e)
{
    if (!e || !e->active || !e->type)
        return false;
    if (!e->type->canReproduce)
        return false;
    if (e->isUndead)
        return false;
    if (e->sex == ENTITY_SEX_UNDEFINED)
        return false;
    if (e->reproductionCooldown > 0.0f)
        return false;
    if (e->affectionTimer > 0.0f)
        return false;
    if (e->isHungry)
        return false;
    return true;
}

static bool behavior_entities_are_idle(const Entity* a)
{
    if (!a)
        return false;
    float speedSq = a->velocity.x * a->velocity.x + a->velocity.y * a->velocity.y;
    return speedSq < 8.0f * 8.0f;
}

static bool behavior_spawn_bone_pile(Map* map, Vector2 position)
{
    if (!map)
        return false;

    int baseTileX = (int)floorf(position.x / (float)TILE_SIZE);
    int baseTileY = (int)floorf(position.y / (float)TILE_SIZE);
    const int radius = 2;

    for (int r = 0; r <= radius; ++r)
    {
        for (int dy = -r; dy <= r; ++dy)
        {
            for (int dx = -r; dx <= r; ++dx)
            {
                if (abs(dx) != r && abs(dy) != r)
                    continue;

                int tx = baseTileX + dx;
                int ty = baseTileY + dy;
                if (tx < 0 || ty < 0 || tx >= map->width || ty >= map->height)
                    continue;

                if (map->objects[ty][tx] != NULL)
                    continue;

                map_place_object(map, OBJ_BONE_PILE, tx, ty);
                return true;
            }
        }
    }

    if (baseTileX >= 0 && baseTileY >= 0 && baseTileX < map->width && baseTileY < map->height)
    {
        map_place_object(map, OBJ_BONE_PILE, baseTileX, baseTileY);
        return true;
    }

    return false;
}

void behavior_handle_entity_death(EntitySystem* sys, Map* map, Entity* victim, Entity* killer)
{
    if (!sys || !victim)
        return;

    Map* mutableMap = map;
    if (mutableMap)
        behavior_spawn_bone_pile(mutableMap, victim->position);

    if (killer && killer->active)
    {
        behavior_reward_nutrition(killer, HUNGER_FEAST_AMOUNT);
        bool stored = behavior_deposit_food(killer, PANTRY_ITEM_MEAT, 1);
        if (!stored)
            behavior_reward_nutrition(killer, GATHER_FEAST_AMOUNT);
        killer->behaviorTargetId = ENTITY_ID_INVALID;
        killer->behaviorTimer    = 1.5f;
    }

    entity_despawn(sys, victim->id);
}

void behavior_hunger_update(EntitySystem* sys, Entity* entity, Map* map)
{
    if (!entity || !entity->active || !entity->type)
        return;

    const float dt    = behavior_last_step_seconds();
    float       decay = HUNGER_DECAY_UNDEAD_PER_SECOND;

    if (!entity->isUndead)
    {
        float secondsPerDay = world_time_get_seconds_per_day();
        if (secondsPerDay <= 0.0f)
            secondsPerDay = 600.0f;
        float maxHunger = entity->maxHunger > 0.0f ? entity->maxHunger : 100.0f;
        float targetSeconds = secondsPerDay * HUNGER_STARVATION_DAYS;
        if (targetSeconds <= 0.0f)
            targetSeconds = secondsPerDay * 5.0f;
        decay = maxHunger / targetSeconds;
    }

    entity->hunger -= decay * dt;
    if (entity->hunger < 0.0f)
        entity->hunger = 0.0f;

    entity->isHungry = (entity->hunger <= HUNGER_ALERT_THRESHOLD);

    if (entity->hunger <= HUNGER_STARVATION_THRESHOLD)
    {
        if (entity->isUndead)
        {
            entity->enraged = true;
        }
        else
        {
            EntitySystem* owner = sys ? sys : entity->system;
            if (owner)
                behavior_handle_entity_death(owner, map, entity, NULL);
        }
        return;
    }

    if (!entity->isUndead)
        entity->enraged = false;
}

void behavior_try_reproduce(Entity* entity, EntityList* entities)
{
    if (!entity || !entity->active)
        return;

    if (!entity->type || !entity->type->canReproduce)
        return;

    if (!behavior_is_night(0.55f))
        return;

    EntitySystem* sys = behavior_get_system(entity, entities);
    if (!sys)
        return;

    if (!behavior_can_mate(entity) || !behavior_entities_are_idle(entity))
        return;

    const EntityType* type = entity->type;
    char              species[ENTITY_TYPE_NAME_MAX];
    behavior_species_label(type, species, sizeof(species));
    float   bestDistSq = REPRODUCTION_DISTANCE * REPRODUCTION_DISTANCE;
    Entity* partner    = NULL;

    for (int i = 0; i <= sys->highestIndex; ++i)
    {
        Entity* other = &sys->entities[i];
        if (!other->active || other->id == entity->id || !other->type)
            continue;
        if (!behavior_can_mate(other) || !behavior_entities_are_idle(other))
            continue;
        if (other->sex == entity->sex)
            continue;

        char otherSpecies[ENTITY_TYPE_NAME_MAX];
        behavior_species_label(other->type, otherSpecies, sizeof(otherSpecies));
        if (species[0] == '\0' || otherSpecies[0] == '\0')
        {
            if (other->type != type)
                continue;
        }
        else if (strcmp(otherSpecies, species) != 0)
        {
            continue;
        }

        float dx     = other->position.x - entity->position.x;
        float dy     = other->position.y - entity->position.y;
        float distSq = dx * dx + dy * dy;
        if (distSq > bestDistSq)
            continue;

        bestDistSq = distSq;
        partner    = other;
    }

    if (!partner)
        return;

    entity->velocity  = (Vector2){0.0f, 0.0f};
    partner->velocity = (Vector2){0.0f, 0.0f};

    entity->affectionTimer        = REPRODUCTION_ANIMATION_SECONDS;
    entity->affectionPhase        = 0.0f;
    entity->reproductionCooldown  = REPRODUCTION_COOLDOWN_SECONDS;
    entity->reproductionPartnerId = partner->id;

    partner->affectionTimer        = REPRODUCTION_ANIMATION_SECONDS;
    partner->affectionPhase        = 0.0f;
    partner->reproductionCooldown  = REPRODUCTION_COOLDOWN_SECONDS;
    partner->reproductionPartnerId = entity->id;

    if (entity->id < partner->id)
    {
        float roll = entity_randomf(sys, 0.0f, 1.0f);
        if (roll <= 0.25f)
        {
            const EntityType* offspringType = behavior_pick_offspring_type(sys, type);
            if (offspringType)
            {
                Vector2 spawnPos = {
                    (entity->position.x + partner->position.x) * 0.5f,
                    (entity->position.y + partner->position.y) * 0.5f,
                };
                float jitter = TILE_SIZE * 0.35f;
                spawnPos.x += entity_randomf(sys, -jitter, jitter);
                spawnPos.y += entity_randomf(sys, -jitter, jitter);

                uint16_t childId = entity_spawn(sys, offspringType->id, spawnPos);
                if (childId != ENTITY_ID_INVALID)
                {
                    Entity* child = entity_acquire(sys, childId);
                    if (child)
                    {
                        child->home    = spawnPos;
                        child->hunger  = child->maxHunger * 0.75f;
                        child->sex     = (child->type && child->type->sex != ENTITY_SEX_UNDEFINED) ? child->type->sex : ENTITY_SEX_UNDEFINED;
                        Building* home = behavior_select_home_building(entity, partner);
                        if (home)
                        {
                            building_add_resident(home, child);
                            building_on_reservation_spawn(home->id);
                        }
                        else
                        {
                            child->homeBuildingId = -1;
                            child->villageId      = -1;
                        }
                    }
                }
            }
        }
    }
}

static bool behavior_is_valid_prey(const Entity* hunter, const Entity* candidate)
{
    if (!hunter || !hunter->type || !hunter->type->canHunt)
        return false;
    if (!candidate || !candidate->active || !candidate->type)
        return false;
    if (candidate->id == hunter->id)
        return false;
    if (hunter->type->speciesId > 0 && candidate->speciesId == hunter->type->speciesId)
        return false;

    if (hunter->type->huntTargetCount == 0)
        return !candidate->isUndead;

    for (int i = 0; i < hunter->type->huntTargetCount; ++i)
    {
        if (behavior_entity_matches_target(candidate, hunter->type->huntTargets[i]))
            return true;
    }

    return false;
}

void behavior_hunt(Entity* entity, EntityList* entities, Map* map)
{
    (void)map;
    if (!entity || !entity->active || entity->isUndead)
        return;

    if (!entity->type || !entity->type->canHunt)
        return;

    if (behavior_is_night(0.55f))
        return;

    if (entity->affectionTimer > 0.0f)
        return;

    EntitySystem* sys = behavior_get_system(entity, entities);
    if (!sys)
        return;

    if (entity->behaviorTargetId != ENTITY_ID_INVALID)
    {
        Entity* target = entity_acquire(sys, entity->behaviorTargetId);
        if (!target || !target->active)
        {
            behavior_reward_nutrition(entity, HUNGER_FEAST_AMOUNT);
            behavior_deposit_food(entity, PANTRY_ITEM_MEAT, 1);
            entity->behaviorTargetId = ENTITY_ID_INVALID;
            entity->behaviorTimer    = 1.5f;
        }
    }

    if (!entity->isHungry && entity->hunger > entity->maxHunger * 0.7f)
    {
        if (entity->behaviorTimer <= 0.0f)
            entity->behaviorTargetId = ENTITY_ID_INVALID;
        return;
    }

    if (entity->behaviorTimer > 0.0f && entity->behaviorTargetId != ENTITY_ID_INVALID)
        return;

    int   radiusTiles = HUNT_SEARCH_RADIUS_TILES + (entity->enraged ? HUNT_ENRAGED_BONUS_TILES : 0);
    float radius      = radiusTiles * (float)TILE_SIZE;
    float radiusSq    = radius * radius;

    Entity* best     = NULL;
    float   bestDist = radiusSq;

    for (int i = 0; i <= sys->highestIndex; ++i)
    {
        Entity* other = &sys->entities[i];
        if (!behavior_is_valid_prey(entity, other))
            continue;

        float dx     = other->position.x - entity->position.x;
        float dy     = other->position.y - entity->position.y;
        float distSq = dx * dx + dy * dy;
        if (distSq >= bestDist)
            continue;

        bestDist = distSq;
        best     = other;
    }

    if (best)
    {
        entity->behaviorTargetId = best->id;
        entity->behaviorTimer    = 1.0f;
    }
    else
    {
        entity->behaviorTargetId = ENTITY_ID_INVALID;
        entity->behaviorTimer    = 0.5f;
    }
}

static bool behavior_object_is_gatherable(const Object* obj)
{
    if (!obj || !obj->type)
        return false;
    const ObjectType* type = obj->type;
    if (type->category && strcmp(type->category, "resource") == 0)
        return true;
    if (type->name && (strstr(type->name, "bush") || strstr(type->name, "plant")))
        return true;
    return false;
}

void behavior_gather(Entity* entity, Map* map)
{
    if (!entity || !entity->active || !map)
        return;

    if (!entity->type || !entity->type->canGather)
        return;

    if (behavior_is_night(0.55f))
    {
        entity->gatherActive = 0;
        return;
    }

    if (entity->isUndead || entity->affectionTimer > 0.0f)
        return;

    if (!entity->isHungry && entity->hunger > entity->maxHunger * 0.75f)
    {
        entity->gatherActive = 0;
        return;
    }

    if (entity->gatherActive)
    {
        float dx      = entity->gatherTarget.x - entity->position.x;
        float dy      = entity->gatherTarget.y - entity->position.y;
        float distSq  = dx * dx + dy * dy;
        float reachSq = (float)(TILE_SIZE * 0.6f) * (float)(TILE_SIZE * 0.6f);
        if (distSq <= reachSq)
        {
            int targetX = (int)floorf(entity->gatherTarget.x / TILE_SIZE);
            int targetY = (int)floorf(entity->gatherTarget.y / TILE_SIZE);
            if (targetX >= 0 && targetX < map->width && targetY >= 0 && targetY < map->height)
            {
                Object* obj = map->objects[targetY][targetX];
                if (behavior_can_gather_object(entity, obj))
                {
                    bool stored = behavior_deposit_food(entity, PANTRY_ITEM_PLANT, 1);
                    if (!stored && obj)
                        map_remove_object(map, targetX, targetY);
                    behavior_reward_nutrition(entity, GATHER_FEAST_AMOUNT);
                    if (stored && obj)
                        map_remove_object(map, targetX, targetY);
                }
            }
            entity->gatherActive  = 0;
            entity->behaviorTimer = 0.8f;
        }
        return;
    }

    if (entity->behaviorTimer > 0.0f)
        return;

    int centerX = (int)floorf(entity->position.x / TILE_SIZE);
    int centerY = (int)floorf(entity->position.y / TILE_SIZE);
    int radius  = GATHER_SEARCH_RADIUS_TILES;

    float   bestDist = (float)(radius * TILE_SIZE) * (float)(radius * TILE_SIZE);
    Vector2 target   = {0.0f, 0.0f};
    bool    found    = false;

    for (int dy = -radius; dy <= radius; ++dy)
    {
        int ty = centerY + dy;
        if (ty < 0 || ty >= map->height)
            continue;

        for (int dx = -radius; dx <= radius; ++dx)
        {
            int tx = centerX + dx;
            if (tx < 0 || tx >= map->width)
                continue;

            Object* obj = map->objects[ty][tx];
            if (!behavior_can_gather_object(entity, obj))
                continue;

            float worldX = (tx + 0.5f) * TILE_SIZE;
            float worldY = (ty + 0.5f) * TILE_SIZE;
            float ddx    = worldX - entity->position.x;
            float ddy    = worldY - entity->position.y;
            float distSq = ddx * ddx + ddy * ddy;
            if (distSq >= bestDist)
                continue;

            bestDist = distSq;
            target   = (Vector2){worldX, worldY};
            found    = true;
        }
    }

    if (found)
    {
        entity->gatherTarget  = target;
        entity->gatherActive  = 1;
        entity->behaviorTimer = 1.0f;
    }
}

void behavior_eat_if_hungry(Entity* entity)
{
    if (!entity || !entity->active)
        return;
    if (entity->isUndead)
        return;
    if (entity->hunger >= entity->maxHunger * 0.5f)
        return;

    if (behavior_withdraw_food(entity, PANTRY_ITEM_MEAT, 1))
        return;

    behavior_withdraw_food(entity, PANTRY_ITEM_PLANT, 1);
}
