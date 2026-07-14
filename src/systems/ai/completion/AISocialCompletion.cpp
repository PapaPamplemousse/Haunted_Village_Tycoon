/**
 * @file AIHostilityCompletion.cpp
 * @brief Completion handlers for hostile social AI tasks.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/ai/AICompletion.hpp"
#include "systems/ai/AICompletionHelpers.hpp"

#include <iostream>

namespace ai::completion {

bool TryCompleteHostilityTask(const AICompletionContext& ctx) {
    EntityManager& em = ctx.em;
    const EntityID entity = ctx.entity;
    const BehaviorComponent& behavior = ctx.behavior();

    if (behavior.currentTask == "intimidating_person") {
        const EntityID target = behavior.currentJobTarget;

        if (IsActiveEntity(em, target) && em.hasSocial[entity] && em.hasSocial[target]) {
            RelationshipEntry& actorToTarget = GetOrCreateRelationship(em, entity, target);
            RelationshipEntry& targetToActor = GetOrCreateRelationship(em, target, entity);

            const float aggression = GetAggression(em, entity);

            actorToTarget.resentment = Clamp100(actorToTarget.resentment - 2.0f);
            actorToTarget.respect = Clamp100(actorToTarget.respect + 1.0f);

            targetToActor.fear = Clamp100(targetToActor.fear + 8.0f + aggression * 8.0f);
            targetToActor.resentment = Clamp100(targetToActor.resentment + 3.0f);
            targetToActor.friendship = Clamp100(targetToActor.friendship - 2.0f);

            std::cout << "[HOSTILITY] Entity #" << entity << " intimidated entity #" << target << "." << std::endl;
        }

        if (em.hasAIContext[entity]) {
            em.aiContexts[entity].socialActionCooldownTimer = 60.0f;
        }

        return true;
    }

    if (behavior.currentTask == "fighting_non_lethal") {
        const EntityID target = behavior.currentJobTarget;

        if (IsActiveEntity(em, target) && em.hasHealth[target] && em.hasSocial[entity] && em.hasSocial[target]) {
            RelationshipEntry& actorToTarget = GetOrCreateRelationship(em, entity, target);
            RelationshipEntry& targetToActor = GetOrCreateRelationship(em, target, entity);

            float damage = 3.0f + GetAggression(em, entity) * 7.0f;

            if (em.hasStats[entity]) {
                damage += em.stats[entity].baseAttack * 0.5f;
            }

            const float minHp = std::max(1.0f, em.healths[target].max * 0.20f);
            em.healths[target].current = std::max(minHp, em.healths[target].current - damage);

            actorToTarget.resentment = Clamp100(actorToTarget.resentment - 4.0f);
            actorToTarget.fear = Clamp100(actorToTarget.fear - 2.0f);

            targetToActor.resentment = Clamp100(targetToActor.resentment + 12.0f);
            targetToActor.fear = Clamp100(targetToActor.fear + 10.0f);
            targetToActor.friendship = Clamp100(targetToActor.friendship - 8.0f);
            targetToActor.trust = Clamp100(targetToActor.trust - 10.0f);

            std::cout << "[HOSTILITY] Entity #" << entity << " fought entity #" << target << " non-lethally." << std::endl;
        }

        if (em.hasAIContext[entity]) {
            em.aiContexts[entity].socialActionCooldownTimer = 90.0f;
        }

        return true;
    }

    if (behavior.currentTask == "murdering_person") {
        const EntityID target = behavior.currentJobTarget;

        if (IsActiveEntity(em, target) && em.hasHealth[target] && em.hasSocial[entity]) {
            const bool darkTrait = HasTrait(em, entity, "VIOLENT") || HasTrait(em, entity, "VENGEFUL");

            if (darkTrait && GetAggression(em, entity) >= 0.85f) {
                em.healths[target].current = 0.0f;
                em.DestroyEntity(target);

                RelationshipEntry& actorToTarget = GetOrCreateRelationship(em, entity, target);
                actorToTarget.resentment = Clamp100(actorToTarget.resentment - 20.0f);
                actorToTarget.fear = Clamp100(actorToTarget.fear + 10.0f);

                std::cout << "[HOSTILITY] Entity #" << entity << " murdered entity #" << target << "." << std::endl;
            }
        }

        if (em.hasAIContext[entity]) {
            em.aiContexts[entity].socialActionCooldownTimer = 180.0f;
        }

        return true;
    }

    return false;
}

} // namespace ai::completion
