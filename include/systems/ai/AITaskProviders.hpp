/**
 * @file AITaskProviders.hpp
 * @brief Declares AI task candidate providers by gameplay domain.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "systems/ai/AIDecisionContext.hpp"
#include "systems/ai/AITaskCandidate.hpp"

#include <vector>

namespace ai::providers {

void AppendThreatCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates);
void AppendNeedCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates);
void AppendDefenseCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates);
void AppendVillageCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates);
void AppendSocialReactionCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates);
void AppendFamilyCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates);
void AppendLogisticsCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates);
void AppendWorkCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates);
void AppendReligionCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates);
void AppendEquipmentCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates);
void AppendProfessionSupportCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates);
void AppendHostilityCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates);

} // namespace ai::providers
