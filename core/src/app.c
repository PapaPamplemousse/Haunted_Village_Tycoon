/**
 * @file app.c
 * @brief Implements the main application loop and orchestrates core systems.
 */

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
#include "entity.h"
// -----------------------------------------------------------------------------
// Global world data
// -----------------------------------------------------------------------------
static Map          G_MAP      = {0};
static Camera2D     G_CAMERA   = {0};
static InputState   G_INPUT    = {0};
static EntitySystem G_ENTITIES = {0};
// ChunkGrid*        gChunks  = NULL;

// -----------------------------------------------------------------------------
// Local helpers
// -----------------------------------------------------------------------------

/**
 * @brief Initializes the rendering context and all gameplay systems.
 */
static void app_init(void)
{
    const int      screenWidth  = 1280;
    const int      screenHeight = 720;
    const uint64_t seed         = 0xA1B2C3D4u;

    // Prepare the rendering window and the frame pacing.
    // SetConfigFlags(FLAG_FULLSCREEN_MODE);
    InitWindow(screenWidth, screenHeight, "Containment Tycoon (Top-Down)");
    SetTargetFPS(40);

    // Load static resources such as tiles and placeable objects.
    init_tile_types();
    init_objects();

    // Build the world and load entity definitions.
    map_init(&G_MAP, seed);
    update_building_detection(&G_MAP);
    if (!entity_system_init(&G_ENTITIES, &G_MAP, seed ^ 0x13572468u, "data/entities.stv"))
        TraceLog(LOG_WARNING, "Entity definitions failed to load, using built-in defaults.");

    // Set up world chunk streaming, the camera and initial input state.
    gChunks  = chunkgrid_create(&G_MAP);
    G_CAMERA = init_camera();
    input_init(&G_INPUT);
}

/**
 * @brief Polls input and advances the simulation by one frame.
 */
static void app_update(void)
{
    // Gather user input for the frame and update UI state that depends on it.
    input_update(&G_INPUT);
    ui_update_inventory(&G_INPUT, &G_ENTITIES);
    update_camera(&G_CAMERA, &G_INPUT.camera);

    // Advance gameplay systems using the frame time delta.
    float dt = GetFrameTime();
    entity_system_update(&G_ENTITIES, &G_MAP, dt);
    bool changed = editor_update(&G_MAP, &G_CAMERA, &G_INPUT, &G_ENTITIES);
    if (changed)
    {
        update_building_detection(&G_MAP);
        // chunkgrid_mark_all(gChunks, &G_MAP);
    }
}

/**
 * @brief Renders the world and overlay information for the current frame.
 */
static void app_draw_world(void)
{
    BeginMode2D(G_CAMERA);

    // Draw static geometry (tiles + static objects)
    chunkgrid_draw_visible(gChunks, &G_MAP, &G_CAMERA);
    entity_system_draw(&G_ENTITIES);

    // --- Mouse highlight ---
    MouseState mouse;
    input_update_mouse(&mouse, &G_CAMERA, &G_MAP);

    if (mouse.insideMap)
    {
        Rectangle highlight = {(float)mouse.tileX * TILE_SIZE, (float)mouse.tileY * TILE_SIZE, TILE_SIZE, TILE_SIZE};
        DrawRectangleLinesEx(highlight, 2.0f, YELLOW);
    }

    // --- Building labels ---
    int totalBuildings = building_total_count();
    for (int i = 0; i < totalBuildings; i++)
    {
        const Building* b = building_get(i);
        if (!b)
            continue;

        int textX = (int)(b->center.x * TILE_SIZE);
        int textY = (int)(b->center.y * TILE_SIZE) - 5;
        if (textY < 0)
            textY = 0;

        if (G_INPUT.showBuildingNames)
        {
            const char* displayName = (b->name[0] != '\0') ? b->name : "Structure";
            char        header[128];
            snprintf(header, sizeof(header), "#%d %s", b->id, displayName);
            DrawText(header, textX, textY, 12, WHITE);

            int infoY = textY + 14;
            if (b->structureDef)
            {
                if (b->auraName[0])
                {
                    char auraLine[160];
                    snprintf(auraLine,
                             sizeof(auraLine),
                             "Aura: %s (r=%.1f, pwr=%.1f)",
                             b->auraName,
                             b->auraRadius,
                             b->auraIntensity);
                    DrawText(auraLine, textX, infoY, 10, ColorAlpha(WHITE, 0.85f));
                    infoY += 12;

                    if (b->auraDescription[0])
                    {
                        DrawText(b->auraDescription, textX, infoY, 9, ColorAlpha(WHITE, 0.7f));
                        infoY += 11;
                    }
                }

                if (b->occupantType > ENTITY_TYPE_INVALID && b->occupantMax > 0)
                {
                    char occLine[160];
                    snprintf(occLine,
                             sizeof(occLine),
                             "Residents: %d (min %d, max %d) %s",
                             b->occupantCurrent,
                             b->occupantMin,
                             b->occupantMax,
                             b->occupantDescription[0] ? b->occupantDescription : "residents");
                    DrawText(occLine, textX, infoY, 10, ColorAlpha(WHITE, 0.9f));
                    infoY += 12;
                }

                if (b->triggerDescription[0])
                {
                    DrawText(b->triggerDescription, textX, infoY, 10, ColorAlpha(WHITE, 0.8f));
                    infoY += 12;
                }
            }
        }
        else
        {
            char label[32];
            snprintf(label, sizeof(label), "%d", b->id);
            DrawText(label, textX, textY, 10, WHITE);
        }
    }

    EndMode2D();

    // Draw optional overlays such as biome debug view and the build inventory.
    static bool showBiomeDebug = false;
    debug_biome_draw(&G_MAP, &G_CAMERA, &showBiomeDebug);

    // Optional: draw current tile/object selection
    ui_draw_inventory(&G_INPUT, &G_ENTITIES);
}

/**
 * @brief Periodically unloads far-away chunks to keep memory usage under control.
 */
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

/**
 * @brief Releases all resources acquired during initialization.
 */
static void app_cleanup(void)
{
    unload_tile_types();
    unload_object_textures();
    entity_system_shutdown(&G_ENTITIES);
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
        // Advance the simulation and render the current frame.
        app_update();

        BeginDrawing();
        ClearBackground(BLACK);

        app_draw_world();
        app_handle_chunk_eviction();

        EndDrawing();
    }

    app_cleanup();
}
