/**
 * @file AIDecisionScoring.hpp
 * @brief Shared scoring helpers for AI decision providers.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "data/ResourceRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "world/EntitySpatialGrid.hpp"

#include <string>

namespace ai::decision {

bool HasCapability(const BehaviorComponent& behavior, const std::string& capability);

float GetHungerRatio(EntityID entity, const EntityManager& em);
float GetFatigueRatio(EntityID entity, const EntityManager& em);

bool HasThreatMemory(EntityID entity, const EntityManager& em);
bool IsCurrentThreatResponseTask(const BehaviorComponent& behavior, EntityID threat);

float EstimateStoreUtility(EntityID entity, const EntityManager& em, const EntitySpatialGrid& spatialGrid);
float EstimateBuildUtility(EntityID entity, const EntityManager& em, const EntitySpatialGrid& spatialGrid);
float EstimateDismantleUtility(EntityID entity, const EntityManager& em, const EntitySpatialGrid& spatialGrid);
float EstimateHuntUtility(EntityID entity, const EntityManager& em, const EntitySpatialGrid& spatialGrid);

float EstimateHarvestUtility(EntityID entity, const EntityManager& em, const ResourceRegistry& resourceReg,
                             const EntitySpatialGrid& spatialGrid);

float EstimateCareChildFoodUtility(EntityID entity, const EntityManager& em, const ResourceRegistry& resourceReg);
float EstimateReturnToVillageCoreUtility(EntityID entity, const EntityManager& em, float currentHour);
float EstimateHaulUtility(EntityID entity, const EntityManager& em, const EntitySpatialGrid& spatialGrid);

float EstimateAvoidPersonUtility(EntityID entity, const EntityManager& em);
float EstimateConfrontPersonUtility(EntityID entity, const EntityManager& em);

float EstimateHighestHostility(EntityID entity, const EntityManager& em);
float EstimateMurderUtility(EntityID entity, const EntityManager& em);

bool HasUrgentPersonalNeed(EntityID entity, const EntityManager& em);

} // namespace ai::decision
