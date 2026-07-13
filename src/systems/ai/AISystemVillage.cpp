/**
 * @file AISystemVillage.cpp
 * @brief AI jobs related to village anchoring and return-to-core behavior.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Config.hpp"
#include "systems/AISystem.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/Pathfinder.hpp"

#include <cmath>
#include <utility>
#include <vector>

namespace {

constexpr float DAWN_RETURN_MIN_HOUR = 5.0f;
constexpr float DAWN_RETURN_MAX_HOUR = 7.0f;
constexpr float VILLAGE_CORE_RETURN_DISTANCE_TILES = 6.0f;

bool IsDawnReturnWindow(float hour) {
    return hour >= DAWN_RETURN_MIN_HOUR && hour < DAWN_RETURN_MAX_HOUR;
}

bool IsValidVillageCore(EntityID villageId, const EntityManager& em) {
    return villageId < em.active.size() && em.active[villageId] && em.hasVillage[villageId] && em.hasTransform[villageId];
}

float DistanceSq(Vector2 a, Vector2 b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;

    return dx * dx + dy * dy;
}

} // namespace

bool AISystem::TryFindReturnToVillageCoreJob(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasBehavior[entity] || !em.hasTransform[entity] ||
        !em.hasVillageMember[entity]) {
        return false;
    }

    const EntityID villageId = em.villageMembers[entity].villageId;

    if (!IsValidVillageCore(villageId, em)) {
        return false;
    }

    const Vector2 entityPos = em.transforms[entity].position;
    const Vector2 villagePos = em.transforms[villageId].position;

    const float minDistance = VILLAGE_CORE_RETURN_DISTANCE_TILES * Config::TILE_SIZE;

    if (DistanceSq(entityPos, villagePos) <= minDistance * minDistance) {
        return false;
    }

    std::vector<Vector2> path = Pathfinder::FindPathToAdjacentTile(entityPos, villagePos, map, tileReg, em, entity);

    if (path.empty()) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[entity];

    behavior.currentTask = "moving_to_village_core";
    behavior.currentJobTarget = villageId;
    behavior.hasJob = true;
    behavior.currentPath = std::move(path);
    behavior.currentPathIndex = 0;
    behavior.currentTarget = behavior.currentPath[0];
    behavior.isMoving = true;
    behavior.stateTimer = 0.0f;

    if (em.hasAIContext[entity]) {
        em.aiContexts[entity].currentTaskPriority = 610.0f;
        em.aiContexts[entity].currentTaskInterruptible = true;
    }

    return true;
}
