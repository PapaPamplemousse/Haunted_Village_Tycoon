#include "systems/AISystem.hpp"

#include "core/Config.hpp"
#include "systems/Pathfinder.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <raymath.h>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr float ATTACK_DAMAGE = 10.0f;
constexpr float ATTACK_DURATION = 0.45f;
constexpr float ATTACK_COOLDOWN = 0.6f;

int ToTileCoord(float worldCoord) {
    return static_cast<int>(std::floor(worldCoord / Config::TILE_SIZE));
}

float SquaredDistance(Vector2 a, Vector2 b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

} // namespace

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

    const bool canHunt = HasCapability(behavior, "hunt");
    const bool canBuild = HasCapability(behavior, "build");
    const bool canDismantle = HasCapability(behavior, "dismantle");
    const bool canWander = HasCapability(behavior, "wander");

    // Combat / hostile behavior first.
    if (canHunt && TryFindHuntJob(i, em, map, tileReg)) {
        return;
    }

    // Work behaviors.
    if (canBuild && TryFindBuildJob(i, em, map, tileReg)) {
        return;
    }

    if (canDismantle && TryFindDismantleJob(i, em, map, tileReg)) {
        return;
    }

    // Fallback behavior.
    if (canWander && TryFindWanderJob(i, em, map, tileReg)) {
        return;
    }
}

void AISystem::InteractWithDoorIfPresent(EntityID entity, int targetX, int targetY, EntityManager& em) {
    for (size_t i = 0; i < em.active.size(); ++i) {
        if (!em.active[i] || !em.hasDoor[i] || !em.hasTransform[i]) {
            continue;
        }

        const int doorX = ToTileCoord(em.transforms[i].position.x);
        const int doorY = ToTileCoord(em.transforms[i].position.y);

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

    const int currentTileX = ToTileCoord(transform.position.x);
    const int currentTileY = ToTileCoord(transform.position.y);

    const int targetTileX = ToTileCoord(behavior.currentTarget.x);
    const int targetTileY = ToTileCoord(behavior.currentTarget.y);

    const int tileDx = std::abs(targetTileX - currentTileX);
    const int tileDy = std::abs(targetTileY - currentTileY);

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
        } else if (behavior.currentTask == "moving_to_hunt") {
            behavior.currentTask = "attacking";
            behavior.stateTimer = ATTACK_DURATION;
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
    } else if (behavior.currentTask == "attacking") {
        EntityID target = behavior.currentJobTarget;

        bool attackSucceeded = false;

        if (target < em.active.size() && em.active[target] && em.hasHealth[target] && em.hasTransform[target] &&
            AreEntitiesAdjacent(i, target, em)) {
            em.healths[target].current -= ATTACK_DAMAGE;
            attackSucceeded = true;

            if (em.healths[target].current <= 0.0f) {
                em.DestroyEntity(target);
            }
        }

        if (attackSucceeded) {
            behavior.stateTimer = ATTACK_COOLDOWN;
        }
    }

    ResetBehaviorState(behavior);
}

// ============================================================================
// HELPERS
// ============================================================================

bool AISystem::HasCapability(const BehaviorComponent& behavior, const std::string& capability) const {
    return std::find(behavior.innateCapabilities.begin(), behavior.innateCapabilities.end(), capability) !=
           behavior.innateCapabilities.end();
}

const BehaviorRule* AISystem::FindBehaviorRule(const BehaviorComponent& behavior, const std::string& ruleName) const {
    for (const BehaviorRule& rule : behavior.innateBehaviorRules) {
        if (rule.name == ruleName) {
            return &rule;
        }
    }

    return nullptr;
}

bool AISystem::IsSpeciesTargetedByRule(const BehaviorRule& rule, const std::string& species) const {
    for (const std::string& targetSpecies : rule.arguments) {
        if (targetSpecies == species) {
            return true;
        }
    }

    return false;
}

bool AISystem::AreEntitiesAdjacent(EntityID a, EntityID b, const EntityManager& em) const {
    if (a >= em.active.size() || b >= em.active.size() || !em.active[a] || !em.active[b] || !em.hasTransform[a] || !em.hasTransform[b]) {
        return false;
    }

    const int ax = ToTileCoord(em.transforms[a].position.x);
    const int ay = ToTileCoord(em.transforms[a].position.y);

    const int bx = ToTileCoord(em.transforms[b].position.x);
    const int by = ToTileCoord(em.transforms[b].position.y);

    const int dx = std::abs(ax - bx);
    const int dy = std::abs(ay - by);

    return std::max(dx, dy) <= 1;
}

void AISystem::ResetBehaviorState(BehaviorComponent& behavior) {
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

bool AISystem::TryFindHuntJob(EntityID hunter, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg) {
    if (hunter >= em.active.size() || !em.active[hunter] || !em.hasBehavior[hunter] || !em.hasTransform[hunter]) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[hunter];

    const BehaviorRule* huntRule = FindBehaviorRule(behavior, "hunt");

    if (huntRule == nullptr || huntRule->arguments.empty()) {
        return false;
    }

    EntityID bestTarget = static_cast<EntityID>(-1);
    float bestDistanceSq = std::numeric_limits<float>::infinity();

    const Vector2 hunterPosition = em.transforms[hunter].position;

    for (size_t target = 0; target < em.active.size(); ++target) {
        if (target == hunter) {
            continue;
        }

        if (!em.active[target] || !em.hasTag[target] || !em.hasTransform[target] || !em.hasHealth[target]) {
            continue;
        }

        if (em.healths[target].current <= 0.0f) {
            continue;
        }

        const std::string& targetSpecies = em.tags[target].species;

        if (!IsSpeciesTargetedByRule(*huntRule, targetSpecies)) {
            continue;
        }

        const float distanceSq = SquaredDistance(hunterPosition, em.transforms[target].position);

        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestTarget = target;
        }
    }

    if (bestTarget == static_cast<EntityID>(-1)) {
        return false;
    }

    if (AreEntitiesAdjacent(hunter, bestTarget, em)) {
        behavior.currentTask = "attacking";
        behavior.currentJobTarget = bestTarget;
        behavior.hasJob = true;
        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        behavior.stateTimer = ATTACK_DURATION;
        return true;
    }

    std::vector<Vector2> path =
        Pathfinder::FindPathToAdjacentTile(em.transforms[hunter].position, em.transforms[bestTarget].position, map, tileReg, em, hunter);

    if (path.empty()) {
        return false;
    }

    behavior.currentTask = "moving_to_hunt";
    behavior.currentJobTarget = bestTarget;
    behavior.hasJob = true;
    behavior.currentPath = std::move(path);
    behavior.currentPathIndex = 0;
    behavior.currentTarget = behavior.currentPath[0];
    behavior.isMoving = true;

    return true;
}

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
