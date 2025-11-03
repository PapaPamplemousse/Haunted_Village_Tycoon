#include "entities_loader.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "behavior.h"
#include "world_structures.h"
#include "biome_loader.h"
#include "tile.h"

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

static void entity_type_add_trait(EntityType* type, const char* trait)
{
    if (!type || !trait)
        return;

    if (type->traitCount >= ENTITY_MAX_TRAITS)
        return;

    char normalized[ENTITY_TRAIT_NAME_MAX];
    normalize_label(trait, normalized, sizeof(normalized));
    if (normalized[0] == '\0')
        return;

    for (int i = 0; i < type->traitCount; ++i)
    {
        if (strcmp(type->traits[i], normalized) == 0)
            return;
    }

    snprintf(type->traits[type->traitCount], ENTITY_TRAIT_NAME_MAX, "%s", normalized);
    type->traitCount++;
}

static void parse_traits_line(EntityType* type, const char* value)
{
    if (!type || !value)
        return;

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s", value);

    char* token = strtok(buffer, "|,");
    while (token)
    {
        trim_inplace(token);
        entity_type_add_trait(type, token);
        token = strtok(NULL, "|,");
    }
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
    Color result = (Color){255, 255, 255, 255};
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

static uint32_t parse_competences(const char* value)
{
    if (!value)
        return 0;

    uint32_t mask = 0;
    char     buf[128];
    snprintf(buf, sizeof(buf), "%s", value);

    char* token = strtok(buf, "|, ");
    while (token)
    {
        if (strcasecmp(token, "open_doors") == 0 || strcasecmp(token, "open_door") == 0)
            mask |= ENTITY_COMPETENCE_OPEN_DOORS;
        else if (strcasecmp(token, "seek_shelter_at_night") == 0 || strcasecmp(token, "shelter_night") == 0
                 || strcasecmp(token, "seek_shelter") == 0)
            mask |= ENTITY_COMPETENCE_SEEK_SHELTER_AT_NIGHT;
        else if (strcasecmp(token, "light_at_night") == 0 || strcasecmp(token, "light_when_night") == 0
                 || strcasecmp(token, "light_torch") == 0 || strcasecmp(token, "light_torches") == 0)
            mask |= ENTITY_COMPETENCE_LIGHT_AT_NIGHT;
        token = strtok(NULL, "|, ");
    }
    return mask;
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

static EntitiesTypeID parse_entity_id(const char* value)
{
    if (!value)
        return ENTITY_TYPE_INVALID;
    char* end = NULL;
    long  id  = strtol(value, &end, 10);
    if (end == value)
        return ENTITY_TYPE_INVALID;
    if (id < ENTITY_TYPE_INVALID || id >= ENTITY_TYPE_COUNT)
        return ENTITY_TYPE_INVALID;
    return (EntitiesTypeID)id;
}

bool entities_loader_load(EntitySystem* sys, const char* path)
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
    currentType.id                = ENTITY_TYPE_INVALID;
    currentType.referredStructure = STRUCT_COUNT;
    entity_spawn_rule_init(&currentSpawn);

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
            if (inSection && currentType.id > ENTITY_TYPE_INVALID)
            {
                if (!currentType.identifier[0] && sectionName[0])
                    snprintf(currentType.identifier, sizeof(currentType.identifier), "%s", sectionName);
                if (!currentType.displayName[0])
                    snprintf(currentType.displayName, sizeof(currentType.displayName), "%s", currentType.identifier);
                if (currentSpawn.id == ENTITY_TYPE_INVALID)
                    currentSpawn.id = currentType.id;
                entity_system_register_type(sys, &currentType, &currentSpawn);
                anyLoaded = true;
            }

            memset(&currentType, 0, sizeof(currentType));
            currentType.id                = ENTITY_TYPE_INVALID;
            currentType.referredStructure = STRUCT_COUNT;
            entity_spawn_rule_init(&currentSpawn);

            if (sscanf(line, "[%31[^]]", sectionName) == 1)
            {
                inSection = true;
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
            currentType.id = parse_entity_id(value);
        }
        else if (strcasecmp(key, "name") == 0)
        {
            snprintf(currentType.identifier, sizeof(currentType.identifier), "%s", value);
        }
        else if (strcasecmp(key, "display_name") == 0)
        {
            snprintf(currentType.displayName, sizeof(currentType.displayName), "%s", value);
        }
        else if (strcasecmp(key, "category") == 0)
        {
            normalize_label(value, currentType.category, sizeof(currentType.category));
        }
        else if (strcasecmp(key, "traits") == 0)
        {
            parse_traits_line(&currentType, value);
        }
        else if (strcasecmp(key, "max_hp") == 0)
        {
            currentType.maxHP = atoi(value);
        }
        else if (strcasecmp(key, "max_speed") == 0)
        {
            currentType.maxSpeed = (float)atof(value);
        }
        else if (strcasecmp(key, "radius") == 0)
        {
            currentType.radius = (float)atof(value);
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
        else if (strcasecmp(key, "competences") == 0)
        {
            currentType.competences = parse_competences(value);
        }
        else if (strcasecmp(key, "referred.structure") == 0 || strcasecmp(key, "referred_structure") == 0)
        {
            StructureKind kind            = structure_kind_from_string(value);
            currentType.referredStructure = kind;
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
            currentSpawn.id = parse_entity_id(value);
        }
    }

    if (inSection && currentType.id > ENTITY_TYPE_INVALID)
    {
        if (!currentType.identifier[0] && sectionName[0])
            snprintf(currentType.identifier, sizeof(currentType.identifier), "%s", sectionName);
        if (!currentType.displayName[0])
            snprintf(currentType.displayName, sizeof(currentType.displayName), "%s", currentType.identifier);
        if (currentSpawn.id == ENTITY_TYPE_INVALID)
            currentSpawn.id = currentType.id;
        entity_system_register_type(sys, &currentType, &currentSpawn);
        anyLoaded = true;
    }

    fclose(f);
    return anyLoaded;
}
