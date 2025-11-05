/**
 * @file building.c
 * @brief Implements building detection and classification logic.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "building.h"
#include "entity.h"
#include "pantry.h"
#include "tile.h"
#include "object.h"
#include "world_structures.h"

/* ===========================================
 * Structures
 * =========================================== */
typedef struct
{
    int       area;
    Rectangle bounds;
    int       doorCount;
    int       wallBoundaryCount;
    bool      nonStructuralBlocker;
    bool      touchesBorder;
} FloodResult;

static Building gGeneratedBuildings[MAX_GENERATED_BUILDINGS];
static Building gPlayerBuildings[MAX_PLAYER_BUILDINGS];
static int      gGeneratedCount = 0;
static int      gPlayerCount    = 0;
static int      gNextBuildingId = 1;

static unsigned int gVisitedStamp[MAP_HEIGHT][MAP_WIDTH];
static unsigned int gVisitedGeneration = 1;
static int          gStructureVillageIds[MAP_HEIGHT][MAP_WIDTH];
static int          gStructureSpeciesIds[MAP_HEIGHT][MAP_WIDTH];

static bool building_residents_reserve(Building* b, int minCapacity);
int         building_generated_count(void)
{
    return gGeneratedCount;
}

int building_player_count(void)
{
    return gPlayerCount;
}

int building_total_count(void)
{
    return gGeneratedCount + gPlayerCount;
}

const Building* building_get_generated(int index)
{
    if (index < 0 || index >= gGeneratedCount)
        return NULL;
    return &gGeneratedBuildings[index];
}

const Building* building_get_player(int index)
{
    if (index < 0 || index >= gPlayerCount)
        return NULL;
    return &gPlayerBuildings[index];
}

const Building* building_get(int index)
{
    if (index < 0)
        return NULL;
    if (index < gGeneratedCount)
        return &gGeneratedBuildings[index];
    index -= gGeneratedCount;
    if (index < gPlayerCount)
        return &gPlayerBuildings[index];
    return NULL;
}

Building* building_get_mutable(int index)
{
    if (index < 0)
        return NULL;
    if (index < gGeneratedCount)
        return &gGeneratedBuildings[index];
    index -= gGeneratedCount;
    if (index < gPlayerCount)
        return &gPlayerBuildings[index];
    return NULL;
}

void building_on_reservation_spawn(int buildingId)
{
    Building* b = building_get_mutable(buildingId);
    if (!b)
        return;
    b->occupantActive++;
}

void building_on_reservation_hibernate(int buildingId)
{
    Building* b = building_get_mutable(buildingId);
    if (!b)
        return;
    b->occupantActive--;
    if (b->occupantActive < 0)
        b->occupantActive = 0;
}

void building_add_resident(Building* b, Entity* e)
{
    if (!b || !e)
        return;

    for (int i = 0; i < b->residentCount; ++i)
    {
        if (b->residents[i] == e->id)
            return;
    }

    if (!building_residents_reserve(b, b->residentCount + 1))
        return;

    b->residents[b->residentCount++] = e->id;

    e->homeBuildingId = b->id;
    e->home           = (Vector2){b->center.x * TILE_SIZE, b->center.y * TILE_SIZE};
    e->villageId      = b->villageId;
}

void building_remove_resident(Building* b, uint16_t entityId)
{
    if (!b || b->residentCount <= 0)
        return;

    for (int i = 0; i < b->residentCount; ++i)
    {
        if (b->residents[i] != entityId)
            continue;
        if (i < b->residentCount - 1)
            memmove(&b->residents[i], &b->residents[i + 1], (size_t)(b->residentCount - i - 1) * sizeof(uint16_t));
        b->residentCount--;
        if (b->occupantActive > 0)
            b->occupantActive--;
        break;
    }
}

int building_active_residents(const Building* b, const EntitySystem* sys)
{
    if (!b || !sys)
        return 0;
    int count = 0;
    for (int i = 0; i < b->residentCount; ++i)
    {
        uint16_t id          = b->residents[i];
        const Entity* entity = entity_get(sys, id);
        if (entity && entity->active)
            count++;
    }
    return count;
}

Building* building_get_at_tile(int tileX, int tileY)
{
    int total = building_total_count();
    for (int i = 0; i < total; ++i)
    {
        Building* b = building_get_mutable(i);
        if (!b)
            continue;

        int startX = (int)floorf(b->bounds.x);
        int startY = (int)floorf(b->bounds.y);
        int endX   = (int)ceilf(b->bounds.x + b->bounds.width);
        int endY   = (int)ceilf(b->bounds.y + b->bounds.height);

        if (tileX >= startX && tileX < endX && tileY >= startY && tileY < endY)
            return b;
    }
    return NULL;
}

void building_debug_print(const Building* b, const EntitySystem* sys)
{
    if (!b)
    {
        printf("[BUILDING] No building at this location.\n");
        return;
    }

    int active = building_active_residents(b, sys);

    printf("=========== BUILDING DEBUG ===========\n");
    printf("ID: %d  Kind: %d  Name: %s\n", b->id, b->structureKind, b->name[0] ? b->name : "(unnamed)");
    printf("Bounds: (%.1f, %.1f, %.1f, %.1f)\n", b->bounds.x, b->bounds.y, b->bounds.width, b->bounds.height);
    printf("Residents: active=%d stored=%d min=%d max=%d target=%d\n", active, b->residentCount, b->occupantMin, b->occupantMax, b->occupantCurrent);
    printf("OccupantType: %d SpeciesId: %d VillageId: %d Pantry: %s\n", b->occupantType, b->speciesId, b->villageId, b->hasPantry ? "yes" : "no");

    if (sys)
    {
        for (int i = 0; i < b->residentCount; ++i)
        {
            uint16_t     id  = b->residents[i];
            const Entity* ent = entity_get(sys, id);
            if (!ent)
            {
                printf("  - Slot %d -> entity %u (missing)\n", i, id);
                continue;
            }

            const EntityType* type = ent->type;
            printf("  - Slot %d -> entity %u (%s) active=%d hunger=%.1f home=%d\n", i, ent->id, type ? type->identifier : "(unknown)", ent->active, ent->hunger, ent->homeBuildingId);
        }
    }

    if (b->hasPantry)
    {
        Pantry* pantry = pantry_get_for_building(b->id);
        if (pantry)
        {
            printf("  Pantry contents: meat=%d plant=%d capacity=%d\n", pantry->counts[PANTRY_ITEM_MEAT], pantry->counts[PANTRY_ITEM_PLANT], pantry->capacity);
        }
        else
        {
            printf("  Pantry contents: <none>\n");
        }
    }

    printf("======================================\n");
}

Building* entity_get_home(const Entity* e)
{
    if (!e || e->homeBuildingId < 0)
        return NULL;
    return building_get_mutable(e->homeBuildingId);
}

Building* building_get_for_species(const char* species, int villageId)
{
    int speciesId = entity_species_id_from_label(species);
    int total     = building_total_count();
    for (int i = 0; i < total; ++i)
    {
        Building* b = building_get_mutable(i);
        if (!b)
            continue;
        if (speciesId > 0 && b->speciesId != speciesId)
            continue;
        if (villageId >= 0 && b->villageId != villageId)
            continue;
        return b;
    }
    return NULL;
}

static inline bool rectangles_overlap(Rectangle a, Rectangle b)
{
    if (a.width <= 0.0f || a.height <= 0.0f || b.width <= 0.0f || b.height <= 0.0f)
        return false;

    return (a.x < b.x + b.width) && (a.x + a.width > b.x) && (a.y < b.y + b.height) && (a.y + a.height > b.y);
}

static inline int clamp_int(int value, int minValue, int maxValue)
{
    if (value < minValue)
        return minValue;
    if (value > maxValue)
        return maxValue;
    return value;
}

static void release_building(Building* b)
{
    if (!b)
        return;

    if (b->hasPantry || b->pantryId >= 0)
        pantry_remove(b->id);

    if (b->objects)
    {
        free(b->objects);
        b->objects = NULL;
    }
    b->objectCount = 0;

    if (b->residents)
    {
        free(b->residents);
        b->residents = NULL;
    }
    b->residentCount    = 0;
    b->residentCapacity = 0;
}

static void reset_building_list(Building* list, int* count, int maxEntries)
{
    if (!list || !count)
        return;

    for (int i = 0; i < *count && i < maxEntries; ++i)
    {
        release_building(&list[i]);
    }

    memset(list, 0, sizeof(Building) * (size_t)maxEntries);
    *count = 0;
}

static bool building_residents_reserve(Building* b, int minCapacity)
{
    if (!b)
        return false;
    if (b->residentCapacity >= minCapacity)
        return true;

    int newCap = (b->residentCapacity > 0) ? b->residentCapacity * 2 : 4;
    if (newCap < minCapacity)
        newCap = minCapacity;

    uint16_t* data = (uint16_t*)realloc(b->residents, (size_t)newCap * sizeof(uint16_t));
    if (!data)
        return false;

    b->residents        = data;
    b->residentCapacity = newCap;
    return true;
}

static void remove_buildings_in_region(Building* list, int* count, Rectangle tileRegion)
{
    if (!list || !count || *count <= 0)
        return;

    for (int i = 0; i < *count;)
    {
        if (rectangles_overlap(list[i].bounds, tileRegion))
        {
            release_building(&list[i]);

            if (i < *count - 1)
                memmove(&list[i], &list[i + 1], (size_t)(*count - i - 1) * sizeof(Building));

            (*count)--;
            continue;
        }

        ++i;
    }
}

/* ===========================================
 * 1. Object analysis utility functions
 * =========================================== */

/**
 * @brief Checks if an object is part of a building's structure.
 * (Wall, door, or any future structural element.)
 */
static inline bool is_structural_object(const Object* obj)
{
    if (!obj)
        return false;

    return is_wall_object(obj) || is_door_object(obj);
}

/**
 * @brief Checks if an object blocks walking without being structural.
 * Example: bed, table, chest -> blocks, but doesn't form a wall.
 */
static inline bool is_non_structural_blocker(const Object* obj)
{
    if (!obj)
        return false;

    if (is_structural_object(obj))
        return false;

    return !object_is_walkable(obj);
}

/**
 * @brief Determines if an object contributes to the "structural boundary".
 * I.e., if it encloses the room.
 */
static inline bool contributes_to_building_boundary(const Object* obj)
{
    if (!obj)
        return false;

    // Wall or door encloses the room
    if (is_wall_object(obj) || is_door_object(obj))
        return true;

    /* FOR FUTURE ???*/
    // if (is_magic_barrier(obj)) return true;
    // if (is_force_field(obj)) return true;

    return false;
}

/* ===========================================
 * 2. Main Flood-fill
 * =========================================== */

/**
 * @brief Flood-fills an empty area to identify a potential room.
 *
 * Rules:
 * - Walls and doors are considered structural (boundary, blocking).
 * - Non-structural furniture (bed, table, etc.) invalidates the room.
 * - Walkable objects do not block.
 * - A room is valid if it is enclosed and does not touch the border.
 */
static StructureKind gStructureMarkers[MAP_HEIGHT][MAP_WIDTH];

void building_clear_structure_markers(void)
{
    for (int y = 0; y < MAP_HEIGHT; ++y)
    {
        for (int x = 0; x < MAP_WIDTH; ++x)
        {
            gStructureMarkers[y][x]    = STRUCT_COUNT;
            gStructureVillageIds[y][x] = -1;
            gStructureSpeciesIds[y][x] = 0;
        }
    }
}

static FloodResult perform_flood_fill(Map* map, int sx, int sy, unsigned int stamp, unsigned int visited[MAP_HEIGHT][MAP_WIDTH])
{
    FloodResult res = {0};

    int minx = map->width, miny = map->height;
    int maxx = -1, maxy = -1;

    const int stackCap = map->width * map->height;
    int*      stack    = (int*)malloc(stackCap * sizeof(int));
    int       top      = 0;

    visited[sy][sx] = stamp;
    stack[top++]    = sy * map->width + sx;

    while (top > 0)
    {
        const int idx = stack[--top];
        const int cx  = idx % map->width;
        const int cy  = idx / map->width;

        res.area++;
        if (cx < minx)
            minx = cx;
        if (cy < miny)
            miny = cy;
        if (cx > maxx)
            maxx = cx;
        if (cy > maxy)
            maxy = cy;

        static const int DIRS[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
        for (int d = 0; d < 4; ++d)
        {
            const int nx = cx + DIRS[d][0];
            const int ny = cy + DIRS[d][1];

            if (nx < 0 || ny < 0 || nx >= map->width || ny >= map->height)
            {
                res.touchesBorder = true;
                continue;
            }

            if (visited[ny][nx] == stamp)
                continue;

            Object* obj = map->objects[ny][nx];

            if (!obj)
            {
                visited[ny][nx] = stamp;
                stack[top++]    = ny * map->width + nx;
            }
            else if (contributes_to_building_boundary(obj))
            {
                // Wall or door = boundary
                if (is_wall_object(obj))
                    res.wallBoundaryCount++;
                else if (is_door_object(obj))
                    res.doorCount++;

                // We don't cross a wall/door: the room stops here
                continue;
            }
            else if (is_non_structural_blocker(obj))
            {
                // Checks if the furniture touches an unvisited outside area (open wall, map border)
                bool touchesOutside = false;
                for (int dd = 0; dd < 4; ++dd)
                {
                    int tx = nx + DIRS[dd][0];
                    int ty = ny + DIRS[dd][1];
                    if (tx < 0 || ty < 0 || tx >= map->width || ty >= map->height)
                    {
                        touchesOutside = true;
                        break;
                    }
                    // Only consider “outside” if you go outside the limits of the map
                    if (tx < 0 || ty < 0 || tx >= map->width || ty >= map->height)
                    {
                        touchesOutside = true;
                        break;
                    }
                }

                if (touchesOutside)
                {
                    // Furniture adjacent to the border or outside: invalidates the room
                    res.nonStructuralBlocker = true;
                }

                // In any case, mark the tile to prevent an infinite loop
                visited[ny][nx] = stamp;
                stack[top++]    = ny * map->width + nx;
            }
            else
            {
                // Walkable or decorative object
                visited[ny][nx] = stamp;
                stack[top++]    = ny * map->width + nx;
            }
        }
    }

    free(stack);

    res.bounds.x      = (float)minx;
    res.bounds.y      = (float)miny;
    res.bounds.width  = (float)(maxx - minx + 1);
    res.bounds.height = (float)(maxy - miny + 1);

    return res;
}

/* ===========================================
 * 3. Room validation
 * =========================================== */
static bool is_valid_building_area(const FloodResult* r)
{
    if (r->area <= 0)
        return false;

    if (r->touchesBorder)
        return false;

    if (r->nonStructuralBlocker)
        return false;

    // At least one structural element (wall or door)
    if ((r->wallBoundaryCount + r->doorCount) <= 0)
        return false;

    return true;
}

/* ===========================================
 * 4. Initialization and collection
 * =========================================== */
static void infer_structure_metadata_from_markers(const FloodResult* res, int* outSpeciesId, int* outVillageId)
{
    if (outSpeciesId)
        *outSpeciesId = 0;
    if (outVillageId)
        *outVillageId = -1;
    if (!res)
        return;

    int speciesIds[STRUCTURE_MAX_RESIDENT_ROLES]   = {0};
    int speciesVotes[STRUCTURE_MAX_RESIDENT_ROLES] = {0};
    int speciesCount                               = 0;
    int villageIds[STRUCTURE_MAX_RESIDENT_ROLES]   = {0};
    int villageVotes[STRUCTURE_MAX_RESIDENT_ROLES] = {0};
    int villageCount                               = 0;

    for (int ty = (int)res->bounds.y; ty < res->bounds.y + res->bounds.height; ++ty)
    {
        if (ty < 0 || ty >= MAP_HEIGHT)
            continue;
        for (int tx = (int)res->bounds.x; tx < res->bounds.x + res->bounds.width; ++tx)
        {
            if (tx < 0 || tx >= MAP_WIDTH)
                continue;

            int sid = gStructureSpeciesIds[ty][tx];
            if (sid > 0)
            {
                bool found = false;
                for (int i = 0; i < speciesCount; ++i)
                {
                    if (speciesIds[i] == sid)
                    {
                        speciesVotes[i]++;
                        found = true;
                        break;
                    }
                }
                if (!found && speciesCount < STRUCTURE_MAX_RESIDENT_ROLES)
                {
                    speciesIds[speciesCount]   = sid;
                    speciesVotes[speciesCount] = 1;
                    speciesCount++;
                }
            }

            int vid = gStructureVillageIds[ty][tx];
            if (vid >= 0)
            {
                bool found = false;
                for (int i = 0; i < villageCount; ++i)
                {
                    if (villageIds[i] == vid)
                    {
                        villageVotes[i]++;
                        found = true;
                        break;
                    }
                }
                if (!found && villageCount < STRUCTURE_MAX_RESIDENT_ROLES)
                {
                    villageIds[villageCount]   = vid;
                    villageVotes[villageCount] = 1;
                    villageCount++;
                }
            }
        }
    }

    if (outSpeciesId && speciesCount > 0)
    {
        int bestVote = 0;
        int bestId   = 0;
        for (int i = 0; i < speciesCount; ++i)
        {
            if (speciesVotes[i] > bestVote)
            {
                bestVote = speciesVotes[i];
                bestId   = speciesIds[i];
            }
        }
        *outSpeciesId = bestId;
    }

    if (outVillageId && villageCount > 0)
    {
        int bestVote = 0;
        int bestId   = -1;
        for (int i = 0; i < villageCount; ++i)
        {
            if (villageVotes[i] > bestVote)
            {
                bestVote = villageVotes[i];
                bestId   = villageIds[i];
            }
        }
        *outVillageId = bestId;
    }
}

static int compute_structure_resident_count(const StructureDef* def, int id, const FloodResult* res)
{
    if (!def || def->occupantType <= ENTITY_TYPE_INVALID)
        return 0;
    if (def->occupantMax < def->occupantMin)
        return def->occupantMin;

    int span = def->occupantMax - def->occupantMin;
    if (span <= 0)
        return def->occupantMin;

    unsigned int seed = (unsigned int)(id * 73856093u) ^ (unsigned int)(res->bounds.x * 19349663u) ^ (unsigned int)(res->bounds.y * 83492791u);
    return def->occupantMin + (int)(seed % (unsigned int)(span + 1));
}

static StructureKind infer_marker_kind(const FloodResult* res)
{
    if (!res)
        return STRUCT_COUNT;

    int counts[STRUCT_COUNT] = {0};
    int startX               = (int)res->bounds.x + 1;
    int endX                 = (int)(res->bounds.x + res->bounds.width) - 1;
    int startY               = (int)res->bounds.y + 1;
    int endY                 = (int)(res->bounds.y + res->bounds.height) - 1;

    if (startX < 0)
        startX = 0;
    if (startY < 0)
        startY = 0;
    if (endX > MAP_WIDTH - 1)
        endX = MAP_WIDTH - 1;
    if (endY > MAP_HEIGHT - 1)
        endY = MAP_HEIGHT - 1;

    for (int y = startY; y <= endY; ++y)
    {
        for (int x = startX; x <= endX; ++x)
        {
            StructureKind marker = gStructureMarkers[y][x];
            if (marker >= 0 && marker < STRUCT_COUNT)
                counts[marker]++;
        }
    }

    int bestCount = 0;
    int bestIndex = STRUCT_COUNT;
    for (int k = 0; k < STRUCT_COUNT; ++k)
    {
        if (counts[k] > bestCount)
        {
            bestCount = counts[k];
            bestIndex = k;
        }
    }

    return (bestCount > 0) ? (StructureKind)bestIndex : STRUCT_COUNT;
}

static void init_building_structure(Building* b, int id, const FloodResult* res, StructureKind kind)
{
    b->id                   = id;
    b->bounds               = res->bounds;
    b->area                 = res->area;
    b->center               = (Vector2){res->bounds.x + res->bounds.width / 2.0f, res->bounds.y + res->bounds.height / 2.0f};
    b->objectCount          = 0;
    b->objects              = NULL;
    b->roomTypeId           = ROOM_NONE;
    b->structureKind        = kind;
    b->speciesId            = 0;
    b->species[0]           = '\0';
    b->villageId            = -1;
    b->hasPantry            = false;
    b->pantryCapacity       = 0;
    b->pantryId             = -1;
    b->roleCount            = 0;
    b->residents            = NULL;
    b->residentCount        = 0;
    b->residentCapacity     = 0;
    const StructureDef* def = (kind >= 0 && kind < STRUCT_COUNT) ? get_structure_def(kind) : NULL;
    b->structureDef         = def;

    int inferredSpecies = 0;
    int inferredVillage = -1;
    infer_structure_metadata_from_markers(res, &inferredSpecies, &inferredVillage);

    if (def)
    {
        const char* displayName = (def->name[0] != '\0') ? def->name : structure_kind_to_string(def->kind);
        snprintf(b->name, sizeof(b->name), "%s", displayName ? displayName : "Structure");
        snprintf(b->auraName, sizeof(b->auraName), "%s", def->auraName);
        snprintf(b->auraDescription, sizeof(b->auraDescription), "%s", def->auraDescription);
        b->auraRadius    = def->auraRadius;
        b->auraIntensity = def->auraIntensity;
        b->occupantType  = def->occupantType;
        b->occupantMin   = def->occupantMin;
        b->occupantMax   = def->occupantMax;
        snprintf(b->occupantDescription, sizeof(b->occupantDescription), "%s", def->occupantDescription);
        snprintf(b->triggerDescription, sizeof(b->triggerDescription), "%s", def->triggerDescription);
        b->occupantCurrent = compute_structure_resident_count(def, id, res);
        b->occupantActive  = 0;
        b->isGenerated     = true;
        if (def->speciesId > 0)
            b->speciesId = def->speciesId;
        else if (def->species[0] != '\0')
            b->speciesId = entity_species_id_from_label(def->species);
        else if (inferredSpecies > 0)
            b->speciesId = inferredSpecies;
        if (def->species[0] != '\0')
            snprintf(b->species, sizeof(b->species), "%s", def->species);
        b->villageId      = inferredVillage;
        b->hasPantry      = def->hasPantry;
        b->pantryCapacity = def->pantryCapacity;
        b->roleCount      = (def->roleCount > STRUCTURE_MAX_RESIDENT_ROLES) ? STRUCTURE_MAX_RESIDENT_ROLES : def->roleCount;
        for (int i = 0; i < b->roleCount; ++i)
            snprintf(b->roles[i], sizeof(b->roles[i]), "%s", def->roles[i]);

        if (b->structureKind == STRUCT_HUT_CANNIBAL)
        {
            b->occupantMin      = 2;
            b->occupantMax      = 2;
            b->occupantCurrent  = 2;
            b->occupantActive   = 0;
        }
    }
    else
    {
        b->structureDef           = NULL;
        b->name[0]                = '\0';
        b->auraName[0]            = '\0';
        b->auraDescription[0]     = '\0';
        b->auraRadius             = 0.0f;
        b->auraIntensity          = 0.0f;
        b->occupantType           = ENTITY_TYPE_INVALID;
        b->occupantMin            = 0;
        b->occupantMax            = 0;
        b->occupantCurrent        = 0;
        b->occupantActive         = 0;
        b->occupantDescription[0] = '\0';
        b->triggerDescription[0]  = '\0';
        b->isGenerated            = false;
    }

    if (b->speciesId == 0 && inferredSpecies > 0)
        b->speciesId = inferredSpecies;
    if (b->speciesId == 0 && b->species[0] != '\0')
        b->speciesId = entity_species_id_from_label(b->species);
    if (b->villageId < 0)
        b->villageId = inferredVillage;
    if (b->hasPantry)
    {
        Pantry* pantry = pantry_create_or_get(b->id, b->pantryCapacity);
        if (pantry)
            b->pantryId = pantry->id;
    }
}

static void collect_building_objects(Map* map, Building* b, const FloodResult* res, unsigned int stamp, unsigned int visited[MAP_HEIGHT][MAP_WIDTH])
{
    Object** temp_objects    = (Object**)malloc(res->area * sizeof(Object*));
    int      collected_count = 0;

    for (int y = (int)res->bounds.y; y < res->bounds.y + res->bounds.height; ++y)
    {
        for (int x = (int)res->bounds.x; x < res->bounds.x + res->bounds.width; ++x)
        {
            if (visited[y][x] != stamp)
                continue;

            Object* obj = map->objects[y][x];
            if (!obj)
                continue;

            // We do not collect walls and doors (structural boundaries)
            if (is_wall_object(obj) || is_door_object(obj))
                continue;

            // All other interior objects (bed, table, torch, decor...) are collected
            temp_objects[collected_count++] = obj;
        }
    }

    b->objectCount = collected_count;
    if (collected_count > 0)
    {
        b->objects = (Object**)malloc(collected_count * sizeof(Object*));
        memcpy(b->objects, temp_objects, collected_count * sizeof(Object*));
    }
    else
    {
        b->objects = NULL;
    }

    free(temp_objects);
}

/* ===========================================
 * 5. Main detection
 * =========================================== */
void update_building_detection(Map* map, Rectangle worldRegion)
{
    if (!map)
        return;

    const float mapWidthPixels  = (float)(map->width * TILE_SIZE);
    const float mapHeightPixels = (float)(map->height * TILE_SIZE);
    const float padding         = (float)TILE_SIZE;

    if (worldRegion.width <= 0.0f || worldRegion.height <= 0.0f)
    {
        worldRegion.x      = 0.0f;
        worldRegion.y      = 0.0f;
        worldRegion.width  = mapWidthPixels;
        worldRegion.height = mapHeightPixels;
    }

    worldRegion.x -= padding;
    worldRegion.y -= padding;
    worldRegion.width += padding * 2.0f;
    worldRegion.height += padding * 2.0f;

    if (worldRegion.x < 0.0f)
        worldRegion.x = 0.0f;
    if (worldRegion.y < 0.0f)
        worldRegion.y = 0.0f;
    if (worldRegion.x + worldRegion.width > mapWidthPixels)
        worldRegion.width = mapWidthPixels - worldRegion.x;
    if (worldRegion.y + worldRegion.height > mapHeightPixels)
        worldRegion.height = mapHeightPixels - worldRegion.y;

    int startX = (int)floorf(worldRegion.x / (float)TILE_SIZE);
    int startY = (int)floorf(worldRegion.y / (float)TILE_SIZE);
    int endX   = (int)ceilf((worldRegion.x + worldRegion.width) / (float)TILE_SIZE) - 1;
    int endY   = (int)ceilf((worldRegion.y + worldRegion.height) / (float)TILE_SIZE) - 1;

    if (endX < startX || endY < startY)
        return;

    startX = clamp_int(startX, 0, map->width - 1);
    startY = clamp_int(startY, 0, map->height - 1);
    endX   = clamp_int(endX, 0, map->width - 1);
    endY   = clamp_int(endY, 0, map->height - 1);

    if (endX < startX || endY < startY)
        return;

    Rectangle tileRegion = {
        .x      = (float)startX,
        .y      = (float)startY,
        .width  = (float)(endX - startX + 1),
        .height = (float)(endY - startY + 1),
    };

    bool fullRebuild = (startX == 0 && startY == 0 && endX == map->width - 1 && endY == map->height - 1);

    if (fullRebuild)
    {
        reset_building_list(gGeneratedBuildings, &gGeneratedCount, MAX_GENERATED_BUILDINGS);
        reset_building_list(gPlayerBuildings, &gPlayerCount, MAX_PLAYER_BUILDINGS);
        gNextBuildingId = 1;
        pantry_system_reset();
        building_clear_structure_markers();
    }
    else
    {
        remove_buildings_in_region(gGeneratedBuildings, &gGeneratedCount, tileRegion);
        remove_buildings_in_region(gPlayerBuildings, &gPlayerCount, tileRegion);
    }

    unsigned int stamp = gVisitedGeneration++;
    if (gVisitedGeneration == 0)
    {
        memset(gVisitedStamp, 0, sizeof(gVisitedStamp));
        gVisitedGeneration = 1;
        stamp              = gVisitedGeneration++;
    }

    for (int y = startY; y <= endY; ++y)
    {
        for (int x = startX; x <= endX; ++x)
        {
            if (gVisitedStamp[y][x] == stamp)
                continue;

            Object* obj = map->objects[y][x];

            if (obj && (is_structural_object(obj) || is_non_structural_blocker(obj)))
            {
                gVisitedStamp[y][x] = stamp;
                continue;
            }

            FloodResult res = perform_flood_fill(map, x, y, stamp, gVisitedStamp);
            if (!is_valid_building_area(&res))
                continue;

            StructureKind kind        = infer_marker_kind(&res);
            bool          isGenerated = (kind >= 0 && kind < STRUCT_COUNT);

            Building* b = NULL;
            if (isGenerated)
            {
                if (gGeneratedCount >= MAX_GENERATED_BUILDINGS)
                    continue;
                b = &gGeneratedBuildings[gGeneratedCount];
            }
            else
            {
                if (gPlayerCount >= MAX_PLAYER_BUILDINGS)
                    continue;
                b = &gPlayerBuildings[gPlayerCount];
            }

            int buildingId = gNextBuildingId++;
            init_building_structure(b, buildingId, &res, kind);
            b->isGenerated = isGenerated && b->structureDef != NULL;

            collect_building_objects(map, b, &res, stamp, gVisitedStamp);

            const StructureDef* detected = analyze_building_type(b);
            if (detected)
            {
                b->structureDef   = detected;
                b->structureKind  = detected->kind;
                b->roomTypeId     = detected->roomId;
                b->auraRadius     = detected->auraRadius;
                b->auraIntensity  = detected->auraIntensity;
                snprintf(b->auraName, sizeof(b->auraName), "%s", detected->auraName);
                snprintf(b->auraDescription, sizeof(b->auraDescription), "%s", detected->auraDescription);
                b->occupantType = detected->occupantType;
                b->occupantMin  = detected->occupantMin;
                b->occupantMax  = detected->occupantMax;
                snprintf(b->occupantDescription, sizeof(b->occupantDescription), "%s", detected->occupantDescription);
                snprintf(b->triggerDescription, sizeof(b->triggerDescription), "%s", detected->triggerDescription);
                b->occupantCurrent = compute_structure_resident_count(detected, buildingId, &res);
                b->occupantActive  = 0;

                int inferredSpecies = 0;
                int inferredVillage = -1;
                infer_structure_metadata_from_markers(&res, &inferredSpecies, &inferredVillage);

                if (detected->species[0] != '\0')
                {
                    snprintf(b->species, sizeof(b->species), "%s", detected->species);
                    b->speciesId = detected->speciesId > 0 ? detected->speciesId : entity_species_id_from_label(detected->species);
                }
                else if (detected->speciesId > 0)
                {
                    b->speciesId = detected->speciesId;
                }
                else if (inferredSpecies > 0 && b->speciesId <= 0)
                {
                    b->speciesId = inferredSpecies;
                }

                if (inferredVillage >= 0)
                    b->villageId = inferredVillage;

                if (detected->name[0] != '\0')
                    snprintf(b->name, sizeof(b->name), "%s", detected->name);
                else if (b->name[0] == '\0')
                    snprintf(b->name, sizeof(b->name), "%s", structure_kind_to_string(detected->kind));

                register_building_with_metadata(map, b->bounds, detected->kind, b->speciesId, b->villageId);
            }
            else
            {
                if (!b->structureDef || b->name[0] == '\0')
                    snprintf(b->name, sizeof(b->name), "Unclassified Room");
                b->roomTypeId = ROOM_NONE;
            }

            if (b->isGenerated)
                gGeneratedCount++;
            else
                gPlayerCount++;
        }
    }
}

void register_building_from_bounds(Map* map, Rectangle bounds, StructureKind kind)
{
    register_building_with_metadata(map, bounds, kind, 0, -1);
}

void register_building_with_metadata(Map* map, Rectangle bounds, StructureKind kind, int speciesId, int villageId)
{
    (void)map;

    int ix = (int)bounds.x + 1;
    int iy = (int)bounds.y + 1;
    int iw = (int)bounds.width - 2;
    int ih = (int)bounds.height - 2;
    if (iw <= 0 || ih <= 0)
        return;

    for (int y = iy; y < iy + ih; ++y)
    {
        if (y < 0 || y >= MAP_HEIGHT)
            continue;
        for (int x = ix; x < ix + iw; ++x)
        {
            if (x < 0 || x >= MAP_WIDTH)
                continue;
            gStructureMarkers[y][x]    = kind;
            gStructureVillageIds[y][x] = villageId;
            gStructureSpeciesIds[y][x] = speciesId;
        }
    }
}
