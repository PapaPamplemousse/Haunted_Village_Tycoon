/**
 * @file AISystemTasks.cpp
 * @brief Routes completed AI tasks to domain-specific completion handlers.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/AISystem.hpp"
#include "systems/ai/AICompletion.hpp"
#include "systems/ai/AICompletionContext.hpp"

void AISystem::HandleTaskCompletion(EntityID i, EntityManager& em, const WorldMap& map, const TileRegistry& tileReg,
                                    const ResourceRegistry& resourceReg, const WeaponRegistry& weaponReg,
                                    const EntitySpatialGrid& spatialGrid, RoomSystem& roomSys) {
    auto& behavior = em.behaviors[i];

    const AICompletionContext completionContext{i, em, map, tileReg, resourceReg, weaponReg, spatialGrid, roomSys};

    using ai::completion::AICompletionStatus;

    const AICompletionStatus handlers[] = {
        ai::completion::TryCompleteNeedsTask(completionContext),        ai::completion::TryCompleteStorageTask(completionContext),
        ai::completion::TryCompleteCraftingTask(completionContext),     ai::completion::TryCompleteRequestTask(completionContext),
        ai::completion::TryCompleteConstructionTask(completionContext), ai::completion::TryCompleteRepairTask(completionContext),
        ai::completion::TryCompleteHarvestTask(completionContext),      ai::completion::TryCompleteCombatTask(completionContext),
        ai::completion::TryCompleteReligionTask(completionContext),     ai::completion::TryCompleteSocialTask(completionContext),
        ai::completion::TryCompleteHostilityTask(completionContext)};

    for (const AICompletionStatus status : handlers) {
        if (status == AICompletionStatus::InProgress) {
            return;
        }

        if (status == AICompletionStatus::Completed) {
            ResetBehaviorState(behavior);
            return;
        }
    }

    ResetBehaviorState(behavior);
}
