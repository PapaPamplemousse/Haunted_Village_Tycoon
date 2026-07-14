/**
 * @file AITaskExecutor.cpp
 * @brief Implementation of centralized AI task start helpers.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/ai/AITaskExecutor.hpp"

#include "core/Config.hpp"
#include "systems/Pathfinder.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace {

int WorldToTileCoord(float value) {
    return static_cast<int>(std::floor(value / Config::TILE_SIZE));
}

bool HasValidBehavior(EntityID entity, const EntityManager& em) {
    return entity < em.active.size() && em.active[entity] && em.hasBehavior[entity] && em.hasTransform[entity];
}

bool HasValidTransform(EntityID entity, const EntityManager& em) {
    return entity < em.active.size() && em.active[entity] && em.hasTransform[entity];
}

} // namespace

bool AITaskExecutor::AreEntitiesAdjacent(EntityID a, EntityID b, const EntityManager& em) {
    if (!HasValidTransform(a, em) || !HasValidTransform(b, em)) {
        return false;
    }

    const int ax = WorldToTileCoord(em.transforms[a].position.x);
    const int ay = WorldToTileCoord(em.transforms[a].position.y);
    const int bx = WorldToTileCoord(em.transforms[b].position.x);
    const int by = WorldToTileCoord(em.transforms[b].position.y);

    return std::max(std::abs(ax - bx), std::abs(ay - by)) <= 1;
}

bool AITaskExecutor::StartAction(EntityID actor, EntityID target, EntityManager& em, const std::string& actionTask, float actionDuration) {
    if (!HasValidBehavior(actor, em)) {
        return false;
    }

    if (target != static_cast<EntityID>(-1) && target >= em.active.size()) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[actor];

    behavior.currentTask = actionTask;
    behavior.currentJobTarget = target;
    behavior.hasJob = true;
    behavior.isMoving = false;
    behavior.currentPath.clear();
    behavior.currentPathIndex = 0;
    behavior.stateTimer = actionDuration;

    return true;
}

bool AITaskExecutor::StartMoveAdjacentToEntity(EntityID actor, EntityID target, EntityManager& em, const WorldMap& map,
                                               const TileRegistry& tileReg, const std::string& moveTask, const std::string& actionTask,
                                               float actionDuration) {
    if (!HasValidBehavior(actor, em) || !HasValidTransform(target, em)) {
        return false;
    }

    if (AreEntitiesAdjacent(actor, target, em)) {
        return StartAction(actor, target, em, actionTask, actionDuration);
    }

    std::vector<Vector2> path =
        Pathfinder::FindPathToAdjacentTile(em.transforms[actor].position, em.transforms[target].position, map, tileReg, em, actor);

    if (path.empty()) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[actor];

    behavior.currentTask = moveTask;
    behavior.currentJobTarget = target;
    behavior.hasJob = true;
    behavior.currentPath = std::move(path);
    behavior.currentPathIndex = 0;
    behavior.currentTarget = behavior.currentPath[0];
    behavior.isMoving = true;
    behavior.stateTimer = 0.0f;

    return true;
}

bool AITaskExecutor::ApplyIntent(EntityID actor, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                 const AIIntent& intent) {
    switch (intent.kind) {
        case AIIntentKind::StartAction:
            return StartAction(actor, intent.targetEntity, em, intent.actionTask, intent.actionDuration);

        case AIIntentKind::MoveAdjacentToEntity:
            return StartMoveAdjacentToEntity(actor, intent.targetEntity, em, map, tileReg, intent.moveTask, intent.actionTask,
                                             intent.actionDuration);

        case AIIntentKind::None:
        default:
            return false;
    }
}
