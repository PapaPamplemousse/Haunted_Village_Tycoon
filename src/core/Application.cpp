#include "core/Application.hpp"

#include "core/Config.hpp"
#include "world/MapGenerator.hpp"
#include "world/WorldMap.hpp"

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

    m_worldMap.Initialize(Config::MAP_WIDTH, Config::MAP_HEIGHT);
    MapGenerator::GenerateIsland(m_worldMap, m_tileRegistry, m_biomeRegistry, 42);

    float midX = (Config::MAP_WIDTH / 2) * (float)Config::TILE_SIZE;
    float midY = (Config::MAP_HEIGHT / 2) * (float)Config::TILE_SIZE;

    m_entityRegistry.SpawnEntity(m_entityManager, "VILLAGER", {midX - 50, midY});
    m_entityRegistry.SpawnEntity(m_entityManager, "CANNIBAL", {midX + 50, midY});
    m_furnitureRegistry.SpawnFurniture(m_entityManager, "CAMPFIRE", {midX, midY + 80}, false);
    m_furnitureRegistry.SpawnFurniture(m_entityManager, "CAMPFIRE", {midX + 100, midY + 80}, true);

    m_camera.GetRaylibCamera(); // Just to access it
}

Application::~Application() {
    // Clean up Raylib resources
    CloseAudioDevice();
    CloseWindow();
}

void Application::Run() {
    while (m_isRunning && !WindowShouldClose()) {
        // Calculate time elapsed since last frame
        float deltaTime = GetFrameTime();

        // Core Loop
        Update(deltaTime);
        Render();
    }
}

void Application::Update(float deltaTime) {
    // 1. Toujours mettre à jour l'UI (pour écouter le bouton E)
    m_uiManager.Update();

    // 2. Si le menu est ouvert, on bloque TOUT LE RESTE (Pause + Pas de mouvement de caméra)
    if (m_uiManager.IsMenuOpen()) {
        return;
    }

    // --- LE JEU NORMAL REPREND ---
    m_inputManager.Update(m_camera);
    m_camera.Update(deltaTime);

    m_timeSystem.Update(deltaTime, m_entityManager);
    m_aiSystem.Update(deltaTime, m_entityManager, m_worldMap, m_tileRegistry);

    // ==========================================
    // LOGIQUE DE PLACEMENT ET SUPPRESSION
    // ==========================================
    std::string prefabToPlace = m_uiManager.GetSelectedPrefab();

    // CLIC GAUCHE : Placer l'objet
    if (m_inputManager.IsInteractPressed() && !prefabToPlace.empty()) {
        // Centrer l'objet sur la tuile visée
        Vector2 spawnPos = {m_inputManager.GetMouseGridX() * Config::TILE_SIZE + (Config::TILE_SIZE / 2.0f),
                            m_inputManager.GetMouseGridY() * Config::TILE_SIZE + (Config::TILE_SIZE / 2.0f)};

        if (m_uiManager.GetSelectedCategory() == BuildCategory::Entities) {
            m_entityRegistry.SpawnEntity(m_entityManager, prefabToPlace, spawnPos);
        } else if (m_uiManager.GetSelectedCategory() == BuildCategory::Furniture) {
            // Note: Si asBlueprint est true, ça spawne un fantôme. On va dire 'false' pour tester directement
            m_furnitureRegistry.SpawnFurniture(m_entityManager, prefabToPlace, spawnPos, false);
        } else {
            // TODO: Créer un ConstructionRegistry plus tard pour WOOD_WALL
            std::cout << "Constructions not yet implemented in Registry!" << std::endl;
        }
    }

    // CLIC DROIT : Supprimer l'entité sous la souris
    if (m_inputManager.IsDeletePressed()) {
        int targetX = m_inputManager.GetMouseGridX();
        int targetY = m_inputManager.GetMouseGridY();

        // On parcourt les entités pour voir si l'une d'elle est sur cette case
        for (size_t i = 0; i < m_entityManager.active.size(); ++i) {
            if (m_entityManager.active[i] && m_entityManager.hasTransform[i]) {
                int entityGridX = static_cast<int>(m_entityManager.transforms[i].position.x / Config::TILE_SIZE);
                int entityGridY = static_cast<int>(m_entityManager.transforms[i].position.y / Config::TILE_SIZE);

                if (entityGridX == targetX && entityGridY == targetY) {
                    m_entityManager.DestroyEntity(i);
                    std::cout << "[GAME] Entity deleted at " << targetX << ", " << targetY << std::endl;
                    break; // On n'en supprime qu'une par clic
                }
            }
        }
    }
}

void Application::Render() {
    BeginDrawing();
    ClearBackground(BLACK);

    BeginMode2D(m_camera.GetRaylibCamera());

    Vector2 topLeft = GetScreenToWorld2D({0, 0}, m_camera.GetRaylibCamera());
    Vector2 bottomRight = GetScreenToWorld2D({(float)GetScreenWidth(), (float)GetScreenHeight()}, m_camera.GetRaylibCamera());

    int startX = std::max(0, (int)(topLeft.x / Config::TILE_SIZE) - 1);
    int startY = std::max(0, (int)(topLeft.y / Config::TILE_SIZE) - 1);
    int endX = std::min(m_worldMap.GetWidth(), (int)(bottomRight.x / Config::TILE_SIZE) + 1);
    int endY = std::min(m_worldMap.GetHeight(), (int)(bottomRight.y / Config::TILE_SIZE) + 1);

    for (int y = startY; y < endY; ++y) {
        for (int x = startX; x < endX; ++x) {
            int tileId = m_worldMap.GetTile(x, y);
            const TileDef* def = m_tileRegistry.GetTileDef(tileId);
            Color color = def ? def->color : MAGENTA;

            DrawRectangle(x * Config::TILE_SIZE, y * Config::TILE_SIZE, Config::TILE_SIZE - 1, Config::TILE_SIZE - 1, color);
        }
    }

    int hoverX = m_inputManager.GetMouseGridX();
    int hoverY = m_inputManager.GetMouseGridY();

    if (hoverX >= 0 && hoverX < m_worldMap.GetWidth() && hoverY >= 0 && hoverY < m_worldMap.GetHeight()) {
        DrawRectangle(hoverX * Config::TILE_SIZE, hoverY * Config::TILE_SIZE, Config::TILE_SIZE, Config::TILE_SIZE,
                      ColorAlpha(WHITE, 0.3f));
    }
    m_renderSystem.Render(m_entityManager, m_camera.GetRaylibCamera());
    EndMode2D();

    DrawText("Engine Foundation V5.0 - Input Manager", 10, 10, 20, WHITE);
    DrawText(TextFormat("Day %d - %02d:00", m_timeSystem.GetDay(), (int)m_timeSystem.GetHour()), 10, 40, 20, GOLD);
    DrawFPS(GetScreenWidth() - 100, 10);
    int screenH = GetScreenHeight();
    const char* coordsText = TextFormat("Grid: [ X: %d | Y: %d ]", hoverX, hoverY);
    DrawText(coordsText, 10, screenH - 30, 20, LIGHTGRAY);

    m_uiManager.Render();

    EndDrawing();
}
