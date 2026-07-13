/**
 * @file AISystemMovement.cpp
 * @brief Handles physical entity movement along paths and door interactions.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/AISystem.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/Pathfinder.hpp"

#include <algorithm>
#include <cmath>
#include <raymath.h>
#include <string>
#include <utility>
#include <vector>

namespace {

void UpdateSpriteFacingFromDirection(EntityID entity, EntityManager& em, Vector2 direction) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasSprite[entity]) {
        return;
    }

    if (std::fabs(direction.x) < 0.001f && std::fabs(direction.y) < 0.001f) {
        return;
    }

    SpriteComponent& sprite = em.sprites[entity];

    if (std::fabs(direction.x) > std::fabs(direction.y)) {
        sprite.facing = direction.x >= 0.0f ? SpriteFacing::Right : SpriteFacing::Left;
    } else {
        sprite.facing = direction.y >= 0.0f ? SpriteFacing::Down : SpriteFacing::Up;
    }
}

bool IsValidHaulContext(EntityID entity, const EntityManager& em) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasAIContext[entity]) {
        return false;
    }

    const AIContextComponent& context = em.aiContexts[entity];

    return context.haulSourceId < em.active.size() && context.haulDestinationId < em.active.size() && em.active[context.haulSourceId] &&
           em.active[context.haulDestinationId] && !context.haulItemId.empty() && context.haulAmount > 0;
}

bool CompleteHaulPickupAndStartDestinationPath(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg) {
    if (!IsValidHaulContext(entity, em) || !em.hasInventory[entity] || !em.hasBehavior[entity] || !em.hasTransform[entity]) {
        return false;
    }

    AIContextComponent& context = em.aiContexts[entity];

    const EntityID source = context.haulSourceId;
    const EntityID destination = context.haulDestinationId;
    const std::string itemId = context.haulItemId;

    if (!em.hasInventory[source] || !em.hasInventory[destination] || !em.hasStorage[destination] || !em.hasTransform[destination]) {
        return false;
    }

    const int remainingCapacity = em.storages[destination].capacity - AISystemUtils::GetInventoryItemCount(em.inventories[destination]);

    if (remainingCapacity <= 0) {
        return false;
    }

    const int amountToMove = std::min(context.haulAmount, remainingCapacity);

    const int removed = AISystemUtils::RemoveItemFromInventory(em.inventories[source], itemId, amountToMove);

    if (removed <= 0) {
        return false;
    }

    em.inventories[entity].items[itemId] += removed;
    context.haulAmount = removed;

    std::vector<Vector2> path =
        Pathfinder::FindPathToAdjacentTile(em.transforms[entity].position, em.transforms[destination].position, map, tileReg, em, entity);

    if (path.empty()) {
        return false;
    }

    BehaviorComponent& behavior = em.behaviors[entity];

    behavior.currentTask = "moving_to_haul_destination";
    behavior.currentJobTarget = destination;
    behavior.currentItemTarget = itemId;
    behavior.hasJob = true;
    behavior.currentPath = std::move(path);
    behavior.currentPathIndex = 0;
    behavior.currentTarget = behavior.currentPath[0];
    behavior.isMoving = true;
    behavior.stateTimer = 0.0f;

    return true;
}

} // namespace

void AISystem::HandleMovingState(EntityID i, float deltaTime, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg) {
    auto& behavior = em.behaviors[i];
    auto& transform = em.transforms[i];

    const float speed = em.stats[i].maxSpeed;

    if (behavior.currentPath.empty() || behavior.currentPathIndex >= behavior.currentPath.size()) {
        if (behavior.currentTask == "moving_to_rest" || behavior.currentTask == "resting") {
            AISystemUtils::ReleaseRestSpotReservation(i, em);
        }

        behavior.isMoving = false;
        behavior.currentPath.clear();
        behavior.currentPathIndex = 0;
        behavior.reservedRestSpot = static_cast<EntityID>(-1);
        return;
    }

    behavior.currentTarget = behavior.currentPath[behavior.currentPathIndex];

    const int currentTileX = AISystemUtils::ToTileCoord(transform.position.x);
    const int currentTileY = AISystemUtils::ToTileCoord(transform.position.y);

    const int targetTileX = AISystemUtils::ToTileCoord(behavior.currentTarget.x);
    const int targetTileY = AISystemUtils::ToTileCoord(behavior.currentTarget.y);

    const int tileDx = std::abs(targetTileX - currentTileX);
    const int tileDy = std::abs(targetTileY - currentTileY);

    if (std::max(tileDx, tileDy) <= 1) {
        InteractWithDoorIfPresent(i, targetTileX, targetTileY, em);
    }

    Vector2 dir = Vector2Subtract(behavior.currentTarget, transform.position);
    UpdateSpriteFacingFromDirection(i, em, dir);

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
        } else if (behavior.currentTask == "moving_to_haul_source") {
            if (!CompleteHaulPickupAndStartDestinationPath(i, em, map, tileReg)) {
                ResetBehaviorState(behavior);

                if (i < em.active.size() && em.active[i] && em.hasAIContext[i]) {
                    em.aiContexts[i].haulSourceId = static_cast<EntityID>(-1);
                    em.aiContexts[i].haulDestinationId = static_cast<EntityID>(-1);
                    em.aiContexts[i].haulItemId.clear();
                    em.aiContexts[i].haulAmount = 0;
                }
            }
        } else if (behavior.currentTask == "moving_to_haul_destination") {
            behavior.currentTask = "hauling_deposit";
            behavior.stateTimer = AISystemUtils::DEPOSIT_DURATION;
        } else if (behavior.currentTask == "moving_to_rest") {
            behavior.currentTask = "resting";
            behavior.stateTimer = 1.0f;
        } else if (behavior.currentTask == "moving_to_storage") {
            behavior.currentTask = "depositing";
            behavior.stateTimer = AISystemUtils::DEPOSIT_DURATION;
        } else if (behavior.currentTask == "moving_to_food_storage") {
            behavior.currentTask = "eating_from_storage";
            behavior.stateTimer = AISystemUtils::EAT_DURATION;
        } else if (behavior.currentTask == "moving_to_feed_child") {
            behavior.currentTask = "feeding_child";
            behavior.stateTimer = AISystemUtils::EAT_DURATION;

        } else if (behavior.currentTask == "moving_to_harvest") {
            behavior.currentTask = "harvesting";
            behavior.stateTimer = 1.0f;
            behavior.actionAccumulator = 0.0f;
        } else if (behavior.currentTask == "moving_to_hunt") {
            behavior.currentTask = "attacking";
            behavior.stateTimer = AISystemUtils::ATTACK_DURATION;
        } else if (behavior.currentTask == "moving_to_flee") {
            behavior.currentTask = "idle";
            behavior.currentJobTarget = 0;
            behavior.hasJob = false;
            behavior.stateTimer = 1.0f;

            if (i < em.active.size() && em.active[i] && em.hasAIContext[i]) {
                em.aiContexts[i].currentTaskPriority = 0.0f;
                em.aiContexts[i].currentTaskInterruptible = true;
            }
        } else if (behavior.currentTask == "moving_to_village_core") {
            behavior.currentTask = "idle";
            behavior.currentJobTarget = 0;
            behavior.hasJob = false;
            behavior.stateTimer = GetRandomValue(10, 30) / 10.0f;

            if (i < em.active.size() && em.active[i] && em.hasAIContext[i]) {
                em.aiContexts[i].currentTaskPriority = 0.0f;
                em.aiContexts[i].currentTaskInterruptible = true;
            }
        } else if (behavior.currentTask == "moving_to_repair") {
            behavior.currentTask = "repairing";
            behavior.stateTimer = 1.0f;
        } else if (behavior.currentTask == "patrolling") {
            behavior.currentTask = "idle";
            behavior.currentJobTarget = 0;
            behavior.hasJob = false;
            behavior.stateTimer = GetRandomValue(10, 30) / 10.0f;
        } else if (behavior.currentTask == "moving_to_weapon_storage") {
            behavior.currentTask = "equipping_weapon";
            behavior.stateTimer = 0.4f;
        } else if (behavior.currentTask == "moving_to_forge") {
            behavior.currentTask = "crafting_weapon";
            behavior.stateTimer = 3.0f;
        } else if (behavior.currentTask == "moving_to_weapon_deposit") {
            behavior.currentTask = "depositing_crafted_weapon";
            behavior.stateTimer = AISystemUtils::DEPOSIT_DURATION;
        } else if (behavior.currentTask == "moving_to_request_weapon") {
            behavior.currentTask = "requesting_weapon";
            behavior.stateTimer = 1.2f;
        } else if (behavior.currentTask == "moving_to_forge") {
            behavior.currentTask = "crafting_weapon";
            behavior.stateTimer = 8.0f;
        } else if (behavior.currentTask == "moving_to_crafted_weapon_storage") {
            behavior.currentTask = "depositing_crafted_weapon";
            behavior.stateTimer = AISystemUtils::DEPOSIT_DURATION;
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

void AISystem::InteractWithDoorIfPresent(EntityID entity, int targetX, int targetY, EntityManager& em) {
    for (size_t i = 0; i < em.active.size(); ++i) {
        if (!em.active[i] || !em.hasDoor[i] || !em.hasTransform[i]) {
            continue;
        }

        const int doorX = AISystemUtils::ToTileCoord(em.transforms[i].position.x);
        const int doorY = AISystemUtils::ToTileCoord(em.transforms[i].position.y);

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
