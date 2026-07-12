/**
 * @file RenderSystem.hpp
 * @brief Responsible for drawing all visible entities and sprites to the screen.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "ecs/EntityManager.hpp"
#include "world/EntitySpatialGrid.hpp"

#include <raylib.h>

/**
 * @class RenderSystem
 * @brief Responsible for drawing all visible entities to the screen.
 */
class RenderSystem {
public:
    RenderSystem() = default;
    ~RenderSystem() = default;

    void Render(const EntityManager& em, const EntitySpatialGrid& spatialGrid, const Camera2D& camera, bool showNames) const;
};
