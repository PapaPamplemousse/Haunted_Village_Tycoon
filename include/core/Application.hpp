/**
 * @file Application.hpp
 * @brief Main game application class that owns high-level systems, registries, and orchestrates the update/render loop.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "core/Chronicle.hpp"
#include "core/GameCamera.hpp"
#include "core/InputManager.hpp"
#include "core/SettlementMetrics.hpp"
#include "data/BehaviorRegistry.hpp"
#include "data/BiomeRegistry.hpp"
#include "data/ConstructionRegistry.hpp"
#include "data/EntityRegistry.hpp"
#include "data/EnvironmentRegistry.hpp"
#include "data/FurnitureRegistry.hpp"
#include "data/NameRegistry.hpp"
#include "data/ProfessionRegistry.hpp"
#include "data/ResourceRegistry.hpp"
#include "data/StructureRegistry.hpp"
#include "data/TileRegistry.hpp"
#include "data/WeaponRegistry.hpp"
#include "debug/InspectionSystem.hpp"
#include "ecs/EntityManager.hpp"
#include "systems/AISystem.hpp"
#include "systems/BuildPlacementSystem.hpp"
#include "systems/ChronicleRenderSystem.hpp"
#include "systems/EventSystem.hpp"
#include "systems/LightingSystem.hpp"
#include "systems/ProfessionSystem.hpp"
#include "systems/RenderSystem.hpp"
#include "systems/RoomSystem.hpp"
#include "systems/TimeSystem.hpp"
#include "systems/VillageSystem.hpp"
#include "systems/WorldRenderSystem.hpp"
#include "ui/UIManager.hpp"
#include "world/EntitySpatialGrid.hpp"
#include "world/WorldMap.hpp"

#include <raylib.h>

/**
 * @class Application
 * @brief Main game application.
 *
 * Owns high-level systems and registries, and orchestrates update/render order.
 */
class Application {
public:
    Application();
    ~Application();

    void Run();

private:
    void Update(float deltaTime);
    void Render();

    bool m_isRunning = true;

    // Data registries
    TileRegistry m_tileRegistry;
    BiomeRegistry m_biomeRegistry;
    EntityRegistry m_entityRegistry;
    FurnitureRegistry m_furnitureRegistry;
    ConstructionRegistry m_constructionRegistry;
    StructureRegistry m_structureRegistry;
    NameRegistry m_nameRegistry;
    EnvironmentRegistry m_environmentRegistry;
    BehaviorRegistry m_behaviorRegistry;
    WeaponRegistry m_weaponRegistry;
    ProfessionRegistry m_professionRegistry;
    ResourceRegistry m_resourceRegistry;

    // World state
    WorldMap m_worldMap;
    EntityManager m_entityManager;
    EntitySpatialGrid m_spatialGrid;
    SettlementMetrics m_settlementMetrics;
    Chronicle m_chronicle;

    // Systems
    BuildPlacementSystem m_buildPlacementSystem;
    WorldRenderSystem m_worldRenderSystem;
    RenderSystem m_renderSystem;
    LightingSystem m_lightingSystem;
    EventSystem m_eventSystem;
    ChronicleRenderSystem m_chronicleRenderSystem;
    TimeSystem m_timeSystem;
    VillageSystem m_villageSystem;
    AISystem m_aiSystem;
    RoomSystem m_roomSystem;
    ProfessionSystem m_professionSystem;
    InspectionSystem m_inspectionSystem;

    // Core services
    GameCamera m_camera;
    InputManager m_inputManager;
    UIManager m_uiManager;
};
