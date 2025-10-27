/**
 * @file world_structures.c
 * @brief Provides procedural structure builders and lookup helpers.
 */

#include "world_structures.h"
#include "building.h" // register_building_from_bounds
#include <stdlib.h>
#include <math.h>
#include "map.h"
#include <ctype.h>
#include <string.h>
#include "world_chunk.h"
#include "biome_loader.h"

// --- helper murs/porte rectangle ---
static void rect_walls(Map* map, int x, int y, int w, int h, ObjectTypeID wall, ObjectTypeID door)
{
    (void)door;

    int ex = x + w - 1, ey = y + h - 1;
    for (int i = x; i <= ex; i++)
    {
        map_place_object(map, wall, i, y);
        map_place_object(map, wall, i, ey);
    }
    for (int j = y + 1; j < ey; j++)
    {
        map_place_object(map, wall, x, j);
        map_place_object(map, wall, ex, j);
    }

    // Porte sur un côté aléatoire
    int side = rand() % 4;
    int px   = x + 1 + rand() % (w - 2);
    int py   = y + 1 + rand() % (h - 2);
    if (side == 0)
        py = y;
    else if (side == 1)
        py = ey;
    else if (side == 2)
        px = x;
    else
        px = ex;
    map_place_object(map, OBJ_DOOR_WOOD, px, py);
}

// ======================= STRUCTURES CONCRÈTES =======================

void build_hut_cannibal(Map* map, int x, int y, uint64_t* rng)
{
    int w = 4 + rand() % 3; // 4..6
    int h = 4 + rand() % 3;

    (void)rng;
    rect_walls(map, x, y, w, h, OBJ_WALL_WOOD, OBJ_DOOR_WOOD);

    // Décor intérieur (ossements, feu, caisse)
    for (int j = y + 1; j < y + h - 1; j++)
        for (int i = x + 1; i < x + w - 1; i++)
        {
            float r = (float)rand() / RAND_MAX;
            if (r < 0.05f)
                map_place_object(map, OBJ_BONE_PILE, i, j);
            else if (r < 0.08f)
                map_place_object(map, OBJ_FIREPIT, i, j);
            else if (r < 0.11f)
                map_place_object(map, OBJ_CRATE, i, j);
        }

    // Liaison auto au système de rooms (bounds = extérieur des murs)
    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    register_building_from_bounds(map, bounds, STRUCT_HUT_CANNIBAL); // détecte et nomme via RoomTypeRule
    // chunkgrid_mark_dirty_rect(gChunks, (Rectangle){(float)x, (float)y, (float)w, (float)h});
}

void build_crypt(Map* map, int x, int y, uint64_t* rng)
{
    int w = 5 + rand() % 4; // 5..8
    int h = 5 + rand() % 4;

    (void)rng;

    rect_walls(map, x, y, w, h, OBJ_WALL_STONE, OBJ_DOOR_WOOD);

    int cx = x + w / 2, cy = y + h / 2;
    map_place_object(map, OBJ_ALTAR, cx, cy);
    if (w > 5 && h > 5)
    {
        map_place_object(map, OBJ_BONE_PILE, cx - 1, cy);
        map_place_object(map, OBJ_BONE_PILE, cx + 1, cy);
    }

    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    register_building_from_bounds(map, bounds, STRUCT_CRYPT);
    // chunkgrid_mark_dirty_rect(gChunks, (Rectangle){(float)x, (float)y, (float)w, (float)h});
}

void build_ruin(Map* map, int x, int y, uint64_t* rng)
{
    int w = 3 + rand() % 3; // 3..5
    int h = 3 + rand() % 3;

    (void)rng;

    rect_walls(map, x, y, w, h, OBJ_WALL_STONE, OBJ_DOOR_WOOD);

    // murs “brisés”
    if (rand() % 2)
        map_place_object(map, OBJ_BONE_PILE, x + 1, y + 1);

    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    register_building_from_bounds(map, bounds, STRUCT_RUIN);

    // chunkgrid_mark_dirty_rect(gChunks, (Rectangle){(float)x, (float)y, (float)w, (float)h});
}

void build_village_house(Map* map, int x, int y, uint64_t* rng)
{
    int w = 4 + rand() % 2; // 4..5
    int h = 4 + rand() % 2;

    (void)rng;
    rect_walls(map, x, y, w, h, OBJ_WALL_WOOD, OBJ_DOOR_WOOD);

    map_place_object(map, OBJ_TABLE_WOOD, x + 1, y + 1);
    map_place_object(map, OBJ_CHAIR_WOOD, x + 2, y + 1);
    map_place_object(map, OBJ_BED_SMALL, x + 1, y + 2);

    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    register_building_from_bounds(map, bounds, STRUCT_VILLAGE_HOUSE);

    // chunkgrid_mark_dirty_rect(gChunks, (Rectangle){(float)x, (float)y, (float)w, (float)h});
}

void build_temple(Map* map, int x, int y, uint64_t* rng)
{
    int w = 6 + rand() % 4; // 6..9
    int h = 6 + rand() % 4;

    (void)rng;

    rect_walls(map, x, y, w, h, OBJ_WALL_STONE, OBJ_DOOR_WOOD);

    map_place_object(map, OBJ_ALTAR, x + w / 2, y + h / 2);
    map_place_object(map, OBJ_TORCH_WALL, x + 1, y + 1);
    map_place_object(map, OBJ_TORCH_WALL, x + w - 2, y + 1);

    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    register_building_from_bounds(map, bounds, STRUCT_TEMPLE);
    // chunkgrid_mark_dirty_rect(gChunks, (Rectangle){(float)x, (float)y, (float)w, (float)h});
}

// ======================= TABLES DATA-DRIVEN =======================

static const StructureDef STRUCTURES[STRUCT_COUNT] = {{"Cannibal Hut", STRUCT_HUT_CANNIBAL, 4, 6, 4, 6, 1.0f, build_hut_cannibal},
                                                      {"Crypt", STRUCT_CRYPT, 5, 8, 5, 8, 0.8f, build_crypt},
                                                      {"Ruin", STRUCT_RUIN, 3, 5, 3, 5, 1.2f, build_ruin},
                                                      {"Village House", STRUCT_VILLAGE_HOUSE, 4, 5, 4, 5, 1.0f, build_village_house},
                                                      {"Temple", STRUCT_TEMPLE, 6, 9, 6, 9, 0.3f, build_temple}};

const StructureDef* get_structure_def(StructureKind kind)
{
    if (kind < 0 || kind >= STRUCT_COUNT)
        return NULL;
    return &STRUCTURES[kind];
}

static void normalize_token(const char* src, char* dst, size_t cap)
{
    size_t len = 0;
    for (const char* p = src; *p && len + 1 < cap; ++p)
    {
        unsigned char c = (unsigned char)*p;
        if (c == '\r' || c == '\n')
            continue;
        if (c == ' ' || c == '-' || c == '\t')
            c = '_';
        dst[len++] = (char)toupper(c);
    }
    dst[len] = '\0';

    if (strncmp(dst, "STRUCT_", 7) == 0)
        memmove(dst, dst + 7, strlen(dst + 7) + 1);
}

StructureKind structure_kind_from_string(const char* name)
{
    if (!name)
        return STRUCT_COUNT;

    char buf[64];
    normalize_token(name, buf, sizeof(buf));

    if (strcmp(buf, "HUT_CANNIBAL") == 0)
        return STRUCT_HUT_CANNIBAL;
    if (strcmp(buf, "CRYPT") == 0)
        return STRUCT_CRYPT;
    if (strcmp(buf, "RUIN") == 0)
        return STRUCT_RUIN;
    if (strcmp(buf, "VILLAGE_HOUSE") == 0)
        return STRUCT_VILLAGE_HOUSE;
    if (strcmp(buf, "TEMPLE") == 0)
        return STRUCT_TEMPLE;

    return STRUCT_COUNT;
}

const char* structure_kind_to_string(StructureKind kind)
{
    switch (kind)
    {
        case STRUCT_HUT_CANNIBAL:
            return "HUT_CANNIBAL";
        case STRUCT_CRYPT:
            return "CRYPT";
        case STRUCT_RUIN:
            return "RUIN";
        case STRUCT_VILLAGE_HOUSE:
            return "VILLAGE_HOUSE";
        case STRUCT_TEMPLE:
            return "TEMPLE";
        case STRUCT_COUNT:
            break;
    }
    return "UNKNOWN";
}

const StructureDef* pick_structure_for_biome(BiomeKind biome, uint64_t* rng)
{
    (void)rng;
    const BiomeDef* def = get_biome_def(biome);
    if (!def || def->structureCount <= 0 || !def->structures)
        return NULL;

    float totalWeight = 0.0f;
    for (int i = 0; i < def->structureCount; ++i)
    {
        const BiomeStructureEntry* entry = &def->structures[i];
        const StructureDef*        sDef  = get_structure_def(entry->kind);
        if (!sDef)
            continue;

        float w = entry->weight;
        if (w <= 0.0f)
            continue;

        totalWeight += w * (sDef->rarity > 0.0f ? sDef->rarity : 1.0f);
    }

    if (totalWeight <= 0.0f)
        return NULL;

    float pick = ((float)rand() / (float)RAND_MAX) * totalWeight;
    float acc  = 0.0f;

    for (int i = 0; i < def->structureCount; ++i)
    {
        const BiomeStructureEntry* entry = &def->structures[i];
        const StructureDef*        sDef  = get_structure_def(entry->kind);
        if (!sDef)
            continue;

        float w = entry->weight;
        if (w <= 0.0f)
            continue;

        float effective = w * (sDef->rarity > 0.0f ? sDef->rarity : 1.0f);
        acc += effective;
        if (pick <= acc)
            return sDef;
    }

    for (int i = def->structureCount - 1; i >= 0; --i)
    {
        const StructureDef* sDef = get_structure_def(def->structures[i].kind);
        if (sDef)
            return sDef;
    }
    return NULL;
}
