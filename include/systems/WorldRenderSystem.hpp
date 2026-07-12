/**
 * @file WorldRenderSystem.hpp
 * @brief Renders terrain layers, room floors, hover tiles, and world labels.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "data/TileRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "world/WorldMap.hpp"

#include <raylib.h>

/**
 * @class WorldRenderSystem
 * @brief Renders terrain, hover tile, room floors and room labels.
 */
class WorldRenderSystem {
public:
    WorldRenderSystem() = default;

    void Render(const EntityManager& entityManager, const WorldMap& worldMap, const TileRegistry& tileRegistry, const Camera2D& camera,
                int hoverX, int hoverY, bool showNames) const;

    void ShowName(const EntityManager& entityManager, const Camera2D& camera, bool showNames) const;
};
