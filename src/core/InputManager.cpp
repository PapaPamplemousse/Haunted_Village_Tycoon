#include "core/InputManager.hpp"

#include "core/Config.hpp"

#include <cmath>

void InputManager::Update(const GameCamera& camera) {
    // 1. Raccourcis Globaux (Ex: Basculer en Plein Écran)
    if (IsKeyPressed(KEY_F11)) {
        ToggleFullscreen();
    }

    // 2. Mouse Picking : Conversion Écran -> Monde
    m_mouseWorldPos = GetScreenToWorld2D(GetMousePosition(), camera.GetRaylibCamera());

    // 3. Conversion Monde -> Grille
    // On utilise std::floor pour éviter les bugs si la souris sort de la carte vers les négatifs
    m_mouseGridX = static_cast<int>(std::floor(m_mouseWorldPos.x / Config::TILE_SIZE));
    m_mouseGridY = static_cast<int>(std::floor(m_mouseWorldPos.y / Config::TILE_SIZE));
}
