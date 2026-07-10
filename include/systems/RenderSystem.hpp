#pragma once
#include "ecs/EntityManager.hpp"

#include <raylib.h>

/**
 * @class RenderSystem
 * @brief Responsible for drawing all entities to the screen based on their SpriteComponent.
 * Acts purely as a renderer, does not mutate the game state.
 */
class RenderSystem {
public:
    RenderSystem() = default;
    ~RenderSystem() = default;

    /**
     * @brief Renders all visible entities.
     * @param em Constant reference to the EntityManager.
     * @param camera Constant reference to the Camera2D for view transformations.
     */
    void Render(const EntityManager& em, const Camera2D& camera, bool showNames) const;
};
