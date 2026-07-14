/**
 * @file EventSystem.hpp
 * @brief Orchestrates simulation events driven by time, seasons, and settlement state.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "core/Chronicle.hpp"
#include "core/SettlementMetrics.hpp"
#include "data/BehaviorRegistry.hpp"
#include "data/EntityRegistry.hpp"
#include "data/EventRuleRegistry.hpp"
#include "data/NameRegistry.hpp"
#include "data/ResourceRegistry.hpp"
#include "data/TileRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "systems/TimeSystem.hpp"
#include "world/WorldMap.hpp"

#include <string>
#include <unordered_map>

enum class EventDayPhase { Dawn, Day, Dusk, Night };

/**
 * @class EventSystem
 * @brief Handles simulation events driven by time, seasons, resources and settlement state.
 */
class EventSystem {
public:
    EventSystem() = default;

    void Update(EntityManager& em, EntityRegistry& entityReg, const NameRegistry& nameReg, const BehaviorRegistry& behaviorReg,
                const EventRuleRegistry& eventRuleReg, const WorldMap& worldMap, const TileRegistry& tileReg, const TimeSystem& timeSystem,
                const ResourceRegistry& resourceReg, SettlementMetrics& metrics, Chronicle& chronicle);

private:
    // Orchestration
    void HandleSeasonEvents(const TimeSystem& timeSystem, SettlementMetrics& metrics, Chronicle& chronicle);

    void HandleNightEvents(EntityManager& em, EntityRegistry& entityReg, const NameRegistry& nameReg, const BehaviorRegistry& behaviorReg,
                           const EventRuleRegistry& eventRuleReg, const WorldMap& worldMap, const TileRegistry& tileReg,
                           const TimeSystem& timeSystem, const ResourceRegistry& resourceReg, SettlementMetrics& metrics,
                           Chronicle& chronicle);

    void HandleDawnEvents(const TimeSystem& timeSystem, SettlementMetrics& metrics, Chronicle& chronicle);

    // Night events

    void TriggerNightOfTheUndead(EntityManager& em, EntityRegistry& entityReg, const NameRegistry& nameReg,
                                 const BehaviorRegistry& behaviorReg, const WorldMap& worldMap, const TileRegistry& tileReg,
                                 const TimeSystem& timeSystem, SettlementMetrics& metrics, Chronicle& chronicle);

    int CountVillagePopulation(const EntityManager& em, EntityID villageId) const;

    EntityID FindPrimaryVillage(const EntityManager& em) const;

    bool FindZombieSpawnPositionNearVillage(const EntityManager& em, const WorldMap& worldMap, const TileRegistry& tileReg,
                                            EntityID villageId, int minRadiusTiles, int maxRadiusTiles, Vector2& outPosition) const;

    bool TryTriggerEventRule(EntityManager& em, EntityRegistry& entityReg, const NameRegistry& nameReg, const BehaviorRegistry& behaviorReg,
                             const EventRuleDef& rule, const WorldMap& worldMap, const TileRegistry& tileReg, const TimeSystem& timeSystem,
                             const ResourceRegistry& resourceReg, SettlementMetrics& metrics, Chronicle& chronicle);

    bool IsRuleEligible(const EventRuleDef& rule, const TimeSystem& timeSystem, int population) const;

    float ComputeRuleChance(const EventRuleDef& rule, const TimeSystem& timeSystem, int population) const;

    bool IsRuleOnCooldown(const EventRuleDef& rule, const TimeSystem& timeSystem) const;

    void MarkRuleTriggered(const EventRuleDef& rule, const TimeSystem& timeSystem);

    bool TriggerSpawnWaveRule(EntityManager& em, EntityRegistry& entityReg, const NameRegistry& nameReg,
                              const BehaviorRegistry& behaviorReg, const EventRuleDef& rule, const WorldMap& worldMap,
                              const TileRegistry& tileReg, const TimeSystem& timeSystem, SettlementMetrics& metrics, Chronicle& chronicle);

    bool TriggerFoodTheftRule(EntityManager& em, const EventRuleDef& rule, const TimeSystem& timeSystem,
                              const ResourceRegistry& resourceReg, SettlementMetrics& metrics, Chronicle& chronicle);

    bool TriggerSettlementEffectRule(const EventRuleDef& rule, const TimeSystem& timeSystem, SettlementMetrics& metrics,
                                     Chronicle& chronicle);

    std::string FormatEventMessage(const std::string& message, int count, const std::string& itemId, int amount) const;

    // Season events
    void TriggerSeasonChanged(const TimeSystem& timeSystem, SettlementMetrics& metrics, Chronicle& chronicle);

    // Shared effects
    void AddFear(SettlementMetrics& metrics, float amount);

    void AddCorruption(SettlementMetrics& metrics, float amount);

    bool RemoveFoodFromAnyStorage(EntityManager& em, const ResourceRegistry& resourceReg, int amountToRemove, std::string& outItemId,
                                  int& outRemovedAmount);

    bool IsFoodItem(const ResourceRegistry& resourceReg, const std::string& itemId) const;

    // Time helpers
    EventDayPhase GetCurrentPhase(float hour) const;
    const char* PhaseToString(EventDayPhase phase) const;

private:
    int m_lastProcessedDay = -1;
    int m_lastProcessedSeasonIndex = -1;

    int m_lastNightEventDay = -1;
    int m_lastDawnEventDay = -1;

    int m_lastUndeadNightDay = -1000;
    std::unordered_map<std::string, int> m_lastTriggeredDayByRule;
};
