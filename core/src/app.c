
#include "app.h"
#include "building.h"
#include "camera.h"
#include "map.h"
#include "raylib.h"

void app_run(void) {
  InitWindow(1280, 720, "Containment Tycoon");
  Camera2D camera = init_camera();

  SetTargetFPS(60);
  map_init();
  building_system_init();

  while (!WindowShouldClose()) {
    update_camera(&camera); // 🎥
    update_map(camera);     // 🧱

    handle_building_input();

    BeginDrawing();
    ClearBackground(DARKGRAY);
    BeginMode2D(camera);

    draw_map(camera); // 🗺️

    Vector2 mouse = GetMousePosition();
    Vector2 worldMouse = GetScreenToWorld2D(mouse, camera);
    draw_building_preview(worldMouse);

    EndMode2D();
    EndDrawing();
  }

  map_unload();
  building_system_unload();

  CloseWindow();
}
