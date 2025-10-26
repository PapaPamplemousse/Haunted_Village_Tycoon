#include <stdio.h>
#include <stdint.h>
#include "raylib.h"

#include "input.h"
#include "ui.h"
#include "editor.h"
#include "map.h"
#include "camera.h"
#include "tile.h"
#include "building.h"
#include "object.h"
#include "world.h"
#include "world_chunk.h"
#include "debug.h"

// -----------------------------------------------------------------------------
// Global world data
// -----------------------------------------------------------------------------
static Map        G_MAP    = {0};
static Camera2D   G_CAMERA = {0};
static InputState G_INPUT  = {0};
// ChunkGrid*        gChunks  = NULL;

// -----------------------------------------------------------------------------
// Local helpers
// -----------------------------------------------------------------------------

static void app_init(void)
{
    const int      screenWidth  = 1280;
    const int      screenHeight = 720;
    const uint64_t seed         = 0xA1B2C3D4u;

    // SetConfigFlags(FLAG_FULLSCREEN_MODE);
    InitWindow(screenWidth, screenHeight, "Containment Tycoon (Top-Down)");
    SetTargetFPS(40);

    init_tile_types();
    init_objects();

    map_init(&G_MAP, seed);
    update_building_detection(&G_MAP);

    gChunks  = chunkgrid_create(&G_MAP);
    G_CAMERA = init_camera();
    input_init(&G_INPUT);
}

static void app_update(void)
{
    input_update(&G_INPUT);
    ui_update_inventory(&G_INPUT);
    update_camera(&G_CAMERA, &G_INPUT.camera);

    bool changed = editor_update(&G_MAP, &G_CAMERA, &G_INPUT);
    if (changed)
    {
        update_building_detection(&G_MAP);
        // chunkgrid_mark_all(gChunks, &G_MAP);
    }
}

static void app_draw_world(void)
{
    BeginMode2D(G_CAMERA);

    // Draw static geometry (tiles + static objects)
    chunkgrid_draw_visible(gChunks, &G_MAP, &G_CAMERA);

    // --- Mouse highlight ---
    MouseState mouse;
    input_update_mouse(&mouse, &G_CAMERA, &G_MAP);

    if (mouse.insideMap)
    {
        Rectangle highlight = {(float)mouse.tileX * TILE_SIZE, (float)mouse.tileY * TILE_SIZE, TILE_SIZE, TILE_SIZE};
        DrawRectangleLinesEx(highlight, 2.0f, YELLOW);
    }

    // --- Building labels ---
    for (int i = 0; i < buildingCount; i++)
    {
        Building* b     = &buildings[i];
        int       textX = (int)(b->center.x * TILE_SIZE);
        int       textY = (int)(b->center.y * TILE_SIZE) - 5;
        if (textY < 0)
            textY = 0;

        char label[128];
        if (G_INPUT.showBuildingNames)
            snprintf(label, sizeof(label), "%d: %s", b->id, b->name);
        else
            snprintf(label, sizeof(label), "%d", b->id);

        DrawText(label, textX, textY, 10, WHITE);
    }

    EndMode2D();
    // Debug biome overlay
    static bool showBiomeDebug = false;
    debug_biome_draw(&G_MAP, &G_CAMERA, &showBiomeDebug);

    // Optional: draw current tile/object selection
    ui_draw_inventory(&G_INPUT);
}

// Called periodically to unload far chunks
static void app_handle_chunk_eviction(void)
{
    static float evictTimer = 0.0f;
    evictTimer += GetFrameTime();

    // Every few seconds to avoid churn
    if (evictTimer > 10.0f)
    {
        chunkgrid_evict_far(gChunks, &G_CAMERA, 5000.0f);
        evictTimer = 0.0f;
    }
}

static void app_cleanup(void)
{
    unload_tile_types();
    unload_object_textures();
    map_unload(&G_MAP);
    chunkgrid_destroy(gChunks);
    gChunks = NULL;

    CloseWindow();
}

// -----------------------------------------------------------------------------
// Main loop
// -----------------------------------------------------------------------------
void app_run(void)
{
    app_init();

    while (!WindowShouldClose())
    {
        app_update();

        BeginDrawing();
        ClearBackground(BLACK);

        app_draw_world();
        app_handle_chunk_eviction();

        EndDrawing();
    }

    app_cleanup();
}
