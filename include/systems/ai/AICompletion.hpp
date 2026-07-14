/**
 * @file AICompletion.hpp
 * @brief Declares domain-based AI task completion handlers.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "systems/ai/AICompletionContext.hpp"

namespace ai::completion {
bool TryCompleteSocialTask(const AICompletionContext& ctx);
bool TryCompleteHostilityTask(const AICompletionContext& ctx);
bool TryCompleteReligionTask(const AICompletionContext& ctx);
bool TryCompleteNeedsTask(const AICompletionContext& ctx);
bool TryCompleteStorageTask(const AICompletionContext& ctx);
bool TryCompleteCraftingTask(const AICompletionContext& ctx);
} // namespace ai::completion
