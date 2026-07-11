#pragma once

#include "core/InputManager.hpp"
#include "ecs/EntityManager.hpp"
#include "world/EntitySpatialGrid.hpp"

/**
 * @class InspectionSystem
 * @brief Renders CTRL + hover debug inspection tooltip.
 */
class InspectionSystem {
public:
    InspectionSystem() = default;

    void Render(const InputManager& inputManager, const EntityManager& entityManager, const EntitySpatialGrid& spatialGrid) const;
};
