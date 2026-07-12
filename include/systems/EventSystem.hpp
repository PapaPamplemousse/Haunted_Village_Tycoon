/**
 * @file EventSystem.hpp
 * @brief Orchestrates simulation events driven by time, seasons, and settlement state.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "core/Chronicle.hpp"
#include "core/SettlementMetrics.hpp"
#include "data/ResourceRegistry.hpp"
#include "ecs/EntityManager.hpp"
#include "systems/TimeSystem.hpp"

#include <string>

enum class EventDayPhase { Dawn, Day, Dusk, Night };

/**
 * @class EventSystem
 * @brief Handles simulation events driven by time, seasons, resources and settlement state.
 */
class EventSystem {
public:
    EventSystem() = default;

    void Update(EntityManager& em, const TimeSystem& timeSystem, const ResourceRegistry& resourceReg, SettlementMetrics& metrics,
                Chronicle& chronicle);

private:
    // Orchestration
    void HandleSeasonEvents(const TimeSystem& timeSystem, SettlementMetrics& metrics, Chronicle& chronicle);

    void HandleNightEvents(EntityManager& em, const TimeSystem& timeSystem, const ResourceRegistry& resourceReg, SettlementMetrics& metrics,
                           Chronicle& chronicle);

    void HandleDawnEvents(const TimeSystem& timeSystem, SettlementMetrics& metrics, Chronicle& chronicle);

    // Night events
    void TriggerQuietNight(const TimeSystem& timeSystem, Chronicle& chronicle);

    void TriggerFoodTheft(EntityManager& em, const TimeSystem& timeSystem, const ResourceRegistry& resourceReg, SettlementMetrics& metrics,
                          Chronicle& chronicle);

    void TriggerOminousWhispers(const TimeSystem& timeSystem, SettlementMetrics& metrics, Chronicle& chronicle);

    void TriggerSuspiciousTracks(const TimeSystem& timeSystem, SettlementMetrics& metrics, Chronicle& chronicle);

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
};
