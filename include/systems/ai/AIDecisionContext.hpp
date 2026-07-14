/**
 * @file AIDecisionContext.hpp
 * @brief Read-only context passed to AI decision providers.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "data/ResourceRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "world/EntitySpatialGrid.hpp"

struct AIDecisionContext {
    EntityID entity = static_cast<EntityID>(-1);

    EntityManager& em;
    const ResourceRegistry& resourceReg;
    const EntitySpatialGrid& spatialGrid;

    float currentHour = 0.0f;

    BehaviorComponent& behavior() const {
        return em.behaviors[entity];
    }

    bool hasAIContext() const {
        return entity < em.active.size() && em.active[entity] && em.hasAIContext[entity];
    }

    AIContextComponent& aiContext() const {
        return em.aiContexts[entity];
    }
};
