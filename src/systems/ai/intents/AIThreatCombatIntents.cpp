/**
 * @file AIThreatCombatIntents.cpp
 * @brief Threat, combat, guard and patrol AI intents.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Config.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/Pathfinder.hpp"
#include "systems/ai/AIIntentFinders.hpp"

#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace {

constexpr float FLEE_DISTANCE_TILES = 8.0f;
constexpr int FLEE_PATH_ATTEMPTS = 10;
constexpr float GUARD_RESPONSE_RADIUS_TILES = 90.0f;
constexpr float PATROL_RADIUS_TILES = 12.0f;

bool IsValidActor(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasTransform[entity] && em.hasBehavior[entity];
}

bool IsValidTarget(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasTransform[entity] && em.hasHealth[entity] &&
           em.healths[entity].current > 0.0f;
}

bool IsHostileSpecies(const std::string& species) {
    return species == "cannibal" || species == "zombie" || species == "eldritch";
}

bool IsHostileEntity(const EntityManager& em, EntityID entity) {
    return IsValidTarget(em, entity) && em.hasTag[entity] && IsHostileSpecies(em.tags[entity].species);
}

Vector2 NormalizeSafe(Vector2 value) {
    const float len = std::sqrt(value.x * value.x + value.y * value.y);

    if (len <= 0.001f) {
        return {1.0f, 0.0f};
    }

    return {value.x / len, value.y / len};
}

Vector2 Rotate(Vector2 value, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);

    return {value.x * c - value.y * s, value.x * s + value.y * c};
}

const BehaviorRule* FindRule(const BehaviorComponent& behavior, const std::string& ruleName) {
    for (const BehaviorRule& rule : behavior.innateBehaviorRules) {
        if (rule.name == ruleName) {
            return &rule;
        }
    }

    return nullptr;
}

bool RuleTargetsEntity(const BehaviorRule& rule, const EntityManager& em, EntityID target) {
    if (target >= em.active.size() || !em.active[target] || !em.hasTag[target]) {
        return false;
    }

    const TagComponent& tag = em.tags[target];

    for (const std::string& arg : rule.arguments) {
        if (arg == tag.species || arg == tag.category || arg == tag.prefabId) {
            return true;
        }
    }

    return false;
}

EntityID FindPrimaryVillageForMember(EntityID entity, const EntityManager& em) {
    if (entity >= em.active.size() || !em.active[entity] || !em.hasVillageMember[entity]) {
        return static_cast<EntityID>(-1);
    }

    return em.villageMembers[entity].villageId;
}

EntityID FindBestHuntTarget(EntityID entity, EntityManager& em, const EntitySpatialGrid& spatialGrid) {
    if (!IsValidActor(em, entity)) {
        return static_cast<EntityID>(-1);
    }

    const BehaviorComponent& behavior = em.behaviors[entity];
    const BehaviorRule* huntRule = FindRule(behavior, "hunt");

    if (huntRule == nullptr) {
        return static_cast<EntityID>(-1);
    }

    const float radius = AISystemUtils::GetActionRadiusWorld(entity, em);

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[entity].position, radius, em);

    EntityID bestTarget = static_cast<EntityID>(-1);
    float bestDistanceSq = std::numeric_limits<float>::infinity();

    for (EntityID candidate : candidates) {
        if (candidate == entity || !IsValidTarget(em, candidate) || !RuleTargetsEntity(*huntRule, em, candidate)) {
            continue;
        }

        const float distanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[candidate].position);

        if (distanceSq < bestDistanceSq) {
            bestDistanceSq = distanceSq;
            bestTarget = candidate;
        }
    }

    return bestTarget;
}

EntityID FindBestGuardThreat(EntityID entity, EntityManager& em, const EntitySpatialGrid& spatialGrid) {
    if (!IsValidActor(em, entity)) {
        return static_cast<EntityID>(-1);
    }

    const EntityID villageId = FindPrimaryVillageForMember(entity, em);

    if (villageId >= em.active.size() || !em.active[villageId] || !em.hasTransform[villageId]) {
        return static_cast<EntityID>(-1);
    }

    const float radius = GUARD_RESPONSE_RADIUS_TILES * Config::TILE_SIZE;

    const std::vector<EntityID> candidates = spatialGrid.GetEntitiesInRadius(em.transforms[villageId].position, radius, em);

    EntityID bestThreat = static_cast<EntityID>(-1);
    float bestScore = -std::numeric_limits<float>::infinity();

    for (EntityID threat : candidates) {
        if (!IsHostileEntity(em, threat)) {
            continue;
        }

        const float guardDistanceSq = AISystemUtils::SquaredDistance(em.transforms[entity].position, em.transforms[threat].position);

        const float villageDistanceSq = AISystemUtils::SquaredDistance(em.transforms[villageId].position, em.transforms[threat].position);

        const float guardDistanceTiles = std::sqrt(guardDistanceSq) / Config::TILE_SIZE;
        const float villageDistanceTiles = std::sqrt(villageDistanceSq) / Config::TILE_SIZE;

        const float score = 300.0f - guardDistanceTiles - villageDistanceTiles * 0.5f;

        if (score > bestScore) {
            bestScore = score;
            bestThreat = threat;
        }
    }

    return bestThreat;
}

Vector2 FindPatrolPointAroundVillage(EntityID entity, EntityManager& em) {
    const EntityID villageId = FindPrimaryVillageForMember(entity, em);

    if (villageId >= em.active.size() || !em.active[villageId] || !em.hasTransform[villageId]) {
        return em.transforms[entity].position;
    }

    const Vector2 center = em.transforms[villageId].position;

    const float angle = static_cast<float>(GetRandomValue(0, 359)) * DEG2RAD;
    const float radius = static_cast<float>(GetRandomValue(4, static_cast<int>(PATROL_RADIUS_TILES))) * Config::TILE_SIZE;

    return {center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius};
}

} // namespace

namespace ai::intents {

std::optional<AIIntent> FindFleeIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                       const ResourceRegistry&, const WeaponRegistry&, const EntitySpatialGrid&) {
    if (!IsValidActor(em, entity) || !em.hasAIContext[entity]) {
        return std::nullopt;
    }

    const EntityID threat = em.aiContexts[entity].lastThreatId;

    if (!IsValidTarget(em, threat)) {
        return std::nullopt;
    }

    const Vector2 entityPos = em.transforms[entity].position;
    const Vector2 threatPos = em.transforms[threat].position;

    Vector2 away = {entityPos.x - threatPos.x, entityPos.y - threatPos.y};

    away = NormalizeSafe(away);

    const float fleeDistance = FLEE_DISTANCE_TILES * Config::TILE_SIZE;

    for (int attempt = 0; attempt < FLEE_PATH_ATTEMPTS; ++attempt) {
        const float angleOffset = static_cast<float>(attempt - FLEE_PATH_ATTEMPTS / 2) * 0.35f;

        const Vector2 direction = Rotate(away, angleOffset);

        const Vector2 targetPosition = {entityPos.x + direction.x * fleeDistance, entityPos.y + direction.y * fleeDistance};

        // Validate at least one path exists before returning the intent.
        const std::vector<Vector2> path = Pathfinder::FindPath(entityPos, targetPosition, map, tileReg, em, entity);

        if (path.empty()) {
            continue;
        }

        AIIntent intent;
        intent.kind = AIIntentKind::MoveToPosition;
        intent.targetEntity = threat;
        intent.targetPosition = targetPosition;
        intent.moveTask = "moving_to_flee";

        return intent;
    }

    return std::nullopt;
}

std::optional<AIIntent> FindDefendIntent(EntityID entity, EntityManager& em, const WorldMap&, const TileRegistry&, const ResourceRegistry&,
                                         const WeaponRegistry&, const EntitySpatialGrid&) {
    if (!IsValidActor(em, entity) || !em.hasAIContext[entity]) {
        return std::nullopt;
    }

    const EntityID threat = em.aiContexts[entity].lastThreatId;

    if (!IsValidTarget(em, threat)) {
        return std::nullopt;
    }

    AIIntent intent;
    intent.kind = AIIntentKind::MoveAdjacentToEntity;
    intent.targetEntity = threat;
    intent.moveTask = "moving_to_hunt";
    intent.actionTask = "attacking";
    intent.actionDuration = AISystemUtils::ATTACK_DURATION;

    return intent;
}

std::optional<AIIntent> FindHuntIntent(EntityID entity, EntityManager& em, const WorldMap&, const TileRegistry&, const ResourceRegistry&,
                                       const WeaponRegistry&, const EntitySpatialGrid& spatialGrid) {
    const EntityID target = FindBestHuntTarget(entity, em, spatialGrid);

    if (target == static_cast<EntityID>(-1)) {
        return std::nullopt;
    }

    AIIntent intent;
    intent.kind = AIIntentKind::MoveAdjacentToEntity;
    intent.targetEntity = target;
    intent.moveTask = "moving_to_hunt";
    intent.actionTask = "attacking";
    intent.actionDuration = AISystemUtils::ATTACK_DURATION;

    return intent;
}

std::optional<AIIntent> FindGuardIntent(EntityID entity, EntityManager& em, const WorldMap&, const TileRegistry&, const ResourceRegistry&,
                                        const WeaponRegistry&, const EntitySpatialGrid& spatialGrid) {
    const EntityID threat = FindBestGuardThreat(entity, em, spatialGrid);

    if (threat == static_cast<EntityID>(-1)) {
        return std::nullopt;
    }

    AIIntent intent;
    intent.kind = AIIntentKind::MoveAdjacentToEntity;
    intent.targetEntity = threat;
    intent.moveTask = "moving_to_hunt";
    intent.actionTask = "attacking";
    intent.actionDuration = AISystemUtils::ATTACK_DURATION;

    return intent;
}

std::optional<AIIntent> FindPatrolIntent(EntityID entity, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                         const ResourceRegistry&, const WeaponRegistry&, const EntitySpatialGrid&) {
    if (!IsValidActor(em, entity) || !em.hasVillageMember[entity]) {
        return std::nullopt;
    }

    constexpr int MAX_ATTEMPTS = 8;

    for (int attempt = 0; attempt < MAX_ATTEMPTS; ++attempt) {
        const Vector2 targetPosition = FindPatrolPointAroundVillage(entity, em);

        const std::vector<Vector2> path = Pathfinder::FindPath(em.transforms[entity].position, targetPosition, map, tileReg, em, entity);

        if (path.empty()) {
            continue;
        }

        AIIntent intent;
        intent.kind = AIIntentKind::MoveToPosition;
        intent.targetEntity = em.villageMembers[entity].villageId;
        intent.targetPosition = targetPosition;
        intent.moveTask = "patrolling";

        return intent;
    }

    return std::nullopt;
}

} // namespace ai::intents
