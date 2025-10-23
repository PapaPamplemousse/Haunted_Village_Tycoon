#include "camera.h"
#include "map.h"
#include "raymath.h"

const float ZOOM_MIN = 0.5f; // dézoom max
const float ZOOM_MAX = 2.5f; // zoom max

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
    const float moveSpeed = 500.0f;
    const float zoomSpeed = 0.1f;
    float       dt        = GetFrameTime();

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

    // --- Wrap caméra toroïdal ---
    float worldWidth  = MAP_WIDTH * TILE_SIZE;
    float worldHeight = MAP_HEIGHT * TILE_SIZE;
    if (camera->target.x < 0)
        camera->target.x += worldWidth;
    if (camera->target.y < 0)
        camera->target.y += worldHeight;
    if (camera->target.x >= worldWidth)
        camera->target.x -= worldWidth;
    if (camera->target.y >= worldHeight)
        camera->target.y -= worldHeight;

    // --- Zoom molette ---
    float wheel = GetMouseWheelMove();
    if (wheel != 0)
    {
        camera->zoom += wheel * zoomSpeed;
        if (camera->zoom < ZOOM_MIN)
            camera->zoom = ZOOM_MIN;
        if (camera->zoom > ZOOM_MAX)
            camera->zoom = ZOOM_MAX;
    }
}
