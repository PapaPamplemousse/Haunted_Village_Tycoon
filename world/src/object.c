#include "object.h"
#include "object_loader.h"
#include "building.h"
#include "map.h"
#include "tile.h"
#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Static and constant global array containing all object type definitions.
// It uses the ObjectTypeID enumeration (e.g., [OBJ_BED_SMALL]) for indexing.
static ObjectType G_OBJECT_TYPES[OBJ_COUNT] = {0};

static const ObjectTypeID WALL_IDS[] = {
    OBJ_WALL_STONE, OBJ_WALL_WOOD,
    // OBJ_WALL_BRICK,
    // OBJ_WALL_ICE
};
static const size_t NUM_WALL_IDS = sizeof(WALL_IDS) / sizeof(WALL_IDS[0]);

static const ObjectTypeID DOOR_IDS[] = {
    OBJ_DOOR_WOOD,
    // OBJ_WALL_BRICK,
    // OBJ_WALL_ICE
};
static const size_t NUM_DOOR_IDS = sizeof(DOOR_IDS) / sizeof(DOOR_IDS[0]);

static RoomTypeRule ROOM_TYPE_RULES[ROOM_COUNT] = {0};

void init_objects(void)
{
    int objCount  = load_objects_from_stv("data/objects.stv", G_OBJECT_TYPES, OBJ_COUNT);
    int roomCount = load_rooms_from_stv("data/rooms.stv", ROOM_TYPE_RULES, ROOM_COUNT, G_OBJECT_TYPES, objCount);

    for (int i = 0; i < OBJ_COUNT; ++i)
    {
        if (G_OBJECT_TYPES[i].texturePath != NULL)
            G_OBJECT_TYPES[i].texture = LoadTexture(G_OBJECT_TYPES[i].texturePath);
    }
    debug_print_objects(G_OBJECT_TYPES, objCount);
    debug_print_rooms(ROOM_TYPE_RULES, roomCount, G_OBJECT_TYPES, objCount);
}

void unload_object_textures(void)
{
    for (int i = 0; i < OBJ_COUNT; ++i)
    {
        if (G_OBJECT_TYPES[i].texturePath != NULL)
            UnloadTexture(G_OBJECT_TYPES[i].texture);
    }
}

const ObjectType* get_object_type(ObjectTypeID id)
{
    // Sanity check
    if (id <= OBJ_NONE)
        return &G_OBJECT_TYPES[0]; // fallback on [OBJ_NONE]

    // Linear search (IDs are not guaranteed to be sequential anymore)
    for (int i = 0; i < OBJ_COUNT; ++i)
    {
        if (G_OBJECT_TYPES[i].id == id)
            return &G_OBJECT_TYPES[i];
    }

    // If not found, fallback
    return &G_OBJECT_TYPES[0];
}

// analsye all room ( with objects and size )
const RoomTypeRule* analyze_building_type(const Building* b)
{
    printf("\n[ANALYZE] Analyzing Building (Area: %d, Object Count: %d)\n", b->area, b->objectCount);
    for (int i = 0; i < ROOM_COUNT; i++)
    {
        const RoomTypeRule* rule = &ROOM_TYPE_RULES[i];

        if (b->area < rule->minArea)
            continue;
        if (rule->maxArea && b->area > rule->maxArea)
            continue;

        bool valid = true;
        for (int j = 0; j < rule->requirementCount; j++)
        {
            const ObjectRequirement* req   = &rule->requirements[j];
            int                      count = 0;
            printf("[ANALYZE] Checking requirement: %s, min: %d\n", get_object_type(req->objectId)->name, req->minCount);
            for (int k = 0; k < b->objectCount; k++)
            {
                if (b->objects[k]->type->id == req->objectId)
                    count++;
            }

            if (count < req->minCount)
            {
                valid = false;
                break;
            }
        }

        if (valid)
            return rule;
    }
    return NULL;
}

Object* create_object(ObjectTypeID id, int x, int y)
{
    Object* obj   = malloc(sizeof(Object));
    obj->type     = get_object_type(id);
    obj->position = (Vector2){x, y};
    obj->hp       = obj->type->maxHP;
    obj->isActive = true;
    return obj;
}

void draw_objects(Map* map, Camera2D* camera)
{
    Rectangle view = {.x      = camera->target.x - (GetScreenWidth() / 2) / camera->zoom,
                      .y      = camera->target.y - (GetScreenHeight() / 2) / camera->zoom,
                      .width  = GetScreenWidth() / camera->zoom,
                      .height = GetScreenHeight() / camera->zoom};

    int startX = (int)(view.x / TILE_SIZE) - 1;
    int startY = (int)(view.y / TILE_SIZE) - 1;
    int endX   = (int)((view.x + view.width) / TILE_SIZE) + 1;
    int endY   = (int)((view.y + view.height) / TILE_SIZE) + 1;

    for (int y = startY; y <= endY; y++)
    {
        for (int x = startX; x <= endX; x++)
        {
            int wx = (x % map->width + map->width) % map->width;
            int wy = (y % map->height + map->height) % map->height;

            Object* obj = map->objects[wy][wx];
            if (!obj)
                continue;

            const ObjectType* type   = obj->type;
            float             worldX = x * TILE_SIZE;
            float             worldY = y * TILE_SIZE;

            // --- If object had a texture ---
            if (type->texture.id != 0)
            {
                DrawTextureEx(type->texture, (Vector2){worldX, worldY}, 0.0f, (float)TILE_SIZE / type->texture.width, WHITE);

                // float scale  = (TILE_SIZE * 0.6f) / type->texture.width;
                // float offset = (TILE_SIZE - TILE_SIZE * 0.6f) / 2.0f;
                // Vector2 pos = {worldX + offset, worldY + offset};
                // DrawTextureEx(type->texture, pos, 0.0f, scale, WHITE);
            }
            else
            {
                // --- otherwise colored rectangle ---
                float size   = TILE_SIZE * 0.6f; // plus petit que la tuile
                float offset = (TILE_SIZE - size) / 2.0f;
                DrawRectangle(worldX + offset, worldY + offset, size, size, type->color);
            }
        }
    }
}

bool is_wall_object(const Object* o)
{
    if (!o)
    {
        return false;
    }
    return o->type->isWall;
}

bool is_door_object(const Object* o)
{
    if (!o)
    {
        return false;
    }
    return o->type->isDoor;
}

bool is_blocking_object(const Object* o)
{
    if (!o)
        return false;
    if (is_door_object(o))
        return false; // the door does not "block" the fill (it counts as a border)
    if (is_wall_object(o))
        return true; // mur = blocking
    return !o->type->walkable;
}
