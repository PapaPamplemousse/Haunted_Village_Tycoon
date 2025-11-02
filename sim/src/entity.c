/**
 * @file entity.c
 * @brief Implements the runtime entity pool and basic AI behaviours.
 */

#include "entity.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "world.h"
#include "building.h"
#include "object.h"
#include "entities_loader.h"
#include "zombie.h"
#include "cannibal.h"
#include "tile.h"

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
}

static bool entity_reservation_schedule(EntitySystem* sys,
                                        EntitiesTypeID typeId,
                                        Vector2 position,
                                        Vector2 home,
                                        StructureKind structure,
                                        int buildingId,
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
    entity_reservations_reset(sys);
}

static void entity_clear_slot(EntitySystem* sys, int index)
{
    Entity* e = &sys->entities[index];
    memset(e, 0, sizeof(*e));
    e->id = (uint16_t)index;
    e->reservationIndex = -1;
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
                                                     -1,
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
                                                -1,
                                                0.0f,
                                                0.0f))
                    spawnedAny = true;
            }
        }
    }

    return spawnedAny;
}

static void entity_schedule_structure_residents(EntitySystem* sys, const Map* map)
{
    if (!sys || !map)
        return;

    int total = building_total_count();
    for (int b = 0; b < total; ++b)
    {
        Building* building = building_get_mutable(b);
        if (!building || !building->structureDef)
            continue;
        building->occupantActive = 0;

        if (building->occupantType <= ENTITY_TYPE_INVALID || building->occupantCurrent <= 0)
            continue;

        const EntityType* type = entity_find_type(sys, building->occupantType);
        if (!type)
            continue;

        Vector2 home = {building->center.x * TILE_SIZE, building->center.y * TILE_SIZE};

        for (int i = 0; i < building->occupantCurrent; ++i)
        {
            bool   placed   = false;
            Vector2 spawnPos = home;

            for (int attempt = 0; attempt < 8 && !placed; ++attempt)
            {
                unsigned int seed = (unsigned int)(building->id * 73856093u) ^ (unsigned int)(i * 19349663u + attempt * 83492791u);
                float angle = ((float)(seed & 0xFFFFu) / 65535.0f) * 2.0f * PI;
                seed         = seed * 1664525u + 1013904223u;
                float dist   = 0.35f + ((float)((seed >> 16) & 0xFFFFu) / 65535.0f) * 1.8f;
                spawnPos     = (Vector2){
                    home.x + cosf(angle) * dist * TILE_SIZE,
                    home.y + sinf(angle) * dist * TILE_SIZE,
                };

                if (!entity_position_is_walkable(map, spawnPos, type->radius))
                    continue;

                placed = entity_reservation_schedule(sys,
                                                     type->id,
                                                     spawnPos,
                                                     home,
                                                     building->structureKind,
                                                     building->id,
                                                     0.0f,
                                                     0.0f);
            }

            if (!placed && entity_position_is_walkable(map, home, type->radius))
            {
                entity_reservation_schedule(sys,
                                            type->id,
                                            home,
                                            home,
                                            building->structureKind,
                                            building->id,
                                            0.0f,
                                            0.0f);
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
    float defaultActivation = baseRadius + sys->streamActivationPadding;
    float defaultDeactivation = baseRadius + sys->streamDeactivationPadding;
    float defaultActivationSq  = defaultActivation * defaultActivation;
    float defaultDeactivationSq = defaultDeactivation * defaultDeactivation;

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
                building_on_reservation_spawn(res->buildingId);
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
                                                         0.0f,
                                                         0.0f))
                            break;
                    }
                }
            }
        }

        entity_schedule_structure_residents(sys, map);
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

void entity_system_update(EntitySystem* sys, const Map* map, const Camera2D* camera, float dt)
{
    if (!sys)
        return;

    entity_stream_reservations(sys, map, camera);

    for (int i = 0; i <= sys->highestIndex; ++i)
    {
        Entity* e = &sys->entities[i];
        if (!e->active)
            continue;

        if (e->behavior && e->behavior->onUpdate)
            e->behavior->onUpdate(sys, e, map, dt);

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
