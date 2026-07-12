/**
 * @file BuildPlacementSystem.hpp
 * @brief Handles player-driven blueprint placement and deconstruction logic.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "core/InputManager.hpp"
#include "data/BehaviorRegistry.hpp"
#include "data/ConstructionRegistry.hpp"
#include "data/EntityRegistry.hpp"
#include "data/FurnitureRegistry.hpp"
#include "data/NameRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "ui/UIManager.hpp"

/**
 * @class BuildPlacementSystem
 * @brief Handles player-driven placement and deletion/deconstruction marking.
 */
class BuildPlacementSystem {
public:
    BuildPlacementSystem() = default;

    void Update(const InputManager& inputManager, const UIManager& uiManager, EntityManager& entityManager, EntityRegistry& entityRegistry,
                FurnitureRegistry& furnitureRegistry, ConstructionRegistry& constructionRegistry, const NameRegistry& nameRegistry,
                const BehaviorRegistry& behaviorRegistry) const;
};
