#include "camera.h"
#include "map.h"
#include "raymath.h"

Camera2D init_camera(void)
{
    Camera2D cam = {0};
    cam.target   = (Vector2){(MAP_WIDTH * TILE_SIZE) / 2.0f, (MAP_HEIGHT * TILE_SIZE) / 2.0f};
    cam.offset   = (Vector2){GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
    cam.rotation = 0.0f;
    cam.zoom     = 1.0f;
    return cam;
}

void update_camera(Camera2D* camera)
{
    const float moveSpeed = 500.0f; // pixels per second
    const float zoomSpeed = 0.1f;   // zoom factor
    float       dt        = GetFrameTime();

    // Déplacement ZQSD ou flèches
    Vector2 movement = {0};
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))
        movement.x -= 1.0f;
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D))
        movement.x += 1.0f;
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))
        movement.y -= 1.0f;
    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))
        movement.y += 1.0f;

    if (movement.x != 0 || movement.y != 0)
    {
        movement       = Vector2Scale(Vector2Normalize(movement), moveSpeed * dt / camera->zoom);
        camera->target = Vector2Add(camera->target, movement);
    }

    // Zoom à la molette
    float wheel = GetMouseWheelMove();
    if (wheel != 0)
    {
        camera->zoom += wheel * zoomSpeed;
        if (camera->zoom < 0.1f)
            camera->zoom = 0.1f;
        if (camera->zoom > 5.0f)
            camera->zoom = 5.0f;
    }
}
