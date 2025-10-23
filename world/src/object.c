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
static const ObjectType OBJECT_TYPES[OBJ_MAX] = {
    //                                                                           // OBJECT FIELDS
    //                                                                           // -----------
    // ID                     Internal_ID     Display_Name     Category     MaxHP  Comfort  Warmth  LightLvl Width Height Walkable Flammable   Color (R,G,B,A)      Texture
    // ----------------------------------------------------------------------------------------------------------------------------------------------------------------------
    [OBJ_NONE] = {OBJ_NONE, "none", "None", "system", 0, 0, 0, 0, 0, 0, true, false, (Color){0, 0, 0, 0}, {0}}, // Default / Empty object.

    // --- Furniture ---
    [OBJ_BED_SMALL]  = {OBJ_BED_SMALL, "bed_small", "Small Bed", "furniture", 50, 2, 0, 0, 1, 2, false, false, (Color){180, 0, 0, 255}, {0}},       // Small bed.
    [OBJ_BED_LARGE]  = {OBJ_BED_LARGE, "bed_large", "Large Bed", "furniture", 100, 4, 0, 0, 2, 2, false, false, (Color){200, 0, 0, 255}, {0}},      // Large bed.
    [OBJ_TABLE_WOOD] = {OBJ_TABLE_WOOD, "table_wood", "Wooden Table", "furniture", 80, 1, 0, 0, 2, 2, false, true, (Color){139, 69, 19, 255}, {0}}, // Wooden table (flammable).
    [OBJ_CHAIR_WOOD] = {OBJ_CHAIR_WOOD, "chair_wood", "Wooden Chair", "furniture", 40, 1, 0, 0, 1, 1, false, true, (Color){139, 69, 19, 255}, {0}}, // Wooden chair (flammable).

    // --- Utility ---
    [OBJ_TORCH_WALL] =
        {OBJ_TORCH_WALL, "torch_wall", "Wall Torch", "utility", 20, 0, 1, 3, 1, 1, true, true, (Color){255, 165, 0, 255}, {0}}, // Wall-mounted torch (light source, minor warmth).
    [OBJ_FIRE_PIT]  = {OBJ_FIRE_PIT, "fire_pit", "Fire Pit", "utility", 30, 0, 4, 2, 2, 2, false, true, (Color){255, 80, 0, 255}, {0}}, // Fire pit (major warmth, light source).
    [OBJ_WORKBENCH] = {OBJ_WORKBENCH, "workbench", "Workbench", "utility", 90, 0, 0, 0, 2, 1, false, true, (Color){139, 69, 19, 255}, {0}}, // Workbench for crafting.

    // --- Storage ---
    [OBJ_CHEST_WOOD] = {OBJ_CHEST_WOOD, "chest_wood", "Wooden Chest", "storage", 60, 0, 0, 0, 2, 1, false, true, (Color){160, 82, 45, 255}, {0}}, // Wooden storage chest.

    // --- Structures ---
    [OBJ_DOOR_WOOD]   = {OBJ_DOOR_WOOD, "door_wood", "Wooden Door", "structure", 100, 0, 0, 0, 1, 2, false, true, (Color){139, 69, 19, 255}, {0}},       // Wooden door.
    [OBJ_WINDOW_WOOD] = {OBJ_WINDOW_WOOD, "window_wood", "Wooden Window", "structure", 50, 0, 0, 0, 1, 1, true, true, (Color){222, 184, 135, 255}, {0}}, // Wooden window (walkable
                                                                                                                                                         // for line-of-sight).
    [OBJ_WALL_STONE] =
        {OBJ_WALL_STONE, "wall_stone", "Stone Wall", "structure", 200, 0, 0, 0, 1, 1, false, false, (Color){100, 100, 100, 255}, {0}}, // Stone wall (highly durable).

    // --- Decoration ---
    [OBJ_DECOR_PLANT] = {OBJ_DECOR_PLANT, "decor_plant", "Potted Plant", "decoration", 15, 1, 0, 0, 1, 1, true, true, (Color){0, 200, 0, 255}, {0}}, // Potted plant (decorative,
                                                                                                                                                     // can be walked over).
};

const ObjectType* get_object_type(ObjectTypeID id)
{
    if (id <= OBJ_NONE || id >= OBJ_MAX)
        return &OBJECT_TYPES[OBJ_NONE];
    return &OBJECT_TYPES[id];
}

// Exemples de règles pour définir des pièces
static const ObjectRequirement ROOM_REQ_BEDROOM[] = {{OBJ_BED_SMALL, 1} /*, {OBJ_TORCH_WALL, 1}*/};
static const ObjectRequirement ROOM_REQ_KITCHEN[] = {{OBJ_TABLE_WOOD, 1}, {OBJ_CHAIR_WOOD, 2}, {OBJ_FIRE_PIT, 1}};

static const RoomTypeRule ROOM_TYPE_RULES[] = {
    {"Bedroom", 2, 25, ROOM_REQ_BEDROOM, 2},
    {"Kitchen", 6, 40, ROOM_REQ_KITCHEN, 3},
    {"Large Room", 40, 0, NULL, 0},
};
static const int ROOM_TYPE_RULE_COUNT = sizeof(ROOM_TYPE_RULES) / sizeof(ROOM_TYPE_RULES[0]);

// Analyse le type de pièce selon les objets et la taille
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
            else
            {
                printf("ici \n");
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
    printf("Create object %d (%s)\n", id, get_object_type(id)->name);

    return obj;
}

void draw_objects(Camera2D* camera)
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
            int wx = (x % G_MAP.width + G_MAP.width) % G_MAP.width;
            int wy = (y % G_MAP.height + G_MAP.height) % G_MAP.height;

            Object* obj = G_MAP.objects[wy][wx];
            if (!obj)
                continue;

            const ObjectType* type   = obj->type;
            float             worldX = x * TILE_SIZE;
            float             worldY = y * TILE_SIZE;

            // --- Si l'objet a une texture ---
            if (type->texture.id != 0)
            {
                DrawTextureEx(type->texture, (Vector2){worldX, worldY}, 0.0f, (float)TILE_SIZE / type->texture.width, WHITE);
            }
            else
            {
                // --- Sinon on le dessine comme un carré coloré ---
                float size   = TILE_SIZE * 0.6f; // plus petit que la tuile
                float offset = (TILE_SIZE - size) / 2.0f;
                DrawRectangle(worldX + offset, worldY + offset, size, size, type->color);
            }
        }
    }
}

bool is_wall_object(const Object* o)
{
    return o && (o->type->id == OBJ_DOOR_WOOD);
}

bool is_door_object(const Object* o)
{
    return o && (o->type->id == OBJ_WALL_STONE);
}

bool is_blocking_object(const Object* o)
{
    if (!o)
        return false;
    if (is_door_object(o))
        return false; // la porte ne "bloque" pas le fill (elle compte comme bordure)
    if (is_wall_object(o))
        return true; // mur = bloquant
    return !o->type->walkable;
}
