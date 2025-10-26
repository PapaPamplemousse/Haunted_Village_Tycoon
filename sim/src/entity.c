#include "entity.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "world.h"
#include "biome_loader.h"

#include "tile.h"

#ifndef PI
#define PI 3.14159265358979323846f
#endif
#define RAD2DEG (180.0f / PI)

// -----------------------------------------------------------------------------
// Local helpers & utilities
// -----------------------------------------------------------------------------

static unsigned int entity_rand(EntitySystem* sys)
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

static float entity_randf(EntitySystem* sys, float min, float max)
{
    if (max <= min)
        return min;
    float t = (float)(entity_rand(sys) & 0xFFFFFF) / (float)0xFFFFFF;
    return min + t * (max - min);
}

static int entity_randi(EntitySystem* sys, int min, int max)
{
    if (max < min)
        return min;
    unsigned int span = (unsigned int)(max - min + 1);
    return min + (int)(entity_rand(sys) % (span ? span : 1));
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

static inline TileTypeID clamp_tile_index(const Map* map, int x, int y, bool* inside)
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

typedef struct ZombieBrain
{
    float wanderTimer;
} ZombieBrain;

static void zombie_pick_direction(EntitySystem* sys, Entity* e, ZombieBrain* brain)
{
    if (!sys || !e || !e->type || !brain)
        return;

    float angle = entity_randf(sys, 0.0f, 2.0f * PI);
    float speed = e->type->maxSpeed * entity_randf(sys, 0.45f, 1.0f);

    e->velocity.x      = cosf(angle) * speed;
    e->velocity.y      = sinf(angle) * speed;
    e->orientation     = angle;
    brain->wanderTimer = entity_randf(sys, 1.2f, 3.6f);
}

static void zombie_on_spawn(EntitySystem* sys, Entity* e)
{
    (void)sys;
    if (!e)
        return;

    memset(e->brain, 0, ENTITY_BRAIN_BYTES);
    e->hp        = e->type ? e->type->maxHP : 10;
    e->animFrame = 0;
    e->animTime  = 0.0f;
}

static void zombie_on_update(EntitySystem* sys, Entity* e, const Map* map, float dt)
{
    if (!sys || !e || !map || !e->type)
        return;

    ZombieBrain* brain = (ZombieBrain*)e->brain;
    if (sizeof(ZombieBrain) > ENTITY_BRAIN_BYTES)
        return;

    if (brain->wanderTimer <= 0.0f || (fabsf(e->velocity.x) < 0.1f && fabsf(e->velocity.y) < 0.1f))
        zombie_pick_direction(sys, e, brain);
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

static const EntityBehavior G_ZOMBIE_BEHAVIOR = {
    .onSpawn   = zombie_on_spawn,
    .onUpdate  = zombie_on_update,
    .onDespawn = NULL,
    .brainSize = sizeof(ZombieBrain),
};

static void entity_assign_builtin_behaviours(EntitySystem* sys)
{
    if (!sys)
        return;

    for (int i = 0; i < sys->typeCount; ++i)
    {
        EntityType* type = &sys->types[i];
        if (strcmp(type->id, "cursed_zombie") == 0)
            type->behavior = &G_ZOMBIE_BEHAVIOR;
    }
}

// -----------------------------------------------------------------------------
// Loader utilities (.stv parsing)
// -----------------------------------------------------------------------------

static void trim_inplace(char* s)
{
    if (!s)
        return;
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1]))
        s[--len] = '\0';
    char* start = s;
    while (*start && isspace((unsigned char)*start))
        ++start;
    if (start != s)
        memmove(s, start, strlen(start) + 1);
}

static void strip_inline_comment(char* line)
{
    if (!line)
        return;
    for (char* p = line; *p; ++p)
    {
        if (*p == '#' || *p == ';')
        {
            *p = '\0';
            break;
        }
    }
}

static Color parse_color(const char* value, bool* ok)
{
    Color result = {255, 255, 255, 255};
    if (ok)
        *ok = false;
    if (!value)
        return result;

    int r = 255, g = 255, b = 255, a = 255;
    if (sscanf(value, " %d , %d , %d , %d", &r, &g, &b, &a) >= 3)
    {
        if (ok)
            *ok = true;
        result = (Color){(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a};
    }
    return result;
}

static Vector2 parse_vector2(const char* value, bool* ok)
{
    Vector2 out = {0};
    if (ok)
        *ok = false;
    if (!value)
        return out;

    float x = 0.0f, y = 0.0f;
    if (sscanf(value, " %f , %f", &x, &y) == 2)
    {
        out = (Vector2){x, y};
        if (ok)
            *ok = true;
    }
    return out;
}

static EntityFlags parse_flags(const char* value)
{
    if (!value)
        return 0;

    EntityFlags flags = 0;
    char        buf[128];
    snprintf(buf, sizeof(buf), "%s", value);

    char* token = strtok(buf, "|, ");
    while (token)
    {
        if (strcasecmp(token, "hostile") == 0)
            flags |= ENTITY_FLAG_HOSTILE;
        else if (strcasecmp(token, "mobile") == 0)
            flags |= ENTITY_FLAG_MOBILE;
        else if (strcasecmp(token, "intelligent") == 0 || strcasecmp(token, "smart") == 0)
            flags |= ENTITY_FLAG_INTELLIGENT;
        else if (strcasecmp(token, "undead") == 0)
            flags |= ENTITY_FLAG_UNDEAD;
        else if (strcasecmp(token, "merchant") == 0)
            flags |= ENTITY_FLAG_MERCHANT;
        else if (strcasecmp(token, "animal") == 0)
            flags |= ENTITY_FLAG_ANIMAL;
        token = strtok(NULL, "|, ");
    }
    return flags;
}

static TileTypeID tile_from_string(const char* value)
{
    if (!value || !*value)
        return TILE_MAX;

    char token[64];
    snprintf(token, sizeof(token), "%s", value);
    trim_inplace(token);

    if (strncasecmp(token, "TILE_", 5) == 0)
        memmove(token, token + 5, strlen(token + 5) + 1);

    for (int i = 0; i < TILE_MAX; ++i)
    {
        const TileType* tt = get_tile_type((TileTypeID)i);
        if (tt && tt->name && strcasecmp(tt->name, token) == 0)
            return (TileTypeID)i;
    }

    return TILE_MAX;
}

static void apply_spawn_rule_defaults(EntitySpawnRule* rule)
{
    memset(rule, 0, sizeof(*rule));
    rule->type      = NULL;
    rule->typeId[0] = '\0';
    rule->biome     = BIO_MAX;
    rule->tile      = TILE_MAX;
    rule->density   = 0.0f;
    rule->groupMin  = 1;
    rule->groupMax  = 1;
}

static bool entity_register_type(EntitySystem* sys, const EntityType* def, const EntitySpawnRule* spawn)
{
    if (!sys || !def || def->id[0] == '\0')
        return false;

    if (sys->typeCount >= ENTITY_MAX_TYPES)
    {
        printf("⚠️  Entity registry full, ignoring '%s'\n", def->id);
        return false;
    }

    EntityType* dst = &sys->types[sys->typeCount++];
    *dst            = *def;

    if (dst->tint.a == 0)
        dst->tint = (Color){200, 200, 200, 255};
    if (dst->radius <= 0.0f)
        dst->radius = 12.0f;
    if (dst->maxHP <= 0)
        dst->maxHP = 10;
    if (dst->maxSpeed <= 0.0f)
        dst->maxSpeed = 24.0f;

    dst->sprite.texture.id = 0; // ensure texture gets loaded once
    entity_load_sprite(&dst->sprite);

    if (spawn && spawn->density > 0.0f && sys->spawnRuleCount < ENTITY_MAX_SPAWN_RULES)
    {
        EntitySpawnRule* sr = &sys->spawnRules[sys->spawnRuleCount++];
        *sr                 = *spawn;
        sr->type            = dst;
        if (sr->groupMin <= 0)
            sr->groupMin = 1;
        if (sr->groupMax < sr->groupMin)
            sr->groupMax = sr->groupMin;
    }

    return true;
}

static bool parse_group_range(const char* value, int* outMin, int* outMax)
{
    if (!value)
        return false;
    int min = 0, max = 0;
    if (sscanf(value, " %d - %d", &min, &max) == 2)
    {
        if (min <= 0)
            min = 1;
        if (max < min)
            max = min;
        *outMin = min;
        *outMax = max;
        return true;
    }
    if (sscanf(value, " %d", &min) == 1)
    {
        if (min <= 0)
            min = 1;
        *outMin = min;
        *outMax = min;
        return true;
    }
    return false;
}

static bool entity_registry_load(EntitySystem* sys, const char* path)
{
    if (!sys || !path)
        return false;

    FILE* f = fopen(path, "r");
    if (!f)
    {
        printf("⚠️  Could not open entity definitions '%s'\n", path);
        return false;
    }

    EntityType      currentType;
    EntitySpawnRule currentSpawn;
    memset(&currentType, 0, sizeof(currentType));
    apply_spawn_rule_defaults(&currentSpawn);

    bool inSection                         = false;
    char sectionName[ENTITY_TYPE_NAME_MAX] = {0};
    char line[256];
    bool anyLoaded = false;

    while (fgets(line, sizeof(line), f))
    {
        strip_inline_comment(line);
        trim_inplace(line);

        if (line[0] == '\0')
            continue;

        if (line[0] == '[')
        {
            if (inSection && sectionName[0])
            {
                if (!currentType.id[0])
                    snprintf(currentType.id, sizeof(currentType.id), "%s", sectionName);
                if (!currentType.displayName[0])
                    snprintf(currentType.displayName, sizeof(currentType.displayName), "%s", currentType.id);
                entity_register_type(sys, &currentType, &currentSpawn);
                anyLoaded = true;
            }

            memset(&currentType, 0, sizeof(currentType));
            apply_spawn_rule_defaults(&currentSpawn);

            if (sscanf(line, "[%31[^]]", sectionName) == 1)
            {
                inSection = true;
                snprintf(currentType.id, sizeof(currentType.id), "%s", sectionName);
            }
            else
            {
                sectionName[0] = '\0';
                inSection      = false;
            }
            continue;
        }

        if (!inSection)
            continue;

        char key[64];
        char value[160];
        if (sscanf(line, "%63[^=]=%159[^\n]", key, value) != 2)
            continue;

        trim_inplace(key);
        trim_inplace(value);

        if (strcasecmp(key, "id") == 0)
        {
            snprintf(currentType.id, sizeof(currentType.id), "%s", value);
        }
        else if (strcasecmp(key, "display_name") == 0)
        {
            snprintf(currentType.displayName, sizeof(currentType.displayName), "%s", value);
        }
        else if (strcasecmp(key, "max_hp") == 0)
        {
            currentType.maxHP = atoi(value);
            if (currentType.maxHP <= 0)
                currentType.maxHP = 10;
        }
        else if (strcasecmp(key, "max_speed") == 0)
        {
            currentType.maxSpeed = (float)atof(value);
        }
        else if (strcasecmp(key, "radius") == 0)
        {
            currentType.radius = (float)atof(value);
            if (currentType.radius <= 0.0f)
                currentType.radius = 12.0f;
        }
        else if (strcasecmp(key, "color") == 0)
        {
            bool  ok = false;
            Color c  = parse_color(value, &ok);
            if (ok)
                currentType.tint = c;
        }
        else if (strcasecmp(key, "texture") == 0)
        {
            snprintf(currentType.sprite.texturePath, sizeof(currentType.sprite.texturePath), "%s", value);
        }
        else if (strcasecmp(key, "sprite.origin") == 0)
        {
            bool ok                   = false;
            currentType.sprite.origin = parse_vector2(value, &ok);
        }
        else if (strcasecmp(key, "sprite.size") == 0)
        {
            int w = 0, h = 0;
            if (sscanf(value, " %d , %d", &w, &h) == 2)
            {
                currentType.sprite.frameWidth  = w;
                currentType.sprite.frameHeight = h;
            }
        }
        else if (strcasecmp(key, "sprite.frames") == 0)
        {
            int   count = 0;
            float dur   = 0.0f;
            if (sscanf(value, " %d , %f", &count, &dur) >= 1)
            {
                currentType.sprite.frameCount    = (count > 0) ? count : 1;
                currentType.sprite.frameDuration = (dur > 0.0f) ? dur : currentType.sprite.frameDuration;
            }
        }
        else if (strcasecmp(key, "flags") == 0)
        {
            currentType.flags = parse_flags(value);
        }
        else if (strcasecmp(key, "spawn.biome") == 0)
        {
            currentSpawn.biome = biome_kind_from_string(value);
        }
        else if (strcasecmp(key, "spawn.tile") == 0)
        {
            currentSpawn.tile = tile_from_string(value);
        }
        else if (strcasecmp(key, "spawn.density") == 0)
        {
            currentSpawn.density = (float)atof(value);
            if (currentSpawn.density < 0.0f)
                currentSpawn.density = 0.0f;
            if (currentSpawn.density > 1.0f)
                currentSpawn.density = 1.0f;
        }
        else if (strcasecmp(key, "spawn.group") == 0)
        {
            parse_group_range(value, &currentSpawn.groupMin, &currentSpawn.groupMax);
        }
        else if (strcasecmp(key, "spawn.type") == 0)
        {
            snprintf(currentSpawn.typeId, sizeof(currentSpawn.typeId), "%s", value);
        }
    }

    if (inSection && sectionName[0])
    {
        if (!currentType.id[0])
            snprintf(currentType.id, sizeof(currentType.id), "%s", sectionName);
        if (!currentType.displayName[0])
            snprintf(currentType.displayName, sizeof(currentType.displayName), "%s", currentType.id);
        entity_register_type(sys, &currentType, &currentSpawn);
        anyLoaded = true;
    }

    fclose(f);

    // fix up spawn rules that referenced explicit type ids
    for (int i = 0; i < sys->spawnRuleCount; ++i)
    {
        EntitySpawnRule* sr = &sys->spawnRules[i];
        if (sr->type)
            continue;
        if (sr->typeId[0] == '\0')
            continue;
        sr->type = entity_find_type(sys, sr->typeId);
    }

    return anyLoaded;
}

static void entity_register_fallbacks(EntitySystem* sys)
{
    if (!sys)
        return;

    EntityType zombie = {0};
    snprintf(zombie.id, sizeof(zombie.id), "%s", "cursed_zombie");
    snprintf(zombie.displayName, sizeof(zombie.displayName), "%s", "Cursed Zombie");
    zombie.flags                = ENTITY_FLAG_HOSTILE | ENTITY_FLAG_MOBILE | ENTITY_FLAG_UNDEAD;
    zombie.maxHP                = 35;
    zombie.maxSpeed             = 32.0f;
    zombie.radius               = 12.0f;
    zombie.tint                 = (Color){120, 197, 120, 255};
    zombie.sprite.frameCount    = 1;
    zombie.sprite.frameDuration = 0.0f;

    EntitySpawnRule rule;
    apply_spawn_rule_defaults(&rule);
    snprintf(rule.typeId, sizeof(rule.typeId), "%s", zombie.id);
    rule.density  = 0.025f;
    rule.groupMin = 2;
    rule.groupMax = 4;
    rule.tile     = TILE_CURSED_FOREST;
    rule.biome    = BIO_CURSED;

    entity_register_type(sys, &zombie, &rule);
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
        loaded = entity_registry_load(sys, definitionsPath);

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
            if (!sys->spawnRules[i].type && sys->spawnRules[i].typeId[0])
                sys->spawnRules[i].type = entity_find_type(sys, sys->spawnRules[i].typeId);
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

                    float roll = entity_randf(sys, 0.0f, 1.0f);
                    if (roll > rule->density)
                        continue;

                    int group = entity_randi(sys, rule->groupMin, rule->groupMax);
                    if (group <= 0)
                        group = 1;

                    for (int g = 0; g < group; ++g)
                    {
                        Vector2 spawnPos = {
                            (x + 0.5f) * TILE_SIZE + entity_randf(sys, -TILE_SIZE * 0.3f, TILE_SIZE * 0.3f),
                            (y + 0.5f) * TILE_SIZE + entity_randf(sys, -TILE_SIZE * 0.3f, TILE_SIZE * 0.3f),
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

uint16_t entity_spawn(EntitySystem* sys, const char* typeId, Vector2 position)
{
    if (!sys || !typeId)
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
            printf("⚠️  Behaviour '%s' requires %zu bytes, but only %d are available\n", type->id, e->behavior->brainSize, ENTITY_BRAIN_BYTES);
        }

        if (e->behavior && e->behavior->onSpawn)
            e->behavior->onSpawn(sys, e);

        if (i > sys->highestIndex)
            sys->highestIndex = i;
        sys->activeCount++;
        return e->id;
    }

    printf("⚠️  Entity pool exhausted, cannot spawn '%s'\n", typeId);
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

const EntityType* entity_find_type(const EntitySystem* sys, const char* typeId)
{
    if (!sys || !typeId)
        return NULL;

    for (int i = 0; i < sys->typeCount; ++i)
    {
        if (strcmp(sys->types[i].id, typeId) == 0)
            return &sys->types[i];
    }
    return NULL;
}
