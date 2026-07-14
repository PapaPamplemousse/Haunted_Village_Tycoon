/**
 * @file AITaskCandidate.hpp
 * @brief Candidate task produced by the AI decision layer.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "systems/ai/AITaskType.hpp"

struct AITaskCandidate {
    AITaskType type = AITaskType::None;
    float priority = 0.0f;
    float score = 0.0f;
};
