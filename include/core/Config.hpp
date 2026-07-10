#pragma once

/**
 * @namespace Config
 * @brief Global compile-time constants for the game engine.
 * Centralizes all "magic numbers" to avoid hardcoded values spread across systems.
 */
namespace Config {
// --- Window Settings ---
constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;
constexpr int TARGET_FPS = 60;

// --- World & Grid Settings ---
constexpr int MAP_WIDTH = 500;
constexpr int MAP_HEIGHT = 500;
constexpr float TILE_SIZE = 32.0f;

// --- Camera Settings ---
constexpr float CAMERA_PAN_SPEED = 800.0f;
constexpr float CAMERA_ZOOM_SPEED = 0.1f;
constexpr float CAMERA_MIN_ZOOM = 0.1f;
constexpr float CAMERA_MAX_ZOOM = 10.0f;

// --- Simulation Settings ---
constexpr int SEED = 100;           // Default seed for procedural generation
constexpr float TIME_SCALE = 20.0f; // 1 real second = 20 in-game minutes
} // namespace Config
