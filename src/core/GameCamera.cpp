/**
 * @file GameCamera.cpp
 * @brief Implementation of the 2D game camera controls.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/GameCamera.hpp"

#include <raymath.h>

GameCamera::GameCamera(int screenWidth, int screenHeight) {
    m_camera = {0};
    m_camera.target = {0.0f, 0.0f}; // Where the camera is looking in the world

    // Offset defines where the 'target' is on the screen.
    // Setting it to the center means our target is always in the middle of the screen.
    m_camera.offset = {screenWidth / 2.0f, screenHeight / 2.0f};
    m_camera.rotation = 0.0f;
    m_camera.zoom = 1.0f;
}

void GameCamera::Update(float deltaTime) {
    // 1. PANNING (Keyboard WASD / Arrows)
    // We divide speed by zoom so panning feels consistent whether zoomed in or out
    float currentSpeed = PAN_SPEED / m_camera.zoom * deltaTime;

    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))
        m_camera.target.y -= currentSpeed;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))
        m_camera.target.y += currentSpeed;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
        m_camera.target.x -= currentSpeed;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
        m_camera.target.x += currentSpeed;

    // 2. PANNING (Mouse Drag - Right Click)
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 delta = GetMouseDelta();
        delta = Vector2Scale(delta, -1.0f / m_camera.zoom);
        m_camera.target = Vector2Add(m_camera.target, delta);
    }

    // 3. ZOOMING (Mouse Wheel)
    float wheel = GetMouseWheelMove();
    if (wheel != 0) {
        // Advanced Tycoon Feature: Zoom towards the mouse cursor!
        Vector2 mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), m_camera);

        // Apply zoom
        m_camera.zoom += wheel * ZOOM_SPEED;

        // Clamp zoom to prevent flipping or lagging
        if (m_camera.zoom < 0.1f)
            m_camera.zoom = 0.1f; // Max zoom out
        if (m_camera.zoom > 10.0f)
            m_camera.zoom = 10.0f; // Max zoom in

        // Adjust target so the mouse cursor stays over the same world point
        m_camera.offset = GetMousePosition();
        m_camera.target = mouseWorldPos;
    }
}
