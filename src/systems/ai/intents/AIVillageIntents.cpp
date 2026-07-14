/**
 * @file AIVillageIntents.cpp
 * @brief Village-related AI intents.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Config.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/ai/AIIntentFinders.hpp"

#include <cmath>

namespace {

bool IsValidVillageCore(EntityID villageId, const EntityManager& em) {
    return villageId < em.active.size() && em.active[villageId] && em.hasVillage[villageId] && em.hasTransform[villageId];
}

float DistanceSq(Vector2 a, Vector2 b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

} // namespace

namespace ai::intents {

std::optional<AIIntent> FindReturnToVillageCoreIntent(EntityID entity, EntityManager& em, const WorldMap&, const TileRegistry&,
                                                      const ResourceRegistry&, const WeaponRegistry&, const EntitySpatialGrid&) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasTransform[entity] || !em.hasVillageMember[entity]) {
        return std::nullopt;
    }

    const EntityID villageId = em.villageMembers[entity].villageId;

    if (!IsValidVillageCore(villageId, em)) {
        return std::nullopt;
    }

    const Vector2 entityPos = em.transforms[entity].position;
    const Vector2 villagePos = em.transforms[villageId].position;

    constexpr float MIN_DISTANCE_TILES = 4.0f;
    const float minDistance = MIN_DISTANCE_TILES * Config::TILE_SIZE;

    if (DistanceSq(entityPos, villagePos) <= minDistance * minDistance) {
        return std::nullopt;
    }

    AIIntent intent;
    intent.kind = AIIntentKind::MoveToPosition;
    intent.targetEntity = villageId;
    intent.targetPosition = villagePos;
    intent.moveTask = "moving_to_village_core";

    return intent;
}

} // namespace ai::intents
