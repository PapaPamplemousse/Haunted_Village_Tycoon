/**
 * @file world_structures.c
 * @brief Provides procedural structure builders and lookup helpers.
 */

#include "world_structures.h"
#include "building.h" // register_building_from_bounds
#include "entity.h"
#include <stdlib.h>
#include <math.h>
#include "map.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <strings.h>
#include "world_chunk.h"
#include "biome_loader.h"

static void trim_inplace(char* s)
{
    if (!s)
        return;
    char* start = s;
    while (*start == ' ' || *start == '\t')
        start++;
    if (start != s)
        memmove(s, start, strlen(start) + 1);

    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t' || s[len - 1] == '\r' || s[len - 1] == '\n'))
        s[--len] = '\0';
}

static void strip_inline_comment(char* s)
{
    if (!s)
        return;
    for (char* p = s; *p; ++p)
    {
        if (*p == '#' || *p == ';')
        {
            *p = '\0';
            break;
        }
    }
}

static EntitiesTypeID parse_entity_type(const char* token)
{
    if (!token)
        return ENTITY_TYPE_INVALID;

    char normalized[ENTITY_TYPE_NAME_MAX];
    size_t len = 0;
    for (const char* p = token; *p && len + 1 < sizeof(normalized); ++p)
    {
        unsigned char c = (unsigned char)*p;
        if (c == '\r' || c == '\n')
            continue;
        if (c == ' ' || c == '-' || c == '\t')
            c = '_';
        normalized[len++] = (char)toupper(c);
    }
    normalized[len] = '\0';

    if (strcmp(normalized, "NONE") == 0 || strcmp(normalized, "INVALID") == 0)
        return ENTITY_TYPE_INVALID;
    if (strcmp(normalized, "CANNIBAL") == 0 || strcmp(normalized, "ENTITY_TYPE_CANNIBAL") == 0)
        return ENTITY_TYPE_CANNIBAL;
    if (strcmp(normalized, "CURSED_ZOMBIE") == 0 || strcmp(normalized, "ENTITY_TYPE_CURSED_ZOMBIE") == 0)
        return ENTITY_TYPE_CURSED_ZOMBIE;

    printf("⚠️  Unknown entity type token '%s' in structure metadata, defaulting to NONE\n", token);
    return ENTITY_TYPE_INVALID;
}

static BiomeKind parse_biome_token(const char* token, bool* outIsWildcard)
{
    if (!token)
        return BIO_MAX;

    if (outIsWildcard)
        *outIsWildcard = false;

    char normalized[32];
    size_t len = 0;
    for (const char* p = token; *p && len + 1 < sizeof(normalized); ++p)
    {
        unsigned char c = (unsigned char)*p;
        if (c == '\r' || c == '\n')
            continue;
        if (c == ' ' || c == '-' || c == '\t')
            c = '_';
        normalized[len++] = (char)toupper(c);
    }
    normalized[len] = '\0';

    if (strcmp(normalized, "FOREST") == 0)
        return BIO_FOREST;
    if (strcmp(normalized, "PLAIN") == 0)
        return BIO_PLAIN;
    if (strcmp(normalized, "SAVANNA") == 0)
        return BIO_SAVANNA;
    if (strcmp(normalized, "TUNDRA") == 0)
        return BIO_TUNDRA;
    if (strcmp(normalized, "DESERT") == 0)
        return BIO_DESERT;
    if (strcmp(normalized, "SWAMP") == 0)
        return BIO_SWAMP;
    if (strcmp(normalized, "MOUNTAIN") == 0)
        return BIO_MOUNTAIN;
    if (strcmp(normalized, "CURSED") == 0)
        return BIO_CURSED;
    if (strcmp(normalized, "HELL") == 0)
        return BIO_HELL;
    if (strcmp(normalized, "ANY") == 0 || strcmp(normalized, "ALL") == 0)
    {
        if (outIsWildcard)
            *outIsWildcard = true;
        return BIO_MAX;
    }

    return BIO_MAX;
}

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

void build_witch_hovel(Map* map, int x, int y, uint64_t* rng)
{
    int w = 5 + rand() % 2; // 5..6
    int h = 5 + rand() % 2;

    (void)rng;

    rect_walls(map, x, y, w, h, OBJ_WALL_WOOD, OBJ_DOOR_WOOD);

    int cx = x + w / 2;
    int cy = y + h / 2;
    map_place_object(map, OBJ_CAULDRON, cx, cy);
    map_place_object(map, OBJ_TOTEM_BLOOD, x + 1, y + 1);
    map_place_object(map, OBJ_TOTEM_BLOOD, x + w - 2, y + h - 2);

    if (rand() % 2)
        map_place_object(map, OBJ_BONE_PILE, cx - 1, cy);
    if (rand() % 2)
        map_place_object(map, OBJ_FIREPIT, cx, cy - 1);

    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    register_building_from_bounds(map, bounds, STRUCT_WITCH_HOVEL);
}

void build_gallows(Map* map, int x, int y, uint64_t* rng)
{
    int w = 5 + rand() % 2; // 5..6
    int h = 6 + rand() % 2; // 6..7

    (void)rng;

    rect_walls(map, x, y, w, h, OBJ_WALL_WOOD, OBJ_DOOR_WOOD);

    int centerX = x + w / 2;
    int centerY = y + h / 2;
    map_place_object(map, OBJ_GALLOW, centerX, centerY);
    map_place_object(map, OBJ_TOTEM_BLOOD, x + 1, centerY);
    map_place_object(map, OBJ_TOTEM_BLOOD, x + w - 2, centerY);

    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    register_building_from_bounds(map, bounds, STRUCT_GALLOWS);
}

void build_blood_garden(Map* map, int x, int y, uint64_t* rng)
{
    int w = 6 + rand() % 3; // 6..8
    int h = 6 + rand() % 3;

    (void)rng;

    rect_walls(map, x, y, w, h, OBJ_WALL_STONE, OBJ_DOOR_WOOD);

    int cx = x + w / 2;
    int cy = y + h / 2;
    map_place_object(map, OBJ_RITUAL_CIRCLE, cx, cy);
    map_place_object(map, OBJ_BONE_PILE, cx - 1, cy);
    map_place_object(map, OBJ_BONE_PILE, cx + 1, cy);

    map_place_object(map, OBJ_TORCH_WALL, x + 1, cy);
    map_place_object(map, OBJ_TORCH_WALL, x + w - 2, cy);

    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    register_building_from_bounds(map, bounds, STRUCT_BLOOD_GARDEN);
}

void build_flesh_pit(Map* map, int x, int y, uint64_t* rng)
{
    int w = 6 + rand() % 3; // 6..8
    int h = 6 + rand() % 3;

    (void)rng;

    rect_walls(map, x, y, w, h, OBJ_WALL_STONE, OBJ_DOOR_WOOD);

    int cx = x + w / 2;
    int cy = y + h / 2;
    map_place_object(map, OBJ_FIREPIT, cx, cy);

    for (int i = x + 1; i < x + w - 1; ++i)
    {
        if (rand() % 3 == 0)
            map_place_object(map, OBJ_MEAT_HOOK, i, y + 1);
        if (rand() % 3 == 0)
            map_place_object(map, OBJ_MEAT_HOOK, i, y + h - 2);
    }
    for (int j = y + 2; j < y + h - 2; ++j)
    {
        if (rand() % 3 == 0)
            map_place_object(map, OBJ_MEAT_HOOK, x + 1, j);
        if (rand() % 3 == 0)
            map_place_object(map, OBJ_MEAT_HOOK, x + w - 2, j);
    }

    for (int j = y + 1; j < y + h - 1; ++j)
    {
        for (int i = x + 1; i < x + w - 1; ++i)
        {
            if (i == cx && j == cy)
                continue;

            float r = (float)rand() / (float)RAND_MAX;
            if (r < 0.14f)
                map_place_object(map, OBJ_BONE_PILE, i, j);
            else if (r < 0.19f)
                map_place_object(map, OBJ_TOTEM_BLOOD, i, j);
        }
    }

    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    register_building_from_bounds(map, bounds, STRUCT_FLESH_PIT);
}

void build_void_obelisk(Map* map, int x, int y, uint64_t* rng)
{
    int w = 5 + rand() % 3; // 5..7
    int h = 5 + rand() % 3;

    (void)rng;

    rect_walls(map, x, y, w, h, OBJ_WALL_STONE, OBJ_DOOR_WOOD);

    int cx = x + w / 2;
    int cy = y + h / 2;
    map_place_object(map, OBJ_VOID_OBELISK, cx, cy);

    for (int d = 0; d < 4; ++d)
    {
        static const int OFF[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
        int px = cx + OFF[d][0];
        int py = cy + OFF[d][1];
        if (px >= x + 1 && px < x + w - 1 && py >= y + 1 && py < y + h - 1)
            map_place_object(map, OBJ_RITUAL_CIRCLE, px, py);
    }

    map_place_object(map, OBJ_TORCH_WALL, x + 1, y + 1);
    map_place_object(map, OBJ_TORCH_WALL, x + w - 2, y + 1);
    map_place_object(map, OBJ_TORCH_WALL, x + 1, y + h - 2);
    map_place_object(map, OBJ_TORCH_WALL, x + w - 2, y + h - 2);

    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    register_building_from_bounds(map, bounds, STRUCT_VOID_OBELISK);
}

void build_plague_nursery(Map* map, int x, int y, uint64_t* rng)
{
    int w = 5 + rand() % 3; // 5..7
    int h = 5 + rand() % 3;

    (void)rng;

    rect_walls(map, x, y, w, h, OBJ_WALL_WOOD, OBJ_DOOR_WOOD);

    int cx = x + w / 2;
    int cy = y + h / 2;
    map_place_object(map, OBJ_CAULDRON, cx, cy);

    for (int j = y + 1; j < y + h - 1; ++j)
    {
        for (int i = x + 1; i < x + w - 1; ++i)
        {
            if (i == cx && j == cy)
                continue;

            float r = (float)rand() / (float)RAND_MAX;
            if (r < 0.25f)
                map_place_object(map, OBJ_PLAGUE_POD, i, j);
            else if (r < 0.3f)
                map_place_object(map, OBJ_BONE_PILE, i, j);
        }
    }

    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    register_building_from_bounds(map, bounds, STRUCT_PLAGUE_NURSERY);
}

// ======================= TABLES DATA-DRIVEN =======================

static StructureDef STRUCTURES[STRUCT_COUNT] = {
    [STRUCT_HUT_CANNIBAL] = {
        .name               = "Cannibal Hut",
        .kind               = STRUCT_HUT_CANNIBAL,
        .minWidth           = 4,
        .maxWidth           = 6,
        .minHeight          = 4,
        .maxHeight          = 6,
        .rarity             = 1.0f,
        .build              = build_hut_cannibal,
        .minInstances       = 0,
        .maxInstances       = 0,
        .allowedBiomesMask  = 0,
        .auraName           = "Stench of Flesh",
        .auraDescription    = "Rotten smoke unsettles anyone nearby.",
        .auraRadius         = 4.0f,
        .auraIntensity      = 2.0f,
        .occupantType       = ENTITY_TYPE_CANNIBAL,
        .occupantMin        = 1,
        .occupantMax        = 3,
        .occupantDescription = "Cannibal raiders",
        .triggerDescription = "Shelters 1-3 cannibal raiders that ambush travellers.",
    },
    [STRUCT_CRYPT] = {
        .name               = "Crypt",
        .kind               = STRUCT_CRYPT,
        .minWidth           = 5,
        .maxWidth           = 8,
        .minHeight          = 5,
        .maxHeight          = 8,
        .rarity             = 0.8f,
        .build              = build_crypt,
        .minInstances       = 0,
        .maxInstances       = 0,
        .allowedBiomesMask  = 0,
        .auraName           = "Chill of the Grave",
        .auraDescription    = "Freezing whispers sap warmth and courage.",
        .auraRadius         = 5.0f,
        .auraIntensity      = 2.5f,
        .occupantType       = ENTITY_TYPE_CURSED_ZOMBIE,
        .occupantMin        = 2,
        .occupantMax        = 4,
        .occupantDescription = "Restless dead",
        .triggerDescription = "Unleashes undead sentries from the crypt depths.",
    },
    [STRUCT_RUIN] = {
        .name               = "Ruin",
        .kind               = STRUCT_RUIN,
        .minWidth           = 3,
        .maxWidth           = 5,
        .minHeight          = 3,
        .maxHeight          = 5,
        .rarity             = 1.2f,
        .build              = build_ruin,
        .minInstances       = 0,
        .maxInstances       = 0,
        .allowedBiomesMask  = 0,
        .auraName           = "Lingering Echoes",
        .auraDescription    = "Anxious murmurs haunt the broken stones.",
        .auraRadius         = 3.0f,
        .auraIntensity      = 1.0f,
        .occupantType       = ENTITY_TYPE_INVALID,
        .occupantMin        = 0,
        .occupantMax        = 0,
        .occupantDescription = "Abandoned",
        .triggerDescription = "Offers eerie ambience but no direct defenders.",
    },
    [STRUCT_VILLAGE_HOUSE] = {
        .name               = "Village House",
        .kind               = STRUCT_VILLAGE_HOUSE,
        .minWidth           = 4,
        .maxWidth           = 5,
        .minHeight          = 4,
        .maxHeight          = 5,
        .rarity             = 1.0f,
        .build              = build_village_house,
        .minInstances       = 0,
        .maxInstances       = 0,
        .allowedBiomesMask  = 0,
        .auraName           = "Faded Hearth",
        .auraDescription    = "Old warmth lingers but offers little comfort.",
        .auraRadius         = 2.5f,
        .auraIntensity      = 0.5f,
        .occupantType       = ENTITY_TYPE_INVALID,
        .occupantMin        = 0,
        .occupantMax        = 0,
        .occupantDescription = "Vacant shelter",
        .triggerDescription = "Provides a safe rest spot with no residents.",
    },
    [STRUCT_TEMPLE] = {
        .name               = "Temple",
        .kind               = STRUCT_TEMPLE,
        .minWidth           = 6,
        .maxWidth           = 9,
        .minHeight          = 6,
        .maxHeight          = 9,
        .rarity             = 0.3f,
        .build              = build_temple,
        .minInstances       = 0,
        .maxInstances       = 0,
        .allowedBiomesMask  = 0,
        .auraName           = "Zealous Chant",
        .auraDescription    = "Chanting reverberates, bolstering fanatic fervour.",
        .auraRadius         = 6.0f,
        .auraIntensity      = 2.2f,
        .occupantType       = ENTITY_TYPE_CURSED_ZOMBIE,
        .occupantMin        = 1,
        .occupantMax        = 2,
        .occupantDescription = "Fanatical guardians",
        .triggerDescription = "Calls forth zealots who defend the sanctuary.",
    },
    [STRUCT_WITCH_HOVEL] = {
        .name               = "Witch Hovel",
        .kind               = STRUCT_WITCH_HOVEL,
        .minWidth           = 5,
        .maxWidth           = 6,
        .minHeight          = 5,
        .maxHeight          = 6,
        .rarity             = 0.5f,
        .build              = build_witch_hovel,
        .minInstances       = 0,
        .maxInstances       = 0,
        .allowedBiomesMask  = 0,
        .auraName           = "Hexed Vapours",
        .auraDescription    = "Toxic fumes erode sanity and resolve.",
        .auraRadius         = 4.5f,
        .auraIntensity      = 2.5f,
        .occupantType       = ENTITY_TYPE_CANNIBAL,
        .occupantMin        = 1,
        .occupantMax        = 2,
        .occupantDescription = "Occult devotees",
        .triggerDescription = "Summons occultists brewing hexes in the hovel.",
    },
    [STRUCT_GALLOWS] = {
        .name               = "Gallows",
        .kind               = STRUCT_GALLOWS,
        .minWidth           = 5,
        .maxWidth           = 6,
        .minHeight          = 6,
        .maxHeight          = 7,
        .rarity             = 0.4f,
        .build              = build_gallows,
        .minInstances       = 0,
        .maxInstances       = 0,
        .allowedBiomesMask  = 0,
        .auraName           = "Lingering Terror",
        .auraDescription    = "The memory of executions chills survivors to the bone.",
        .auraRadius         = 5.0f,
        .auraIntensity      = 1.8f,
        .occupantType       = ENTITY_TYPE_INVALID,
        .occupantMin        = 0,
        .occupantMax        = 0,
        .occupantDescription = "Desolate scaffold",
        .triggerDescription = "Spreads dread that weakens morale near executions.",
    },
    [STRUCT_BLOOD_GARDEN] = {
        .name               = "Blood Garden",
        .kind               = STRUCT_BLOOD_GARDEN,
        .minWidth           = 6,
        .maxWidth           = 8,
        .minHeight          = 6,
        .maxHeight          = 8,
        .rarity             = 0.45f,
        .build              = build_blood_garden,
        .minInstances       = 0,
        .maxInstances       = 0,
        .allowedBiomesMask  = 0,
        .auraName           = "Blood Bloom",
        .auraDescription    = "Crimson spores invigorate the undead.",
        .auraRadius         = 6.0f,
        .auraIntensity      = 3.0f,
        .occupantType       = ENTITY_TYPE_CURSED_ZOMBIE,
        .occupantMin        = 2,
        .occupantMax        = 4,
        .occupantDescription = "Blood-drunk shamblers",
        .triggerDescription = "Incubates undead guardians nourished by the garden.",
    },
    [STRUCT_FLESH_PIT] = {
        .name               = "Flesh Pit",
        .kind               = STRUCT_FLESH_PIT,
        .minWidth           = 6,
        .maxWidth           = 8,
        .minHeight          = 6,
        .maxHeight          = 8,
        .rarity             = 0.35f,
        .build              = build_flesh_pit,
        .minInstances       = 0,
        .maxInstances       = 0,
        .allowedBiomesMask  = 0,
        .auraName           = "Blood Sickness",
        .auraDescription    = "Stagnant gore breeds feverish malaise.",
        .auraRadius         = 5.5f,
        .auraIntensity      = 3.4f,
        .occupantType       = ENTITY_TYPE_CANNIBAL,
        .occupantMin        = 2,
        .occupantMax        = 5,
        .occupantDescription = "Butchers and thralls",
        .triggerDescription = "Spawns cannibal butchers guarding the grisly pit.",
    },
    [STRUCT_VOID_OBELISK] = {
        .name               = "Void Obelisk",
        .kind               = STRUCT_VOID_OBELISK,
        .minWidth           = 5,
        .maxWidth           = 7,
        .minHeight          = 5,
        .maxHeight          = 7,
        .rarity             = 0.25f,
        .build              = build_void_obelisk,
        .minInstances       = 0,
        .maxInstances       = 0,
        .allowedBiomesMask  = 0,
        .auraName           = "Void Resonance",
        .auraDescription    = "Low hum saps courage and draws in darkness.",
        .auraRadius         = 6.0f,
        .auraIntensity      = 3.6f,
        .occupantType       = ENTITY_TYPE_INVALID,
        .occupantMin        = 0,
        .occupantMax        = 0,
        .occupantDescription = "Dormant monolith",
        .triggerDescription = "Emits void pulses that empower nearby occultists.",
    },
    [STRUCT_PLAGUE_NURSERY] = {
        .name               = "Plague Nursery",
        .kind               = STRUCT_PLAGUE_NURSERY,
        .minWidth           = 5,
        .maxWidth           = 7,
        .minHeight          = 5,
        .maxHeight          = 7,
        .rarity             = 0.30f,
        .build              = build_plague_nursery,
        .minInstances       = 0,
        .maxInstances       = 0,
        .allowedBiomesMask  = 0,
        .auraName           = "Plague Spores",
        .auraDescription    = "Miasma infects lungs and fortifies the cursed.",
        .auraRadius         = 4.8f,
        .auraIntensity      = 2.9f,
        .occupantType       = ENTITY_TYPE_CURSED_ZOMBIE,
        .occupantMin        = 3,
        .occupantMax        = 6,
        .occupantDescription = "Plaguebound husks",
        .triggerDescription = "Breeds plague-ridden husks from swollen cocoons.",
    },
};

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
    if (strcmp(buf, "WITCH_HOVEL") == 0)
        return STRUCT_WITCH_HOVEL;
    if (strcmp(buf, "GALLOWS") == 0)
        return STRUCT_GALLOWS;
    if (strcmp(buf, "BLOOD_GARDEN") == 0)
        return STRUCT_BLOOD_GARDEN;
    if (strcmp(buf, "FLESH_PIT") == 0)
        return STRUCT_FLESH_PIT;
    if (strcmp(buf, "VOID_OBELISK") == 0 || strcmp(buf, "OBELISK_VOID") == 0)
        return STRUCT_VOID_OBELISK;
    if (strcmp(buf, "PLAGUE_NURSERY") == 0)
        return STRUCT_PLAGUE_NURSERY;

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
        case STRUCT_WITCH_HOVEL:
            return "WITCH_HOVEL";
        case STRUCT_GALLOWS:
            return "GALLOWS";
        case STRUCT_BLOOD_GARDEN:
            return "BLOOD_GARDEN";
        case STRUCT_FLESH_PIT:
            return "FLESH_PIT";
        case STRUCT_VOID_OBELISK:
            return "VOID_OBELISK";
        case STRUCT_PLAGUE_NURSERY:
            return "PLAGUE_NURSERY";
        case STRUCT_COUNT:
            break;
    }
    return "UNKNOWN";
}

void load_structure_metadata(const char* path)
{
    if (!path)
        return;

    FILE* f = fopen(path, "r");
    if (!f)
    {
        printf("⚠️  Unable to open structure metadata file '%s'\n", path);
        return;
    }

    char          line[256];
    StructureKind current = STRUCT_COUNT;

    while (fgets(line, sizeof(line), f))
    {
        strip_inline_comment(line);
        trim_inplace(line);
        if (line[0] == '\0')
            continue;

        if (line[0] == '[')
        {
            char* end = strchr(line, ']');
            if (!end)
                continue;
            *end = '\0';
            char token[64];
            normalize_token(line + 1, token, sizeof(token));
            current = structure_kind_from_string(token);
            if (current == STRUCT_COUNT)
                printf("⚠️  Unknown structure section '%s' in metadata file\n", token);
            continue;
        }

        if (current == STRUCT_COUNT)
            continue;

        char* sep = strchr(line, '=');
        if (!sep)
            continue;
        *sep = '\0';
        char* key   = line;
        char* value = sep + 1;
        trim_inplace(key);
        trim_inplace(value);

        StructureDef* def = &STRUCTURES[current];

        if (strcasecmp(key, "display_name") == 0 || strcasecmp(key, "name") == 0)
        {
            snprintf(def->name, sizeof(def->name), "%s", value);
        }
        else if (strcasecmp(key, "min_instances") == 0)
        {
            def->minInstances = atoi(value);
            if (def->minInstances < 0)
                def->minInstances = 0;
        }
        else if (strcasecmp(key, "max_instances") == 0)
        {
            def->maxInstances = atoi(value);
            if (def->maxInstances < 0)
                def->maxInstances = 0;
        }
        else if (strcasecmp(key, "aura_name") == 0)
        {
            snprintf(def->auraName, sizeof(def->auraName), "%s", value);
        }
        else if (strcasecmp(key, "aura_description") == 0)
        {
            snprintf(def->auraDescription, sizeof(def->auraDescription), "%s", value);
        }
        else if (strcasecmp(key, "aura_radius") == 0)
        {
            def->auraRadius = (float)atof(value);
            if (def->auraRadius < 0.0f)
                def->auraRadius = 0.0f;
        }
        else if (strcasecmp(key, "aura_intensity") == 0)
        {
            def->auraIntensity = (float)atof(value);
            if (def->auraIntensity < 0.0f)
                def->auraIntensity = 0.0f;
        }
        else if (strcasecmp(key, "occupant_type") == 0)
        {
            def->occupantType = parse_entity_type(value);
        }
        else if (strcasecmp(key, "occupant_min") == 0)
        {
            def->occupantMin = atoi(value);
            if (def->occupantMin < 0)
                def->occupantMin = 0;
        }
        else if (strcasecmp(key, "occupant_max") == 0)
        {
            def->occupantMax = atoi(value);
            if (def->occupantMax < def->occupantMin)
                def->occupantMax = def->occupantMin;
        }
        else if (strcasecmp(key, "occupant_description") == 0)
        {
            snprintf(def->occupantDescription, sizeof(def->occupantDescription), "%s", value);
        }
        else if (strcasecmp(key, "entity_action") == 0 || strcasecmp(key, "trigger_description") == 0 ||
                 strcasecmp(key, "trigger") == 0)
        {
            snprintf(def->triggerDescription, sizeof(def->triggerDescription), "%s", value);
        }
        else if (strcasecmp(key, "allowed_biomes") == 0)
        {
            char buffer[256];
            strncpy(buffer, value, sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0';

            def->allowedBiomesMask = 0;
            bool unrestricted      = false;
            bool anyAssigned       = false;

            char* token = strtok(buffer, ",");
            while (token)
            {
                trim_inplace(token);
                if (*token == '\0')
                {
                    token = strtok(NULL, ",");
                    continue;
                }

                bool      wildcard = false;
                BiomeKind bk       = parse_biome_token(token, &wildcard);
                if (wildcard)
                {
                    unrestricted = true;
                    break;
                }
                if (bk == BIO_MAX)
                {
                    printf("⚠️  Unknown biome token '%s' in structure metadata, ignoring\n", token);
                }
                else
                {
                    def->allowedBiomesMask |= (1u << bk);
                    anyAssigned             = true;
                }

                token = strtok(NULL, ",");
            }

            if (unrestricted || !anyAssigned)
                def->allowedBiomesMask = 0;
        }
    }

    fclose(f);
}

const StructureDef* pick_structure_for_biome(BiomeKind biome, uint64_t* rng, const int* structureCounts)
{
    (void)rng;
    const BiomeDef* def = get_biome_def(biome);
    if (!def || def->structureCount <= 0 || !def->structures)
        return NULL;

    float totalWeight = 0.0f;
    uint32_t biomeMask = 1u << biome;
    for (int i = 0; i < def->structureCount; ++i)
    {
        const BiomeStructureEntry* entry = &def->structures[i];
        const StructureDef*        sDef  = get_structure_def(entry->kind);
        if (!sDef)
            continue;

        if (sDef->allowedBiomesMask != 0 && (sDef->allowedBiomesMask & biomeMask) == 0)
            continue;

        if (structureCounts && sDef->maxInstances > 0 && structureCounts[sDef->kind] >= sDef->maxInstances)
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

        if (sDef->allowedBiomesMask != 0 && (sDef->allowedBiomesMask & biomeMask) == 0)
            continue;

        if (structureCounts && sDef->maxInstances > 0 && structureCounts[sDef->kind] >= sDef->maxInstances)
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
        if (!sDef)
            continue;
        if (sDef->allowedBiomesMask != 0 && (sDef->allowedBiomesMask & biomeMask) == 0)
            continue;
        if (structureCounts && sDef->maxInstances > 0 && structureCounts[sDef->kind] >= sDef->maxInstances)
            continue;
        if (sDef)
            return sDef;
    }
    return NULL;
}
