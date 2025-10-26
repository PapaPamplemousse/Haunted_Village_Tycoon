#include "object.h"
#include "building.h"
#include "map.h"
#include "tile.h"
#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Static and constant global array containing all object type definitions.
// It uses the ObjectTypeID enumeration (e.g., [OBJ_BED_SMALL]) for indexing.
static ObjectType G_OBJECT_TYPES[OBJ_COUNT] = {
    // ----------------------------------------------------------------------------------------------------------------------------------------------------------------------
    // ID            Internal_ID     Display_Name    Category    MaxHP  Comfort  Warmth  LightLvl Width Height Walkable Flammable   Color (R,G,B,A)       Texture
    // ----------------------------------------------------------------------------------------------------------------------------------------------------------------------
    [OBJ_NONE] = {OBJ_NONE, "none", "None", "system", 0, 0, 0, 0, 0, 0, true, false, (Color){0, 0, 0, 0}, NULL, {0}}, // Default / Empty object.

    // --- Furniture ---
    [OBJ_BED_SMALL] = {OBJ_BED_SMALL, "bed_small", "Small Bed", "furniture", 50, 2, 0, 0, 1, 2, false, false, (Color){180, 0, 0, 255}, "assets/objects/furniture/single_bed.png", {0}},        // Small
                                                                                                                                                                                               // bed.
    [OBJ_BED_LARGE] = {OBJ_BED_LARGE, "bed_large", "Large Bed", "furniture", 100, 4, 0, 0, 2, 2, false, false, (Color){200, 0, 0, 255}, "assets/objects/furniture/double_bed.png", {0}},       // Large
                                                                                                                                                                                               // bed.
    [OBJ_TABLE_WOOD] = {OBJ_TABLE_WOOD, "table_wood", "Wooden Table", "furniture", 80, 1, 0, 0, 2, 2, false, true, (Color){139, 69, 19, 255}, "assets/objects/furniture/table_wood.png", {0}}, // Wooden table
                                                                                                                                                                                               // (flammable).
    [OBJ_CHAIR_WOOD] = {OBJ_CHAIR_WOOD, "chair_wood", "Wooden Chair", "furniture", 40, 1, 0, 0, 1, 1, false, true, (Color){139, 69, 19, 255}, "assets/objects/furniture/chair_wood.png", {0}}, // Wooden chair
                                                                                                                                                                                               // (flammable).

    // --- Utility ---
    [OBJ_TORCH_WALL] = {OBJ_TORCH_WALL, "torch_wall", "Wall Torch", "utility", 20, 0, 1, 3, 1, 1, true, true, (Color){255, 165, 0, 255}, "assets/objects/utility/torch_wall.png", {0}}, // Wall-mounted torch (light
                                                                                                                                                                                        // source, minor warmth).
    [OBJ_WORKBENCH] = {OBJ_WORKBENCH, "workbench", "Workbench", "utility", 90, 0, 0, 0, 2, 1, false, true, (Color){139, 69, 19, 255}, "assets/objects/utility/workbench.png", {0}},     // Workbench for crafting.

    // --- Storage ---
    [OBJ_CHEST_WOOD] = {OBJ_CHEST_WOOD, "chest_wood", "Wooden Chest", "storage", 60, 0, 0, 0, 2, 1, false, true, (Color){160, 82, 45, 255}, "assets/objects/storage/chest_wood.png", {0}}, // Wooden storage chest.

    // --- Structures ---
    [OBJ_DOOR_WOOD] = {OBJ_DOOR_WOOD, "door_wood", "Wooden Door", "structure", 100, 0, 0, 0, 1, 2, false, true, (Color){139, 69, 19, 255}, "assets/objects/structure/door_wood.png", {0}}, // Wooden door.
                                                                                                                                                                                           // for line-of-sight).
    [OBJ_WALL_STONE] =
        {OBJ_WALL_STONE, "wall_stone", "Stone Wall", "structure", 200, 0, 0, 0, 1, 1, false, false, (Color){100, 100, 100, 255}, "assets/objects/structure/wall_stone.png", {0}},        // Stone wall (highly durable).
    [OBJ_WALL_WOOD] = {OBJ_WALL_WOOD, "wall_wood", "Wood Wall", "structure", 150, 0, 0, 0, 1, 1, false, true, (Color){120, 80, 40, 255}, "assets/objects/structure/wall_wood.png", {0}}, // Wall segment, flammable.

    // --- Decoration ---
    [OBJ_DECOR_PLANT] =
        {OBJ_DECOR_PLANT, "decor_plant", "Potted Plant", "decoration", 15, 1, 0, 0, 1, 1, true, true, (Color){0, 200, 0, 255}, "assets/objects/decoration/decor_plant.png", {0}}, // Potted plant (decorative, walkable).

    // --- Ressources / Nature / Dangers (AJOUTS) ---
    // Note: 'Walkable' est l'inverse de 'solid'. Les objets 'solid=true' sont 'Walkable=false'.
    [OBJ_ROCK] = {OBJ_ROCK, "rock", "Rock", "resource", 300, 0, 0, 0, 1, 1, false, false, (Color){110, 110, 110, 255}, "assets/objects/resource/rock.png", {0}}, // Solid obstacle, harvestable resource.
    [OBJ_TREE] = {OBJ_TREE, "tree", "Tree", "resource", 250, 0, 0, 0, 1, 2, false, true, (Color){20, 120, 20, 255}, "assets/objects/resource/tree.png", {0}},    // Solid obstacle, flammable, harvestable wood.
    [OBJ_DEAD_TREE] =
        {OBJ_DEAD_TREE, "dead_tree", "Dead Tree", "resource", 180, 0, 0, 0, 1, 2, false, true, (Color){90, 70, 50, 255}, "assets/objects/resource/dead_tree.png", {0}},    // Solid obstacle, flammable, less resource.
    [OBJ_STDBUSH] = {OBJ_STDBUSH, "bush", "Bush", "resource", 50, 0, 0, 0, 1, 1, true, true, (Color){40, 140, 40, 255}, "assets/objects/resource/standard_bush.png", {0}}, // Walkable
                                                                                                                                                                           // (solid=false),
                                                                                                                                                                           // minor obstacle,
                                                                                                                                                                           // harvestable.
    [OBJ_STDBUSH_DRY] =
        {OBJ_STDBUSH_DRY, "bush_dry", "Dry Bush", "resource", 30, 0, 0, 0, 1, 1, true, true, (Color){150, 130, 60, 255}, "assets/objects/resource/dry_bush.png", {0}}, // Walkable (solid=false), highly flammable.
    [OBJ_SULFUR_VENT] = {OBJ_SULFUR_VENT, "sulfur_vent", "Sulfur Vent", "hazard", 150, -2, 2, 0, 1, 1, false, false, (Color){220, 200, 40, 255}, "assets/objects/hazard/sulfur_vent.png", {0}}, // Solid, low comfort, minor
                                                                                                                                                                                                // warmth, no light.
    [OBJ_BONE_PILE] = {OBJ_BONE_PILE, "bone_pile", "Bones", "decoration", 10, 0, 0, 0, 1, 1, true, false, (Color){200, 200, 200, 255}, "assets/objects/decoration/bone_pile.png", {0}},         // Walkable (solid=false),
                                                                                                                                                                                        // decoration/minor resource.

    [OBJ_CRATE]   = {OBJ_CRATE, "crate", "Crate", "storage", 70, 0, 0, 0, 1, 1, false, true, (Color){160, 110, 60, 255}, "assets/objects/storage/crate.png", {0}},                    // Solid obstacle, flammable.
    [OBJ_FIREPIT] = {OBJ_FIREPIT, "fireppit", "Exterior Fire Pit", "utility", 50, 0, 3, 2, 2, 2, false, true, (Color){255, 120, 20, 255}, "assets/objects/utility/firepit.png", {0}}, // Solid, warmth, light. (Attention au
    [OBJ_ALTAR]   = {OBJ_ALTAR, "altar", "Altar", "utility", 250, 0, 0, 0, 2, 1, false, false, (Color){180, 180, 220, 255}, "assets/objects/utility/altar.png", {0}},                 // Solid, non-flammable structure.

    // ----------------------------------------------------------------------------------------------------------------------------------------------------------------------
    // [OBJ_COUNT] End of table
    // ----------------------------------------------------------------------------------------------------------------------------------------------------------------------
};

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

static const ObjectRequirement ROOM_REQ_BEDROOM[]   = {{OBJ_BED_SMALL, 1} /*, {OBJ_TORCH_WALL, 1}*/};
static const ObjectRequirement ROOM_REQ_KITCHEN[]   = {{OBJ_TABLE_WOOD, 1}, {OBJ_CHAIR_WOOD, 2}, {OBJ_FIREPIT, 1}};
static const ObjectRequirement ROOM_REQ_HUT[]       = {{OBJ_FIREPIT, 1}, {OBJ_CRATE, 1}};
static const ObjectRequirement ROOM_REQ_CRYPT[]     = {{OBJ_ALTAR, 1}, {OBJ_BONE_PILE, 1}};
static const ObjectRequirement ROOM_REQ_SANCTUARY[] = {{OBJ_ALTAR, 1}, {OBJ_TORCH_WALL, 2}};
static const ObjectRequirement ROOM_REQ_HOUSE[]     = {{OBJ_BED_SMALL, 1}, {OBJ_TABLE_WOOD, 1}, {OBJ_CHAIR_WOOD, 1}};

static const RoomTypeRule ROOM_TYPE_RULES[] = {
    {"Bedroom", 2, 25, ROOM_REQ_BEDROOM, 2}, {"Kitchen", 6, 40, ROOM_REQ_KITCHEN, 3},       {"Large Room", 40, 0, NULL, 0},      {"Hut", 6, 36, ROOM_REQ_HUT, 2},
    {"Crypt", 9, 64, ROOM_REQ_CRYPT, 2},     {"Sanctuary", 12, 128, ROOM_REQ_SANCTUARY, 2}, {"House", 8, 49, ROOM_REQ_HOUSE, 3},
};
static const int ROOM_TYPE_RULE_COUNT = sizeof(ROOM_TYPE_RULES) / sizeof(ROOM_TYPE_RULES[0]);

const ObjectType* get_object_type(ObjectTypeID id)
{
    if (id <= OBJ_NONE || id >= OBJ_COUNT)
        return &G_OBJECT_TYPES[OBJ_NONE];
    return &G_OBJECT_TYPES[id];
}

// analsye all room ( with objects and size )
const RoomTypeRule* analyze_building_type(const Building* b)
{
    printf("\n[ANALYZE] Analyzing Building (Area: %d, Object Count: %d)\n", b->area, b->objectCount);
    for (int i = 0; i < ROOM_TYPE_RULE_COUNT; i++)
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

void init_object_textures(void)
{
    for (int i = 0; i < OBJ_COUNT; ++i)
    {
        if (G_OBJECT_TYPES[i].texturePath != NULL)
            G_OBJECT_TYPES[i].texture = LoadTexture(G_OBJECT_TYPES[i].texturePath);
    }
}

void unload_object_textures(void)
{
    for (int i = 0; i < OBJ_COUNT; ++i)
    {
        if (G_OBJECT_TYPES[i].texturePath != NULL)
            UnloadTexture(G_OBJECT_TYPES[i].texture);
    }
}
Object* create_object(ObjectTypeID id, int x, int y)
{
    Object* obj   = malloc(sizeof(Object));
    obj->type     = get_object_type(id);
    obj->position = (Vector2){x, y};
    obj->hp       = obj->type->maxHP;
    obj->isActive = true;
    // printf("Create object %d (%s)\n", id, get_object_type(id)->name);

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

    for (size_t i = 0; i < NUM_DOOR_IDS; ++i)
    {
        if (o->type->id == DOOR_IDS[i])
        {
            return true;
        }
    }

    return false;
}

bool is_door_object(const Object* o)
{
    if (!o)
    {
        return false;
    }

    for (size_t i = 0; i < NUM_WALL_IDS; ++i)
    {
        if (o->type->id == WALL_IDS[i])
        {
            return true;
        }
    }

    return false;
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
