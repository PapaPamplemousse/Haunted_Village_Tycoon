/**
 * @file app.c
 * @brief Implements the main application loop and orchestrates core systems.
 */

#include <math.h>
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
static bool         G_BUILDING_DIRTY       = false;
static Rectangle    G_BUILDING_DIRTY_BBOX  = {0};

// -----------------------------------------------------------------------------
// Local helpers
// -----------------------------------------------------------------------------

static bool rect_is_empty(Rectangle r)
{
    return r.width <= 0.0f || r.height <= 0.0f;
}

static bool rects_overlap(Rectangle a, Rectangle b)
{
    if (rect_is_empty(a) || rect_is_empty(b))
        return false;

    return (a.x < b.x + b.width) && (a.x + a.width > b.x) && (a.y < b.y + b.height) && (a.y + a.height > b.y);
}

static Rectangle rect_union(Rectangle a, Rectangle b)
{
    if (rect_is_empty(a))
        return b;
    if (rect_is_empty(b))
        return a;

    float minX = fminf(a.x, b.x);
    float minY = fminf(a.y, b.y);
    float maxX = fmaxf(a.x + a.width, b.x + b.width);
    float maxY = fmaxf(a.y + a.height, b.y + b.height);

    Rectangle result = {
        .x      = minX,
        .y      = minY,
        .width  = maxX - minX,
        .height = maxY - minY,
    };
    return result;
}

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
    Rectangle fullRegion = {
        .x      = 0.0f,
        .y      = 0.0f,
        .width  = (float)(G_MAP.width * TILE_SIZE),
        .height = (float)(G_MAP.height * TILE_SIZE),
    };
    update_building_detection(&G_MAP, fullRegion);
    G_BUILDING_DIRTY      = false;
    G_BUILDING_DIRTY_BBOX = (Rectangle){0.0f, 0.0f, 0.0f, 0.0f};
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
    entity_system_update(&G_ENTITIES, &G_MAP, &G_CAMERA, dt);

    Rectangle dirtyWorld = {0.0f, 0.0f, 0.0f, 0.0f};
    bool      changed    = editor_update(&G_MAP, &G_CAMERA, &G_INPUT, &G_ENTITIES, &dirtyWorld);
    if (changed)
    {
        if (G_BUILDING_DIRTY)
            G_BUILDING_DIRTY_BBOX = rect_union(G_BUILDING_DIRTY_BBOX, dirtyWorld);
        else
        {
            G_BUILDING_DIRTY_BBOX = dirtyWorld;
            G_BUILDING_DIRTY      = true;
        }
    }

    float     viewWidth  = GetScreenWidth() / G_CAMERA.zoom;
    float     viewHeight = GetScreenHeight() / G_CAMERA.zoom;
    Rectangle worldView  = {
         .x      = G_CAMERA.target.x - viewWidth * 0.5f,
         .y      = G_CAMERA.target.y - viewHeight * 0.5f,
         .width  = viewWidth,
         .height = viewHeight,
    };

    Rectangle paddedView = worldView;
    paddedView.x -= TILE_SIZE;
    paddedView.y -= TILE_SIZE;
    paddedView.width += TILE_SIZE * 2.0f;
    paddedView.height += TILE_SIZE * 2.0f;

    if (G_BUILDING_DIRTY && rects_overlap(G_BUILDING_DIRTY_BBOX, paddedView))
    {
        update_building_detection(&G_MAP, paddedView);
        G_BUILDING_DIRTY      = false;
        G_BUILDING_DIRTY_BBOX = (Rectangle){0.0f, 0.0f, 0.0f, 0.0f};
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
    float     viewWidth  = GetScreenWidth() / G_CAMERA.zoom;
    float     viewHeight = GetScreenHeight() / G_CAMERA.zoom;
    Rectangle worldView = {
        .x      = G_CAMERA.target.x - viewWidth * 0.5f,
        .y      = G_CAMERA.target.y - viewHeight * 0.5f,
        .width  = viewWidth,
        .height = viewHeight,
    };
    float viewMinX = worldView.x - TILE_SIZE;
    float viewMaxX = worldView.x + worldView.width + TILE_SIZE;
    float viewMinY = worldView.y - TILE_SIZE;
    float viewMaxY = worldView.y + worldView.height + TILE_SIZE;

    int totalBuildings = building_total_count();
    for (int i = 0; i < totalBuildings; i++)
    {
        const Building* b = building_get(i);
        if (!b)
            continue;

        float centerX = b->center.x * TILE_SIZE;
        float centerY = b->center.y * TILE_SIZE;
        if (centerX < viewMinX || centerX > viewMaxX || centerY < viewMinY || centerY > viewMaxY)
            continue;

        int textX = (int)centerX;
        int textY = (int)centerY - 5;
        if (textY < 0)
            textY = 0;

        if (G_INPUT.showBuildingNames)
        {
            const char* displayName = (b->name[0] != '\0') ? b->name : "Structure";
            DrawText(displayName, textX, textY, 12, WHITE);

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
                             "Residents: %d/%d (min %d, max %d) %s",
                             b->occupantActive,
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
