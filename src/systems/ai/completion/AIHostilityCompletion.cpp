/**
 * @file AISocialCompletion.cpp
 * @brief Completion handlers for social AI tasks.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/ai/AICompletion.hpp"
#include "systems/ai/AICompletionHelpers.hpp"

namespace ai::completion {

AICompletionStatus TryCompleteSocialTask(const AICompletionContext& ctx) {
    EntityManager& em = ctx.em;
    const EntityID entity = ctx.entity;
    const BehaviorComponent& behavior = ctx.behavior();

    if (behavior.currentTask == "socializing") {
        const EntityID target = behavior.currentJobTarget;

        if (IsActiveEntity(em, target) && em.hasSocial[entity] && em.hasSocial[target]) {
            RelationshipEntry& relToTarget = GetOrCreateRelationship(em, entity, target);
            RelationshipEntry& relFromTarget = GetOrCreateRelationship(em, target, entity);

            const float kindnessA = GetKindness(em, entity);
            const float kindnessB = GetKindness(em, target);

            relToTarget.friendship = Clamp100(relToTarget.friendship + 4.0f + kindnessA * 3.0f);
            relFromTarget.friendship = Clamp100(relFromTarget.friendship + 3.0f + kindnessB * 2.0f);

            relToTarget.trust = Clamp100(relToTarget.trust + 1.0f);
            relFromTarget.trust = Clamp100(relFromTarget.trust + 1.0f);

            relToTarget.resentment = Clamp100(relToTarget.resentment - 2.0f);
            relFromTarget.resentment = Clamp100(relFromTarget.resentment - 1.0f);
        }

        return AICompletionStatus::Completed;
    }

    if (behavior.currentTask == "confronting_person") {
        const EntityID target = behavior.currentJobTarget;

        if (IsActiveEntity(em, target) && em.hasSocial[entity] && em.hasSocial[target]) {
            RelationshipEntry& relToTarget = GetOrCreateRelationship(em, entity, target);
            RelationshipEntry& relFromTarget = GetOrCreateRelationship(em, target, entity);

            const float aggression = GetAggression(em, entity);
            const float patience = GetPatience(em, entity);

            const bool escalates = aggression > 0.45f || patience < 0.35f;

            if (escalates) {
                relToTarget.resentment = Clamp100(relToTarget.resentment + 4.0f + aggression * 6.0f);
                relFromTarget.resentment = Clamp100(relFromTarget.resentment + 6.0f + aggression * 4.0f);
                relFromTarget.fear = Clamp100(relFromTarget.fear + aggression * 6.0f);

                relToTarget.friendship = Clamp100(relToTarget.friendship - 2.0f);
                relFromTarget.friendship = Clamp100(relFromTarget.friendship - 3.0f);
            } else {
                relToTarget.resentment = Clamp100(relToTarget.resentment - 5.0f);
                relFromTarget.resentment = Clamp100(relFromTarget.resentment + 1.0f);
                relToTarget.trust = Clamp100(relToTarget.trust + 1.0f);
            }
        }

        if (em.hasAIContext[entity]) {
            em.aiContexts[entity].socialActionCooldownTimer = 45.0f;
        }

        return AICompletionStatus::Completed;
    }

    return AICompletionStatus::NotHandled;
}

} // namespace ai::completion
