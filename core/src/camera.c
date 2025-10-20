
#include "camera.h"

Camera2D init_camera(void) {
    Camera2D cam = { 0 };
    cam.offset = (Vector2){640, 360};
    cam.target = (Vector2){0, 0};
    cam.zoom = 1.0f;
    return cam;
}

void update_camera(Camera2D *camera) {
    if (IsKeyDown(KEY_W)) camera->target.y -= 10;
    if (IsKeyDown(KEY_S)) camera->target.y += 10;
    if (IsKeyDown(KEY_A)) camera->target.x -= 10;
    if (IsKeyDown(KEY_D)) camera->target.x += 10;

    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        camera->zoom += wheel * 0.1f;
        if (camera->zoom < 0.2f) camera->zoom = 0.2f;
        if (camera->zoom > 3.0f) camera->zoom = 3.0f;
    }
}
