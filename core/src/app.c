#include <stdio.h>
#include "input.h"
#include "editor.h"
#include "map.h"
#include "camera.h"
#include "tile.h"
#include "building.h"
#include "object.h"
#include "world.h"
#include <stdint.h>
#include "world_chunk.h"
static Map G_MAP = {0};

void app_run(void)
{
    int      screenWidth  = 1280;
    int      screenHeight = 720;
    uint64_t seed         = 0xA1B2C3D4u;

    InitWindow(screenWidth, screenHeight, "Containment Tycoon (Top-Down)");
    SetTargetFPS(60);

    init_tile_types();
    init_object_textures();

    map_init(&G_MAP, seed);
    update_building_detection(&G_MAP);

    gChunks = chunkgrid_create(&G_MAP);
    // chunkgrid_mark_all(gChunks, &G_MAP);

    InputState input;
    input_init(&input);

    Camera2D camera = init_camera();

    while (!WindowShouldClose())
    {
        input_update(&input);
        update_camera(&camera, &input.camera);

        bool changed = editor_update(&G_MAP, &camera, &input);
        if (changed)
            update_building_detection(&G_MAP);

        BeginDrawing();
        ClearBackground(BLACK);
        // chunkgrid_rebuild_dirty(gChunks, &G_MAP);
        BeginMode2D(camera);

        chunkgrid_draw_visible(gChunks, &G_MAP, &camera); // draw static

        // draw_map(&G_MAP, &camera);
        // draw_objects(&G_MAP, &camera);

        for (int i = 0; i < buildingCount; i++)
        {
            Building* b     = &buildings[i];
            int       textX = (int)(b->center.x * TILE_SIZE);
            int       textY = (int)(b->center.y * TILE_SIZE) - 5;
            if (textY < 0)
                textY = 0;

            char label[64];
            if (input.showBuildingNames)
                snprintf(label, sizeof(label), "%d: %s", b->id, b->name);
            else
                snprintf(label, sizeof(label), "%d", b->id);

            DrawText(label, textX, textY, 10, WHITE);
        }

        EndMode2D();
        chunkgrid_evict_far(gChunks, &camera, 3000.0f); // unload chunks farther than 3000 px

        EndDrawing();
    }

    unload_tile_types();
    unload_object_textures();
    map_unload(&G_MAP);
    chunkgrid_destroy(gChunks);
    gChunks = NULL;

    CloseWindow();
}
