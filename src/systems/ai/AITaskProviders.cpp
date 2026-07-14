/**
 * @file AITaskProviders.cpp
 * @brief Domain-based AI task candidate providers.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/ai/AITaskProviders.hpp"

#include "systems/AISystemUtils.hpp"
#include "systems/ai/AIDecisionScoring.hpp"
#include "systems/ai/AIPriority.hpp"

namespace ai::providers {

void AppendThreatCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates) {
    const BehaviorComponent& behavior = ctx.behavior();

    const bool canFlee = decision::HasCapability(behavior, "flee");
    const bool canDefend = decision::HasCapability(behavior, "defend") || decision::HasCapability(behavior, "hunt");

    if (!decision::HasThreatMemory(ctx.entity, ctx.em)) {
        return;
    }

    const EntityID threat = ctx.aiContext().lastThreatId;

    if (canFlee) {
        candidates.push_back({AITaskType::Flee, AIPriority::Threat, 100.0f});
    }

    if (canDefend && !decision::IsCurrentThreatResponseTask(behavior, threat)) {
        candidates.push_back({AITaskType::Defend, AIPriority::Threat - 20.0f, 90.0f});
    }
}

void AppendNeedCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates) {
    const BehaviorComponent& behavior = ctx.behavior();

    const bool canSeekFood = decision::HasCapability(behavior, "seek_food");
    const bool canRest = decision::HasCapability(behavior, "rest");

    if (canSeekFood) {
        const float hungerRatio = decision::GetHungerRatio(ctx.entity, ctx.em);

        if (hungerRatio <= AISystemUtils::SEEK_FOOD_THRESHOLD_RATIO) {
            const float priority = hungerRatio <= 0.25f ? AIPriority::CriticalNeed : AIPriority::HighNeed;
            const float score = (1.0f - hungerRatio) * 100.0f;

            candidates.push_back({AITaskType::SeekFood, priority, score});
        }
    }

    if (canRest && AISystemUtils::ShouldRest(ctx.entity, ctx.em, behavior, ctx.currentHour)) {
        const float fatigueRatio = decision::GetFatigueRatio(ctx.entity, ctx.em);
        const float priority = fatigueRatio >= 0.90f ? AIPriority::CriticalNeed : AIPriority::Rest;
        const float score = fatigueRatio * 100.0f;

        candidates.push_back({AITaskType::Rest, priority, score});
    }
}

void AppendDefenseCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates) {
    const BehaviorComponent& behavior = ctx.behavior();

    if (!decision::HasCapability(behavior, "guard")) {
        return;
    }

    float guardScore = 100.0f;

    if (ctx.currentHour >= 20.0f || ctx.currentHour < 6.0f) {
        guardScore += 80.0f;
    }

    candidates.push_back({AITaskType::Guard, AIPriority::Guard, guardScore});
}

void AppendVillageCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates) {
    const BehaviorComponent& behavior = ctx.behavior();

    if (!decision::HasCapability(behavior, "return_village_core")) {
        return;
    }

    const float returnScore = decision::EstimateReturnToVillageCoreUtility(ctx.entity, ctx.em, ctx.currentHour);

    if (returnScore > 0.0f) {
        candidates.push_back({AITaskType::ReturnToVillageCore, AIPriority::ReturnToVillage, returnScore});
    }
}

void AppendSocialReactionCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates) {
    const BehaviorComponent& behavior = ctx.behavior();

    if (!decision::HasCapability(behavior, "avoid_person")) {
        return;
    }

    const float avoidScore = decision::EstimateAvoidPersonUtility(ctx.entity, ctx.em);

    if (avoidScore > 0.0f) {
        candidates.push_back({AITaskType::AvoidPerson, AIPriority::AvoidPerson, avoidScore});
    }
}

void AppendFamilyCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates) {
    const BehaviorComponent& behavior = ctx.behavior();

    if (!decision::HasCapability(behavior, "care_child")) {
        return;
    }

    const float careScore = decision::EstimateCareChildFoodUtility(ctx.entity, ctx.em, ctx.resourceReg);

    if (careScore > 0.0f) {
        candidates.push_back({AITaskType::CareChildFood, AIPriority::Care, careScore});
    }
}

void AppendLogisticsCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates) {
    const BehaviorComponent& behavior = ctx.behavior();

    const bool canStore = decision::HasCapability(behavior, "store");
    const bool canHaul = decision::HasCapability(behavior, "haul");

    if (canStore && AISystemUtils::ShouldDepositInventory(ctx.entity, ctx.em, behavior, ctx.currentHour)) {
        const float storeScore = decision::EstimateStoreUtility(ctx.entity, ctx.em, ctx.spatialGrid);

        if (storeScore > 0.0f) {
            candidates.push_back({AITaskType::Store, AIPriority::Logistics, storeScore});
        }
    }

    if (canHaul) {
        const float haulScore = decision::EstimateHaulUtility(ctx.entity, ctx.em, ctx.spatialGrid);

        if (haulScore > 0.0f) {
            candidates.push_back({AITaskType::Haul, AIPriority::Haul, haulScore});
        }
    }
}

void AppendWorkCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates) {
    const BehaviorComponent& behavior = ctx.behavior();

    if (!AISystemUtils::CanStartWorkNow(behavior, ctx.currentHour)) {
        return;
    }

    if (decision::HasCapability(behavior, "hunt")) {
        const float huntScore = decision::EstimateHuntUtility(ctx.entity, ctx.em, ctx.spatialGrid);

        if (huntScore > 0.0f) {
            candidates.push_back({AITaskType::Hunt, AIPriority::Work + 40.0f, huntScore});
        }
    }

    if (decision::HasCapability(behavior, "build")) {
        const float buildScore = decision::EstimateBuildUtility(ctx.entity, ctx.em, ctx.spatialGrid);

        if (buildScore > 0.0f) {
            candidates.push_back({AITaskType::Build, AIPriority::Work + 30.0f, buildScore});
        }
    }

    if (decision::HasCapability(behavior, "dismantle")) {
        const float dismantleScore = decision::EstimateDismantleUtility(ctx.entity, ctx.em, ctx.spatialGrid);

        if (dismantleScore > 0.0f) {
            candidates.push_back({AITaskType::Dismantle, AIPriority::Work + 20.0f, dismantleScore});
        }
    }

    if (decision::HasCapability(behavior, "harvest")) {
        const float harvestScore = decision::EstimateHarvestUtility(ctx.entity, ctx.em, ctx.resourceReg, ctx.spatialGrid);

        if (harvestScore > 0.0f) {
            candidates.push_back({AITaskType::Harvest, AIPriority::Work + 10.0f, harvestScore});
        }
    }
}

void AppendReligionCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates) {
    const BehaviorComponent& behavior = ctx.behavior();

    if (decision::HasCapability(behavior, "comfort_frightened")) {
        candidates.push_back({AITaskType::ComfortFrightened, AIPriority::ComfortFrightened, 100.0f});
    }

    if (decision::HasCapability(behavior, "preach")) {
        candidates.push_back({AITaskType::Preach, AIPriority::Preach, 70.0f});
    }

    if (decision::HasCapability(behavior, "hold_ritual")) {
        candidates.push_back({AITaskType::HoldRitual, AIPriority::HoldRitual, 60.0f});
    }
}

void AppendEquipmentCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates) {
    const BehaviorComponent& behavior = ctx.behavior();

    if (decision::HasCapability(behavior, "equip_weapon")) {
        candidates.push_back({AITaskType::EquipWeapon, AIPriority::EquipWeapon, 100.0f});
    }

    if (decision::HasCapability(behavior, "request_weapon")) {
        candidates.push_back({AITaskType::RequestWeapon, AIPriority::RequestWeapon, 80.0f});
    }

    if (decision::HasCapability(behavior, "fulfill_weapon_request")) {
        candidates.push_back({AITaskType::FulfillWeaponRequest, AIPriority::FulfillRequest, 100.0f});
    }
}

void AppendProfessionSupportCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates) {
    const BehaviorComponent& behavior = ctx.behavior();

    if (decision::HasCapability(behavior, "repair")) {
        candidates.push_back({AITaskType::Repair, AIPriority::Repair, 80.0f});
    }

    if (decision::HasCapability(behavior, "patrol")) {
        candidates.push_back({AITaskType::Patrol, AIPriority::Patrol, 10.0f});
    }
}

void AppendHostilityCandidates(const AIDecisionContext& ctx, std::vector<AITaskCandidate>& candidates) {
    const BehaviorComponent& behavior = ctx.behavior();

    const bool socialCooldownReady = !ctx.em.hasAIContext[ctx.entity] || ctx.em.aiContexts[ctx.entity].socialActionCooldownTimer <= 0.0f;

    const bool canConsiderHostility = socialCooldownReady && !decision::HasUrgentPersonalNeed(ctx.entity, ctx.em);

    if (!canConsiderHostility) {
        return;
    }

    if (decision::HasCapability(behavior, "confront_person")) {
        const float confrontScore = decision::EstimateConfrontPersonUtility(ctx.entity, ctx.em);

        if (confrontScore > 0.0f) {
            candidates.push_back({AITaskType::ConfrontPerson, AIPriority::ConfrontPerson, confrontScore});
        }
    }

    if (decision::HasCapability(behavior, "murder")) {
        const float murderScore = decision::EstimateMurderUtility(ctx.entity, ctx.em);

        if (murderScore > 0.0f) {
            candidates.push_back({AITaskType::Murder, AIPriority::Murder, murderScore});
        }
    }

    if (decision::HasCapability(behavior, "fight_non_lethal")) {
        const float hostilityScore = decision::EstimateHighestHostility(ctx.entity, ctx.em);

        if (hostilityScore >= 75.0f) {
            candidates.push_back({AITaskType::FightNonLethal, AIPriority::FightNonLethal, hostilityScore});
        }
    }

    if (decision::HasCapability(behavior, "intimidate")) {
        const float hostilityScore = decision::EstimateHighestHostility(ctx.entity, ctx.em);

        if (hostilityScore >= 55.0f) {
            candidates.push_back({AITaskType::Intimidate, AIPriority::Intimidate, hostilityScore});
        }
    }
}

} // namespace ai::providers
