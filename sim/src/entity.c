/**
 * @file entity.c
 * @brief Implements the runtime entity pool and basic AI behaviours.
 */

#include "entity.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "world.h"

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

static void entity_system_reset(EntitySystem* sys)
{
    memset(sys, 0, sizeof(*sys));
    sys->highestIndex = -1;
}

static void entity_clear_slot(EntitySystem* sys, int index)
{
    Entity* e = &sys->entities[index];
    memset(e, 0, sizeof(*e));
    e->id = (uint16_t)index;
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
        switch (type->id)
        {
            case ENTITY_TYPE_CURSED_ZOMBIE:
                type->behavior = entity_zombie_behavior();
                break;
            case ENTITY_TYPE_CANNIBAL:
                type->behavior = entity_cannibal_behavior();
                break;
            default:
                break;
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

    entity_spawn_rule_init(&rule);
    rule.id       = cannibal.id;
    rule.density  = 0.02f;
    rule.groupMin = 1;
    rule.groupMax = 3;
    rule.tile     = TILE_SAVANNA;
    rule.biome    = BIO_SAVANNA;

    entity_system_register_type(sys, &cannibal, &rule);
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

        // Seed the world with initial spawns
        for (int r = 0; r < sys->spawnRuleCount; ++r)
        {
            const EntitySpawnRule* rule = &sys->spawnRules[r];
            if (!rule->type)
                continue;

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
                        if (entity_spawn(sys, rule->type->id, spawnPos) == ENTITY_ID_INVALID)
                            break;
                    }
                }
            }
        }
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

void entity_system_update(EntitySystem* sys, const Map* map, float dt)
{
    if (!sys)
        return;

    for (int i = 0; i <= sys->highestIndex; ++i)
    {
        Entity* e = &sys->entities[i];
        if (!e->active)
            continue;

        if (e->behavior && e->behavior->onUpdate)
            e->behavior->onUpdate(sys, e, map, dt);

        entity_update_animation(e, dt);
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
        e->active      = true;
        e->position    = position;
        e->type        = type;
        e->behavior    = type->behavior;
        e->hp          = (type->maxHP > 0) ? type->maxHP : 10;
        e->orientation = 0.0f;
        e->velocity    = (Vector2){0};
        e->animFrame   = 0;
        e->animTime    = 0.0f;
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
