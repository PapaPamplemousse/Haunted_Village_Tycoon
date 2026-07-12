/**
 * @file InspectionSystem.hpp
 * @brief Renders the CTRL + Hover debug inspection tooltips for entities.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "core/InputManager.hpp"
#include "data/ResourceRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "world/EntitySpatialGrid.hpp"

/**
 * @class InspectionSystem
 * @brief Renders CTRL + hover debug inspection tooltip.
 */
class InspectionSystem {
public:
    InspectionSystem() = default;

    // void Render(const InputManager& inputManager, const EntityManager& entityManager, const EntitySpatialGrid& spatialGrid) const;
    void Render(const InputManager& input, const EntityManager& em, const EntitySpatialGrid& spatialGrid,
                const ResourceRegistry& resourceReg) const;
};
