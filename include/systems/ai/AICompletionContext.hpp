/**
 * @file AICompletionContext.hpp
 * @brief Shared context passed to AI task completion handlers.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "data/ResourceRegistry.hpp"
#include "data/TileRegistry.hpp"
#include "data/WeaponRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "systems/RoomSystem.hpp"
#include "world/EntitySpatialGrid.hpp"
#include "world/WorldMap.hpp"

struct AICompletionContext {
    EntityID entity = static_cast<EntityID>(-1);

    EntityManager& em;
    const WorldMap& map;
    const TileRegistry& tileReg;
    const ResourceRegistry& resourceReg;
    const WeaponRegistry& weaponReg;
    const EntitySpatialGrid& spatialGrid;
    RoomSystem& roomSys;

    BehaviorComponent& behavior() const {
        return em.behaviors[entity];
    }
};
