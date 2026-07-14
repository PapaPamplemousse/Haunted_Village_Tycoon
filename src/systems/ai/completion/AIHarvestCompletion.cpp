/**
 * @file AIHarvestCompletion.cpp
 * @brief Completion handlers for harvesting tasks.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/ai/AICompletion.hpp"

#include <raylib.h>

namespace ai::completion {

AICompletionStatus TryCompleteHarvestTask(const AICompletionContext& ctx) {
    EntityManager& em = ctx.em;
    const EntityID entity = ctx.entity;
    BehaviorComponent& behavior = ctx.behavior();

    if (behavior.currentTask != "harvesting") {
        return AICompletionStatus::NotHandled;
    }

    const EntityID target = behavior.currentJobTarget;

    if (target >= em.active.size() || !em.active[target] || !em.hasHarvestable[target] || !em.hasHealth[target]) {
        return AICompletionStatus::Completed;
    }

    const HarvestableComponent& harvestable = em.harvestables[target];

    float damage = 1.0f;
    std::string toolType = "none";

    if (em.hasStats[entity]) {
        damage = em.stats[entity].baseAttack;
    }

    if (em.hasEquipment[entity]) {
        damage += em.equipments[entity].rightHandDamage;
        toolType = em.equipments[entity].rightHandToolType;
    }

    em.healths[target].current -= damage;
    behavior.actionAccumulator += 1.0f;

    if (behavior.actionAccumulator >= 3.0f) {
        behavior.actionAccumulator = 0.0f;

        if (harvestable.requiredTool == "none" || harvestable.requiredTool == toolType) {
            if (em.hasInventory[entity]) {
                for (const DropEntry& drop : harvestable.drops) {
                    const float roll = static_cast<float>(GetRandomValue(0, 100)) / 100.0f;

                    if (roll <= drop.chance) {
                        em.inventories[entity].items[drop.itemId] += drop.amount;
                    }
                }
            }
        }
    }

    if (em.healths[target].current <= 0.0f) {
        if (em.hasConstruction[target]) {
            ctx.roomSys.MarkDirty();
        }

        em.DestroyEntity(target);
        return AICompletionStatus::Completed;
    }

    behavior.stateTimer = 1.0f;
    return AICompletionStatus::InProgress;
}

} // namespace ai::completion
