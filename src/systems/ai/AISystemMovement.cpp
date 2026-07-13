/**
 * @file AISystemMovement.cpp
 * @brief Handles physical entity movement along paths and door interactions.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/AISystem.hpp"
#include "systems/AISystemUtils.hpp"

#include <algorithm>
#include <cmath>
#include <raymath.h>

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

} // namespace

void AISystem::HandleMovingState(EntityID i, float deltaTime, EntityManager& em) {
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
