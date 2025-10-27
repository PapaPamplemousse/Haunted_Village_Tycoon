/**
 * @file biome_loader.c
 * @brief Parses biome definitions from configuration files.
 */

#include "biome_loader.h"
#include "world_structures.h"
#include "tile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "world.h"
#include <strings.h>
#define LINE_MAX 256

BiomeDef gBiomeDefs[BIO_MAX];
int      gBiomeCount = 0;

/**
 * @brief Removes leading and trailing whitespace from a string in-place.
 */
static void trim_inplace(char* s)
{
    if (!s)
        return;
    // left trim
    char* p = s;
    while (*p == ' ' || *p == '\t')
        p++;
    if (p != s)
        memmove(s, p, strlen(p) + 1);
    // right trim
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' || s[len - 1] == '\n'))
        s[--len] = '\0';
}

/**
 * @brief Lightweight strdup replacement that works in the sandboxed environment.
 */
static char* str_dup(const char* s)
{
    if (!s)
        return NULL;
    size_t len  = strlen(s) + 1;
    char*  copy = malloc(len);
    if (copy)
        memcpy(copy, s, len);
    return copy;
}

/**
 * @brief Truncates a line at the first comment delimiter (# or ;) character.
 */
static void strip_inline_comment(char* s)
{
    if (!s)
        return;
    // stop at first '#' or ';'
    for (char* p = s; *p; ++p)
    {
        if (*p == '#' || *p == ';')
        {
            *p = '\0';
            break;
        }
    }
}

// Case- and prefix-tolerant resolver:
// Accepts "TILE_FOREST", "forest", "Forest", etc.
/**
 * @brief Resolves a tile identifier from a textual token.
 */
static TileTypeID tile_from_name(const char* name)
{
    if (!name || !*name)
        return TILE_GRASS;

    // Copy and trim
    char buf[64];
    snprintf(buf, sizeof(buf), "%s", name);

    // Trim leading/trailing spaces and CR/LF
    // (simple in-place trim)
    char* s = buf;
    while (*s == ' ' || *s == '\t')
        s++;
    char* end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
        *--end = '\0';

    // Drop "TILE_" prefix if present
    if (strncasecmp(s, "TILE_", 5) == 0)
        s += 5;

    // Try to match by tile display/internal name (get_tile_type(i)->name)
    for (int i = 0; i < TILE_MAX; i++)
    {
        const TileType* tt = get_tile_type((TileTypeID)i);
        if (tt && tt->name && strcasecmp(tt->name, s) == 0)
            return (TileTypeID)i;
    }

    // Also try matching by enum-like token (FOREST, PLAIN, etc.)
    // crude map: rely on get_tile_type names; already tried above.
    // If you expose a token->id map, use it here.

    printf("⚠️  Unknown tile token '%s', defaulting to GRASS\n", s);
    return TILE_GRASS;
}

/**
 * @brief Parses a comma-separated list of structures for a biome definition.
 */
static void parse_structure_list(const char* value, BiomeDef* cur, const char* biomeName)
{
    if (!value || !cur)
        return;

    BiomeStructureEntry entries[STRUCT_COUNT];
    int                 entryCount = 0;

    char* copy = str_dup(value);
    if (!copy)
        return;

    char* token = strtok(copy, ",");
    while (token && entryCount < STRUCT_COUNT)
    {
        trim_inplace(token);
        if (*token == '\0')
        {
            token = strtok(NULL, ",");
            continue;
        }

        float weight = 1.0f;
        char* sep    = strpbrk(token, ":xX*");
        if (sep)
        {
            *sep            = '\0';
            char* weightStr = sep + 1;
            trim_inplace(weightStr);
            if (*weightStr)
                weight = (float)atof(weightStr);
        }

        trim_inplace(token);
        if (*token == '\0')
        {
            token = strtok(NULL, ",");
            continue;
        }

        StructureKind kind = structure_kind_from_string(token);
        if (kind == STRUCT_COUNT)
        {
            printf("⚠️  Unknown structure '%s' in biome '%s'\n", token, biomeName ? biomeName : "?");
            token = strtok(NULL, ",");
            continue;
        }

        entries[entryCount].kind   = kind;
        entries[entryCount].weight = weight;
        entryCount++;

        token = strtok(NULL, ",");
    }

    free(copy);

    if (cur->structures)
    {
        free(cur->structures);
        cur->structures     = NULL;
        cur->structureCount = 0;
    }

    if (entryCount <= 0)
        return;

    cur->structures = (BiomeStructureEntry*)malloc((size_t)entryCount * sizeof(BiomeStructureEntry));
    if (!cur->structures)
        return;

    memcpy(cur->structures, entries, (size_t)entryCount * sizeof(BiomeStructureEntry));
    cur->structureCount = entryCount;
}

void load_biome_definitions(const char* path)
{
    gBiomeCount = 0;
    for (int i = 0; i < gBiomeCount; ++i)
    {
        free(gBiomeDefs[i].structures);
        gBiomeDefs[i].structures     = NULL;
        gBiomeDefs[i].structureCount = 0;
    }
    FILE* f = fopen(path, "r");
    if (!f)
    {
        printf("❌ Cannot open biome definitions: %s\n", path);
        return;
    }

// --- Local helpers -----------------------------------------------------

// reset a biome def to defaults
#define RESET_DEF(d)                                                                                                                                                                                                       \
    do                                                                                                                                                                                                                     \
    {                                                                                                                                                                                                                      \
        memset((d), 0, sizeof(*(d)));                                                                                                                                                                                      \
        (d)->primary        = TILE_GRASS;                                                                                                                                                                                  \
        (d)->secondary      = TILE_GRASS;                                                                                                                                                                                  \
        (d)->tempMin        = 0.0f;                                                                                                                                                                                        \
        (d)->tempMax        = 1.0f;                                                                                                                                                                                        \
        (d)->humidMin       = 0.0f;                                                                                                                                                                                        \
        (d)->humidMax       = 1.0f;                                                                                                                                                                                        \
        (d)->heightMin      = 0.0f;                                                                                                                                                                                        \
        (d)->heightMax      = 1.0f;                                                                                                                                                                                        \
        (d)->treeMul        = 0.0f;                                                                                                                                                                                        \
        (d)->bushMul        = 0.0f;                                                                                                                                                                                        \
        (d)->rockMul        = 0.0f;                                                                                                                                                                                        \
        (d)->structMul      = 1.0f;                                                                                                                                                                                        \
        (d)->maxInstances   = -1;                                                                                                                                                                                          \
        (d)->structures     = NULL;                                                                                                                                                                                        \
        (d)->structureCount = 0;                                                                                                                                                                                           \
    } while (0)

    // save the current biome into the global array
    void finalize_biome(const char* name, BiomeDef* cur)
    {
        if (!name || !*name)
            return;
        if (gBiomeCount >= BIO_MAX)
        {
            printf("⚠️  Too many biomes, skipping '%s'\n", name);
            return;
        }

        BiomeDef* stored = &gBiomeDefs[gBiomeCount++];
        *stored          = *cur;
        printf("✅ Biome registered: %-10s | primary=%s | secondary=%s | "
               "T[%.2f..%.2f] H[%.2f..%.2f] Z[%.2f..%.2f] | max=%d\n",
               name, get_tile_type(stored->primary)->name, get_tile_type(stored->secondary)->name, stored->tempMin, stored->tempMax, stored->humidMin, stored->humidMax, stored->heightMin, stored->heightMax,
               stored->maxInstances);

        if (stored->structureCount > 0 && stored->structures)
        {
            printf("   Structures: ");
            for (int i = 0; i < stored->structureCount; ++i)
            {
                const BiomeStructureEntry* entry = &stored->structures[i];
                printf("%s(%.2f)", structure_kind_to_string(entry->kind), entry->weight);
                if (i + 1 < stored->structureCount)
                    printf(", ");
            }
            printf("\n");
        }

        cur->structures     = NULL;
        cur->structureCount = 0;
    }

    // --- Parsing loop ------------------------------------------------------

    char     line[256];
    char     curName[64] = {0};
    BiomeDef cur;
    RESET_DEF(&cur);

    while (fgets(line, sizeof(line), f))
    {
        strip_inline_comment(line);
        trim_inplace(line);

        if (line[0] == '\0' || line[0] == '#')
            continue;

        // new section header
        if (line[0] == '[')
        {
            // commit previous section
            if (curName[0])
                finalize_biome(curName, &cur);

            // parse new section name
            memset(curName, 0, sizeof(curName));
            if (sscanf(line, "[%63[^]]", curName) == 1)
            {
                RESET_DEF(&cur);
                cur.kind = biome_kind_from_string(curName);
            }
            else
            {
                printf("⚠️  Malformed section header: %s\n", line);
                memset(curName, 0, sizeof(curName));
            }
            continue;
        }

        // parse key=value
        char* eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq       = '\0';
        char* key = line;
        char* val = eq + 1;
        trim_inplace(key);
        trim_inplace(val);

        if (strcasecmp(key, "primary") == 0)
            cur.primary = tile_from_name(val);
        else if (strcasecmp(key, "secondary") == 0)
            cur.secondary = tile_from_name(val);
        else if (strcasecmp(key, "temperature_min") == 0)
            cur.tempMin = (float)atof(val);
        else if (strcasecmp(key, "temperature_max") == 0)
            cur.tempMax = (float)atof(val);
        else if (strcasecmp(key, "humidity_min") == 0)
            cur.humidMin = (float)atof(val);
        else if (strcasecmp(key, "humidity_max") == 0)
            cur.humidMax = (float)atof(val);
        else if (strcasecmp(key, "height_min") == 0)
            cur.heightMin = (float)atof(val);
        else if (strcasecmp(key, "height_max") == 0)
            cur.heightMax = (float)atof(val);
        else if (strcasecmp(key, "treeMul") == 0)
            cur.treeMul = (float)atof(val);
        else if (strcasecmp(key, "bushMul") == 0)
            cur.bushMul = (float)atof(val);
        else if (strcasecmp(key, "rockMul") == 0)
            cur.rockMul = (float)atof(val);
        else if (strcasecmp(key, "structMul") == 0)
            cur.structMul = (float)atof(val);
        else if (strcasecmp(key, "max_instances") == 0)
            cur.maxInstances = atoi(val);
        else if (strcasecmp(key, "structures") == 0)
            parse_structure_list(val, &cur, curName);
    }

    // finalize last biome at EOF
    if (curName[0])
        finalize_biome(curName, &cur);

    fclose(f);
    printf("✅ Loaded %d biome definitions from %s\n", gBiomeCount, path);
}

const char* get_biome_name(BiomeKind k)
{
    switch (k)
    {
        case BIO_FOREST:
            return "FOREST";
        case BIO_PLAIN:
            return "PLAIN";
        case BIO_SAVANNA:
            return "SAVANNA";
        case BIO_TUNDRA:
            return "TUNDRA";
        case BIO_DESERT:
            return "DESERT";
        case BIO_SWAMP:
            return "SWAMP";
        case BIO_MOUNTAIN:
            return "MOUNTAIN";
        case BIO_CURSED:
            return "CURSED";
        case BIO_HELL:
            return "HELL";
        default:
            return "UNKNOWN";
    }
}

const BiomeDef* get_biome_def(BiomeKind kind)
{
    for (int i = 0; i < gBiomeCount; i++)
        if (gBiomeDefs[i].kind == kind)
            return &gBiomeDefs[i];
    return NULL;
}

BiomeKind biome_kind_from_string(const char* s)
{
    if (!s)
        return BIO_PLAIN;
    for (int i = 0; i < BIO_MAX; i++)
        if (strcasecmp(biome_kind_to_string(i), s) == 0)
            return (BiomeKind)i;
    return BIO_PLAIN;
}

const char* biome_kind_to_string(BiomeKind k)
{
    switch (k)
    {
        case BIO_FOREST:
            return "FOREST";
        case BIO_PLAIN:
            return "PLAIN";
        case BIO_SAVANNA:
            return "SAVANNA";
        case BIO_TUNDRA:
            return "TUNDRA";
        case BIO_DESERT:
            return "DESERT";
        case BIO_SWAMP:
            return "SWAMP";
        case BIO_MOUNTAIN:
            return "MOUNTAIN";
        case BIO_CURSED:
            return "CURSED";
        case BIO_HELL:
            return "HELL";
        default:
            return "UNKNOWN";
    }
}
