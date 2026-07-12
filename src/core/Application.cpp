/**
 * @file Application.cpp
 * @brief Implementation of the main game application and system orchestration.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Application.hpp"

#include "core/Config.hpp"
#include "world/MapGenerator.hpp"

#include <iostream>

Application::Application()
    : m_isRunning(true)
    , m_camera(Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(Config::WINDOW_WIDTH, Config::WINDOW_HEIGHT, "Haunted Village Tycoon");
    SetTargetFPS(Config::TARGET_FPS);

    if (!m_tileRegistry.LoadFromSTV("data/tiles.stv")) {
        std::cerr << "Failed to load tiles!" << std::endl;
    }

    if (!m_biomeRegistry.LoadFromSTV("data/biomes.stv", m_tileRegistry)) {
        std::cerr << "Failed to load biomes!" << std::endl;
    }

    if (!m_entityRegistry.LoadFromSTV("data/entities.stv")) {
        std::cerr << "Failed to load entities!" << std::endl;
    }

    if (!m_furnitureRegistry.LoadFromSTV("data/furniture.stv")) {
        std::cerr << "Failed to load furniture!" << std::endl;
    }

    if (!m_constructionRegistry.LoadFromSTV("data/constructions.stv")) {
        std::cerr << "Failed to load constructions!" << std::endl;
    }

    if (!m_structureRegistry.LoadFromSTV("data/structures.stv")) {
        std::cerr << "Failed to load structures!" << std::endl;
    }

    if (!m_nameRegistry.LoadFromSTV("data/names.stv")) {
        std::cerr << "Failed to load names!" << std::endl;
    }

    if (!m_environmentRegistry.LoadFromSTV("data/environment.stv")) {
        std::cerr << "Failed to load environment!" << std::endl;
    }

    if (!m_behaviorRegistry.LoadFromSTV("data/behaviors.stv")) {
        std::cerr << "Failed to load behaviors!" << std::endl;
    }

    if (!m_weaponRegistry.LoadFromSTV("data/weapons.stv")) {
        std::cerr << "Failed to load weapons!" << std::endl;
    }

    if (!m_professionRegistry.LoadFromSTV("data/professions.stv")) {
        std::cerr << "Failed to load professions!" << std::endl;
    }

    if (!m_resourceRegistry.LoadFromSTV("data/resources.stv")) {
        std::cerr << "Failed to load resources!" << std::endl;
    }

    m_worldMap.Initialize(Config::MAP_WIDTH, Config::MAP_HEIGHT);

    MapGenerator::GenerateIsland(m_worldMap, m_entityManager, m_tileRegistry, m_biomeRegistry, m_environmentRegistry, Config::SEED);

    m_uiManager.Initialize(m_entityRegistry, m_furnitureRegistry, m_constructionRegistry);

    const float midX = (Config::MAP_WIDTH / 2.0f) * Config::TILE_SIZE;
    const float midY = (Config::MAP_HEIGHT / 2.0f) * Config::TILE_SIZE;

    const Vector2 preferredVillageCenter = {midX, midY};

    EntityID villageCore =
        m_villageSystem.InitializeStartingVillage(m_entityManager, m_entityRegistry, m_furnitureRegistry, m_nameRegistry,
                                                  m_behaviorRegistry, m_worldMap, m_tileRegistry, preferredVillageCenter);

    m_spatialGrid.Rebuild(m_entityManager);

    if (villageCore < m_entityManager.active.size() && m_entityManager.active[villageCore] && m_entityManager.hasTransform[villageCore]) {
        m_camera.SetTarget(m_entityManager.transforms[villageCore].position);
    } else {
        m_camera.SetTarget(preferredVillageCenter);
    }

    Camera2D& rayCamera = const_cast<Camera2D&>(m_camera.GetRaylibCamera());

    rayCamera.offset.x = Config::WINDOW_WIDTH / 2.0f;
    rayCamera.offset.y = Config::WINDOW_HEIGHT / 2.0f;
}

Application::~Application() {
    CloseAudioDevice();
    CloseWindow();
}

void Application::Run() {
    while (m_isRunning && !WindowShouldClose()) {
        const float deltaTime = GetFrameTime();

        Update(deltaTime);
        Render();
    }
}

void Application::Update(float deltaTime) {
    m_villageMenu.Update(m_entityManager, m_camera, m_professionRegistry, m_behaviorRegistry);

    if (m_villageMenu.IsOpen()) {
        return;
    }

    m_uiManager.Update();

    if (m_uiManager.IsMenuOpen()) {
        return;
    }

    m_inputManager.Update(m_camera);
    m_camera.Update(deltaTime);

    m_buildPlacementSystem.Update(m_inputManager, m_uiManager, m_entityManager, m_entityRegistry, m_furnitureRegistry,
                                  m_constructionRegistry, m_nameRegistry, m_behaviorRegistry);

    m_timeSystem.Update(deltaTime, m_entityManager);

    m_villageSystem.Update(deltaTime, m_entityManager, m_entityRegistry, m_nameRegistry, m_behaviorRegistry, m_worldMap, m_tileRegistry,
                           m_resourceRegistry, m_timeSystem);

    m_eventSystem.Update(m_entityManager, m_timeSystem, m_resourceRegistry, m_settlementMetrics, m_chronicle);

    m_roomSystem.Update(m_entityManager, m_worldMap, m_structureRegistry);

    m_professionSystem.Update(deltaTime, m_entityManager, m_professionRegistry, m_behaviorRegistry);

    const Vector2 simulationCenter = m_camera.GetRaylibCamera().target;

    m_spatialGrid.Rebuild(m_entityManager);

    m_aiSystem.Update(deltaTime, m_entityManager, m_worldMap, m_tileRegistry, m_resourceRegistry, m_spatialGrid, simulationCenter,
                      static_cast<float>(Config::SIMULATION_ACTIVE_RADIUS_TILES), m_timeSystem.GetHour(), m_roomSystem);

    m_spatialGrid.Rebuild(m_entityManager);
}

void Application::Render() {
    BeginDrawing();
    ClearBackground(BLACK);

    const Camera2D& camera = m_camera.GetRaylibCamera();

    const int hoverX = m_inputManager.GetMouseGridX();
    const int hoverY = m_inputManager.GetMouseGridY();
    const bool showNames = m_inputManager.IsShowNamesPressed();

    BeginMode2D(camera);

    m_worldRenderSystem.Render(m_entityManager, m_worldMap, m_tileRegistry, camera, hoverX, hoverY, showNames);

    m_renderSystem.Render(m_entityManager, m_spatialGrid, camera, showNames);

    m_worldRenderSystem.ShowName(m_entityManager, camera, showNames);

    EndMode2D();

    m_lightingSystem.RenderOverlay(m_timeSystem.GetHour(), m_timeSystem.GetSeasonIndex());

    DrawText(TextFormat("Day %d - %s %d/5 - %s - %02d:00", m_timeSystem.GetDay(), m_timeSystem.GetSeasonName(),
                        m_timeSystem.GetDayInSeason(),
                        m_lightingSystem.GetDayPhaseName(m_timeSystem.GetHour(), m_timeSystem.GetSeasonIndex()),
                        static_cast<int>(m_timeSystem.GetHour())),
             10, 40, 20, GOLD);

    m_chronicleRenderSystem.Render(m_chronicle, m_settlementMetrics);

    DrawFPS(GetScreenWidth() - 100, 10);

    const int screenH = GetScreenHeight();

    DrawText(TextFormat("Grid: [ X: %d | Y: %d ]", hoverX, hoverY), 10, screenH - 30, 20, LIGHTGRAY);

    m_uiManager.Render();

    if (!m_villageMenu.IsOpen()) {
        m_inspectionSystem.Render(m_inputManager, m_entityManager, m_spatialGrid, m_resourceRegistry);
    }

    m_villageMenu.Render(m_entityManager, m_resourceRegistry, m_timeSystem);

    EndDrawing();
}
