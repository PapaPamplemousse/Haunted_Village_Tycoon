/**
 * @file AIReligionCompletion.cpp
 * @brief Completion handlers for religion AI tasks.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/ai/AICompletion.hpp"
#include "systems/ai/AICompletionHelpers.hpp"

namespace ai::completion {

AICompletionStatus TryCompleteReligionTask(const AICompletionContext& ctx) {
    EntityManager& em = ctx.em;
    const EntityID entity = ctx.entity;
    const BehaviorComponent& behavior = ctx.behavior();

    if (behavior.currentTask == "praying") {
        if (em.hasFaction[entity]) {
            FactionComponent& faction = em.factions[entity];

            if (faction.factionId == "COMMON_FOLK") {
                faction.factionId = "OLD_FAITH";
                faction.conviction = std::max(faction.conviction, 15.0f);
            } else if (faction.factionId == "OLD_FAITH") {
                faction.conviction = Clamp100(faction.conviction + 6.0f);
            }
        }

        return AICompletionStatus::Completed;
    }

    if (behavior.currentTask == "preaching") {
        const EntityID target = behavior.currentJobTarget;

        if (IsHuman(em, target) && em.hasFaction[target] && em.hasSocial[entity] && em.hasSocial[target]) {
            FactionComponent& targetFaction = em.factions[target];

            RelationshipEntry& priestToTarget = GetOrCreateRelationship(em, entity, target);
            RelationshipEntry& targetToPriest = GetOrCreateRelationship(em, target, entity);

            if (targetFaction.factionId == "COMMON_FOLK") {
                targetFaction.factionId = "OLD_FAITH";
                targetFaction.conviction = std::max(targetFaction.conviction, 12.0f);

                targetToPriest.trust = Clamp100(targetToPriest.trust + 6.0f);
                priestToTarget.respect = Clamp100(priestToTarget.respect + 2.0f);
            } else if (targetFaction.factionId == "OLD_FAITH") {
                targetFaction.conviction = Clamp100(targetFaction.conviction + 5.0f);
                targetToPriest.trust = Clamp100(targetToPriest.trust + 3.0f);
            } else if (targetFaction.factionId == "CULT_OF_THE_HOLLOW") {
                targetToPriest.resentment = Clamp100(targetToPriest.resentment + 5.0f);
                targetToPriest.trust = Clamp100(targetToPriest.trust - 3.0f);
                priestToTarget.resentment = Clamp100(priestToTarget.resentment + 2.0f);
            }
        }

        return AICompletionStatus::Completed;
    }

    if (behavior.currentTask == "holding_ritual") {
        if (em.hasVillageMember[entity]) {
            const EntityID villageId = em.villageMembers[entity].villageId;

            for (EntityID target = 0; target < em.active.size(); ++target) {
                if (!IsHuman(em, target) || !em.hasVillageMember[target] || !em.hasFaction[target] ||
                    em.villageMembers[target].villageId != villageId) {
                    continue;
                }

                FactionComponent& targetFaction = em.factions[target];

                if (targetFaction.factionId == "OLD_FAITH") {
                    targetFaction.conviction = Clamp100(targetFaction.conviction + 4.0f);
                } else if (targetFaction.factionId == "COMMON_FOLK") {
                    targetFaction.conviction = Clamp100(targetFaction.conviction + 1.0f);
                }

                if (em.hasSocial[target]) {
                    for (RelationshipEntry& relationship : em.socials[target].relationships) {
                        relationship.fear = Clamp100(relationship.fear - 3.0f);
                    }
                }
            }
        }

        return AICompletionStatus::Completed;
    }

    if (behavior.currentTask == "comforting_frightened") {
        const EntityID target = behavior.currentJobTarget;

        if (IsHuman(em, target) && em.hasSocial[entity] && em.hasSocial[target]) {
            RelationshipEntry& priestToTarget = GetOrCreateRelationship(em, entity, target);
            RelationshipEntry& targetToPriest = GetOrCreateRelationship(em, target, entity);

            targetToPriest.trust = Clamp100(targetToPriest.trust + 6.0f);
            targetToPriest.friendship = Clamp100(targetToPriest.friendship + 3.0f);
            priestToTarget.respect = Clamp100(priestToTarget.respect + 2.0f);

            for (RelationshipEntry& relationship : em.socials[target].relationships) {
                relationship.fear = Clamp100(relationship.fear - 8.0f);
            }

            if (em.hasFaction[target] && em.factions[target].factionId == "OLD_FAITH") {
                em.factions[target].conviction = Clamp100(em.factions[target].conviction + 3.0f);
            }
        }

        return AICompletionStatus::Completed;
    }

    return AICompletionStatus::NotHandled;
}

} // namespace ai::completion
