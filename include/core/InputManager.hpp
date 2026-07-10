#pragma once
#include "core/GameCamera.hpp"

#include <raylib.h>

/**
 * @class InputManager
 * @brief Handles global user inputs (mouse picking, shortcuts) to keep the Application class clean.
 */
class InputManager {
public:
    InputManager() = default;
    ~InputManager() = default;

    /**
     * @brief Reads keyboard and mouse state, and calculates world intersections.
     * @param camera The active game camera (needed for Screen-to-World conversion).
     */
    void Update(const GameCamera& camera);

    // Getters for Mouse Picking
    Vector2 GetMouseWorldPos() const {
        return m_mouseWorldPos;
    }
    int GetMouseGridX() const {
        return m_mouseGridX;
    }
    int GetMouseGridY() const {
        return m_mouseGridY;
    }

    // Check if player clicked (left click)
    bool IsInteractPressed() const {
        return IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    }
    // Check if player clicked (right click)
    bool IsDeletePressed() const {
        return IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    }

private:
    Vector2 m_mouseWorldPos = {0.0f, 0.0f};
    int m_mouseGridX = 0;
    int m_mouseGridY = 0;
};
