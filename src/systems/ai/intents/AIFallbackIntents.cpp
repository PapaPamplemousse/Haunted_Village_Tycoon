/**
 * @file AIFallbackIntents.cpp
 * @brief Fallback AI intents such as wandering.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Config.hpp"
#include "systems/ai/AIIntentFinders.hpp"

#include <cmath>

namespace ai::intents {

std::optional<AIIntent> FindWanderIntent(EntityID entity, EntityManager& em, const WorldMap&, const TileRegistry&, const ResourceRegistry&,
                                         const WeaponRegistry&, const EntitySpatialGrid&) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasTransform[entity]) {
        return std::nullopt;
    }

    constexpr int MAX_ATTEMPTS = 10;
    constexpr float WANDER_RADIUS_TILES = 8.0f;

    const Vector2 origin = em.transforms[entity].position;

    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        const float angle = static_cast<float>(GetRandomValue(0, 359)) * DEG2RAD;
        const float distance = static_cast<float>(GetRandomValue(2, static_cast<int>(WANDER_RADIUS_TILES))) * Config::TILE_SIZE;

        AIIntent intent;
        intent.kind = AIIntentKind::MoveToPosition;
        intent.targetEntity = static_cast<EntityID>(-1);
        intent.targetPosition = {origin.x + std::cos(angle) * distance, origin.y + std::sin(angle) * distance};
        intent.moveTask = "wandering";

        return intent;
    }

    return std::nullopt;
}

} // namespace ai::intents
