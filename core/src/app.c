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
#include "ui_theme.h"
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
#include "world_time.h"
#include "music.h"
#include "world_structures.h"
#include "localization.h"
// -----------------------------------------------------------------------------
// Global world data
// -----------------------------------------------------------------------------
static Map          G_MAP        = {0};
static Camera2D     G_CAMERA     = {0};
static InputState   G_INPUT      = {0};
static EntitySystem G_ENTITIES   = {0};
static WorldTime    G_WORLD_TIME = {0};
// ChunkGrid*        gChunks  = NULL;
static bool      G_BUILDING_DIRTY      = false;
static Rectangle G_BUILDING_DIRTY_BBOX = {0};

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

static const char* fallback_text(const char* text)
{
    return (text && text[0] != '\0') ? text : NULL;
}

static StructureKind resolve_structure_kind(const Building* building)
{
    if (!building)
        return STRUCT_COUNT;
    if (building->structureDef)
        return building->structureDef->kind;
    return building->structureKind;
}

static const char* localized_structure_field(StructureKind kind, const char* field, const char* fallback)
{
    if (kind >= 0 && kind < STRUCT_COUNT)
    {
        const char* token = structure_kind_to_string(kind);
        if (token && token[0] != '\0')
        {
            char key[128];
            snprintf(key, sizeof(key), "structure.%s.%s", token, field);
            const char* value = localization_try(key);
            if (value)
                return value;
        }
    }
    return fallback;
}

static const char* building_display_name(const Building* building)
{
    const char* fallback = NULL;
    if (building)
    {
        if (building->name[0] != '\0')
            fallback = building->name;
        else if (building->structureDef && building->structureDef->name[0] != '\0')
            fallback = building->structureDef->name;
    }

    const char* text = localized_structure_field(resolve_structure_kind(building), "name", fallback);
    if (text && text[0] != '\0')
        return text;
    return localization_get("structure.generic");
}

/**
 * @brief Initializes the rendering context and all gameplay systems.
 */
static void app_init(void)
{
    const int      screenWidth  = 1280;
    const int      screenHeight = 720;
    const uint64_t seed         = 0x12042023; // 0xA1B2C3D4u;

    if (!localization_init(NULL))
        TraceLog(LOG_WARNING, "Localization system failed to initialize, falling back to keys.");

    // Prepare the rendering window and the frame pacing.
    // SetConfigFlags(FLAG_FULLSCREEN_MODE);
    InitWindow(screenWidth, screenHeight, "Containment Tycoon (Top-Down)");
    SetExitKey(KEY_NULL);
    SetTargetFPS(40);

    // Load static resources such as tiles and placeable objects.
    init_tile_types();
    init_objects();

    // Build the world and load entity definitions.
    map_init(&G_MAP, seed);
    world_time_init(&G_WORLD_TIME);
    world_apply_season_effects(&G_MAP, &G_WORLD_TIME);
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

    if (!music_system_init("data/music.stv", "gameplay"))
        TraceLog(LOG_WARNING, "Music system failed to initialize.");
    if (!ui_init("assets/ui/ui.png"))
        TraceLog(LOG_WARNING, "UI theme failed to initialize.");

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
    input_update(&G_INPUT);

    float dt = GetFrameTime();
    ui_update(&G_INPUT, &G_ENTITIES, dt);

    update_camera(&G_CAMERA, &G_INPUT.camera);

    bool paused = ui_is_paused();
    if (!paused)
    {
        if (IsKeyPressed(KEY_T))
            world_time_cycle_timewarp(&G_WORLD_TIME);

        if (IsKeyPressed(KEY_F))
        {
            MouseState mouse;
            input_update_mouse(&mouse, &G_CAMERA, &G_MAP);
            if (mouse.insideMap)
            {
                int     tx  = mouse.tileX;
                int     ty  = mouse.tileY;
                Object* obj = G_MAP.objects[ty][tx];
                if (object_has_activation(obj) && object_toggle(obj))
                    chunkgrid_redraw_cell(gChunks, &G_MAP, tx, ty);
            }
        }
    }

    music_system_update(dt);

    if (paused)
        return;

    world_time_update(&G_WORLD_TIME, dt);
    world_apply_season_effects(&G_MAP, &G_WORLD_TIME);
    entity_system_update(&G_ENTITIES, &G_MAP, &G_CAMERA, dt);
    object_update_system(&G_MAP, dt);

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
    object_draw_environment(&G_MAP, &G_CAMERA);
    object_draw_dynamic(&G_MAP, &G_CAMERA);
    entity_system_draw(&G_ENTITIES);

    // --- Mouse highlight ---
    MouseState mouse;
    input_update_mouse(&mouse, &G_CAMERA, &G_MAP);

    if (mouse.insideMap)
    {
        Rectangle highlight = {(float)mouse.tileX * TILE_SIZE, (float)mouse.tileY * TILE_SIZE, TILE_SIZE, TILE_SIZE};
        const UiTheme* ui = ui_theme_get();
        if (ui && ui_theme_is_ready())
        {
            Rectangle dest = {highlight.x - 2.0f, highlight.y - 2.0f, highlight.width + 4.0f, highlight.height + 4.0f};
            DrawTexturePro(ui->atlas, ui->tileHighlight, dest, (Vector2){0.0f, 0.0f}, 0.0f, ColorAlpha(WHITE, 0.85f));
        }
        else
        {
            DrawRectangleLinesEx(highlight, 2.0f, YELLOW);
        }
    }

    // --- Building labels ---
    float     viewWidth  = GetScreenWidth() / G_CAMERA.zoom;
    float     viewHeight = GetScreenHeight() / G_CAMERA.zoom;
    Rectangle worldView  = {
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
            const UiTheme* uiTheme    = ui_theme_get();
            const char*    displayName = building_display_name(b);
            int            labelWidth  = MeasureText(displayName, 12);
            if (uiTheme && ui_theme_is_ready())
            {
                Rectangle labelRect = {(float)textX - 6.0f, (float)textY - 4.0f, (float)labelWidth + 12.0f, 18.0f};
                DrawRectangleRounded(labelRect, 0.2f, 4, ColorAlpha(uiTheme->overlayDim, 0.75f));
            }
            else
            {
                Rectangle labelRect = {(float)textX - 6.0f, (float)textY - 4.0f, (float)labelWidth + 12.0f, 18.0f};
                DrawRectangleRounded(labelRect, 0.2f, 4, ColorAlpha(BLACK, 0.6f));
            }
            DrawText(displayName, textX, textY, 12, WHITE);

            int            infoY = textY + 18;
            StructureKind  kind  = resolve_structure_kind(b);
            const UiTheme* uiPtr  = uiTheme;

            const char* auraName = localized_structure_field(kind, "aura_name", fallback_text(b->auraName));
            if (auraName && auraName[0])
            {
                char auraLine[160];
                snprintf(auraLine, sizeof(auraLine), localization_get("buildings.aura_line"), auraName, b->auraRadius, b->auraIntensity);
                int auraWidth = MeasureText(auraLine, 10);
                Rectangle auraRect = {(float)textX - 6.0f, (float)infoY - 2.0f, (float)auraWidth + 12.0f, 16.0f};
                if (uiPtr && ui_theme_is_ready())
                    DrawRectangleRounded(auraRect, 0.2f, 4, ColorAlpha(uiPtr->overlayDim, 0.6f));
                else
                    DrawRectangleRounded(auraRect, 0.2f, 4, ColorAlpha(BLACK, 0.5f));
                DrawText(auraLine, textX, infoY, 10, ColorAlpha(WHITE, 0.9f));
                infoY += 16;

                const char* auraDesc = localized_structure_field(kind, "aura_description", fallback_text(b->auraDescription));
                if (auraDesc && auraDesc[0])
                {
                    int auraDescWidth = MeasureText(auraDesc, 9);
                    Rectangle descRect = {(float)textX - 6.0f, (float)infoY - 2.0f, (float)auraDescWidth + 12.0f, 14.0f};
                    if (uiPtr && ui_theme_is_ready())
                        DrawRectangleRounded(descRect, 0.2f, 4, ColorAlpha(uiPtr->overlayDim, 0.55f));
                    else
                        DrawRectangleRounded(descRect, 0.2f, 4, ColorAlpha(BLACK, 0.45f));
                    DrawText(auraDesc, textX, infoY, 9, ColorAlpha(WHITE, 0.75f));
                    infoY += 14;
                }
            }

            if (b->occupantType > ENTITY_TYPE_INVALID && b->occupantMax > 0)
            {
                const char* occLabel = localized_structure_field(kind, "occupant_description", fallback_text(b->occupantDescription));
                if (!occLabel || occLabel[0] == '\0')
                    occLabel = localization_get("buildings.residents_fallback");

                char occLine[160];
                snprintf(occLine,
                         sizeof(occLine),
                         localization_get("buildings.residents_line"),
                         b->occupantActive,
                         b->occupantCurrent,
                         b->occupantMin,
                         b->occupantMax,
                         occLabel);
                int occWidth = MeasureText(occLine, 10);
                Rectangle occRect = {(float)textX - 6.0f, (float)infoY - 2.0f, (float)occWidth + 12.0f, 16.0f};
                if (uiPtr && ui_theme_is_ready())
                    DrawRectangleRounded(occRect, 0.2f, 4, ColorAlpha(uiPtr->overlayDim, 0.6f));
                else
                    DrawRectangleRounded(occRect, 0.2f, 4, ColorAlpha(BLACK, 0.5f));
                DrawText(occLine, textX, infoY, 10, ColorAlpha(WHITE, 0.9f));
                infoY += 16;
            }

            const char* triggerText = localized_structure_field(kind, "trigger_description", fallback_text(b->triggerDescription));
            if (triggerText && triggerText[0])
            {
                int triggerWidth = MeasureText(triggerText, 10);
                Rectangle triggerRect = {(float)textX - 6.0f, (float)infoY - 2.0f, (float)triggerWidth + 12.0f, 16.0f};
                if (uiPtr && ui_theme_is_ready())
                    DrawRectangleRounded(triggerRect, 0.2f, 4, ColorAlpha(uiPtr->overlayDim, 0.6f));
                else
                    DrawRectangleRounded(triggerRect, 0.2f, 4, ColorAlpha(BLACK, 0.5f));
                DrawText(triggerText, textX, infoY, 10, ColorAlpha(WHITE, 0.85f));
                infoY += 16;
            }
        }
    }

    EndMode2D();

    float darkness = world_time_get_darkness();
    if (darkness > 0.0f)
    {
        float alpha = fminf(1.0f, darkness * 0.75f);
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), ColorAlpha(BLACK, alpha));
    }

    // Draw optional overlays such as biome debug view and the build inventory.
    static bool showBiomeDebug = false;
    debug_biome_draw(&G_MAP, &G_CAMERA, &showBiomeDebug);

    world_time_draw_ui(&G_WORLD_TIME, &G_MAP, &G_CAMERA);

    // Optional: draw current tile/object selection and overlays
    ui_draw(&G_INPUT, &G_ENTITIES);
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

    music_system_shutdown();
    ui_shutdown();

    localization_shutdown();

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
        if (ui_should_close_application())
            break;

        BeginDrawing();
        ClearBackground(BLANK);

        app_draw_world();
        app_handle_chunk_eviction();

        EndDrawing();
    }

    app_cleanup();
}
