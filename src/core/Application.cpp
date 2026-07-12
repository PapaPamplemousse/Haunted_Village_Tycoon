/**
 * @file Application.cpp
 * @brief Implementation of the main game application and system orchestration.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Application.hpp"

#include "core/Config.hpp"
#include "world/MapGenerator.hpp"

#include <cmath>
#include <iostream>
#include <string>

namespace {

constexpr const char* DEBUG_WALL_PREFAB = "WOOD_WALL";
constexpr const char* DEBUG_DOOR_PREFAB = "WOOD_DOOR";

int WorldToTile(float worldCoord) {
    return static_cast<int>(std::floor(worldCoord / Config::TILE_SIZE));
}

Vector2 TileToWorldCenter(int tileX, int tileY) {
    return {tileX * Config::TILE_SIZE + Config::TILE_SIZE * 0.5f, tileY * Config::TILE_SIZE + Config::TILE_SIZE * 0.5f};
}

void ClearDebugArea(EntityManager& em, int minX, int minY, int maxX, int maxY) {
    for (EntityID id = 0; id < em.active.size(); ++id) {
        if (!em.active[id] || !em.hasTransform[id]) {
            continue;
        }

        // Do not delete villagers or the Village Core.
        if (em.hasBehavior[id] || em.hasVillage[id]) {
            continue;
        }

        const int tx = WorldToTile(em.transforms[id].position.x);
        const int ty = WorldToTile(em.transforms[id].position.y);

        if (tx >= minX && tx <= maxX && ty >= minY && ty <= maxY) {
            em.DestroyEntity(id);
        }
    }
}

void SpawnDebugConstruction(EntityManager& em, ConstructionRegistry& constructionReg, const std::string& prefabId, int tileX, int tileY) {
    const Vector2 position = TileToWorldCenter(tileX, tileY);

    // If your ConstructionRegistry signature differs, this is the only line to adapt.
    constructionReg.SpawnConstruction(em, prefabId, position, false);
}

void SpawnDebugFurniture(EntityManager& em, FurnitureRegistry& furnitureReg, const std::string& prefabId, int tileX, int tileY) {
    const Vector2 position = TileToWorldCenter(tileX, tileY);
    furnitureReg.SpawnFurniture(em, prefabId, position, false);
}

/**
 * @brief Spawns a rectangular closed room around an interior area.
 *
 * interiorX/interiorY = top-left tile of the room interior.
 * interiorW/interiorH = walkable interior area.
 *
 * A wall ring is placed around the interior.
 * A door is placed on the south wall.
 */
void SpawnDebugRoomBox(EntityManager& em, ConstructionRegistry& constructionReg, int interiorX, int interiorY, int interiorW,
                       int interiorH) {
    const int wallMinX = interiorX - 1;
    const int wallMinY = interiorY - 1;
    const int wallMaxX = interiorX + interiorW;
    const int wallMaxY = interiorY + interiorH;

    const int doorX = interiorX;
    const int doorY = wallMaxY;

    ClearDebugArea(em, wallMinX, wallMinY, wallMaxX, wallMaxY);

    for (int y = wallMinY; y <= wallMaxY; ++y) {
        for (int x = wallMinX; x <= wallMaxX; ++x) {
            const bool isBorder = x == wallMinX || x == wallMaxX || y == wallMinY || y == wallMaxY;

            if (!isBorder) {
                continue;
            }

            if (x == doorX && y == doorY) {
                SpawnDebugConstruction(em, constructionReg, DEBUG_DOOR_PREFAB, x, y);
            } else {
                SpawnDebugConstruction(em, constructionReg, DEBUG_WALL_PREFAB, x, y);
            }
        }
    }
}

void SpawnDebugVillageTestStructures(EntityManager& em, FurnitureRegistry& furnitureReg, ConstructionRegistry& constructionReg,
                                     RoomSystem& roomSystem, StructureRegistry& structureReg, const WorldMap& worldMap,
                                     EntityID villageCore) {
    if (villageCore >= em.active.size() || !em.active[villageCore] || !em.hasTransform[villageCore]) {
        return;
    }

    const int coreX = WorldToTile(em.transforms[villageCore].position.x);
    const int coreY = WorldToTile(em.transforms[villageCore].position.y);

    // =========================================================
    // Lumberjack Sawmill
    // Structure:
    //   [SAWMILL_ROOM]
    //   min_area = 4
    //   requirements = SAWMILL_BENCH:1
    //   job_slots = lumberjack:2
    //
    // Interior: 2x2 = 4
    // =========================================================
    {
        const int x = coreX + 6;
        const int y = coreY - 5;

        SpawnDebugRoomBox(em, constructionReg, x, y, 2, 2);
        SpawnDebugFurniture(em, furnitureReg, "SAWMILL_BENCH", x, y);
    }

    // =========================================================
    // Gathering Tent
    // Structure:
    //   [GATHERING_TENT]
    //   min_area = 4
    //   requirements = GATHERING_BASKET:1
    //   job_slots = gatherer:1
    //
    // Interior: 2x2 = 4
    // =========================================================
    {
        const int x = coreX + 6;
        const int y = coreY + 4;

        SpawnDebugRoomBox(em, constructionReg, x, y, 2, 2);
        SpawnDebugFurniture(em, furnitureReg, "GATHERING_BASKET", x, y);
    }

    // =========================================================
    // Small Bedroom
    // Structure:
    //   [SMALL_BEDROOM]
    //   min_area = 4
    //   max_area = 4
    //   requirements = SMALL_BED:1
    //
    // Interior: 2x2 = 4
    // Housing capacity: 1
    // =========================================================
    {
        const int x = coreX - 7;
        const int y = coreY - 5;

        SpawnDebugRoomBox(em, constructionReg, x, y, 2, 2);
        SpawnDebugFurniture(em, furnitureReg, "SMALL_BED", x, y);
    }

    // =========================================================
    // Small Bedroom
    // Structure:
    //   [SMALL_BEDROOM]
    //   min_area = 4
    //   max_area = 4
    //   requirements = SMALL_BED:1
    //
    // Interior: 2x2 = 4
    // Housing capacity: 1
    // =========================================================
    {
        const int x = coreX - 13;
        const int y = coreY - 5;

        SpawnDebugRoomBox(em, constructionReg, x, y, 2, 2);
        SpawnDebugFurniture(em, furnitureReg, "SMALL_BED", x, y);
    }

    // =========================================================
    // Large Bedroom
    // Structure:
    //   [LARGE_BEDROOM]
    //   min_area = 4
    //   max_area = 8
    //   requirements = DOUBLE_BED:1
    //
    // Interior: 2x4 = 8
    // Housing capacity: 2
    // =========================================================
    {
        const int x = coreX - 7;
        const int y = coreY + 4;

        SpawnDebugRoomBox(em, constructionReg, x, y, 2, 4);
        SpawnDebugFurniture(em, furnitureReg, "DOUBLE_BED", x, y);
    }

    // =========================================================
    // Empty room / Spare Room
    // Interior area: 2x2 = 4
    //
    // Purpose:
    // - debug spare closed room;
    // - does not provide housing capacity yet;
    // - can later receive a SMALL_BED or another furniture;
    // - useful for testing room ownership / family assignment later.
    // =========================================================
    {
        const int x = coreX - 13;
        const int y = coreY + 5;

        SpawnDebugRoomBox(em, constructionReg, x, y, 2, 2);
    }

    roomSystem.MarkDirty();
    roomSystem.Update(em, worldMap, structureReg);

    std::cout << "[DEBUG] Spawned village test structures around Village Core." << std::endl;
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

    // TEMP DEBUG TEST SETUP.
    // Remove this once normal early-game construction is stable.
    SpawnDebugVillageTestStructures(m_entityManager, m_furnitureRegistry, m_constructionRegistry, m_roomSystem, m_structureRegistry,
                                    m_worldMap, villageCore);

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

    // m_villageSystem.Update(deltaTime, m_entityManager, m_entityRegistry, m_nameRegistry, m_behaviorRegistry, m_worldMap, m_tileRegistry,
    //                        m_resourceRegistry, m_timeSystem);

    m_timeSystem.Update(deltaTime, m_entityManager);

    m_eventSystem.Update(m_entityManager, m_timeSystem, m_resourceRegistry, m_settlementMetrics, m_chronicle);

    m_roomSystem.Update(m_entityManager, m_worldMap, m_structureRegistry);

    m_professionSystem.Update(deltaTime, m_entityManager, m_professionRegistry, m_behaviorRegistry);

    const Vector2 simulationCenter = m_camera.GetRaylibCamera().target;

    m_spatialGrid.Rebuild(m_entityManager);

    m_aiSystem.Update(deltaTime, m_entityManager, m_worldMap, m_tileRegistry, m_resourceRegistry, m_spatialGrid, simulationCenter,
                      static_cast<float>(Config::SIMULATION_ACTIVE_RADIUS_TILES), m_timeSystem.GetHour(), m_roomSystem);

    m_spatialGrid.Rebuild(m_entityManager);

    m_socialSystem.Update(deltaTime, m_entityManager, m_spatialGrid);

    m_householdSystem.Update(deltaTime, m_entityManager);

    m_villageSystem.Update(deltaTime, m_entityManager, m_entityRegistry, m_nameRegistry, m_behaviorRegistry, m_worldMap, m_tileRegistry,
                           m_resourceRegistry, m_timeSystem);
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

    // m_worldRenderSystem.ShowName(m_entityManager, camera, showNames);

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

    m_villageMenu.Render(m_entityManager, m_resourceRegistry, m_timeSystem, m_professionRegistry);

    EndDrawing();
}
