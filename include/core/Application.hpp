#pragma once
#include "core/GameCamera.hpp"
#include "core/InputManager.hpp"
#include "data/BehaviorRegistry.hpp"
#include "data/BiomeRegistry.hpp"
#include "data/ConstructionRegistry.hpp"
#include "data/EntityRegistry.hpp"
#include "data/EnvironmentRegistry.hpp"
#include "data/FurnitureRegistry.hpp"
#include "data/NameRegistry.hpp"
#include "data/StructureRegistry.hpp"
#include "data/TileRegistry.hpp"
#include "data/WeaponRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "systems/AISystem.hpp"
#include "systems/RenderSystem.hpp"
#include "systems/RoomSystem.hpp"
#include "systems/TimeSystem.hpp"
#include "ui/UIManager.hpp"
#include "world/WorldMap.hpp"

#include <raylib.h>

/**
 * @class Application
 * @brief The core engine class responsible for the main game loop, window management, and module routing.
 * * The Application class owns the highest level of the game architecture. It initializes Raylib,
 * calculates the delta time, and delegates the logic to the respective Systems.
 */
class Application {
public:
    /**
     * @brief Constructs the Application and initializes the window.
     */
    Application();

    /**
     * @brief Destroys the Application and safely closes the window.
     */
    ~Application();

    /**
     * @brief Starts the main infinite game loop.
     * Blocks execution until the user requests to close the window.
     */
    void Run();

private:
    /**
     * @brief Updates the game logic.
     * @param deltaTime Time elapsed since the last frame in seconds.
     */
    void Update(float deltaTime);

    /**
     * @brief Renders the game state to the screen.
     */
    void Render();

    const int WINDOW_WIDTH = 1280;
    const int WINDOW_HEIGHT = 720;

    bool m_isRunning;
    TileRegistry m_tileRegistry;
    BiomeRegistry m_biomeRegistry;
    WorldMap m_worldMap;
    EntityManager m_entityManager;
    EntityRegistry m_entityRegistry;
    FurnitureRegistry m_furnitureRegistry;
    RenderSystem m_renderSystem;
    TimeSystem m_timeSystem;
    AISystem m_aiSystem;
    GameCamera m_camera;
    InputManager m_inputManager;
    UIManager m_uiManager;
    ConstructionRegistry m_constructionRegistry;
    StructureRegistry m_structureRegistry;
    RoomSystem m_roomSystem;
    NameRegistry m_nameRegistry;
    EnvironmentRegistry m_environmentRegistry;
    BehaviorRegistry m_behaviorRegistry;
    WeaponRegistry m_weaponRegistry;
};
