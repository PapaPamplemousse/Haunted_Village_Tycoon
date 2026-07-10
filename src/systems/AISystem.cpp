#include "systems/AISystem.hpp"

#include "core/Config.hpp"

#include <cmath>
#include <raymath.h>

const float TILE_SIZE = 32.0f;

void AISystem::Update(float deltaTime, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg) {
    for (size_t i = 0; i < em.active.size(); ++i) {
        if (!em.active[i] || !em.hasBehavior[i] || !em.hasTransform[i] || !em.hasStats[i])
            continue;

        auto& behavior = em.behaviors[i];
        auto& transform = em.transforms[i];
        const auto& stats = em.stats[i];

        // 1. Handle waiting state (doing nothing for a few seconds)
        if (behavior.stateTimer > 0.0f) {
            behavior.stateTimer -= deltaTime;
            continue;
        }

        // 2. Simple State Machine: WANDERING
        // Check if the entity has the "wander" capability natively
        bool canWander = false;
        for (const auto& cap : behavior.innateCapabilities) {
            if (cap == "wander")
                canWander = true;
        }

        if (canWander) {
            if (!behavior.isMoving) {
                // DECISION: Pick a random nearby destination (20 to 80 pixels away)
                float angle = GetRandomValue(0, 360) * DEG2RAD;
                float distance = GetRandomValue(20, 80);

                Vector2 proposedTarget = {transform.position.x + std::cos(angle) * distance,
                                          transform.position.y + std::sin(angle) * distance};

                // Convert world position to grid coordinates
                int gridX = static_cast<int>(proposedTarget.x / Config::TILE_SIZE); // TILE_SIZE = 8
                int gridY = static_cast<int>(proposedTarget.y / Config::TILE_SIZE);

                // Check if the target is physically walkable!
                int tileId = map.GetTile(gridX, gridY);
                const TileDef* tileDef = tileReg.GetTileDef(tileId);

                if (tileDef && tileDef->walkable) {
                    behavior.currentTarget = proposedTarget;
                    behavior.isMoving = true;
                } else {
                    // It hit water or a wall. Wait a bit, then try again.
                    behavior.stateTimer = GetRandomValue(5, 15) / 10.0f;
                }
            } else {
                // ACTION: Move towards the target
                Vector2 dir = Vector2Subtract(behavior.currentTarget, transform.position);
                float distanceToTarget = Vector2Length(dir);

                if (distanceToTarget < 2.0f) {
                    // Arrived !
                    behavior.isMoving = false;
                    behavior.stateTimer = GetRandomValue(10, 40) / 10.0f; // Wait 1 to 4 seconds
                } else {
                    // Walk using the speed defined in entities.stv !
                    Vector2 normalizedDir = Vector2Scale(dir, 1.0f / distanceToTarget);
                    transform.position.x += normalizedDir.x * stats.maxSpeed * deltaTime;
                    transform.position.y += normalizedDir.y * stats.maxSpeed * deltaTime;
                }
            }
        }
    }
}
