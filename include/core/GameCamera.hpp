/**
 * @file GameCamera.hpp
 * @brief Manages the 2D view of the game world, allowing panning and zooming.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once
#include <raylib.h>

/**
 * @class GameCamera
 * @brief Manages the 2D view of the game world, allowing panning and zooming.
 * Encapsulates Raylib's Camera2D struct and provides Tycoon-style controls.
 */
class GameCamera {
public:
    /**
     * @brief Constructs the camera and centers it.
     * @param screenWidth Width of the application window.
     * @param screenHeight Height of the application window.
     */
    GameCamera(int screenWidth, int screenHeight);
    ~GameCamera() = default;

    /**
     * @brief Updates camera position and zoom based on player input.
     * @param deltaTime Time elapsed since the last frame.
     */
    void Update(float deltaTime);

    /**
     * @brief Gets the underlying Raylib Camera2D struct for rendering.
     */
    const Camera2D& GetRaylibCamera() const {
        return m_camera;
    }

    void SetTarget(Vector2 targetPos) {
        m_camera.target = targetPos;
    }

private:
    Camera2D m_camera;
    const float PAN_SPEED = 800.0f; // Base speed in pixels per second
    const float ZOOM_SPEED = 0.1f;
};
