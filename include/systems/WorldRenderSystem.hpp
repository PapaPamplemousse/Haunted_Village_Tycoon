/**
 * @file WorldRenderSystem.hpp
 * @brief Renders terrain layers, room floors and hover tiles.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "data/TileRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "graphics/TextureCache.hpp"
#include "world/WorldMap.hpp"

#include <raylib.h>

/**
 * @class WorldRenderSystem
 * @brief Renders terrain, hover tile and room floor overlays.
 */
class WorldRenderSystem {
public:
    WorldRenderSystem() = default;
    ~WorldRenderSystem() = default;

    void Render(const EntityManager& entityManager, const WorldMap& worldMap, const TileRegistry& tileRegistry, const Camera2D& camera,
                int hoverX, int hoverY, bool showNames, int seasonIndex, TextureCache& textureCache) const;
};
