// ui.c
#include "ui.h"
#include "tile.h"
#include "object.h"
#include <raylib.h>
#include <string.h>
#include <math.h>

#define SLOT_SIZE 40
#define SLOT_MARGIN 6
#define MAX_SLOTS_PER_ROW 10

static bool inventoryOpen = false;
static int  inventoryTab  = 0; // 0 = Tiles, 1 = Objects

void ui_toggle_inventory(void)
{
    inventoryOpen = !inventoryOpen;
}

bool ui_is_inventory_open(void)
{
    return inventoryOpen;
}

void ui_update_inventory(InputState* input)
{
    if (!inventoryOpen)
        return;

    if (IsKeyPressed(KEY_LEFT))
        inventoryTab = (inventoryTab + 1) % 2;
    if (IsKeyPressed(KEY_RIGHT))
        inventoryTab = (inventoryTab + 1) % 2;

    Vector2 mouse = GetMousePosition();
    int     slots = (inventoryTab == 0) ? TILE_MAX : OBJ_COUNT;
    int     rows  = (slots + MAX_SLOTS_PER_ROW - 1) / MAX_SLOTS_PER_ROW;

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
            if (inventoryTab == 0)
            {
                input->selectedTile   = (TileTypeID)i;
                input->selectedObject = OBJ_NONE;
            }
            else
            {
                input->selectedTile   = TILE_GRASS;
                input->selectedObject = (ObjectTypeID)i;
            }
        }
    }
}

void ui_draw_inventory(const InputState* input)
{
    if (!inventoryOpen)
        return;

    int screenW = GetScreenWidth();
    int screenH = GetScreenHeight();

    DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.5f));

    int slots = (inventoryTab == 0) ? TILE_MAX : OBJ_COUNT;
    int rows  = (slots + MAX_SLOTS_PER_ROW - 1) / MAX_SLOTS_PER_ROW;

    int panelW = SLOT_SIZE * MAX_SLOTS_PER_ROW + SLOT_MARGIN * (MAX_SLOTS_PER_ROW + 1);
    int panelH = SLOT_SIZE * rows + SLOT_MARGIN * (rows + 1) + 30;

    Rectangle panel = {(screenW - panelW) / 2.0f, (screenH - panelH) / 2.0f, (float)panelW, (float)panelH};

    DrawRectangleRec(panel, DARKGRAY);
    DrawText(inventoryTab == 0 ? "Tiles" : "Objects", panel.x + 10, panel.y + 5, 20, WHITE);

    for (int i = 0; i < slots; i++)
    {
        int row = i / MAX_SLOTS_PER_ROW;
        int col = i % MAX_SLOTS_PER_ROW;

        float x = panel.x + SLOT_MARGIN + col * (SLOT_SIZE + SLOT_MARGIN);
        float y = panel.y + 30 + SLOT_MARGIN + row * (SLOT_SIZE + SLOT_MARGIN);

        Rectangle slot = {x, y, SLOT_SIZE, SLOT_SIZE};
        DrawRectangleRec(slot, GRAY);
        DrawRectangleLinesEx(slot, 2.0f, WHITE);

        Texture2D tex;
        bool      hasTexture = false;

        if (inventoryTab == 0) // TILES
        {
            if (i < TILE_MAX)
            {
                const TileType* tile = (get_tile_type((TileTypeID)i));

                tex        = tile->texture;
                hasTexture = true;
            }
        }
        else // OBJECTS
        {
            if (i > 0 && i < OBJ_COUNT)
            {
                const ObjectType* object = get_object_type((TileTypeID)i);
                tex                      = object->texture;
                hasTexture               = true;
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
        bool isSelected = (inventoryTab == 0 && i == input->selectedTile) || (inventoryTab == 1 && i == input->selectedObject);

        if (isSelected)
        {
            DrawRectangleLinesEx(slot, 3.0f, YELLOW);
        }
    }
}
