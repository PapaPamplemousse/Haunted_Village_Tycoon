/**
 * @file ui.c
 * @brief Implements the in-game inventory panel used by the map editor.
 */

#include "ui.h"
#include "tile.h"
#include "object.h"
#include <raylib.h>
#include <string.h>
#include <math.h>

#define SLOT_SIZE 40
#define SLOT_MARGIN 6
#define MAX_SLOTS_PER_ROW 10
#define INVENTORY_TAB_COUNT 3

enum
{
    TAB_TILES    = 0,
    TAB_OBJECTS  = 1,
    TAB_ENTITIES = 2
};

static const char* TAB_NAMES[INVENTORY_TAB_COUNT] = {"Tiles", "Objects", "Entities"};

/** @brief Tracks whether the inventory overlay is visible. */
static bool inventoryOpen = false;
/** @brief Tracks the active tab (Tiles, Objects, Entities). */
static int inventoryTab = TAB_TILES;

void ui_toggle_inventory(void)
{
    inventoryOpen = !inventoryOpen;
}

bool ui_is_inventory_open(void)
{
    return inventoryOpen;
}

void ui_update_inventory(InputState* input, const EntitySystem* entities)
{
    if (!inventoryOpen)
        return;

    // Allow switching tabs with left/right to quickly browse assets.
    if (IsKeyPressed(KEY_LEFT))
        inventoryTab = (inventoryTab + INVENTORY_TAB_COUNT - 1) % INVENTORY_TAB_COUNT;
    if (IsKeyPressed(KEY_RIGHT))
        inventoryTab = (inventoryTab + 1) % INVENTORY_TAB_COUNT;

    Vector2 mouse = GetMousePosition();
    int     slots = 0;
    if (inventoryTab == TAB_TILES)
        slots = TILE_MAX;
    else if (inventoryTab == TAB_OBJECTS)
        slots = OBJ_COUNT;
    else if (inventoryTab == TAB_ENTITIES && entities)
        slots = entity_system_type_count(entities);

    int rows = (slots > 0) ? (slots + MAX_SLOTS_PER_ROW - 1) / MAX_SLOTS_PER_ROW : 1;

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();

    int panelW = SLOT_SIZE * MAX_SLOTS_PER_ROW + SLOT_MARGIN * (MAX_SLOTS_PER_ROW + 1);
    int panelH = SLOT_SIZE * rows + SLOT_MARGIN * (rows + 1) + 30;

    Rectangle panel = {(screenW - panelW) / 2.0f, (screenH - panelH) / 2.0f, (float)panelW, (float)panelH};

    for (int i = 0; i < slots; i++)
    {
        int row = i / MAX_SLOTS_PER_ROW;
        int col = i % MAX_SLOTS_PER_ROW;

        float x = panel.x + SLOT_MARGIN + col * (SLOT_SIZE + SLOT_MARGIN);
        float y = panel.y + 30 + SLOT_MARGIN + row * (SLOT_SIZE + SLOT_MARGIN);

        Rectangle slot = {x, y, SLOT_SIZE, SLOT_SIZE};
        if (CheckCollisionPointRec(mouse, slot) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (inventoryTab == TAB_TILES)
            {
                // Selecting a tile deselects any object placement.
                input->selectedTile   = (TileTypeID)i;
                input->selectedObject = OBJ_NONE;
                input->selectedEntity = ENTITY_TYPE_INVALID;
                input->currentMode    = MODE_TILE;
            }
            else if (inventoryTab == TAB_OBJECTS)
            {
                // Selecting an object ensures a sensible default tile is used.
                input->selectedTile   = TILE_GRASS;
                input->selectedObject = (ObjectTypeID)i;
                input->selectedEntity = ENTITY_TYPE_INVALID;
                input->currentMode    = MODE_OBJECT;
            }
            else if (inventoryTab == TAB_ENTITIES)
            {
                const EntityType* type = entity_system_type_at(entities, i);
                if (type)
                {
                    input->selectedTile   = TILE_MAX;
                    input->selectedObject = OBJ_NONE;
                    input->selectedEntity = type->id;
                    input->currentMode    = MODE_ENTITY;
                }
            }
        }
    }
}

void ui_draw_inventory(const InputState* input, const EntitySystem* entities)
{
    if (!inventoryOpen)
        return;

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();

    DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.5f));

    int slots = 0;
    if (inventoryTab == TAB_TILES)
        slots = TILE_MAX;
    else if (inventoryTab == TAB_OBJECTS)
        slots = OBJ_COUNT;
    else if (inventoryTab == TAB_ENTITIES && entities)
        slots = entity_system_type_count(entities);

    int rows = (slots > 0) ? (slots + MAX_SLOTS_PER_ROW - 1) / MAX_SLOTS_PER_ROW : 1;

    int panelW = SLOT_SIZE * MAX_SLOTS_PER_ROW + SLOT_MARGIN * (MAX_SLOTS_PER_ROW + 1);
    int panelH = SLOT_SIZE * rows + SLOT_MARGIN * (rows + 1) + 30;

    Rectangle panel = {(screenW - panelW) / 2.0f, (screenH - panelH) / 2.0f, (float)panelW, (float)panelH};

    DrawRectangleRec(panel, DARKGRAY);
    DrawText(TAB_NAMES[inventoryTab], panel.x + 10, panel.y + 5, 20, WHITE);

    if (inventoryTab == TAB_ENTITIES && slots == 0)
    {
        DrawText("No entities available", panel.x + 10, panel.y + 40, 14, LIGHTGRAY);
        return;
    }

    for (int i = 0; i < slots; i++)
    {
        int row = i / MAX_SLOTS_PER_ROW;
        int col = i % MAX_SLOTS_PER_ROW;

        float x = panel.x + SLOT_MARGIN + col * (SLOT_SIZE + SLOT_MARGIN);
        float y = panel.y + 30 + SLOT_MARGIN + row * (SLOT_SIZE + SLOT_MARGIN);

        Rectangle slot = {x, y, SLOT_SIZE, SLOT_SIZE};
        DrawRectangleRec(slot, GRAY);
        DrawRectangleLinesEx(slot, 2.0f, WHITE);

        Texture2D         tex;
        bool              hasTexture = false;
        const EntityType* type       = NULL;

        if (inventoryTab == TAB_TILES) // TILES
        {
            if (i < TILE_MAX)
            {
                const TileType* tile = (get_tile_type((TileTypeID)i));

                tex        = tile->texture;
                hasTexture = true;
            }
        }
        else if (inventoryTab == TAB_OBJECTS)
        {
            if (i > 0 && i < OBJ_COUNT)
            {
                const ObjectType* object = get_object_type((ObjectTypeID)i);
                tex                      = object->texture;
                hasTexture               = true;
            }
        }
        else if (inventoryTab == TAB_ENTITIES && entities)
        {
            type = entity_system_type_at(entities, i);
            if (type)
            {
                const EntitySprite* sprite = &type->sprite;
                if (sprite->texture.id != 0)
                {
                    tex        = sprite->texture;
                    hasTexture = true;
                }
                else
                {
                    float radius = SLOT_SIZE * 0.35f;
                    DrawCircle(x + SLOT_SIZE * 0.5f, y + SLOT_SIZE * 0.5f, radius, type->tint);
                }

                DrawText(type->displayName, (int)(x + 4), (int)(y + SLOT_SIZE - 12), 10, RAYWHITE);
            }
        }

        if (hasTexture && tex.id != 0)
        {
            float scale = fminf(SLOT_SIZE / (float)tex.width, SLOT_SIZE / (float)tex.height);
            float drawW = tex.width * scale;
            float drawH = tex.height * scale;
            float drawX = x + (SLOT_SIZE - drawW) / 2.0f;
            float drawY = y + (SLOT_SIZE - drawH) / 2.0f;

            DrawTextureEx(tex, (Vector2){drawX, drawY}, 0.0f, scale, WHITE);
        }
        bool isSelected = false;
        if (inventoryTab == TAB_TILES)
            isSelected = (i == input->selectedTile);
        else if (inventoryTab == TAB_OBJECTS)
            isSelected = (i == input->selectedObject);
        else if (inventoryTab == TAB_ENTITIES && entities)
        {
            if (type && input->selectedEntity == type->id)
                isSelected = true;
        }

        if (isSelected)
        {
            DrawRectangleLinesEx(slot, 3.0f, YELLOW);
        }
    }
    DrawText("Use ←/→ to change tabs", panel.x + 10, panel.y + panel.height - 18, 10, LIGHTGRAY);
}
