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
    if (!m_constructionRegistry.LoadFromSTV("data/constructions.stv")) {
        std::cerr << "Failed to load constructions!" << std::endl;
    }
    if (!m_structureRegistry.LoadFromSTV("data/structures.stv")) {
        std::cerr << "Failed to load structures!" << std::endl;
    }

    m_worldMap.Initialize(Config::MAP_WIDTH, Config::MAP_HEIGHT);
    MapGenerator::GenerateIsland(m_worldMap, m_tileRegistry, m_biomeRegistry, 42);

    m_uiManager.Initialize(m_entityRegistry, m_furnitureRegistry, m_constructionRegistry);

    m_camera.GetRaylibCamera();
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
            // Les entités (PNJ) ne changent pas les pièces, pas besoin de MarkDirty

        } else if (m_uiManager.GetSelectedCategory() == BuildCategory::Furniture) {
            m_furnitureRegistry.SpawnFurniture(m_entityManager, prefabToPlace, spawnPos, false);
            m_roomSystem.MarkDirty();

        } else if (m_uiManager.GetSelectedCategory() == BuildCategory::Constructions) {
            m_constructionRegistry.SpawnConstruction(m_entityManager, prefabToPlace, spawnPos, false);
            m_roomSystem.MarkDirty();
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
                    if (m_entityManager.hasConstruction[i] || (m_entityManager.hasTag[i] && !m_entityManager.hasBehavior[i])) {
                        m_roomSystem.MarkDirty();
                    }
                    m_entityManager.DestroyEntity(i);
                    std::cout << "[GAME] Entity deleted at " << targetX << ", " << targetY << std::endl;
                    break; // On n'en supprime qu'une par clic
                }
            }
        }
    }
    m_roomSystem.Update(m_entityManager, m_worldMap, m_structureRegistry);
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

    bool showNames = m_inputManager.IsShowNamesPressed();

    // --- 1. DESSIN DU SOL DES PIÈCES ---
    for (size_t i = 0; i < m_entityManager.active.size(); ++i) {
        if (m_entityManager.active[i] && m_entityManager.hasRoom[i]) {
            const auto& room = m_entityManager.rooms[i];
            Color roomTint = (room.structureId == "EMPTY_ROOM") ? ColorAlpha(GRAY, 0.3f) : ColorAlpha(BLUE, 0.3f);

            for (const Vector2& tile : room.floorTiles) {
                DrawRectangle(tile.x * Config::TILE_SIZE, tile.y * Config::TILE_SIZE, Config::TILE_SIZE, Config::TILE_SIZE, roomTint);
            }
        }
    }

    // --- 2. DESSIN DES ENTITÉS ---
    m_renderSystem.Render(m_entityManager, m_camera.GetRaylibCamera(), showNames);

    // --- 3. DESSIN DU NOM DES PIÈCES (Au-dessus de tout) ---
    if (showNames) {
        for (size_t i = 0; i < m_entityManager.active.size(); ++i) {
            if (m_entityManager.active[i] && m_entityManager.hasRoom[i]) {
                const auto& room = m_entityManager.rooms[i];
                if (room.floorTiles.empty())
                    continue;

                // Calcul du centre de la pièce
                float sumX = 0, sumY = 0;
                for (const Vector2& tile : room.floorTiles) {
                    sumX += tile.x;
                    sumY += tile.y;
                }
                float centerX = (sumX / room.floorTiles.size()) * Config::TILE_SIZE + (Config::TILE_SIZE / 2.0f);
                float centerY = (sumY / room.floorTiles.size()) * Config::TILE_SIZE + (Config::TILE_SIZE / 2.0f);

                const char* name = room.name.c_str();
                int fontSize = 30; // Très grand pour les pièces !
                int textWidth = MeasureText(name, fontSize);
                int padding = 6;

                float textX = centerX - textWidth / 2.0f;
                float textY = centerY - fontSize / 2.0f;

                // Fond sombre et texte doré/gris
                Color textCol = (room.structureId == "EMPTY_ROOM") ? LIGHTGRAY : GOLD;
                DrawRectangle(textX - padding, textY - padding, textWidth + padding * 2, fontSize + padding * 2, ColorAlpha(BLACK, 0.8f));
                DrawText(name, textX, textY, fontSize, textCol);
            }
        }
    }
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
