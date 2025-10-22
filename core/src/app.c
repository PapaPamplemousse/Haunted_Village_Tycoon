#include <raylib.h>
#include "map.h"
#include "camera.h"
#include "tile.h"

void app_run(void)
{
    InitWindow(1280, 720, "Containment Tycoon (Top‑Down)");
    SetTargetFPS(60);

    // Init ressources
    init_tile_types();
    map_init();

    Camera2D camera = init_camera();

    while (!WindowShouldClose())
    {
        update_camera(&camera);
        update_map(&camera);

        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode2D(camera);

        draw_map(&camera);

        EndMode2D();
        EndDrawing();
    }

    // Unload ressources
    unload_tile_types();
    map_unload();
    CloseWindow();
}
