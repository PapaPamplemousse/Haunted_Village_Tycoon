/**
 * @file AICombatCompletion.cpp
 * @brief Completion handlers for combat tasks.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/AISystemUtils.hpp"
#include "systems/ai/AICompletion.hpp"
#include "systems/ai/AITaskExecutor.hpp"

namespace ai::completion {

AICompletionStatus TryCompleteCombatTask(const AICompletionContext& ctx) {
    EntityManager& em = ctx.em;
    const EntityID entity = ctx.entity;
    BehaviorComponent& behavior = ctx.behavior();

    if (behavior.currentTask != "attacking") {
        return AICompletionStatus::NotHandled;
    }

    const EntityID target = behavior.currentJobTarget;
    bool attackSucceeded = false;

    if (target < em.active.size() && em.active[target] && em.hasHealth[target] && em.hasTransform[target] &&
        AITaskExecutor::AreEntitiesAdjacent(entity, target, em)) {
        float damage = 0.0f;

        if (em.hasStats[entity]) {
            damage += em.stats[entity].baseAttack;
        }

        if (em.hasEquipment[entity]) {
            damage += em.equipments[entity].rightHandDamage;
        }

        if (damage <= 0.0f) {
            damage = 1.0f;
        }

        em.healths[target].current -= damage;
        attackSucceeded = true;

        if (em.healths[target].current <= 0.0f) {
            if (em.hasInventory[entity]) {
                AISystemUtils::GiveLootToInventory(entity, target, em);
            }

            em.DestroyEntity(target);
            return AICompletionStatus::Completed;
        }
    }

    if (attackSucceeded) {
        behavior.stateTimer = AISystemUtils::ATTACK_COOLDOWN;
        return AICompletionStatus::InProgress;
    }

    return AICompletionStatus::Completed;
}

} // namespace ai::completion
