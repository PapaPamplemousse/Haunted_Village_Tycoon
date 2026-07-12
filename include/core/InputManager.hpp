/**
 * @file InputManager.hpp
 * @brief Handles global user inputs and world intersections to keep the Application class clean.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
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

    // Check if player is holding the "Show Names" key (Tab)
    bool IsShowNamesPressed() const {
        return IsKeyDown(KEY_TAB);
    }

    // Check if player is holding CTRL to inspect
    bool IsInspectPressed() const {
        return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    }

private:
    Vector2 m_mouseWorldPos = {0.0f, 0.0f};
    int m_mouseGridX = 0;
    int m_mouseGridY = 0;
};
