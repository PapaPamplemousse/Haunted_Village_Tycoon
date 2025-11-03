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

static bool parse_int_range(const char* value, int* outMin, int* outMax)
{
    if (!value)
        return false;
    int min = 0, max = 0;
    if (sscanf(value, " %d - %d", &min, &max) == 2)
    {
        if (max < min)
            max = min;
    }
    else if (sscanf(value, " %d", &min) == 1)
    {
        max = min;
    }
    else
    {
        return false;
    }
    if (outMin)
        *outMin = min;
    if (outMax)
        *outMax = max;
    return true;
}

static bool parse_float_range(const char* value, float* outMin, float* outMax)
{
    if (!value)
        return false;
    float min = 0.0f, max = 0.0f;
    if (sscanf(value, " %f - %f", &min, &max) == 2)
    {
        if (max < min)
            max = min;
    }
    else if (sscanf(value, " %f", &min) == 1)
    {
        max = min;
    }
    else
    {
        return false;
    }
    if (outMin)
        *outMin = min;
    if (outMax)
        *outMax = max;
    return true;
}

static bool parse_bool_token(const char* value, bool* out)
{
    if (!value)
        return false;

    if (strcasecmp(value, "true") == 0 || strcasecmp(value, "yes") == 0 || strcasecmp(value, "on") == 0 ||
        strcmp(value, "1") == 0)
    {
        if (out)
            *out = true;
        return true;
    }
    if (strcasecmp(value, "false") == 0 || strcasecmp(value, "no") == 0 || strcasecmp(value, "off") == 0 ||
        strcmp(value, "0") == 0)
    {
        if (out)
            *out = false;
        return true;
    }
    return false;
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
    if (strcmp(normalized, "CANNIBAL") == 0 || strcmp(normalized, "ENTITY_TYPE_CANNIBAL") == 0 ||
        strcmp(normalized, "CANNIBAL_MAN") == 0 || strcmp(normalized, "CANNIBAL_WARRIOR") == 0)
        return ENTITY_TYPE_CANNIBAL;
    if (strcmp(normalized, "CANNIBAL_WOMAN") == 0)
        return ENTITY_TYPE_CANNIBAL_WOMAN;
    if (strcmp(normalized, "CANNIBAL_CHILD") == 0)
        return ENTITY_TYPE_CANNIBAL_CHILD;
    if (strcmp(normalized, "CANNIBAL_SCOUT") == 0)
        return ENTITY_TYPE_CANNIBAL_SCOUT;
    if (strcmp(normalized, "CANNIBAL_COOK") == 0 || strcmp(normalized, "CANNIBAL_BUTCHER") == 0)
        return ENTITY_TYPE_CANNIBAL_COOK;
    if (strcmp(normalized, "CANNIBAL_SHAMAN") == 0)
        return ENTITY_TYPE_CANNIBAL_SHAMAN;
    if (strcmp(normalized, "CANNIBAL_CHIEFTAIN") == 0 || strcmp(normalized, "CANNIBAL_WARLORD") == 0)
        return ENTITY_TYPE_CANNIBAL_CHIEFTAIN;
    if (strcmp(normalized, "CANNIBAL_ZEALOT") == 0)
        return ENTITY_TYPE_CANNIBAL_ZEALOT;
    if (strcmp(normalized, "CANNIBAL_ELDER") == 0 || strcmp(normalized, "CANNIBAL_MATRON") == 0)
        return ENTITY_TYPE_CANNIBAL_ELDER;
    if (strcmp(normalized, "CANNIBAL_BERSERKER") == 0)
        return ENTITY_TYPE_CANNIBAL_BERSERKER;
    if (strcmp(normalized, "CURSED_ZOMBIE") == 0)
        return ENTITY_TYPE_CURSED_ZOMBIE;
    if (strcmp(normalized, "VILLAGER") == 0)
        return ENTITY_TYPE_VILLAGER;
    if (strcmp(normalized, "VILLAGER_CHILD") == 0 || strcmp(normalized, "CHILD") == 0)
        return ENTITY_TYPE_VILLAGER_CHILD;
    if (strcmp(normalized, "GUARD") == 0 || strcmp(normalized, "WATCHMAN") == 0)
        return ENTITY_TYPE_GUARD;
    if (strcmp(normalized, "GUARD_CAPTAIN") == 0 || strcmp(normalized, "MILITIA_CAPTAIN") == 0)
        return ENTITY_TYPE_GUARD_CAPTAIN;
    if (strcmp(normalized, "FARMER") == 0)
        return ENTITY_TYPE_FARMER;
    if (strcmp(normalized, "FISHERMAN") == 0)
        return ENTITY_TYPE_FISHERMAN;
    if (strcmp(normalized, "LUMBERJACK") == 0 || strcmp(normalized, "WOODCUTTER") == 0)
        return ENTITY_TYPE_LUMBERJACK;
    if (strcmp(normalized, "MINER") == 0)
        return ENTITY_TYPE_MINER;
    if (strcmp(normalized, "BLACKSMITH") == 0 || strcmp(normalized, "TOOLMAKER") == 0)
        return ENTITY_TYPE_BLACKSMITH;
    if (strcmp(normalized, "BAKER") == 0 || strcmp(normalized, "COOK") == 0)
        return ENTITY_TYPE_BAKER;
    if (strcmp(normalized, "APOTHECARY") == 0 || strcmp(normalized, "HERBALIST") == 0)
        return ENTITY_TYPE_APOTHECARY;
    if (strcmp(normalized, "DOCTOR") == 0 || strcmp(normalized, "BARBER_SURGEON") == 0)
        return ENTITY_TYPE_DOCTOR;
    if (strcmp(normalized, "GRAVEDIGGER") == 0 || strcmp(normalized, "MORTICIAN") == 0)
        return ENTITY_TYPE_GRAVEDIGGER;
    if (strcmp(normalized, "PRIEST") == 0)
        return ENTITY_TYPE_PRIEST;
    if (strcmp(normalized, "ACOLYTE") == 0 || strcmp(normalized, "MONK") == 0)
        return ENTITY_TYPE_ACOLYTE;
    if (strcmp(normalized, "TEACHER") == 0 || strcmp(normalized, "SCHOLAR") == 0)
        return ENTITY_TYPE_TEACHER;
    if (strcmp(normalized, "TAVERNKEEPER") == 0 || strcmp(normalized, "INNKEEPER") == 0)
        return ENTITY_TYPE_TAVERNKEEPER;
    if (strcmp(normalized, "MERCHANT") == 0 || strcmp(normalized, "SHOPKEEPER") == 0)
        return ENTITY_TYPE_MERCHANT;
    if (strcmp(normalized, "WATCHDOG") == 0 || strcmp(normalized, "DOG") == 0)
        return ENTITY_TYPE_WATCHDOG;
    if (strcmp(normalized, "HORSE") == 0 || strcmp(normalized, "MULE") == 0 || strcmp(normalized, "DONKEY") == 0)
        return ENTITY_TYPE_HORSE;
    if (strcmp(normalized, "GOAT") == 0)
        return ENTITY_TYPE_GOAT;
    if (strcmp(normalized, "PIG") == 0 || strcmp(normalized, "BOAR") == 0)
        return ENTITY_TYPE_PIG;
    if (strcmp(normalized, "CHICKEN") == 0 || strcmp(normalized, "ROOSTER") == 0)
        return ENTITY_TYPE_CHICKEN;
    if (strcmp(normalized, "CROW") == 0 || strcmp(normalized, "RAVEN") == 0)
        return ENTITY_TYPE_CROW;
    if (strcmp(normalized, "RAT") == 0 || strcmp(normalized, "BAT") == 0)
        return ENTITY_TYPE_RAT;
    if (strcmp(normalized, "DEER") == 0 || strcmp(normalized, "HARE") == 0)
        return ENTITY_TYPE_DEER;
    if (strcmp(normalized, "GHOULHOUND") == 0)
        return ENTITY_TYPE_GHOULHOUND;
    if (strcmp(normalized, "WENDIGO") == 0)
        return ENTITY_TYPE_WENDIGO;
    if (strcmp(normalized, "MARSH_HORROR") == 0)
        return ENTITY_TYPE_MARSH_HORROR;
    if (strcmp(normalized, "BOG_FIEND") == 0)
        return ENTITY_TYPE_BOG_FIEND;
    if (strcmp(normalized, "CORPSE_BOAR") == 0)
        return ENTITY_TYPE_CORPSE_BOAR;
    if (strcmp(normalized, "BLIGHT_ELK") == 0)
        return ENTITY_TYPE_BLIGHT_ELK;
    if (strcmp(normalized, "DIRE_WOLF") == 0 || strcmp(normalized, "HOWLING_WOLF") == 0)
        return ENTITY_TYPE_DIRE_WOLF;
    if (strcmp(normalized, "CAVE_SPIDER") == 0 || strcmp(normalized, "MARSH_LEECH") == 0)
        return ENTITY_TYPE_CAVE_SPIDER;
    if (strcmp(normalized, "VAMPIRE_NOBLE") == 0 || strcmp(normalized, "VAMPIRE_COUNTESS") == 0)
        return ENTITY_TYPE_VAMPIRE_NOBLE;
    if (strcmp(normalized, "VAMPIRE_THRALL") == 0 || strcmp(normalized, "FAMILIAR") == 0)
        return ENTITY_TYPE_VAMPIRE_THRALL;
    if (strcmp(normalized, "NOSFERATU") == 0 || strcmp(normalized, "GHAST") == 0)
        return ENTITY_TYPE_NOSFERATU;
    if (strcmp(normalized, "BLOOD_PRIEST") == 0)
        return ENTITY_TYPE_BLOOD_PRIEST;
    if (strcmp(normalized, "CRIMSON_BAT") == 0 || strcmp(normalized, "BLOOD_MIST") == 0)
        return ENTITY_TYPE_CRIMSON_BAT;
    if (strcmp(normalized, "CULT_INITIATE") == 0)
        return ENTITY_TYPE_CULTIST_INITIATE;
    if (strcmp(normalized, "CULT_ADEPT") == 0)
        return ENTITY_TYPE_CULTIST_ADEPT;
    if (strcmp(normalized, "CULT_LEADER") == 0 || strcmp(normalized, "FALSE_SAINT") == 0)
        return ENTITY_TYPE_CULT_LEADER;
    if (strcmp(normalized, "OCCULT_SCHOLAR") == 0)
        return ENTITY_TYPE_OCCULT_SCHOLAR;
    if (strcmp(normalized, "SHADOWBOUND_ACOLYTE") == 0)
        return ENTITY_TYPE_SHADOWBOUND_ACOLYTE;
    if (strcmp(normalized, "POSSESSED_VILLAGER") == 0)
        return ENTITY_TYPE_POSSESSED_VILLAGER;
    if (strcmp(normalized, "DOOMSPEAKER") == 0)
        return ENTITY_TYPE_DOOMSPEAKER;
    if (strcmp(normalized, "BUTCHER_PROPHET") == 0)
        return ENTITY_TYPE_BUTCHER_PROPHET;
    if (strcmp(normalized, "RESTLESS_SPIRIT") == 0)
        return ENTITY_TYPE_RESTLESS_SPIRIT;
    if (strcmp(normalized, "GRAVEBOUND_CORPSE") == 0)
        return ENTITY_TYPE_GRAVEBOUND_CORPSE;
    if (strcmp(normalized, "FALSE_RESURRECTION") == 0 || strcmp(normalized, "ZOMBIE_DOCILE") == 0)
        return ENTITY_TYPE_FALSE_RESURRECTION;
    if (strcmp(normalized, "BONEWALKER") == 0 || strcmp(normalized, "OSSUARY_GUARD") == 0)
        return ENTITY_TYPE_BONEWALKER;
    if (strcmp(normalized, "PLAGUE_DEAD") == 0)
        return ENTITY_TYPE_PLAGUE_DEAD;
    if (strcmp(normalized, "WRAITH") == 0 || strcmp(normalized, "APPARITION") == 0)
        return ENTITY_TYPE_WRAITH;
    if (strcmp(normalized, "HAUNTING_SHADE") == 0)
        return ENTITY_TYPE_HAUNTING_SHADE;
    if (strcmp(normalized, "SPECTRAL_BRIDE") == 0 || strcmp(normalized, "GHOST_CHILD") == 0)
        return ENTITY_TYPE_SPECTRAL_BRIDE;
    if (strcmp(normalized, "CRYPT_LORD") == 0)
        return ENTITY_TYPE_CRYPT_LORD;
    if (strcmp(normalized, "SHADOW_PHANTOM") == 0)
        return ENTITY_TYPE_SHADOW_PHANTOM;
    if (strcmp(normalized, "ECHOED_VOICE") == 0)
        return ENTITY_TYPE_ECHOED_VOICE;
    if (strcmp(normalized, "MIRROR_SHADE") == 0)
        return ENTITY_TYPE_MIRROR_SHADE;
    if (strcmp(normalized, "NAMELESS_MOURNER") == 0)
        return ENTITY_TYPE_NAMELESS_MOURNER;
    if (strcmp(normalized, "NIGHTMARE_APPARITION") == 0)
        return ENTITY_TYPE_NIGHTMARE_APPARITION;
    if (strcmp(normalized, "SPECTER_OF_THE_WELL") == 0 || strcmp(normalized, "WELL_SPECTER") == 0)
        return ENTITY_TYPE_WELL_SPECTER;
    if (strcmp(normalized, "ASH_WIDOW") == 0)
        return ENTITY_TYPE_ASH_WIDOW;
    if (strcmp(normalized, "CLOCKWORK_WRAITH") == 0)
        return ENTITY_TYPE_CLOCKWORK_WRAITH;
    if (strcmp(normalized, "TRAVELING_MERCHANT") == 0 || strcmp(normalized, "CARAVAN_TRADER") == 0)
        return ENTITY_TYPE_TRAVELING_MERCHANT;
    if (strcmp(normalized, "PILGRIM_BAND") == 0)
        return ENTITY_TYPE_PILGRIM_BAND;
    if (strcmp(normalized, "INQUISITION_ENVOY") == 0)
        return ENTITY_TYPE_INQUISITION_ENVOY;
    if (strcmp(normalized, "NEIGHBOR_DELEGATION") == 0)
        return ENTITY_TYPE_NEIGHBOR_DELEGATION;
    if (strcmp(normalized, "TRAVELING_PERFORMER") == 0 || strcmp(normalized, "CHARLATAN") == 0)
        return ENTITY_TYPE_TRAVELING_PERFORMER;
    if (strcmp(normalized, "WITCH_HUNTER") == 0)
        return ENTITY_TYPE_WITCH_HUNTER;
    if (strcmp(normalized, "MISSIONARY") == 0 || strcmp(normalized, "MONK_VISITOR") == 0)
        return ENTITY_TYPE_MISSIONARY;
    if (strcmp(normalized, "GRAVEDUST_PEDDLER") == 0)
        return ENTITY_TYPE_GRAVEDUST_PEDDLER;
    if (strcmp(normalized, "WHISPERER_BENEATH_THE_MIRE") == 0 || strcmp(normalized, "WHISPERER_BENEATH") == 0)
        return ENTITY_TYPE_WHISPERER_BENEATH;
    if (strcmp(normalized, "PALE_CHILD") == 0)
        return ENTITY_TYPE_PALE_CHILD;
    if (strcmp(normalized, "BLOOD_MOON_APPARITION") == 0)
        return ENTITY_TYPE_BLOOD_MOON_APPARITION;
    if (strcmp(normalized, "ARCHITECT_GHOST") == 0)
        return ENTITY_TYPE_ARCHITECT_GHOST;
    if (strcmp(normalized, "NAMELESS_ARCHIVIST") == 0)
        return ENTITY_TYPE_NAMELESS_ARCHIVIST;
    if (strcmp(normalized, "THING_WITH_A_THOUSAND_TEETH") == 0 || strcmp(normalized, "THOUSAND_TEETH") == 0)
        return ENTITY_TYPE_THOUSAND_TEETH;
    if (strcmp(normalized, "HOLLOW_WATCHER") == 0)
        return ENTITY_TYPE_HOLLOW_WATCHER;
    if (strcmp(normalized, "CANDLE_EATER") == 0)
        return ENTITY_TYPE_CANDLE_EATER;
    if (strcmp(normalized, "FLESH_LIBRARIAN") == 0)
        return ENTITY_TYPE_FLESH_LIBRARIAN;
    if (strcmp(normalized, "CRIMSON_CHOIR") == 0)
        return ENTITY_TYPE_CRIMSON_CHOIR;
    if (strcmp(normalized, "HEART_OF_THE_TOWN") == 0 || strcmp(normalized, "HEART_OF_TOWN") == 0)
        return ENTITY_TYPE_HEART_OF_TOWN;
    if (strcmp(normalized, "DREAMING_GOD") == 0)
        return ENTITY_TYPE_DREAMING_GOD;

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

static void fill_tiles(Map* map, int startX, int startY, int width, int height, TileTypeID tile)
{
    if (!map || width <= 0 || height <= 0)
        return;

    int endX = startX + width;
    int endY = startY + height;
    for (int y = startY; y < endY; ++y)
    {
        if (y < 0 || y >= map->height)
            continue;
        for (int x = startX; x < endX; ++x)
        {
            if (x < 0 || x >= map->width)
                continue;
            map_set_tile(map, x, y, tile);
        }
    }
}

// ======================= STRUCTURES CONCRÈTES =======================

void build_hut_cannibal(Map* map, int x, int y, uint64_t* rng)
{
    int w = 4 + rand() % 3; // 4..6
    int h = 4 + rand() % 3;

    (void)rng;
    rect_walls(map, x, y, w, h, OBJ_WALL_WOOD, OBJ_DOOR_WOOD);
    fill_tiles(map, x + 1, y + 1, w - 2, h - 2, TILE_STRAW_FLOOR);

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
    register_building_from_bounds(map, bounds, STRUCT_HUT_CANNIBAL); // détecte et nomme via classification intégrée
    // chunkgrid_mark_dirty_rect(gChunks, (Rectangle){(float)x, (float)y, (float)w, (float)h});
}

void build_cannibal_longhouse(Map* map, int x, int y, uint64_t* rng)
{
    int w = 7 + rand() % 2; // 7..8
    int h = 6 + rand() % 2; // 6..7

    (void)rng;
    rect_walls(map, x, y, w, h, OBJ_WALL_WOOD, OBJ_DOOR_WOOD);
    fill_tiles(map, x + 1, y + 1, w - 2, h - 2, TILE_WOOD_FLOOR);

    int centerX = x + w / 2;
    int centerY = y + h / 2;
    map_place_object(map, OBJ_FIREPIT, centerX, centerY);
    map_place_object(map, OBJ_TABLE_WOOD, x + 1, y + 1);
    map_place_object(map, OBJ_CHAIR_WOOD, x + 2, y + 1);
    map_place_object(map, OBJ_MEAT_HOOK, centerX - 1, y + h - 2);
    map_place_object(map, OBJ_MEAT_HOOK, centerX + 1, y + h - 2);
    map_place_object(map, OBJ_BONE_PILE, centerX, y + h - 2);

    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    register_building_from_bounds(map, bounds, STRUCT_CANNIBAL_LONGHOUSE);
}

void build_cannibal_cook_tent(Map* map, int x, int y, uint64_t* rng)
{
    int w = 5 + rand() % 2;
    int h = 5 + rand() % 2;

    (void)rng;
    rect_walls(map, x, y, w, h, OBJ_WALL_WOOD, OBJ_DOOR_WOOD);
    fill_tiles(map, x + 1, y + 1, w - 2, h - 2, TILE_STONE_FLOOR);

    int centerX = x + w / 2;
    int centerY = y + h / 2;
    map_place_object(map, OBJ_CAULDRON, centerX, centerY);
    map_place_object(map, OBJ_FIREPIT, centerX, centerY - 1);

    for (int i = x + 1; i < x + w - 1; ++i)
    {
        if (rand() % 2)
            map_place_object(map, OBJ_MEAT_HOOK, i, y + 1);
        if (rand() % 2)
            map_place_object(map, OBJ_MEAT_HOOK, i, y + h - 2);
    }

    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    register_building_from_bounds(map, bounds, STRUCT_CANNIBAL_COOK_TENT);
}

void build_cannibal_shaman_hut(Map* map, int x, int y, uint64_t* rng)
{
    int w = 5 + rand() % 2;
    int h = 5 + rand() % 2;

    (void)rng;
    rect_walls(map, x, y, w, h, OBJ_WALL_WOOD, OBJ_DOOR_WOOD);
    fill_tiles(map, x + 1, y + 1, w - 2, h - 2, TILE_STONE_FLOOR);

    int centerX = x + w / 2;
    int centerY = y + h / 2;
    map_place_object(map, OBJ_RITUAL_CIRCLE, centerX, centerY);
    map_place_object(map, OBJ_TOTEM_BLOOD, x + 1, y + 1);
    map_place_object(map, OBJ_TOTEM_BLOOD, x + w - 2, y + h - 2);
    map_place_object(map, OBJ_BONE_PILE, centerX, y + 1);

    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    register_building_from_bounds(map, bounds, STRUCT_CANNIBAL_SHAMAN_HUT);
}

void build_cannibal_bone_pit(Map* map, int x, int y, uint64_t* rng)
{
    int w = 6 + rand() % 2;
    int h = 6 + rand() % 2;

    (void)rng;
    rect_walls(map, x, y, w, h, OBJ_WALL_STONE, OBJ_DOOR_WOOD);
    fill_tiles(map, x + 1, y + 1, w - 2, h - 2, TILE_STONE_FLOOR);

    for (int j = y + 1; j < y + h - 1; ++j)
    {
        for (int i = x + 1; i < x + w - 1; ++i)
        {
            if (rand() % 3 == 0)
                map_place_object(map, OBJ_BONE_PILE, i, j);
        }
    }

    map_place_object(map, OBJ_TOTEM_BLOOD, x + w / 2, y + 1);
    map_place_object(map, OBJ_TOTEM_BLOOD, x + w / 2, y + h - 2);

    Rectangle bounds = {(float)x, (float)y, (float)w, (float)h};
    register_building_from_bounds(map, bounds, STRUCT_CANNIBAL_BONE_PIT);
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
    [STRUCT_CANNIBAL_LONGHOUSE] = {
        .name               = "Cannibal Longhouse",
        .kind               = STRUCT_CANNIBAL_LONGHOUSE,
        .minWidth           = 7,
        .maxWidth           = 8,
        .minHeight          = 6,
        .maxHeight          = 7,
        .rarity             = 0.4f,
        .build              = build_cannibal_longhouse,
        .minInstances       = 0,
        .maxInstances       = 0,
        .allowedBiomesMask  = 0,
        .auraName           = "War Council",
        .auraDescription    = "Strategists plot raids beneath blooded banners.",
        .auraRadius         = 5.0f,
        .auraIntensity      = 2.6f,
        .occupantType       = ENTITY_TYPE_CANNIBAL_CHIEFTAIN,
        .occupantMin        = 1,
        .occupantMax        = 1,
        .occupantDescription = "Cannibal chieftain",
        .triggerDescription = "Anchors a cannibal war-camp with its leader.",
    },
    [STRUCT_CANNIBAL_COOK_TENT] = {
        .name               = "Butcher Tent",
        .kind               = STRUCT_CANNIBAL_COOK_TENT,
        .minWidth           = 5,
        .maxWidth           = 6,
        .minHeight          = 5,
        .maxHeight          = 6,
        .rarity             = 0.6f,
        .build              = build_cannibal_cook_tent,
        .minInstances       = 0,
        .maxInstances       = 0,
        .allowedBiomesMask  = 0,
        .auraName           = "Sickly Aroma",
        .auraDescription    = "Clotted smoke speaks of grisly feasts.",
        .auraRadius         = 4.0f,
        .auraIntensity      = 2.2f,
        .occupantType       = ENTITY_TYPE_CANNIBAL_COOK,
        .occupantMin        = 1,
        .occupantMax        = 2,
        .occupantDescription = "Cannibal butcher",
        .triggerDescription = "Staffed with cooks preparing trophies for rituals.",
    },
    [STRUCT_CANNIBAL_SHAMAN_HUT] = {
        .name               = "Shaman Hut",
        .kind               = STRUCT_CANNIBAL_SHAMAN_HUT,
        .minWidth           = 5,
        .maxWidth           = 6,
        .minHeight          = 5,
        .maxHeight          = 6,
        .rarity             = 0.5f,
        .build              = build_cannibal_shaman_hut,
        .minInstances       = 0,
        .maxInstances       = 0,
        .allowedBiomesMask  = 0,
        .auraName           = "Blood Ritual",
        .auraDescription    = "Hexes and chants twist the minds of intruders.",
        .auraRadius         = 4.5f,
        .auraIntensity      = 2.8f,
        .occupantType       = ENTITY_TYPE_CANNIBAL_SHAMAN,
        .occupantMin        = 1,
        .occupantMax        = 1,
        .occupantDescription = "Cannibal shaman",
        .triggerDescription = "Shamans weave hexes that empower nearby clansmen.",
    },
    [STRUCT_CANNIBAL_BONE_PIT] = {
        .name               = "Bone Pit",
        .kind               = STRUCT_CANNIBAL_BONE_PIT,
        .minWidth           = 6,
        .maxWidth           = 7,
        .minHeight          = 6,
        .maxHeight          = 7,
        .rarity             = 0.5f,
        .build              = build_cannibal_bone_pit,
        .minInstances       = 0,
        .maxInstances       = 0,
        .allowedBiomesMask  = 0,
        .auraName           = "Trophy Ground",
        .auraDescription    = "Mountains of bones rattle with frenzied energy.",
        .auraRadius         = 4.0f,
        .auraIntensity      = 2.4f,
        .occupantType       = ENTITY_TYPE_CANNIBAL_BERSERKER,
        .occupantMin        = 1,
        .occupantMax        = 2,
        .occupantDescription = "Berserk guardians",
        .triggerDescription = "Frenzied berserkers guard the grisly trophy pit.",
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
    if (strcmp(buf, "CANNIBAL_LONGHOUSE") == 0)
        return STRUCT_CANNIBAL_LONGHOUSE;
    if (strcmp(buf, "CANNIBAL_COOK_TENT") == 0 || strcmp(buf, "BUTCHER_TENT") == 0)
        return STRUCT_CANNIBAL_COOK_TENT;
    if (strcmp(buf, "CANNIBAL_SHAMAN_HUT") == 0)
        return STRUCT_CANNIBAL_SHAMAN_HUT;
    if (strcmp(buf, "CANNIBAL_BONE_PIT") == 0)
        return STRUCT_CANNIBAL_BONE_PIT;
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
        case STRUCT_CANNIBAL_LONGHOUSE:
            return "CANNIBAL_LONGHOUSE";
        case STRUCT_CANNIBAL_COOK_TENT:
            return "CANNIBAL_COOK_TENT";
        case STRUCT_CANNIBAL_SHAMAN_HUT:
            return "CANNIBAL_SHAMAN_HUT";
        case STRUCT_CANNIBAL_BONE_PIT:
            return "CANNIBAL_BONE_PIT";
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

static void parse_cluster_members(StructureDef* def, const char* value)
{
    if (!def)
        return;
    def->clusterMemberCount = 0;
    if (!value)
        return;

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s", value);

    char* token = strtok(buffer, ",");
    while (token && def->clusterMemberCount < STRUCTURE_CLUSTER_MAX_MEMBERS)
    {
        trim_inplace(token);
        if (*token == '\0')
        {
            token = strtok(NULL, ",");
            continue;
        }

        char name[64];
        char counts[64] = {0};
        const char* colon = strchr(token, ':');
        if (colon)
        {
            size_t nameLen = (size_t)(colon - token);
            if (nameLen >= sizeof(name))
                nameLen = sizeof(name) - 1;
            memcpy(name, token, nameLen);
            name[nameLen] = '\0';
            snprintf(counts, sizeof(counts), "%s", colon + 1);
        }
        else
        {
            snprintf(name, sizeof(name), "%s", token);
        }

        trim_inplace(name);
        trim_inplace(counts);

        StructureKind kind = structure_kind_from_string(name);
        if (kind == STRUCT_COUNT)
        {
            token = strtok(NULL, ",");
            continue;
        }

        int minCount = 1;
        int maxCount = 1;
        if (counts[0] != '\0' && !parse_int_range(counts, &minCount, &maxCount))
        {
            minCount = 1;
            maxCount = 1;
        }

        StructureClusterMember* entry = &def->clusterMembers[def->clusterMemberCount++];
        entry->kind      = kind;
        entry->minCount  = minCount;
        entry->maxCount  = maxCount;

        token = strtok(NULL, ",");
    }
}

typedef struct
{
    const char* token;
    RoomTypeID  id;
} RoomTokenMap;

static RoomTypeID parse_room_identifier(const char* value)
{
    static const RoomTokenMap MAP[] = {
        {"ROOM_NONE", ROOM_NONE},
        {"ROOM_BEDROOM", ROOM_BEDROOM},
        {"ROOM_KITCHEN", ROOM_KITCHEN},
        {"ROOM_HUT", ROOM_HUT},
        {"ROOM_CRYPT", ROOM_CRYPT},
        {"ROOM_SANCTUARY", ROOM_SANCTUARY},
        {"ROOM_HOUSE", ROOM_HOUSE},
        {"ROOM_LARGEROOM", ROOM_LARGEROOM},
        {"ROOM_CANNIBAL_DEN", ROOM_CANNIBAL_DEN},
        {"ROOM_CANNIBAL_LONGHOUSE", ROOM_CANNIBAL_LONGHOUSE},
        {"ROOM_BUTCHER_TENT", ROOM_BUTCHER_TENT},
        {"ROOM_SHAMAN_HUT", ROOM_SHAMAN_HUT},
        {"ROOM_BONE_PIT", ROOM_BONE_PIT},
        {"ROOM_WHISPERING_CRYPT", ROOM_WHISPERING_CRYPT},
        {"ROOM_FORSAKEN_RUIN", ROOM_FORSAKEN_RUIN},
        {"ROOM_DESERTED_HOME", ROOM_DESERTED_HOME},
        {"ROOM_BLOODBOUND_TEMPLE", ROOM_BLOODBOUND_TEMPLE},
        {"ROOM_HEXSPEAKER_HOVEL", ROOM_HEXSPEAKER_HOVEL},
        {"ROOM_SORROW_GALLOWS", ROOM_SORROW_GALLOWS},
        {"ROOM_BLOODROSE_GARDEN", ROOM_BLOODROSE_GARDEN},
        {"ROOM_FLESH_PIT", ROOM_FLESH_PIT},
        {"ROOM_VOID_OBELISK", ROOM_VOID_OBELISK},
        {"ROOM_PLAGUE_NURSERY", ROOM_PLAGUE_NURSERY},
        {NULL, ROOM_NONE}};

    if (!value)
        return ROOM_NONE;

    char token[64];
    snprintf(token, sizeof(token), "%s", value);
    trim_inplace(token);

    size_t len = strlen(token);
    if (len >= 2 && ((token[0] == '"' && token[len - 1] == '"') || (token[0] == '\'' && token[len - 1] == '\'')))
    {
        token[len - 1] = '\0';
        memmove(token, token + 1, len - 1);
        trim_inplace(token);
    }

    if (token[0] == '\0')
        return ROOM_NONE;

    bool numeric = true;
    for (const char* p = token; *p; ++p)
    {
        if (!isdigit((unsigned char)*p))
        {
            numeric = false;
            break;
        }
    }
    if (numeric)
    {
        int id = atoi(token);
        if (id >= ROOM_NONE && id < ROOM_COUNT)
            return (RoomTypeID)id;
        return ROOM_NONE;
    }

    char normalized[64];
    size_t outLen = 0;
    for (const char* p = token; *p && outLen + 1 < sizeof(normalized); ++p)
    {
        unsigned char c = (unsigned char)*p;
        if (c == ' ' || c == '-')
            c = '_';
        normalized[outLen++] = (char)toupper(c);
    }
    normalized[outLen] = '\0';

    if (normalized[0] == '\0')
        return ROOM_NONE;

    for (const RoomTokenMap* entry = MAP; entry->token; ++entry)
    {
        if (strcmp(normalized, entry->token) == 0)
            return entry->id;
    }

    if (strncmp(normalized, "ROOM_", 5) != 0)
    {
        char prefixed[64];
        snprintf(prefixed, sizeof(prefixed), "ROOM_%s", normalized);
        for (const RoomTokenMap* entry = MAP; entry->token; ++entry)
        {
            if (strcmp(prefixed, entry->token) == 0)
                return entry->id;
        }
    }

    return ROOM_NONE;
}

static void parse_structure_requirements(StructureDef* def, const char* value)
{
    if (!def)
        return;

    def->requirementCount = 0;
    if (!value)
        return;

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s", value);

    char* token = strtok(buffer, ",");
    while (token && def->requirementCount < STRUCTURE_MAX_REQUIREMENTS)
    {
        trim_inplace(token);
        if (*token == '\0')
        {
            token = strtok(NULL, ",");
            continue;
        }

        char objectName[64];
        int  minCount = 0;
        if (sscanf(token, "%63[^:]:%d", objectName, &minCount) == 2)
        {
            trim_inplace(objectName);
            if (minCount < 0)
                minCount = 0;
            ObjectTypeID id = object_type_id_from_name(objectName);
            if (id != OBJ_NONE)
            {
                ObjectRequirement* req = &def->requirements[def->requirementCount++];
                req->objectId          = id;
                req->minCount          = minCount;
            }
            else
            {
                printf("⚠️  Unknown object requirement '%s' for structure '%s'\n", objectName, def->name);
            }
        }

        token = strtok(NULL, ",");
    }
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

            const char* lookup = token;
            if (strncmp(token, "STRUCTURE_", 10) == 0)
                lookup = token + 10;

            current = structure_kind_from_string(lookup);
            if (current == STRUCT_COUNT)
                printf("⚠️  Unknown structure section '%s' in metadata file\n", token);
            else
            {
                StructureDef* def = &STRUCTURES[current];
                def->clusterGroup[0]  = '\0';
                def->clusterAnchor    = false;
                def->clusterMinMembers = 0;
                def->clusterMaxMembers = 0;
                def->clusterRadiusMin = 0.0f;
                def->clusterRadiusMax = 0.0f;
                def->clusterMemberCount = 0;
                def->roomId           = ROOM_NONE;
                def->minArea          = 0;
                def->maxArea          = 0;
                def->requirementCount = 0;
            }
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
        else if (strcasecmp(key, "cluster.group") == 0 || strcasecmp(key, "cluster_id") == 0)
        {
            snprintf(def->clusterGroup, sizeof(def->clusterGroup), "%s", value);
        }
        else if (strcasecmp(key, "cluster.anchor") == 0)
        {
            bool flag = false;
            if (parse_bool_token(value, &flag))
                def->clusterAnchor = flag;
        }
        else if (strcasecmp(key, "cluster.min_members") == 0)
        {
            def->clusterMinMembers = atoi(value);
            if (def->clusterMinMembers < 0)
                def->clusterMinMembers = 0;
        }
        else if (strcasecmp(key, "cluster.max_members") == 0)
        {
            def->clusterMaxMembers = atoi(value);
            if (def->clusterMaxMembers < 0)
                def->clusterMaxMembers = 0;
        }
        else if (strcasecmp(key, "cluster.radius") == 0)
        {
            float rMin = 0.0f;
            float rMax = 0.0f;
            if (parse_float_range(value, &rMin, &rMax))
            {
                def->clusterRadiusMin = rMin;
                def->clusterRadiusMax = rMax;
            }
        }
        else if (strcasecmp(key, "cluster.radius_min") == 0)
        {
            def->clusterRadiusMin = (float)atof(value);
            if (def->clusterRadiusMin < 0.0f)
                def->clusterRadiusMin = 0.0f;
        }
        else if (strcasecmp(key, "cluster.radius_max") == 0)
        {
            def->clusterRadiusMax = (float)atof(value);
            if (def->clusterRadiusMax < 0.0f)
                def->clusterRadiusMax = 0.0f;
        }
        else if (strcasecmp(key, "cluster.members") == 0)
        {
            parse_cluster_members(def, value);
        }
        else if (strcasecmp(key, "id") == 0)
        {
            def->roomId = parse_room_identifier(value);
        }
        else if (strcasecmp(key, "min_area") == 0)
        {
            def->minArea = atoi(value);
            if (def->minArea < 0)
                def->minArea = 0;
        }
        else if (strcasecmp(key, "max_area") == 0)
        {
            def->maxArea = atoi(value);
            if (def->maxArea < 0)
                def->maxArea = 0;
        }
        else if (strcasecmp(key, "requirement") == 0)
        {
            parse_structure_requirements(def, value);
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
