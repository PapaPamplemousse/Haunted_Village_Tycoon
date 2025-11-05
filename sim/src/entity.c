/**
 * @file entity.c
 * @brief Implements the runtime entity pool and basic AI behaviours.
 */

#include "entity.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "world.h"
#include "building.h"
#include "object.h"
#include "entities_loader.h"
#include "zombie.h"
#include "cannibal.h"
#include "tile.h"
#include "behavior.h"
#include "world_time.h"

#ifndef PI
#define PI 3.14159265358979323846f
#endif
#ifndef RAD2DEG
#define RAD2DEG (180.0f / PI)
#endif
// -----------------------------------------------------------------------------
// Local helpers & utilities
// -----------------------------------------------------------------------------

unsigned int entity_random(EntitySystem* sys)
{
    unsigned int x = sys->rngState;
    if (x == 0)
        x = 0xBA5EBA11u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    sys->rngState = x;
    return x;
}

float entity_randomf(EntitySystem* sys, float min, float max)
{
    if (max <= min)
        return min;
    float t = (float)(entity_random(sys) & 0xFFFFFF) / (float)0xFFFFFF;
    return min + t * (max - min);
}

int entity_randomi(EntitySystem* sys, int min, int max)
{
    if (max < min)
        return min;
    unsigned int span = (unsigned int)(max - min + 1);
    return min + (int)(entity_random(sys) % (span ? span : 1));
}

static float entity_sim_days_step(void)
{
    float secondsPerDay = world_time_get_seconds_per_day();
    if (secondsPerDay <= 0.0f)
        return 0.0f;
    float stepSeconds = world_time_get_last_step_seconds();
    if (stepSeconds <= 0.0f)
        stepSeconds = 1.0f / 60.0f;
    return stepSeconds / secondsPerDay;
}

static void entity_reservation_reset(EntityReservation* res)
{
    if (!res)
        return;
    memset(res, 0, sizeof(*res));
    res->entityId        = ENTITY_ID_INVALID;
    res->buildingId      = -1;
    res->homeStructure   = STRUCT_COUNT;
    res->activationRadius   = 0.0f;
    res->deactivationRadius = 0.0f;
    res->villageId       = -1;
    res->speciesId       = 0;
    res->used            = false;
    res->active          = false;
}

static void entity_reservations_reset(EntitySystem* sys)
{
    if (!sys)
        return;
    sys->reservationCount = 0;
    for (int i = 0; i < ENTITY_MAX_RESERVATIONS; ++i)
        entity_reservation_reset(&sys->reservations[i]);
}

static EntityReservation* entity_reservation_acquire(EntitySystem* sys)
{
    if (!sys)
        return NULL;
    if (sys->reservationCount >= ENTITY_MAX_RESERVATIONS)
        return NULL;

    EntityReservation* res = &sys->reservations[sys->reservationCount++];
    entity_reservation_reset(res);
    res->used = true;
    return res;
}

static inline float entity_distance_sq(Vector2 a, Vector2 b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

static void entity_reservation_capture(EntityReservation* res, const Entity* ent)
{
    if (!res || !ent)
        return;
    res->position       = ent->position;
    res->velocity       = ent->velocity;
    res->orientation    = ent->orientation;
    res->hp             = ent->hp;
    res->home           = ent->home;
    res->homeStructure  = ent->homeStructure;
    res->buildingId     = ent->homeBuildingId;
    res->villageId      = ent->villageId;
    res->speciesId      = ent->speciesId;
}

static void entity_reservation_apply(EntityReservation* res, Entity* ent)
{
    if (!res || !ent)
        return;
    ent->position      = res->position;
    ent->velocity      = res->velocity;
    ent->orientation   = res->orientation;
    if (res->hp > 0 && ent->type && res->hp <= ent->type->maxHP)
        ent->hp = res->hp;
    ent->home          = res->home;
    ent->homeStructure = res->homeStructure;
    ent->homeBuildingId = res->buildingId;
    ent->villageId      = res->villageId;
    if (res->speciesId != 0)
        ent->speciesId = res->speciesId;
}

static bool entity_reservation_schedule(EntitySystem* sys,
                                        EntitiesTypeID typeId,
                                        Vector2 position,
                                        Vector2 home,
                                        StructureKind structure,
                                        int buildingId,
                                        int villageId,
                                        int speciesId,
                                        float activationRadius,
                                        float deactivationRadius)
{
    if (!sys || typeId <= ENTITY_TYPE_INVALID)
        return false;

    EntityReservation* res = entity_reservation_acquire(sys);
    if (!res)
        return false;

    res->typeId             = typeId;
    res->position           = position;
    res->home               = home;
    res->homeStructure      = structure;
    res->buildingId         = buildingId;
    res->villageId          = villageId;
    res->speciesId          = speciesId;
    res->activationRadius   = activationRadius;
    res->deactivationRadius = deactivationRadius;
    res->velocity           = (Vector2){0.0f, 0.0f};
    res->orientation        = 0.0f;
    res->hp                 = 0;
    return true;
}

static void entity_system_reset(EntitySystem* sys)
{
    if (!sys)
        return;
    memset(sys, 0, sizeof(*sys));
    sys->highestIndex = -1;
    sys->streamActivationPadding   = TILE_SIZE * 8.0f;
    sys->streamDeactivationPadding = TILE_SIZE * 12.0f;
    sys->speciesCount              = 0;
    sys->residentRefreshTimer      = 0.0f;
    entity_reservations_reset(sys);
}

static void entity_clear_slot(EntitySystem* sys, int index)
{
    Entity* e = &sys->entities[index];
    memset(e, 0, sizeof(*e));
    e->id = (uint16_t)index;
    e->reservationIndex = -1;
    e->system                 = sys;
    e->sex                    = ENTITY_SEX_UNDEFINED;
    e->hunger                 = 0.0f;
    e->maxHunger              = 100.0f;
    e->isUndead               = false;
    e->isHungry               = false;
    e->enraged                = false;
    e->reproductionCooldown   = 0.0f;
    e->affectionTimer         = 0.0f;
    e->affectionPhase         = 0.0f;
    e->reproductionPartnerId  = ENTITY_ID_INVALID;
    e->behaviorTargetId       = ENTITY_ID_INVALID;
    e->behaviorTimer          = 0.0f;
    e->gatherTarget           = (Vector2){0.0f, 0.0f};
    e->gatherActive           = 0;
    e->homeBuildingId         = -1;
    e->villageId              = -1;
    e->speciesId              = 0;
    e->ageDays                = 0.0f;
    e->isElder                = false;
}

static void entity_unload_sprite(EntitySprite* sprite)
{
    if (sprite && sprite->texture.id != 0)
    {
        UnloadTexture(sprite->texture);
        sprite->texture.id = 0;
    }
}

static void entity_load_sprite(EntitySprite* sprite)
{
    if (!sprite || sprite->texture.id != 0)
        return;

    if (sprite->texturePath[0] == '\0')
        return;

    Texture2D tex = LoadTexture(sprite->texturePath);
    if (tex.id == 0)
    {
        printf("⚠️  Failed to load entity texture '%s'\n", sprite->texturePath);
        return;
    }

    sprite->texture = tex;
    if (sprite->frameWidth <= 0)
        sprite->frameWidth = tex.width;
    if (sprite->frameHeight <= 0)
        sprite->frameHeight = tex.height;
    if (sprite->frameCount <= 0)
        sprite->frameCount = 1;
}

static BiomeKind infer_biome_from_tile(TileTypeID tile)
{
    switch (tile)
    {
        case TILE_FOREST:
            return BIO_FOREST;
        case TILE_PLAIN:
        case TILE_GRASS:
            return BIO_PLAIN;
        case TILE_SAVANNA:
            return BIO_SAVANNA;
        case TILE_TUNDRA:
        case TILE_TUNDRA_2:
            return BIO_TUNDRA;
        case TILE_DESERT:
            return BIO_DESERT;
        case TILE_SWAMP:
        case TILE_POISON:
            return BIO_SWAMP;
        case TILE_MOUNTAIN:
            return BIO_MOUNTAIN;
        case TILE_CURSED_FOREST:
            return BIO_CURSED;
        case TILE_HELL:
        case TILE_LAVA:
            return BIO_HELL;
        default:
            return BIO_PLAIN;
    }
}

static void normalize_label(const char* src, char* dst, size_t cap)
{
    if (!dst || cap == 0)
        return;

    if (!src)
    {
        dst[0] = '\0';
        return;
    }

    size_t len = 0;
    while (*src && len + 1 < cap)
    {
        unsigned char c = (unsigned char)*src++;
        if (c == ' ' || c == '-' || c == '\t')
            c = '_';
        dst[len++] = (char)tolower(c);
    }
    dst[len] = '\0';
}

static uint32_t entity_hash_string(const char* text)
{
    const uint32_t FNV_OFFSET = 2166136261u;
    const uint32_t FNV_PRIME  = 16777619u;

    uint32_t hash = FNV_OFFSET;
    if (!text)
        return hash;

    while (*text)
    {
        hash ^= (uint32_t)(unsigned char)(*text++);
        hash *= FNV_PRIME;
    }
    if (hash == 0u)
        hash = FNV_OFFSET;
    return hash;
}

int entity_species_id_from_label(const char* label)
{
    char normalised[ENTITY_SPECIES_NAME_MAX];
    normalize_label(label, normalised, sizeof(normalised));
    if (normalised[0] == '\0')
        return 0;
    uint32_t hash = entity_hash_string(normalised);
    return (int)(hash & 0x7FFFFFFFu);
}

static int entity_system_find_species(const EntitySystem* sys, const char* normalised)
{
    if (!sys || !normalised)
        return -1;
    for (int i = 0; i < sys->speciesCount; ++i)
    {
        if (strcmp(sys->speciesLabels[i], normalised) == 0)
            return i;
    }
    return -1;
}

int entity_system_register_species(EntitySystem* sys, const char* label)
{
    if (!sys)
        return 0;

    char normalised[ENTITY_SPECIES_NAME_MAX];
    normalize_label(label, normalised, sizeof(normalised));
    if (normalised[0] == '\0')
        return 0;

    int index = entity_system_find_species(sys, normalised);
    if (index >= 0)
        return entity_species_id_from_label(normalised);

    if (sys->speciesCount < ENTITY_MAX_SPECIES)
    {
        snprintf(sys->speciesLabels[sys->speciesCount], ENTITY_SPECIES_NAME_MAX, "%s", normalised);
        sys->speciesCount++;
    }

    return entity_species_id_from_label(normalised);
}

const char* entity_system_species_label(const EntitySystem* sys, int speciesId)
{
    if (!sys || speciesId <= 0)
        return NULL;

    for (int i = 0; i < sys->speciesCount; ++i)
    {
        if (entity_species_id_from_label(sys->speciesLabels[i]) == speciesId)
            return sys->speciesLabels[i];
    }
    return NULL;
}

bool entity_position_is_walkable(const Map* map, Vector2 position, float radius)
{
    if (!map)
        return false;

    if (radius < 0.0f)
        radius = 0.0f;

    int minX = (int)floorf((position.x - radius) / TILE_SIZE);
    int maxX = (int)floorf((position.x + radius) / TILE_SIZE);
    int minY = (int)floorf((position.y - radius) / TILE_SIZE);
    int maxY = (int)floorf((position.y + radius) / TILE_SIZE);

    for (int y = minY; y <= maxY; ++y)
    {
        if (y < 0 || y >= map->height)
            return false;

        for (int x = minX; x <= maxX; ++x)
        {
            if (x < 0 || x >= map->width)
                return false;

            TileTypeID tid = map->tiles[y][x];
            TileType*  tt  = get_tile_type(tid);
            if (!tt || !tt->walkable)
                return false;

            Object* obj = map->objects[y][x];
            if (obj && !object_is_walkable(obj))
                return false;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
// Behaviour helpers
// -----------------------------------------------------------------------------

static void entity_assign_builtin_behaviours(EntitySystem* sys)
{
    if (!sys)
        return;

    for (int i = 0; i < sys->typeCount; ++i)
    {
        EntityType* type = &sys->types[i];
        if (!type)
            continue;

        if (entity_type_has_trait(type, "cannibal"))
        {
            type->behavior = entity_cannibal_behavior();
            continue;
        }

        if (entity_type_has_trait(type, "undead"))
        {
            type->behavior = entity_zombie_behavior();
            continue;
        }
    }
}

// -----------------------------------------------------------------------------
// Registry helpers (shared with loader)
// -----------------------------------------------------------------------------

static void entity_type_apply_defaults(EntityType* type)
{
    if (!type)
        return;
    if (type->tint.a == 0)
        type->tint = (Color){200, 200, 200, 255};
    if (type->radius <= 0.0f)
        type->radius = 12.0f;
    if (type->maxHP <= 0)
        type->maxHP = 10;
    if (type->maxSpeed <= 0.0f)
        type->maxSpeed = 24.0f;
    if (type->identifier[0] == '\0')
        snprintf(type->identifier, sizeof(type->identifier), "type_%d", type->id);
    if (type->displayName[0] == '\0')
        snprintf(type->displayName, sizeof(type->displayName), "%s", type->identifier);
    if (type->category[0] == '\0')
        snprintf(type->category, sizeof(type->category), "%s", "unknown");
    if (type->traitCount < 0)
        type->traitCount = 0;
    if (type->traitCount > ENTITY_MAX_TRAITS)
        type->traitCount = ENTITY_MAX_TRAITS;
    if (type->referredStructure < 0 || type->referredStructure >= STRUCT_COUNT)
        type->referredStructure = STRUCT_COUNT;
}

void entity_spawn_rule_init(EntitySpawnRule* rule)
{
    if (!rule)
        return;

    memset(rule, 0, sizeof(*rule));
    rule->type     = NULL;
    rule->id       = ENTITY_TYPE_INVALID;
    rule->biome    = BIO_MAX;
    rule->tile     = TILE_MAX;
    rule->density  = 0.0f;
    rule->groupMin = 1;
    rule->groupMax = 1;
}

bool entity_system_register_type(EntitySystem* sys, const EntityType* def, const EntitySpawnRule* spawn)
{
    if (!sys || !def)
        return false;

    if (def->id <= ENTITY_TYPE_INVALID || def->id >= ENTITY_TYPE_COUNT)
    {
        printf("⚠️  Invalid entity id %d ignored\n", def->id);
        return false;
    }

    EntityType* dst = NULL;
    for (int i = 0; i < sys->typeCount; ++i)
    {
        if (sys->types[i].id == def->id)
        {
            dst = &sys->types[i];
            entity_unload_sprite(&dst->sprite);
            break;
        }
    }

    if (!dst)
    {
        if (sys->typeCount >= ENTITY_MAX_TYPES)
        {
            printf("⚠️  Entity registry full, ignoring id %d\n", def->id);
            return false;
        }
        dst = &sys->types[sys->typeCount++];
    }

    *dst = *def;
    if (dst->traitCount < 0)
        dst->traitCount = 0;
    if (dst->traitCount > ENTITY_MAX_TRAITS)
        dst->traitCount = ENTITY_MAX_TRAITS;
    entity_type_apply_defaults(dst);

    dst->sprite.texture.id = 0;
    entity_load_sprite(&dst->sprite);

    if (spawn && spawn->density > 0.0f)
    {
        if (sys->spawnRuleCount >= ENTITY_MAX_SPAWN_RULES)
        {
            printf("⚠️  Spawn rule list full, ignoring rule for entity %d\n", def->id);
        }
        else
        {
            EntitySpawnRule rule = *spawn;
            if (rule.id == ENTITY_TYPE_INVALID)
                rule.id = dst->id;
            if (rule.groupMin <= 0)
                rule.groupMin = 1;
            if (rule.groupMax < rule.groupMin)
                rule.groupMax = rule.groupMin;
            rule.type                              = (rule.id == dst->id) ? dst : entity_find_type(sys, rule.id);
            sys->spawnRules[sys->spawnRuleCount++] = rule;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
// Loader utilities (.stv parsing)
// -----------------------------------------------------------------------------
static void entity_register_fallbacks(EntitySystem* sys)
{
    if (!sys)
        return;

    EntityType zombie = {0};
    zombie.id         = ENTITY_TYPE_CURSED_ZOMBIE;
    snprintf(zombie.identifier, sizeof(zombie.identifier), "%s", "cursed_zombie");
    snprintf(zombie.displayName, sizeof(zombie.displayName), "%s", "Cursed Zombie");
    zombie.flags                = ENTITY_FLAG_HOSTILE | ENTITY_FLAG_MOBILE | ENTITY_FLAG_UNDEAD;
    zombie.maxHP                = 35;
    zombie.maxSpeed             = 32.0f;
    zombie.radius               = 12.0f;
    zombie.tint                 = (Color){120, 197, 120, 255};
    zombie.sprite.frameCount    = 1;
    zombie.sprite.frameDuration = 0.0f;
    normalize_label("undead", zombie.category, sizeof(zombie.category));
    zombie.traitCount = 1;
    normalize_label("undead", zombie.traits[0], sizeof(zombie.traits[0]));
    zombie.referredStructure = STRUCT_COUNT;

    EntitySpawnRule rule;
    entity_spawn_rule_init(&rule);
    rule.id       = zombie.id;
    rule.density  = 0.025f;
    rule.groupMin = 2;
    rule.groupMax = 4;
    rule.tile     = TILE_CURSED_FOREST;
    rule.biome    = BIO_CURSED;

    entity_system_register_type(sys, &zombie, &rule);

    EntityType cannibal = {0};
    cannibal.id         = ENTITY_TYPE_CANNIBAL;
    snprintf(cannibal.identifier, sizeof(cannibal.identifier), "%s", "cannibal");
    snprintf(cannibal.displayName, sizeof(cannibal.displayName), "%s", "Cannibal");
    cannibal.flags                = ENTITY_FLAG_HOSTILE | ENTITY_FLAG_MOBILE;
    cannibal.maxHP                = 80;
    cannibal.maxSpeed             = 42.0f;
    cannibal.radius               = 14.0f;
    cannibal.tint                 = (Color){137, 81, 41, 255};
    cannibal.sprite.frameCount    = 1;
    cannibal.sprite.frameDuration = 0.0f;
    normalize_label("humanoid", cannibal.category, sizeof(cannibal.category));
    cannibal.traitCount = 1;
    normalize_label("cannibal", cannibal.traits[0], sizeof(cannibal.traits[0]));
    cannibal.referredStructure = STRUCT_HUT_CANNIBAL;

    entity_spawn_rule_init(&rule);
    rule.id       = cannibal.id;
    rule.density  = 0.02f;
    rule.groupMin = 1;
    rule.groupMax = 3;
    rule.tile     = TILE_SAVANNA;
    rule.biome    = BIO_SAVANNA;

    entity_system_register_type(sys, &cannibal, &rule);
}

static bool entity_schedule_near_structures(EntitySystem* sys, const EntitySpawnRule* rule, const Map* map)
{
    if (!sys || !rule || !rule->type || !map)
        return false;

    if (rule->type->referredStructure == STRUCT_COUNT)
        return false;

    bool spawnedAny = false;
    int total = building_total_count();
    for (int b = 0; b < total; ++b)
    {
        const Building* building = building_get(b);
        if (!building || building->structureKind != rule->type->referredStructure)
            continue;

        Vector2 home  = {building->center.x * TILE_SIZE, building->center.y * TILE_SIZE};
        int     speciesId = building->speciesId;
        if (rule->type && speciesId == 0 && rule->type->speciesId > 0)
            speciesId = rule->type->speciesId;
        int group = entity_randomi(sys, rule->groupMin, rule->groupMax);
        if (group <= 0)
            group = 1;

        for (int g = 0; g < group; ++g)
        {
            bool   placed = false;
            Vector2 spawnPos;

            for (int attempt = 0; attempt < 6 && !placed; ++attempt)
            {
                float angle     = entity_randomf(sys, 0.0f, 2.0f * PI);
                float distTiles = entity_randomf(sys, 0.5f, 3.5f);
                spawnPos        = (Vector2){
                    home.x + cosf(angle) * distTiles * TILE_SIZE,
                    home.y + sinf(angle) * distTiles * TILE_SIZE,
                };

                if (!entity_position_is_walkable(map, spawnPos, rule->type->radius))
                    continue;

                placed = entity_reservation_schedule(sys,
                                                     rule->type->id,
                                                     spawnPos,
                                                     home,
                                                     rule->type->referredStructure,
                                                     building->id,
                                                     building->villageId,
                                                     speciesId,
                                                     0.0f,
                                                     0.0f);
                spawnedAny |= placed;
            }

            if (!placed && entity_position_is_walkable(map, home, rule->type->radius))
            {
                if (entity_reservation_schedule(sys,
                                                rule->type->id,
                                                home,
                                                home,
                                                rule->type->referredStructure,
                                                building->id,
                                                building->villageId,
                                                speciesId,
                                                0.0f,
                                                0.0f))
                    spawnedAny = true;
            }
        }
    }

    return spawnedAny;
}

typedef struct ResidentDemand
{
    EntitiesTypeID typeId;
    int            desired;
} ResidentDemand;

static int entity_count_residents_of_type(const Building* building, EntitySystem* sys, EntitiesTypeID typeId)
{
    if (!building || typeId <= ENTITY_TYPE_INVALID)
        return 0;

    int count = 0;
    for (int i = 0; i < building->residentCount; ++i)
    {
        uint16_t id = building->residents[i];
        Entity*  ent = entity_acquire(sys, id);
        if (!ent || !ent->active || !ent->type)
            continue;
        if (ent->type->id == typeId)
            count++;
    }
    return count;
}

static int entity_count_pending_reservations(const EntitySystem* sys, int buildingId, EntitiesTypeID typeId)
{
    if (!sys)
        return 0;
    int count = 0;
    for (int i = 0; i < sys->reservationCount; ++i)
    {
        const EntityReservation* res = &sys->reservations[i];
        if (!res->used || res->buildingId != buildingId)
            continue;
        if (typeId > ENTITY_TYPE_INVALID && res->typeId != typeId)
            continue;
        if (!res->active)
            count++;
    }
    return count;
}

static Entity* entity_find_homeless_near(EntitySystem* sys, const Building* building, EntitiesTypeID typeId, float radius)
{
    if (!sys || !building || typeId <= ENTITY_TYPE_INVALID)
        return NULL;

    float radiusSq = radius * radius;
    Vector2 center = {building->center.x * TILE_SIZE, building->center.y * TILE_SIZE};

    for (int i = 0; i <= sys->highestIndex; ++i)
    {
        Entity* ent = &sys->entities[i];
        if (!ent->active || !ent->type)
            continue;
        if (ent->homeBuildingId >= 0)
            continue;
        if (ent->type->id != typeId)
            continue;

        float dx = ent->position.x - center.x;
        float dy = ent->position.y - center.y;
        if ((dx * dx + dy * dy) > radiusSq)
            continue;

        return ent;
    }

    return NULL;
}

static int entity_collect_resident_demands(const Building* building, ResidentDemand* demands, int maxDemands)
{
    if (!building || !building->structureDef || building->occupantCurrent <= 0)
        return 0;

    EntitiesTypeID blueprint[STRUCTURE_MAX_RESIDENT_ROLES];
    int            blueprintCount = 0;

    const StructureDef* def = building->structureDef;

    if (building->structureKind == STRUCT_HUT_CANNIBAL)
    {
        if (blueprintCount < STRUCTURE_MAX_RESIDENT_ROLES && blueprintCount < building->occupantCurrent)
            blueprint[blueprintCount++] = ENTITY_TYPE_CANNIBAL;
        if (blueprintCount < STRUCTURE_MAX_RESIDENT_ROLES && blueprintCount < building->occupantCurrent)
            blueprint[blueprintCount++] = ENTITY_TYPE_CANNIBAL_WOMAN;
        if (blueprintCount < STRUCTURE_MAX_RESIDENT_ROLES && blueprintCount < building->occupantCurrent)
            blueprint[blueprintCount++] = ENTITY_TYPE_CANNIBAL_CHILD;
        while (blueprintCount < building->occupantCurrent && blueprintCount < STRUCTURE_MAX_RESIDENT_ROLES)
            blueprint[blueprintCount++] = ENTITY_TYPE_CANNIBAL;
    }
    else
    {
        EntitiesTypeID baseType = def->occupantType;
        if (baseType <= ENTITY_TYPE_INVALID)
            return 0;
        for (int i = 0; i < building->occupantCurrent && i < STRUCTURE_MAX_RESIDENT_ROLES; ++i)
            blueprint[blueprintCount++] = baseType;
    }

    int demandCount = 0;
    for (int i = 0; i < blueprintCount; ++i)
    {
        EntitiesTypeID typeId = blueprint[i];
        if (typeId <= ENTITY_TYPE_INVALID)
            continue;

        bool found = false;
        for (int j = 0; j < demandCount; ++j)
        {
            if (demands[j].typeId == typeId)
            {
                demands[j].desired++;
                found = true;
                break;
            }
        }

        if (!found && demandCount < maxDemands)
        {
            demands[demandCount].typeId = typeId;
            demands[demandCount].desired = 1;
            demandCount++;
        }
    }

    return demandCount;
}

static void entity_rebuild_building_occupancy(EntitySystem* sys)
{
    if (!sys)
        return;

    int totalBuildings = building_total_count();
    if (totalBuildings <= 0)
        return;

    int maxId = 0;
    for (int b = 0; b < totalBuildings; ++b)
    {
        Building* building = building_get_mutable(b);
        if (!building)
            continue;

        building->residentCount  = 0;
        building->occupantActive = 0;

        if (building->id > maxId)
            maxId = building->id;
    }

    if (maxId < 0)
        return;

    Building** lookup = (Building**)calloc((size_t)(maxId + 1), sizeof(Building*));
    if (!lookup)
        return;

    for (int b = 0; b < totalBuildings; ++b)
    {
        Building* building = building_get_mutable(b);
        if (!building)
            continue;
        if (building->id < 0 || building->id > maxId)
            continue;
        lookup[building->id] = building;
    }

    for (int i = 0; i <= sys->highestIndex; ++i)
    {
        Entity* ent = &sys->entities[i];
        if (!ent->active)
            continue;
        if (ent->homeBuildingId < 0)
            continue;
        if (ent->homeBuildingId > maxId)
        {
            ent->homeBuildingId = -1;
            ent->villageId      = -1;
            continue;
        }

        Building* target = lookup[ent->homeBuildingId];
        if (!target)
        {
            ent->homeBuildingId = -1;
            ent->villageId      = -1;
            continue;
        }

        int before = target->residentCount;
        building_add_resident(target, ent);
        if (target->residentCount > before)
            target->occupantActive++;
    }

    free(lookup);
}

static bool entity_schedule_resident(EntitySystem* sys, const Map* map, Building* building, const EntityType* type)
{
    if (!sys || !map || !building || !type)
        return false;

    int speciesId = type->speciesId > 0 ? type->speciesId : entity_species_id_from_label(type->species[0] ? type->species : type->identifier);
    Vector2 home = {building->center.x * TILE_SIZE, building->center.y * TILE_SIZE};

    bool   placed   = false;
    Vector2 spawnPos = home;

    for (int attempt = 0; attempt < 8 && !placed; ++attempt)
    {
        unsigned int seed = entity_random(sys);
        float angle       = ((float)(seed & 0xFFFFu) / 65535.0f) * 2.0f * PI;
        float dist        = 0.35f + ((float)((seed >> 16) & 0xFFFFu) / 65535.0f) * 1.8f;
        spawnPos          = (Vector2){home.x + cosf(angle) * dist * TILE_SIZE, home.y + sinf(angle) * dist * TILE_SIZE};

        if (!entity_position_is_walkable(map, spawnPos, type->radius))
            continue;

        placed = entity_reservation_schedule(sys,
                                             type->id,
                                             spawnPos,
                                             home,
                                             building->structureKind,
                                             building->id,
                                             building->villageId,
                                             speciesId,
                                             0.0f,
                                             0.0f);
    }

    if (!placed)
    {
        int minTileX = (int)floorf(building->bounds.x) - 1;
        int maxTileX = (int)ceilf(building->bounds.x + building->bounds.width) + 1;
        int minTileY = (int)floorf(building->bounds.y) - 1;
        int maxTileY = (int)ceilf(building->bounds.y + building->bounds.height) + 1;

        if (minTileX < 0)
            minTileX = 0;
        if (minTileY < 0)
            minTileY = 0;
        if (maxTileX >= map->width)
            maxTileX = map->width - 1;
        if (maxTileY >= map->height)
            maxTileY = map->height - 1;

        for (int ty = minTileY; ty <= maxTileY && !placed; ++ty)
        {
            for (int tx = minTileX; tx <= maxTileX && !placed; ++tx)
            {
                bool onPerimeter = (tx == minTileX || tx == maxTileX || ty == minTileY || ty == maxTileY);
                if (!onPerimeter)
                    continue;

                Vector2 perimeterPos = {((float)tx + 0.5f) * TILE_SIZE, ((float)ty + 0.5f) * TILE_SIZE};

                if (!entity_position_is_walkable(map, perimeterPos, type->radius))
                    continue;

                placed = entity_reservation_schedule(sys,
                                                     type->id,
                                                     perimeterPos,
                                                     home,
                                                     building->structureKind,
                                                     building->id,
                                                     building->villageId,
                                                     speciesId,
                                                     0.0f,
                                                     0.0f);
            }
        }
    }

    if (!placed)
    {
        placed = entity_reservation_schedule(sys,
                                             type->id,
                                             home,
                                             home,
                                             building->structureKind,
                                             building->id,
                                             building->villageId,
                                             speciesId,
                                             0.0f,
                                             0.0f);
    }

    return placed;
}

static void entity_schedule_structure_residents(EntitySystem* sys, const Map* map, bool refreshing)
{
    if (!sys || !map)
        return;

    int total = building_total_count();
    for (int b = 0; b < total; ++b)
    {
        Building* building = building_get_mutable(b);
        if (!building || !building->structureDef)
            continue;
        if (!refreshing)
            building->occupantActive = 0;

        if (building->occupantCurrent <= 0)
            continue;

        ResidentDemand demands[STRUCTURE_MAX_RESIDENT_ROLES];
        int            demandCount = entity_collect_resident_demands(building, demands, STRUCTURE_MAX_RESIDENT_ROLES);
        if (demandCount <= 0)
            continue;
        for (int d = 0; d < demandCount; ++d)
        {
            EntitiesTypeID typeId = demands[d].typeId;
            const EntityType* type = entity_find_type(sys, typeId);
            if (!type)
                continue;

            int have    = entity_count_residents_of_type(building, sys, typeId);
            int pending = entity_count_pending_reservations(sys, building->id, typeId);
            int needed  = demands[d].desired - (have + pending);
            if (needed <= 0)
                continue;

            const float recruitRadius = TILE_SIZE * 6.0f;

            while (needed > 0)
            {
                Entity* candidate = entity_find_homeless_near(sys, building, typeId, recruitRadius);
                if (!candidate)
                    break;
                building_add_resident(building, candidate);
                building_on_reservation_spawn(building->id);
                needed--;
            }

            while (needed > 0)
            {
                if (!entity_schedule_resident(sys, map, building, type))
                    break;
                needed--;
            }
        }
    }
}

static void entity_stream_reservations(EntitySystem* sys, const Map* map, const Camera2D* camera)
{
    if (!sys)
        return;

    float viewWidth  = (float)GetScreenWidth();
    float viewHeight = (float)GetScreenHeight();
    float zoom       = (camera && camera->zoom > 0.0f) ? camera->zoom : 1.0f;
    viewWidth /= zoom;
    viewHeight /= zoom;

    Vector2 focus = camera ? camera->target : (Vector2){viewWidth * 0.5f, viewHeight * 0.5f};

    float halfW             = viewWidth * 0.5f;
    float halfH             = viewHeight * 0.5f;
    float baseRadius        = sqrtf(halfW * halfW + halfH * halfH);
    float defaultActivation   = baseRadius + sys->streamActivationPadding;
    float defaultDeactivation = baseRadius + sys->streamDeactivationPadding;

    for (int i = 0; i < sys->reservationCount; ++i)
    {
        EntityReservation* res = &sys->reservations[i];
        if (!res->used)
            continue;

        float activationRadius   = (res->activationRadius > 0.0f) ? res->activationRadius : defaultActivation;
        float deactivationRadius = (res->deactivationRadius > 0.0f) ? res->deactivationRadius : defaultDeactivation;
        float activationSq       = activationRadius * activationRadius;
        float deactivationSq     = deactivationRadius * deactivationRadius;

        float distSq = entity_distance_sq(res->position, focus);

        if (!res->active && distSq <= activationSq)
        {
            const EntityType* type = entity_find_type(sys, res->typeId);
            if (!type)
                continue;

            if (!entity_position_is_walkable(map, res->position, type->radius))
                continue;

            uint16_t id = entity_spawn(sys, res->typeId, res->position);
            if (id == ENTITY_ID_INVALID)
                continue;

            Entity* ent = entity_acquire(sys, id);
            if (!ent)
            {
                entity_despawn(sys, id);
                continue;
            }

            res->entityId          = id;
            res->active            = true;
            ent->reservationIndex  = i;
            if (res->hp <= 0 && type->maxHP > 0)
                res->hp = type->maxHP;
            entity_reservation_apply(res, ent);
            ent->hp = (res->hp > 0) ? res->hp : ent->hp;
            if (res->buildingId >= 0)
            {
                Building* home = building_get_mutable(res->buildingId);
                if (home)
                    building_add_resident(home, ent);
                building_on_reservation_spawn(res->buildingId);
            }
        }
        else if (res->active && distSq >= deactivationSq)
        {
            Entity* ent = entity_acquire(sys, res->entityId);
            if (ent)
            {
                entity_reservation_capture(res, ent);
                ent->reservationIndex = -1;
            }

            if (res->buildingId >= 0)
                building_on_reservation_hibernate(res->buildingId);

            entity_despawn(sys, res->entityId);
            res->entityId = ENTITY_ID_INVALID;
            res->active   = false;
        }
    }
}

// -----------------------------------------------------------------------------
// Core system operations
// -----------------------------------------------------------------------------

bool entity_system_init(EntitySystem* sys, const Map* map, unsigned int seed, const char* definitionsPath)
{
    if (!sys)
        return false;

    entity_system_reset(sys);
    sys->rngState = seed ? seed : 0xCAFEBABEu;

    bool loaded = false;
    if (definitionsPath)
        loaded = entities_loader_load(sys, definitionsPath);

    if (!loaded)
    {
        printf("⚠️  Falling back to built-in entity definitions.\n");
        entity_register_fallbacks(sys);
    }

    entity_assign_builtin_behaviours(sys);

    if (map)
    {
        for (int i = 0; i < sys->spawnRuleCount; ++i)
        {
            if (!sys->spawnRules[i].type && sys->spawnRules[i].id > ENTITY_TYPE_INVALID)
                sys->spawnRules[i].type = entity_find_type(sys, sys->spawnRules[i].id);
        }

        for (int r = 0; r < sys->spawnRuleCount; ++r)
        {
            const EntitySpawnRule* rule = &sys->spawnRules[r];
            if (!rule->type)
                continue;

            if (rule->type->referredStructure != STRUCT_COUNT)
            {
                entity_schedule_near_structures(sys, rule, map);
                continue;
            }

            for (int y = 0; y < map->height; ++y)
            {
                for (int x = 0; x < map->width; ++x)
                {
                    TileTypeID tid = map->tiles[y][x];
                    if (rule->tile != TILE_MAX && tid != rule->tile)
                        continue;

                    if (rule->biome != BIO_MAX)
                    {
                        BiomeKind b = infer_biome_from_tile(tid);
                        if (b != rule->biome)
                            continue;
                    }

                    float roll = entity_randomf(sys, 0.0f, 1.0f);
                    if (roll > rule->density)
                        continue;

                    int group = entity_randomi(sys, rule->groupMin, rule->groupMax);
                    if (group <= 0)
                        group = 1;

                    for (int g = 0; g < group; ++g)
                    {
                        Vector2 spawnPos = {
                            (x + 0.5f) * TILE_SIZE + entity_randomf(sys, -TILE_SIZE * 0.3f, TILE_SIZE * 0.3f),
                            (y + 0.5f) * TILE_SIZE + entity_randomf(sys, -TILE_SIZE * 0.3f, TILE_SIZE * 0.3f),
                        };
                        if (!entity_position_is_walkable(map, spawnPos, rule->type->radius))
                            continue;
                        if (!entity_reservation_schedule(sys,
                                                         rule->type->id,
                                                         spawnPos,
                                                         spawnPos,
                                                         STRUCT_COUNT,
                                                         -1,
                                                         -1,
                                                         rule->type->speciesId,
                                                         0.0f,
                                                         0.0f))
                            break;
                    }
                }
            }
        }

        entity_schedule_structure_residents(sys, map, false);
    }

    return loaded;
}

void entity_system_shutdown(EntitySystem* sys)
{
    if (!sys)
        return;

    for (int i = 0; i <= sys->highestIndex; ++i)
    {
        Entity* e = &sys->entities[i];
        if (!e->active)
            continue;
        if (e->behavior && e->behavior->onDespawn)
            e->behavior->onDespawn(sys, e);
    }

    for (int t = 0; t < sys->typeCount; ++t)
        entity_unload_sprite(&sys->types[t].sprite);

    entity_system_reset(sys);
}

static void entity_update_animation(Entity* e, float dt)
{
    if (!e || !e->type)
        return;

    const EntitySprite* sprite = &e->type->sprite;
    if (sprite->frameCount <= 1 || sprite->frameDuration <= 0.0f)
        return;

    e->animTime += dt;
    while (e->animTime >= sprite->frameDuration)
    {
        e->animTime -= sprite->frameDuration;
        e->animFrame = (e->animFrame + 1) % sprite->frameCount;
    }
}

static void entity_update_behavior_timers(Entity* e, float dt)
{
    if (!e)
        return;

    if (e->reproductionCooldown > 0.0f)
    {
        e->reproductionCooldown -= dt;
        if (e->reproductionCooldown < 0.0f)
            e->reproductionCooldown = 0.0f;
    }

    if (e->behaviorTimer > 0.0f)
    {
        e->behaviorTimer -= dt;
        if (e->behaviorTimer < 0.0f)
            e->behaviorTimer = 0.0f;
    }

    if (e->affectionTimer > 0.0f)
    {
        e->affectionTimer -= dt;
        if (e->affectionTimer < 0.0f)
            e->affectionTimer = 0.0f;

        const float twoPi = 6.28318530718f;
        e->affectionPhase += dt * 4.0f;
        if (e->affectionPhase > twoPi)
            e->affectionPhase = fmodf(e->affectionPhase, twoPi);

        if (e->system && e->reproductionPartnerId != ENTITY_ID_INVALID)
        {
            Entity* partner = entity_acquire(e->system, e->reproductionPartnerId);
            if (partner && partner->active)
            {
                float angle = atan2f(partner->position.y - e->position.y, partner->position.x - e->position.x);
                e->orientation = angle;
            }
            else
            {
                e->reproductionPartnerId = ENTITY_ID_INVALID;
            }
        }

        if (e->affectionTimer <= 0.0f)
        {
            e->reproductionPartnerId = ENTITY_ID_INVALID;
            e->affectionPhase        = 0.0f;
        }
    }
}

static void entity_draw_affection(const Entity* e)
{
    if (!e || !e->type)
        return;

    if (e->affectionTimer <= 0.0f)
        return;

    float radius = (e->type->radius > 0.0f) ? e->type->radius : 12.0f;
    float bob    = sinf(e->affectionPhase) * 3.5f;
    float baseY  = e->position.y - radius - 14.0f + bob;
    float centerX = e->position.x;

    unsigned char alpha = (unsigned char)fminf(255.0f, 170.0f + fabsf(sinf(e->affectionPhase * 0.5f)) * 70.0f);
    Color heartColor    = (Color){220, 50, 90, alpha};

    Vector2 left    = {centerX - 5.5f, baseY};
    Vector2 right   = {centerX + 5.5f, baseY};
    Vector2 bottomA = {centerX - 9.0f, baseY + 7.0f};
    Vector2 bottomB = {centerX, baseY + 15.0f};
    Vector2 bottomC = {centerX + 9.0f, baseY + 7.0f};

    DrawCircleV(left, 3.8f, heartColor);
    DrawCircleV(right, 3.8f, heartColor);
    DrawTriangle(bottomA, bottomB, bottomC, heartColor);
}

void entity_system_update(EntitySystem* sys, const Map* map, const Camera2D* camera, float dt)
{
    if (!sys)
        return;

    entity_stream_reservations(sys, map, camera);
    entity_rebuild_building_occupancy(sys);

    sys->residentRefreshTimer += dt;
    if (sys->residentRefreshTimer >= 5.0f)
    {
        entity_schedule_structure_residents(sys, map, true);
        sys->residentRefreshTimer = 0.0f;
    }

    float dtDays = entity_sim_days_step();

    for (int i = 0; i <= sys->highestIndex; ++i)
    {
        Entity* e = &sys->entities[i];
        if (!e->active)
            continue;

        behavior_hunger_update(sys, e, (Map*)map);
        if (!e->active)
            continue;

        behavior_eat_if_hungry(e);
        if (!e->active)
            continue;

        if (dtDays > 0.0f)
        {
            age_update(e, dtDays);
            if (!e->active)
                continue;
        }

        if (e->behavior && e->behavior->onUpdate)
            e->behavior->onUpdate(sys, e, map, dt);

        entity_update_behavior_timers(e, dt);
        entity_update_animation(e, dt);

        if (e->reservationIndex >= 0 && e->reservationIndex < sys->reservationCount)
        {
            EntityReservation* res = &sys->reservations[e->reservationIndex];
            if (res->used && res->active && res->entityId == e->id)
                entity_reservation_capture(res, e);
        }
    }
}

void entity_system_draw(const EntitySystem* sys)
{
    if (!sys)
        return;

    for (int i = 0; i <= sys->highestIndex; ++i)
    {
        const Entity* e = &sys->entities[i];
        if (!e->active || !e->type)
            continue;

        const EntityType*   type   = e->type;
        const EntitySprite* sprite = &type->sprite;

        if (sprite->texture.id != 0 && sprite->frameWidth > 0 && sprite->frameHeight > 0)
        {
            int       frameWidth  = sprite->frameWidth;
            int       frameHeight = sprite->frameHeight;
            Rectangle src         = {(float)(frameWidth * e->animFrame), 0.0f, (float)frameWidth, (float)frameHeight};
            Rectangle dst         = {e->position.x, e->position.y, (float)frameWidth, (float)frameHeight};
            Vector2   origin      = sprite->origin;
            if (origin.x == 0.0f && origin.y == 0.0f)
                origin = (Vector2){frameWidth * 0.5f, frameHeight * 0.5f};

            DrawTexturePro(sprite->texture, src, dst, origin, e->orientation * RAD2DEG, WHITE);
        }
        else
        {
            DrawCircleV(e->position, (type->radius > 0.0f) ? type->radius : 10.0f, type->tint);
            Vector2 facing = {
                e->position.x + cosf(e->orientation) * (type->radius > 0.0f ? type->radius : 10.0f),
                e->position.y + sinf(e->orientation) * (type->radius > 0.0f ? type->radius : 10.0f),
            };
            DrawLineV(e->position, facing, DARKGREEN);
        }

        entity_draw_affection(e);
    }
}

uint16_t entity_spawn(EntitySystem* sys, EntitiesTypeID typeId, Vector2 position)
{
    if (!sys || typeId <= ENTITY_TYPE_INVALID)
        return ENTITY_ID_INVALID;

    const EntityType* type = entity_find_type(sys, typeId);
    if (!type)
        return ENTITY_ID_INVALID;

    for (int i = 0; i < MAX_ENTITIES; ++i)
    {
        Entity* e = &sys->entities[i];
        if (e->active)
            continue;

        entity_clear_slot(sys, i);
        e->active        = true;
        e->position      = position;
        e->type          = type;
        e->behavior      = type->behavior;
        e->hp            = (type->maxHP > 0) ? type->maxHP : 10;
        e->orientation   = 0.0f;
        e->velocity      = (Vector2){0};
        e->animFrame     = 0;
        e->animTime      = 0.0f;
        e->home          = position;
        e->homeStructure = type->referredStructure;
        memset(e->brain, 0, sizeof(e->brain));
        e->system                = sys;
        int speciesId            = 0;
        if (type->species[0] != '\0')
            speciesId = entity_system_register_species(sys, type->species);
        else if (type->identifier[0] != '\0')
            speciesId = entity_system_register_species(sys, type->identifier);
        if (type->speciesId > 0)
            speciesId = type->speciesId;
        e->speciesId             = speciesId;
        e->sex                   = (type->sex != ENTITY_SEX_UNDEFINED) ? type->sex : ENTITY_SEX_UNDEFINED;
        e->maxHunger             = 100.0f;
        e->hunger                = e->maxHunger;
        e->isUndead              = (type->flags & ENTITY_FLAG_UNDEAD) != 0;
        e->isHungry              = false;
        e->enraged               = false;
        e->reproductionCooldown  = 0.0f;
        e->affectionTimer        = 0.0f;
        e->affectionPhase        = 0.0f;
        e->reproductionPartnerId = ENTITY_ID_INVALID;
        e->behaviorTargetId      = ENTITY_ID_INVALID;
        e->behaviorTimer         = 0.0f;
        e->gatherTarget          = (Vector2){0.0f, 0.0f};
        e->gatherActive          = 0;
        e->homeBuildingId        = -1;
        e->villageId             = -1;
        e->ageDays               = 0.0f;
        e->isElder               = false;

        if (e->behavior && e->behavior->brainSize > ENTITY_BRAIN_BYTES)
        {
            printf("⚠️  Behaviour '%s' requires %zu bytes, but only %d are available\n", type->identifier, e->behavior->brainSize, ENTITY_BRAIN_BYTES);
        }

        if (e->behavior && e->behavior->onSpawn)
            e->behavior->onSpawn(sys, e);

        if (i > sys->highestIndex)
            sys->highestIndex = i;
        sys->activeCount++;
        return e->id;
    }

    printf("⚠️  Entity pool exhausted, cannot spawn entity %d\n", typeId);
    return ENTITY_ID_INVALID;
}

void entity_despawn(EntitySystem* sys, uint16_t id)
{
    if (!sys || id >= MAX_ENTITIES)
        return;

    Entity* e = &sys->entities[id];
    if (!e->active)
        return;

    if (e->behavior && e->behavior->onDespawn)
        e->behavior->onDespawn(sys, e);

    if (e->homeBuildingId >= 0)
    {
        Building* home = building_get_mutable(e->homeBuildingId);
        if (home)
            building_remove_resident(home, e->id);
        e->homeBuildingId = -1;
        e->villageId      = -1;
    }

    e->active = false;
    e->reservationIndex = -1;
    sys->activeCount--;
    if (sys->activeCount < 0)
        sys->activeCount = 0;

    if ((int)id == sys->highestIndex)
    {
        while (sys->highestIndex >= 0 && !sys->entities[sys->highestIndex].active)
            --sys->highestIndex;
    }
}

Entity* entity_acquire(EntitySystem* sys, uint16_t id)
{
    if (!sys || id >= MAX_ENTITIES)
        return NULL;
    Entity* e = &sys->entities[id];
    return e->active ? e : NULL;
}

const Entity* entity_get(const EntitySystem* sys, uint16_t id)
{
    if (!sys || id >= MAX_ENTITIES)
        return NULL;
    const Entity* e = &sys->entities[id];
    return e->active ? e : NULL;
}

const EntityType* entity_find_type(const EntitySystem* sys, EntitiesTypeID typeId)
{
    if (!sys || typeId <= ENTITY_TYPE_INVALID)
        return NULL;

    for (int i = 0; i < sys->typeCount; ++i)
    {
        if (sys->types[i].id == typeId)
            return &sys->types[i];
    }
    return NULL;
}

int entity_system_type_count(const EntitySystem* sys)
{
    return sys ? sys->typeCount : 0;
}

const EntityType* entity_system_type_at(const EntitySystem* sys, int index)
{
    if (!sys || index < 0 || index >= sys->typeCount)
        return NULL;
    return &sys->types[index];
}

bool entity_type_has_trait(const EntityType* type, const char* trait)
{
    if (!type || !trait)
        return false;

    char needle[ENTITY_TRAIT_NAME_MAX];
    normalize_label(trait, needle, sizeof(needle));
    if (needle[0] == '\0')
        return false;

    for (int i = 0; i < type->traitCount; ++i)
    {
        if (strcmp(type->traits[i], needle) == 0)
            return true;
    }
    return false;
}

bool entity_type_has_competence(const EntityType* type, uint32_t competenceMask)
{
    if (!type || competenceMask == 0)
        return false;
    return (type->competences & competenceMask) == competenceMask;
}

bool entity_type_is_category(const EntityType* type, const char* category)
{
    if (!type || !category)
        return false;

    char normalized[ENTITY_CATEGORY_NAME_MAX];
    normalize_label(category, normalized, sizeof(normalized));
    if (normalized[0] == '\0')
        return false;
    return strcmp(type->category, normalized) == 0;
}

bool entity_has_trait(const Entity* entity, const char* trait)
{
    return entity && entity->type ? entity_type_has_trait(entity->type, trait) : false;
}

bool entity_is_category(const Entity* entity, const char* category)
{
    return entity && entity->type ? entity_type_is_category(entity->type, category) : false;
}

static bool entity_type_is_elder_variant(const EntityType* type)
{
    if (!type)
        return false;
    if (entity_type_has_trait(type, "elder"))
        return true;
    if (type->identifier[0] != '\0' && strstr(type->identifier, "elder"))
        return true;
    if (type->displayName[0] != '\0' && strstr(type->displayName, "Elder"))
        return true;
    return false;
}

static const EntityType* entity_find_elder_variant(const Entity* entity)
{
    if (!entity || !entity->system || !entity->type)
        return NULL;

    const EntitySystem* sys       = entity->system;
    const EntityType*   baseType  = entity->type;
    int                  speciesId = baseType->speciesId;
    const EntityType*   fallback  = NULL;

    for (int i = 0; i < sys->typeCount; ++i)
    {
        const EntityType* candidate = &sys->types[i];
        if (candidate == baseType)
            continue;
        if (speciesId > 0 && candidate->speciesId != speciesId)
            continue;
        if (!entity_type_is_elder_variant(candidate))
            continue;
        if (!fallback)
            fallback = candidate;
        if (baseType->speciesId > 0 && candidate->speciesId == baseType->speciesId)
            return candidate;
    }

    return fallback;
}

void entity_promote_to_elder(Entity* entity)
{
    if (!entity || entity->isElder)
        return;

    const EntityType* elderType = entity_find_elder_variant(entity);
    if (!elderType)
        return;

    entity->type      = elderType;
    entity->behavior  = elderType->behavior;
    entity->hp        = (elderType->maxHP > 0) ? elderType->maxHP : entity->hp;
    entity->sex       = (elderType->sex != ENTITY_SEX_UNDEFINED) ? elderType->sex : entity->sex;
    entity->speciesId = (elderType->speciesId > 0) ? elderType->speciesId : entity->speciesId;
    entity->isElder   = true;

    if (entity->behavior && entity->behavior->onSpawn)
        entity->behavior->onSpawn(entity->system, entity);
}

void age_update(Entity* entity, float dtDays)
{
    if (!entity || !entity->active || dtDays <= 0.0f)
        return;

    entity->ageDays += dtDays;
    const EntityType* type = entity->type;
    if (!type)
        return;

    if (!entity->isElder && type->ageElderAfterDays > 0.0f && entity->ageDays >= type->ageElderAfterDays)
        entity_promote_to_elder(entity);

    type = entity->type; /* May have changed after promotion. */
    if (type && type->ageDieAfterDays > 0.0f && entity->ageDays >= type->ageDieAfterDays)
    {
        if (entity->system)
            entity_despawn(entity->system, entity->id);
    }
}
