/**
 * @file SocialSystem.hpp
 * @brief Handles lightweight friendship, romance and partner formation between village members.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "ecs/EntityManager.hpp"
#include "world/EntitySpatialGrid.hpp"

/**
 * @class SocialSystem
 * @brief Updates social relationships between nearby compatible entities.
 *
 * V1 rules:
 * - only humans are processed;
 * - entities must belong to the same village;
 * - nearby entities gain friendship;
 * - adult compatible entities with enough friendship gain romance;
 * - high romance creates a stable partner link in FamilyComponent.
 */
class SocialSystem {
public:
    SocialSystem() = default;

    void Update(float deltaTime, EntityManager& em, const EntitySpatialGrid& spatialGrid);

private:
    float m_updateAccumulator = 0.0f;
};
