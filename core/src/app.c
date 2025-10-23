#include <raylib.h>
#include <stdio.h>
#include "map.h"
#include "camera.h"
#include "tile.h"
#include "building.h"
#include "object.h"

void app_run(void)
{
    int screenWidth  = 1280; // MAP_WIDTH * TILE_SIZE;
    int screenHeight = 720;  // MAP_HEIGHT * TILE_SIZE;
    InitWindow(screenWidth, screenHeight, "Containment Tycoon (Top‑Down)");
    SetTargetFPS(60);

    // Init ressources
    init_tile_types();
    map_init();
    update_building_detection();

    Camera2D camera = init_camera();

    while (!WindowShouldClose())
    {
        update_camera(&camera);
        bool changed = update_map(&camera);
        if (changed)
        {
            update_building_detection();
        }

        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode2D(camera);

        draw_map(&camera);
        draw_objects(&camera);

        for (int i = 0; i < buildingCount; i++)
        {
            Building* b = &buildings[i];
            // Convert center (in tile coords) to screen coords (pixel), offset to center of tile
            int textX = (int)(b->center.x * TILE_SIZE);
            int textY = (int)(b->center.y * TILE_SIZE);
            // Adjust text position a bit above center (optional)
            textY -= 5;
            if (textY < 0)
                textY = 0;
            // Prepare label text
            char label[64];
            if (IsKeyDown(KEY_TAB))
            {
                // Show full name with ID
                snprintf(label, sizeof(label), "%d: %s", b->id, b->name);
            }
            else
            {
                // Show only ID
                snprintf(label, sizeof(label), "%d", b->id);
            }
            // Choose text color (white for clarity against dark background)
            Color textColor = WHITE;
            DrawText(label, textX, textY, 10, textColor);
        }
        EndMode2D();
        EndDrawing();
    }

    // Unload ressources
    unload_tile_types();
    map_unload();
    CloseWindow();
}
