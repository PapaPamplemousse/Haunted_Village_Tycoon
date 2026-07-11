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
constexpr int SEED = 20;            // Default seed for procedural generation
constexpr float TIME_SCALE = 20.0f; // 1 real second = 20 in-game minutes

// --- Simulation Optimization ---
constexpr int SIMULATION_ACTIVE_RADIUS_TILES = 80;
constexpr float PROFESSION_UPDATE_INTERVAL = 1.0f;
constexpr float HUNGER_DECAY_PER_SECOND = 0.5f;

// --- Spatial Optimization ---
constexpr int SPATIAL_CELL_SIZE_TILES = 16;
constexpr int RENDER_ENTITY_MARGIN_TILES = 4;
constexpr int AI_SEARCH_RADIUS_TILES = 80;

} // namespace Config
