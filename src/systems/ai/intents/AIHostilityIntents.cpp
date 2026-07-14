/**
 * @file AIHostilityIntents.cpp
 * @brief Hostile social AI intents.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Config.hpp"
#include "systems/AISystemUtils.hpp"
#include "systems/ai/AIIntentFinders.hpp"

#include <cmath>
#include <limits>

namespace {

bool IsValidHuman(const EntityManager& em, EntityID entity) {
    return entity < em.active.size() && em.active[entity] && em.hasTag[entity] && em.hasTransform[entity] && em.hasVillageMember[entity] &&
           em.hasSocial[entity] && em.tags[entity].species == "human";
}

bool AreSameVillage(const EntityManager& em, EntityID a, EntityID b) {
    return em.hasVillageMember[a] && em.hasVillageMember[b] && em.villageMembers[a].villageId == em.villageMembers[b].villageId;
}

EntityID FindHighestHostilityTarget(EntityID entity, const EntityManager& em, float minScore) {
    if (!IsValidHuman(em, entity)) {
        return static_cast<EntityID>(-1);
    }

    EntityID best = static_cast<EntityID>(-1);
    float bestScore = minScore;

    for (const RelationshipEntry& relationship : em.socials[entity].relationships) {
        const EntityID other = relationship.otherId;

        if (!IsValidHuman(em, other) || !AreSameVillage(em, entity, other)) {
            continue;
        }

        const float score = relationship.resentment - relationship.friendship * 0.5f + relationship.fear * 0.2f;

        if (score > bestScore) {
            bestScore = score;
            best = other;
        }
    }

    return best;
}

std::optional<AIIntent> MakeHostileAdjacentIntent(EntityID target, const std::string& moveTask, const std::string& actionTask,
                                                  float duration) {
    if (target == static_cast<EntityID>(-1)) {
        return std::nullopt;
    }

    AIIntent intent;
    intent.kind = AIIntentKind::MoveAdjacentToEntity;
    intent.targetEntity = target;
    intent.moveTask = moveTask;
    intent.actionTask = actionTask;
    intent.actionDuration = duration;

    return intent;
}

} // namespace

namespace ai::intents {

std::optional<AIIntent> FindIntimidateIntent(EntityID entity, EntityManager& em, const WorldMap&, const TileRegistry&,
                                             const ResourceRegistry&, const WeaponRegistry&, const EntitySpatialGrid&) {
    const EntityID target = FindHighestHostilityTarget(entity, em, 55.0f);

    return MakeHostileAdjacentIntent(target, "moving_to_intimidate", "intimidating_person", 1.2f);
}

std::optional<AIIntent> FindFightNonLethalIntent(EntityID entity, EntityManager& em, const WorldMap&, const TileRegistry&,
                                                 const ResourceRegistry&, const WeaponRegistry&, const EntitySpatialGrid&) {
    const EntityID target = FindHighestHostilityTarget(entity, em, 75.0f);

    return MakeHostileAdjacentIntent(target, "moving_to_fight_non_lethal", "fighting_non_lethal", 1.0f);
}

std::optional<AIIntent> FindMurderIntent(EntityID entity, EntityManager& em, const WorldMap&, const TileRegistry&, const ResourceRegistry&,
                                         const WeaponRegistry&, const EntitySpatialGrid&) {
    const EntityID target = FindHighestHostilityTarget(entity, em, 95.0f);

    return MakeHostileAdjacentIntent(target, "moving_to_murder", "murdering_person", 1.5f);
}

} // namespace ai::intents
