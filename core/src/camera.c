#include "camera.h"
#include "map.h"
#include "raymath.h"

Camera2D init_camera(void)
{
    Camera2D cam = {0};
    cam.target   = (Vector2){(MAP_WIDTH * TILE_SIZE) / 2.0f, (MAP_HEIGHT * TILE_SIZE) / 2.0f};
    cam.offset   = (Vector2){GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
    cam.rotation = 0.0f;

    float zoomX = (float)GetScreenWidth() / (MAP_WIDTH * TILE_SIZE);
    float zoomY = (float)GetScreenHeight() / (MAP_HEIGHT * TILE_SIZE);
    cam.zoom    = fminf(zoomX, zoomY) * 1.2f;
    if (cam.zoom > ZOOM_MAX)
        cam.zoom = ZOOM_MAX;
    if (cam.zoom < ZOOM_MIN)
        cam.zoom = ZOOM_MIN;

    return cam;
}

void update_camera(Camera2D* camera, const CameraInput* input)
{
    const float moveSpeed = 500.0f;
    const float zoomSpeed = 0.1f;
    const float dt        = GetFrameTime();

    // --- Keep camera centered relative to screen size ---
    camera->offset = (Vector2){GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};

    // --- Apply movement ---
    if (input->moveDir.x != 0 || input->moveDir.y != 0)
    {
        Vector2 dir    = Vector2Normalize(input->moveDir);
        Vector2 move   = Vector2Scale(dir, moveSpeed * dt / camera->zoom);
        camera->target = Vector2Add(camera->target, move);
    }

    // --- Toroidal wrapping ---
    const float worldWidth  = MAP_WIDTH * TILE_SIZE;
    const float worldHeight = MAP_HEIGHT * TILE_SIZE;

    if (camera->target.x < 0)
        camera->target.x += worldWidth;
    else if (camera->target.x >= worldWidth)
        camera->target.x -= worldWidth;
    if (camera->target.y < 0)
        camera->target.y += worldHeight;
    else if (camera->target.y >= worldHeight)
        camera->target.y -= worldHeight;

    // --- Zoom ---
    if (input->zoomDelta != 0.0f)
    {
        camera->zoom += input->zoomDelta * zoomSpeed;
        if (camera->zoom < ZOOM_MIN)
            camera->zoom = ZOOM_MIN;
        if (camera->zoom > ZOOM_MAX)
            camera->zoom = ZOOM_MAX;
    }
}
