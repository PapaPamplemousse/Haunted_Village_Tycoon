#include "systems/AISystem.hpp"

#include "core/Config.hpp"
#include "systems/Pathfinder.hpp"

#include <algorithm>
#include <cmath>
#include <raymath.h>
#include <utility>
#include <vector>
void AISystem::Update(float deltaTime, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg, RoomSystem& roomSys) {
    for (size_t i = 0; i < em.active.size(); ++i) {
        if (!em.active[i] || !em.hasBehavior[i] || !em.hasTransform[i] || !em.hasStats[i]) {
            continue;
        }

        auto& behavior = em.behaviors[i];

        if (behavior.stateTimer > 0.0f) {
            behavior.stateTimer -= deltaTime;
            continue;
        }

        if (behavior.currentTask == "idle") {
            HandleIdleState(i, em, map, tileReg);
        } else if (behavior.isMoving) {
            HandleMovingState(i, deltaTime, em);
        } else {
            HandleTaskCompletion(i, em, roomSys);
        }
    }
}

// ============================================================================
// STATE HANDLERS
// ============================================================================

void AISystem::HandleIdleState(EntityID i, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg) {
    auto& behavior = em.behaviors[i];

    bool canBuild = false;
    bool canDismantle = false;
    bool canWander = false;

    for (const auto& cap : behavior.innateCapabilities) {
        if (cap == "build") {
            canBuild = true;
        }

        if (cap == "dismantle") {
            canDismantle = true;
        }

        if (cap == "wander") {
            canWander = true;
        }
    }

    if (canBuild && TryFindBuildJob(i, em, map, tileReg)) {
        return;
    }

    if (canDismantle && TryFindDismantleJob(i, em, map, tileReg)) {
        return;
    }

    if (canWander && TryFindWanderJob(i, em, map, tileReg)) {
        return;
    }
}

void AISystem::InteractWithDoorIfPresent(EntityID entity, int targetX, int targetY, EntityManager& em) {
    for (size_t i = 0; i < em.active.size(); ++i) {
        if (!em.active[i] || !em.hasDoor[i] || !em.hasTransform[i]) {
            continue;
        }

        const int doorX = static_cast<int>(std::floor(em.transforms[i].position.x / Config::TILE_SIZE));

        const int doorY = static_cast<int>(std::floor(em.transforms[i].position.y / Config::TILE_SIZE));

        if (doorX != targetX || doorY != targetY) {
            continue;
        }

        DoorComponent& door = em.doors[i];

        if (door.state == DoorState::LOCKED && door.ownerId != entity) {
            return;
        }

        if (door.state == DoorState::CLOSED) {
            door.state = DoorState::OPEN;
        }

        return;
    }
}

void AISystem::HandleMovingState(EntityID i, float deltaTime, EntityManager& em) {
    auto& behavior = em.behaviors[i];
    auto& transform = em.transforms[i];

    const float speed = em.stats[i].maxSpeed;

    if (behavior.currentPath.empty() || behavior.currentPathIndex >= behavior.currentPath.size()) {
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        return;
    }

    behavior.currentTarget = behavior.currentPath[behavior.currentPathIndex];

    const int currentTileX = static_cast<int>(std::floor(transform.position.x / Config::TILE_SIZE));

    const int currentTileY = static_cast<int>(std::floor(transform.position.y / Config::TILE_SIZE));

    const int targetTileX = static_cast<int>(std::floor(behavior.currentTarget.x / Config::TILE_SIZE));

    const int targetTileY = static_cast<int>(std::floor(behavior.currentTarget.y / Config::TILE_SIZE));

    const int tileDx = std::abs(targetTileX - currentTileX);
    const int tileDy = std::abs(targetTileY - currentTileY);

    // Chebyshev distance: works for both orthogonal and diagonal adjacency.
    if (std::max(tileDx, tileDy) <= 1) {
        InteractWithDoorIfPresent(i, targetTileX, targetTileY, em);
    }

    Vector2 dir = Vector2Subtract(behavior.currentTarget, transform.position);
    const float distanceToTarget = Vector2Length(dir);
    const float step = speed * deltaTime;

    if (distanceToTarget <= step || distanceToTarget < 1.0f) {
        transform.position = behavior.currentTarget;

        behavior.currentPathIndex++;

        if (behavior.currentPathIndex < behavior.currentPath.size()) {
            behavior.currentTarget = behavior.currentPath[behavior.currentPathIndex];
            return;
        }

        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;

        if (behavior.currentTask == "moving_to_build") {
            behavior.currentTask = "building";
            behavior.stateTimer = 2.0f;
        } else if (behavior.currentTask == "moving_to_dismantle") {
            behavior.currentTask = "dismantling";
            behavior.stateTimer = 2.0f;
        } else if (behavior.currentTask == "wandering") {
            behavior.currentTask = "idle";
            behavior.stateTimer = GetRandomValue(10, 40) / 10.0f;
        }

        return;
    }

    if (distanceToTarget <= 0.0f) {
        return;
    }

    Vector2 normalizedDir = Vector2Scale(dir, 1.0f / distanceToTarget);

    transform.position.x += normalizedDir.x * step;
    transform.position.y += normalizedDir.y * step;
}

void AISystem::HandleTaskCompletion(EntityID i, EntityManager& em, RoomSystem& roomSys) {
    auto& behavior = em.behaviors[i];

    if (behavior.currentTask == "building") {
        EntityID target = behavior.currentJobTarget;

        if (target < em.active.size() && em.active[target] && em.hasBlueprint[target]) {
            if (em.hasInventory[i]) {
                for (const auto& req : em.blueprints[target].requiredMaterials) {
                    em.inventories[i].items[req.first] -= req.second;
                }
            }

            em.blueprints[target].isFinished = true;
            em.hasBlueprint[target] = false;

            if (em.hasConstruction[target] || (em.hasTag[target] && !em.hasBehavior[target])) {
                roomSys.MarkDirty();
            }
        }
    } else if (behavior.currentTask == "dismantling") {
        EntityID target = behavior.currentJobTarget;

        if (target < em.active.size() && em.active[target] && em.hasDeconstruct[target]) {
            if (em.hasCost[target] && em.hasInventory[i]) {
                for (const auto& req : em.costs[target].materials) {
                    int refund = std::max(1, req.second / 2);
                    em.inventories[i].items[req.first] += refund;
                }
            }

            if (em.hasConstruction[target] || (em.hasTag[target] && !em.hasBehavior[target])) {
                roomSys.MarkDirty();
            }

            em.DestroyEntity(target);
        }
    }

    behavior.currentTask = "idle";
    behavior.hasJob = false;
    behavior.currentJobTarget = 0;
    behavior.currentPath.clear();
    behavior.currentPathIndex = 0;
    behavior.isMoving = false;
}

// ============================================================================
// JOB SEARCHERS
// ============================================================================

bool AISystem::TryFindBuildJob(EntityID i, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg) {
    for (size_t j = 0; j < em.active.size(); ++j) {
        if (!em.active[j] || !em.hasBlueprint[j] || em.blueprints[j].isFinished || !em.hasTransform[j]) {
            continue;
        }

        bool canAfford = true;

        if (em.hasInventory[i]) {
            for (const auto& req : em.blueprints[j].requiredMaterials) {
                if (em.inventories[i].items[req.first] < req.second) {
                    canAfford = false;
                    break;
                }
            }
        }

        if (!canAfford) {
            continue;
        }

        std::vector<Vector2> path =
            Pathfinder::FindPathToAdjacentTile(em.transforms[i].position, em.transforms[j].position, map, tileReg, em, i);

        if (path.empty()) {
            continue;
        }

        auto& behavior = em.behaviors[i];

        behavior.currentTask = "moving_to_build";
        behavior.currentJobTarget = j;
        behavior.hasJob = true;
        behavior.currentPath = std::move(path);
        behavior.currentPathIndex = 0;
        behavior.currentTarget = behavior.currentPath[0];
        behavior.isMoving = true;

        return true;
    }

    return false;
}

bool AISystem::TryFindDismantleJob(EntityID i, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg) {
    for (size_t j = 0; j < em.active.size(); ++j) {
        if (!em.active[j] || !em.hasDeconstruct[j] || !em.hasTransform[j]) {
            continue;
        }

        std::vector<Vector2> path =
            Pathfinder::FindPathToAdjacentTile(em.transforms[i].position, em.transforms[j].position, map, tileReg, em, i);

        if (path.empty()) {
            continue;
        }

        auto& behavior = em.behaviors[i];

        behavior.currentTask = "moving_to_dismantle";
        behavior.currentJobTarget = j;
        behavior.hasJob = true;
        behavior.currentPath = std::move(path);
        behavior.currentPathIndex = 0;
        behavior.currentTarget = behavior.currentPath[0];
        behavior.isMoving = true;

        return true;
    }

    return false;
}

bool AISystem::TryFindWanderJob(EntityID i, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg) {
    auto& behavior = em.behaviors[i];

    constexpr int MaxAttempts = 8;

    for (int attempt = 0; attempt < MaxAttempts; ++attempt) {
        const float angle = GetRandomValue(0, 360) * DEG2RAD;

        const float distance =
            static_cast<float>(GetRandomValue(static_cast<int>(Config::TILE_SIZE * 2), static_cast<int>(Config::TILE_SIZE * 8)));

        Vector2 proposedTarget = {em.transforms[i].position.x + std::cos(angle) * distance,
                                  em.transforms[i].position.y + std::sin(angle) * distance};

        std::vector<Vector2> path = Pathfinder::FindPath(em.transforms[i].position, proposedTarget, map, tileReg, em, i);

        if (path.empty()) {
            continue;
        }

        auto& behavior = em.behaviors[i];

        behavior.currentTask = "wandering";
        behavior.currentPath = std::move(path);
        behavior.currentPathIndex = 0;
        behavior.currentTarget = behavior.currentPath[0];
        behavior.isMoving = true;

        return true;
    }

    behavior.stateTimer = 1.0f;
    return false;
}
