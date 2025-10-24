#include "world_structures.h"
#include "building.h" // register_building_from_bounds
#include <stdlib.h>
#include <math.h>
#include "map.h"
// --- helper murs/porte rectangle ---
static void rect_walls(Map* map, int x, int y, int w, int h, ObjectTypeID wall, ObjectTypeID door)
{
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
    register_building_from_bounds(map, bounds); // détecte et nomme via RoomTypeRule
}

void build_crypt(Map* map, int x, int y, uint64_t* rng)
{
    int w = 5 + rand() % 4; // 5..8
    int h = 5 + rand() % 4;
    rect_walls(map, x, y, w, h, OBJ_WALL_STONE, OBJ_DOOR_WOOD);

    int cx = x + w / 2, cy = y + h / 2;
    map_place_object(map, OBJ_ALTAR, cx, cy);
    if (w > 5 && h > 5)
    {
        map_place_object(map, OBJ_BONE_PILE, cx - 1, cy);
        map_place_object(map, OBJ_BONE_PILE, cx + 1, cy);
    }

    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    register_building_from_bounds(map, bounds);
}

void build_ruin(Map* map, int x, int y, uint64_t* rng)
{
    int w = 3 + rand() % 3; // 3..5
    int h = 3 + rand() % 3;
    rect_walls(map, x, y, w, h, OBJ_WALL_STONE, OBJ_DOOR_WOOD);

    // murs “brisés”
    if (rand() % 2)
        map_place_object(map, OBJ_BONE_PILE, x + 1, y + 1);

    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    register_building_from_bounds(map, bounds);
}

void build_village_house(Map* map, int x, int y, uint64_t* rng)
{
    int w = 4 + rand() % 2; // 4..5
    int h = 4 + rand() % 2;
    rect_walls(map, x, y, w, h, OBJ_WALL_WOOD, OBJ_DOOR_WOOD);

    map_place_object(map, OBJ_TABLE_WOOD, x + 1, y + 1);
    map_place_object(map, OBJ_CHAIR_WOOD, x + 2, y + 1);
    map_place_object(map, OBJ_BED_SMALL, x + 1, y + 2);

    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    register_building_from_bounds(map, bounds);
}

void build_temple(Map* map, int x, int y, uint64_t* rng)
{
    int w = 6 + rand() % 4; // 6..9
    int h = 6 + rand() % 4;
    rect_walls(map, x, y, w, h, OBJ_WALL_STONE, OBJ_DOOR_WOOD);

    map_place_object(map, OBJ_ALTAR, x + w / 2, y + h / 2);
    map_place_object(map, OBJ_TORCH_WALL, x + 1, y + 1);
    map_place_object(map, OBJ_TORCH_WALL, x + w - 2, y + 1);

    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    register_building_from_bounds(map, bounds);
}

// ======================= TABLES DATA-DRIVEN =======================

static const StructureDef STRUCTURES[STRUCT_COUNT] = {{"Cannibal Hut", STRUCT_HUT_CANNIBAL, 4, 6, 4, 6, 1.0f, build_hut_cannibal},
                                                      {"Crypt", STRUCT_CRYPT, 5, 8, 5, 8, 0.8f, build_crypt},
                                                      {"Ruin", STRUCT_RUIN, 3, 5, 3, 5, 1.2f, build_ruin},
                                                      {"Village House", STRUCT_VILLAGE_HOUSE, 4, 5, 4, 5, 1.0f, build_village_house},
                                                      {"Temple", STRUCT_TEMPLE, 6, 9, 6, 9, 0.3f, build_temple}};

// Profils Biome -> Structures
static const StructureDef* FOREST_STRUCTS[]   = {&STRUCTURES[STRUCT_RUIN], &STRUCTURES[STRUCT_TEMPLE]};
static const StructureDef* PLAIN_STRUCTS[]    = {&STRUCTURES[STRUCT_VILLAGE_HOUSE], &STRUCTURES[STRUCT_HUT_CANNIBAL]};
static const StructureDef* SAVANNA_STRUCTS[]  = {&STRUCTURES[STRUCT_HUT_CANNIBAL], &STRUCTURES[STRUCT_VILLAGE_HOUSE]};
static const StructureDef* TUNDRA_STRUCTS[]   = {&STRUCTURES[STRUCT_CRYPT], &STRUCTURES[STRUCT_RUIN]};
static const StructureDef* DESERT_STRUCTS[]   = {&STRUCTURES[STRUCT_RUIN]};
static const StructureDef* SWAMP_STRUCTS[]    = {&STRUCTURES[STRUCT_HUT_CANNIBAL], &STRUCTURES[STRUCT_RUIN]};
static const StructureDef* MOUNTAIN_STRUCTS[] = {&STRUCTURES[STRUCT_CRYPT], &STRUCTURES[STRUCT_RUIN]};
static const StructureDef* CURSED_STRUCTS[]   = {&STRUCTURES[STRUCT_CRYPT], &STRUCTURES[STRUCT_TEMPLE]};
static const StructureDef* HELL_STRUCTS[]     = {/* rien */};

static const BiomeStructureProfile PROFILES[] = {
    {BIO_FOREST, FOREST_STRUCTS, 2}, {BIO_PLAIN, PLAIN_STRUCTS, 2},       {BIO_SAVANNA, SAVANNA_STRUCTS, 2}, {BIO_TUNDRA, TUNDRA_STRUCTS, 2}, {BIO_DESERT, DESERT_STRUCTS, 1},
    {BIO_SWAMP, SWAMP_STRUCTS, 2},   {BIO_MOUNTAIN, MOUNTAIN_STRUCTS, 2}, {BIO_CURSED, CURSED_STRUCTS, 2},   {BIO_HELL, HELL_STRUCTS, 0},
};

const BiomeStructureProfile* get_biome_struct_profiles(int* count)
{
    if (count)
        *count = (int)(sizeof(PROFILES) / sizeof(PROFILES[0]));
    return PROFILES;
}

const StructureDef* pick_structure_for_biome(BiomeKind biome, uint64_t* rng)
{
    int                          n   = 0;
    const BiomeStructureProfile* all = get_biome_struct_profiles(&n);
    for (int i = 0; i < n; i++)
        if (all[i].biome == biome)
        {
            const BiomeStructureProfile* p = &all[i];
            if (p->structureCount <= 0)
                return NULL;

            float sum = 0.0f;
            for (int k = 0; k < p->structureCount; k++)
                sum += p->structures[k]->rarity;
            float r = ((float)rand() / RAND_MAX) * sum;

            float acc = 0.0f;
            for (int k = 0; k < p->structureCount; k++)
            {
                acc += p->structures[k]->rarity;
                if (r <= acc)
                    return p->structures[k];
            }
            return p->structures[p->structureCount - 1];
        }
    return NULL;
}
