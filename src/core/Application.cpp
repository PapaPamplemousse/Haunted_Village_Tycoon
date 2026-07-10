#include "core/Application.hpp"

#include "core/Config.hpp"
#include "world/MapGenerator.hpp"
#include "world/WorldMap.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

static constexpr EntityID INVALID_ENTITY = static_cast<EntityID>(-1);

static const char* DoorStateToString(DoorState state) {
    switch (state) {
        case DoorState::OPEN:
            return "OPEN";
        case DoorState::CLOSED:
            return "CLOSED";
        case DoorState::LOCKED:
            return "LOCKED";
        default:
            return "UNKNOWN";
    }
}

static const char* BoolToString(bool value) {
    return value ? "true" : "false";
}

static int WorldToTile(float value) {
    return static_cast<int>(std::floor(value / Config::TILE_SIZE));
}

static std::string JoinStrings(const std::vector<std::string>& values, const std::string& separator) {
    std::string result;

    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            result += separator;
        }

        result += values[i];
    }

    return result;
}

static std::string BehaviorRuleToString(const BehaviorRule& rule) {
    if (rule.arguments.empty()) {
        return rule.name;
    }

    return rule.name + "(" + JoinStrings(rule.arguments, ",") + ")";
}

} // namespace

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

    m_worldMap.Initialize(Config::MAP_WIDTH, Config::MAP_HEIGHT);
    MapGenerator::GenerateIsland(m_worldMap, m_entityManager, m_tileRegistry, m_biomeRegistry, m_environmentRegistry, Config::SEED);

    m_uiManager.Initialize(m_entityRegistry, m_furnitureRegistry, m_constructionRegistry);

    float midX = (Config::MAP_WIDTH / 2) * (float)Config::TILE_SIZE;
    float midY = (Config::MAP_HEIGHT / 2) * (float)Config::TILE_SIZE;

    // Cherche la ligne où tu spawn ton villageois :
    EntityID vId = m_entityRegistry.SpawnEntity(m_entityManager, "VILLAGER", {midX - 50, midY}, m_nameRegistry);

    // --- : Cheat code d'inventaire ---
    m_entityManager.inventories[vId].items["wood"] = 500; // Il a 500 de bois !
    m_entityManager.inventories[vId].items["rope"] = 50;  // Et 50 cordes !

    m_camera.SetTarget({midX, midY});
    // Assign offset components individually to avoid brace-init issues
    Camera2D& rayCamera = const_cast<Camera2D&>(m_camera.GetRaylibCamera());
    rayCamera.offset.x = Config::WINDOW_WIDTH / 2.0f;
    rayCamera.offset.y = Config::WINDOW_HEIGHT / 2.0f;
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
    m_aiSystem.Update(deltaTime, m_entityManager, m_worldMap, m_tileRegistry, m_roomSystem);
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
            m_entityRegistry.SpawnEntity(m_entityManager, prefabToPlace, spawnPos, m_nameRegistry);

        } else if (m_uiManager.GetSelectedCategory() == BuildCategory::Furniture) {
            m_furnitureRegistry.SpawnFurniture(m_entityManager, prefabToPlace, spawnPos, true);

        } else if (m_uiManager.GetSelectedCategory() == BuildCategory::Constructions) {
            m_constructionRegistry.SpawnConstruction(m_entityManager, prefabToPlace, spawnPos, true);
        }
    }

    // CLIC DROIT : Supprimer l'entité sous la souris
    if (m_inputManager.IsDeletePressed()) {
        int targetX = m_inputManager.GetMouseGridX();
        int targetY = m_inputManager.GetMouseGridY();

        for (size_t i = 0; i < m_entityManager.active.size(); ++i) {
            if (m_entityManager.active[i] && m_entityManager.hasTransform[i]) {
                int entityGridX = static_cast<int>(m_entityManager.transforms[i].position.x / Config::TILE_SIZE);
                int entityGridY = static_cast<int>(m_entityManager.transforms[i].position.y / Config::TILE_SIZE);

                if (entityGridX == targetX && entityGridY == targetY) {
                    // Si c'est un blueprint (projet non commencé), on l'annule instantanément
                    if (m_entityManager.hasBlueprint[i] && !m_entityManager.blueprints[i].isFinished) {
                        m_entityManager.DestroyEntity(i);
                    }
                    // Si c'est un objet réel, on place un Ordre de Démolition !
                    else if (!m_entityManager.hasDeconstruct[i] && !m_entityManager.hasBehavior[i]) {
                        m_entityManager.hasDeconstruct[i] = true;
                        m_entityManager.deconstructs[i] = {true};
                    }
                    break;
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

    // ==========================================================
    // 3. INSPECTION (CTRL + Hover)
    // ==========================================================
    if (m_inputManager.IsInspectPressed()) {
        int hoverX = m_inputManager.GetMouseGridX();
        int hoverY = m_inputManager.GetMouseGridY();

        EntityID hoveredEntity = static_cast<EntityID>(-1);
        EntityID firstEntityOnTile = static_cast<EntityID>(-1);
        EntityID doorEntityOnTile = static_cast<EntityID>(-1);
        EntityID behaviorEntityOnTile = static_cast<EntityID>(-1);

        // On cherche les entités sur cette case.
        // Priorité: PNJ/AI > Porte > autre entité.
        for (size_t i = 0; i < m_entityManager.active.size(); ++i) {
            if (!m_entityManager.active[i] || !m_entityManager.hasTransform[i]) {
                continue;
            }

            int ex = WorldToTile(m_entityManager.transforms[i].position.x);
            int ey = WorldToTile(m_entityManager.transforms[i].position.y);

            if (ex != hoverX || ey != hoverY) {
                continue;
            }

            if (firstEntityOnTile == static_cast<EntityID>(-1)) {
                firstEntityOnTile = i;
            }

            if (m_entityManager.hasDoor[i]) {
                doorEntityOnTile = i;
            }

            if (m_entityManager.hasBehavior[i]) {
                behaviorEntityOnTile = i;
            }
        }

        if (behaviorEntityOnTile != static_cast<EntityID>(-1)) {
            hoveredEntity = behaviorEntityOnTile;
        } else if (doorEntityOnTile != static_cast<EntityID>(-1)) {
            hoveredEntity = doorEntityOnTile;
        } else {
            hoveredEntity = firstEntityOnTile;
        }

        if (hoveredEntity != static_cast<EntityID>(-1)) {
            std::vector<std::string> lines;
            EntityID i = hoveredEntity;

            lines.push_back("Entity ID: " + std::to_string(i));

            if (m_entityManager.hasTag[i]) {
                const auto& tag = m_entityManager.tags[i];

                if (!tag.firstName.empty()) {
                    lines.push_back(tag.firstName + " the " + tag.name);
                } else {
                    lines.push_back(tag.name);
                }

                lines.push_back("Prefab: " + tag.prefabId);

                if (!tag.species.empty()) {
                    lines.push_back("Species: " + tag.species);
                }
            }

            if (m_entityManager.hasTransform[i]) {
                int tileX = WorldToTile(m_entityManager.transforms[i].position.x);
                int tileY = WorldToTile(m_entityManager.transforms[i].position.y);

                lines.push_back("Tile: " + std::to_string(tileX) + ", " + std::to_string(tileY));
            }

            if (m_entityManager.hasDoor[i]) {
                const auto& door = m_entityManager.doors[i];

                lines.push_back("Door state: " + std::string(DoorStateToString(door.state)));
                lines.push_back("Owner ID: " + std::to_string(door.ownerId));
            }

            if (m_entityManager.hasHealth[i]) {
                lines.push_back(TextFormat("HP: %.0f / %.0f", m_entityManager.healths[i].current, m_entityManager.healths[i].max));
            }

            if (m_entityManager.hasStats[i]) {
                lines.push_back(TextFormat("Speed: %.0f", m_entityManager.stats[i].maxSpeed));
            }

            if (m_entityManager.hasNeeds[i]) {
                lines.push_back(TextFormat("Hunger: %.0f / %.0f", m_entityManager.needs[i].hunger, m_entityManager.needs[i].maxHunger));
            }

            bool hasInv = m_entityManager.hasInventory[i];
            bool hasHarv = m_entityManager.hasHarvestable[i];

            if (hasInv || hasHarv) {
                lines.push_back("--- Inventory ---");
                bool isEmpty = true;

                // 1. On affiche le vrai inventaire s'il existe
                if (hasInv && !m_entityManager.inventories[i].items.empty()) {
                    for (const auto& item : m_entityManager.inventories[i].items) {
                        lines.push_back(item.first + ": " + std::to_string(item.second));
                    }
                    isEmpty = false;
                }

                // 2. On affiche le contenu récoltable comme si c'était dans l'inventaire
                if (hasHarv) {
                    const auto& harvestable = m_entityManager.harvestables[i];
                    if (harvestable.dropAmount > 0 && !harvestable.dropItemId.empty()) {
                        // On précise que c'est un "Yield" (Rendement) pour que ça soit clair
                        lines.push_back(harvestable.dropItemId + ": " + std::to_string(harvestable.dropAmount) + " (Yield)");
                        isEmpty = false;
                    }
                }

                if (isEmpty) {
                    lines.push_back("Empty");
                }
            }

            if (m_entityManager.hasBehavior[i]) {
                const auto& behavior = m_entityManager.behaviors[i];

                lines.push_back("--- AI ---");
                lines.push_back("Task: " + behavior.currentTask);
            }

            if (!lines.empty()) {
                Vector2 mousePos = GetMousePosition();

                float boxWidth = 300.0f;
                float lineHeight = 22.0f;
                float boxHeight = lines.size() * lineHeight + 12.0f;

                float boxX = mousePos.x + 15.0f;
                float boxY = mousePos.y + 15.0f;

                if (boxX + boxWidth > GetScreenWidth()) {
                    boxX = mousePos.x - boxWidth - 15.0f;
                }

                if (boxY + boxHeight > GetScreenHeight()) {
                    boxY = mousePos.y - boxHeight - 15.0f;
                }

                DrawRectangle(static_cast<int>(boxX), static_cast<int>(boxY), static_cast<int>(boxWidth), static_cast<int>(boxHeight),
                              ColorAlpha(BLACK, 0.9f));

                DrawRectangleLines(static_cast<int>(boxX), static_cast<int>(boxY), static_cast<int>(boxWidth), static_cast<int>(boxHeight),
                                   DARKGRAY);

                for (size_t l = 0; l < lines.size(); ++l) {
                    Color c = LIGHTGRAY;

                    if (l == 0) {
                        c = GOLD;
                    } else if (lines[l] == "--- AI ---" || lines[l] == "--- Inventory ---") {
                        c = SKYBLUE;
                    }

                    DrawText(lines[l].c_str(), static_cast<int>(boxX + 10.0f), static_cast<int>(boxY + 8.0f + l * lineHeight), 18, c);
                }
            }
        }
    }
    EndDrawing();
}
