/**
 * @file AICompletion.hpp
 * @brief Declares domain-based AI task completion handlers.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "systems/ai/AICompletionContext.hpp"

namespace ai::completion {

enum class AICompletionStatus { NotHandled, Completed, InProgress };

// Needs / logistics / crafting
AICompletionStatus TryCompleteNeedsTask(const AICompletionContext& ctx);
AICompletionStatus TryCompleteStorageTask(const AICompletionContext& ctx);
AICompletionStatus TryCompleteCraftingTask(const AICompletionContext& ctx);

// Social domains
AICompletionStatus TryCompleteSocialTask(const AICompletionContext& ctx);
AICompletionStatus TryCompleteHostilityTask(const AICompletionContext& ctx);
AICompletionStatus TryCompleteReligionTask(const AICompletionContext& ctx);

// Work / combat domains
AICompletionStatus TryCompleteRequestTask(const AICompletionContext& ctx);
AICompletionStatus TryCompleteConstructionTask(const AICompletionContext& ctx);
AICompletionStatus TryCompleteRepairTask(const AICompletionContext& ctx);
AICompletionStatus TryCompleteHarvestTask(const AICompletionContext& ctx);
AICompletionStatus TryCompleteCombatTask(const AICompletionContext& ctx);

} // namespace ai::completion
