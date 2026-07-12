/**
 * @file EventSystemNight.cpp
 * @brief Logic for triggering random and systemic night-time events.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/EventSystem.hpp"

#include <raylib.h>

void EventSystem::HandleNightEvents(EntityManager& em, const TimeSystem& timeSystem, const ResourceRegistry& resourceReg,
                                    SettlementMetrics& metrics, Chronicle& chronicle) {
    const EventDayPhase phase = GetCurrentPhase(timeSystem.GetHour());

    if (phase != EventDayPhase::Night) {
        return;
    }

    // Trigger night events only when night starts, not after midnight.
    if (timeSystem.GetHour() < 20.0f) {
        return;
    }

    if (m_lastNightEventDay == timeSystem.GetDay()) {
        return;
    }

    m_lastNightEventDay = timeSystem.GetDay();

    const int roll = GetRandomValue(1, 100);

    if (roll <= 35) {
        TriggerQuietNight(timeSystem, chronicle);
    } else if (roll <= 60) {
        TriggerOminousWhispers(timeSystem, metrics, chronicle);
    } else if (roll <= 80) {
        TriggerSuspiciousTracks(timeSystem, metrics, chronicle);
    } else {
        TriggerFoodTheft(em, timeSystem, resourceReg, metrics, chronicle);
    }
}

void EventSystem::TriggerQuietNight(const TimeSystem& timeSystem, Chronicle& chronicle) {
    chronicle.Add(timeSystem.GetDay(), timeSystem.GetSeasonName(), "Night", "The village endured a quiet night. Suspiciously quiet.");
}

void EventSystem::TriggerOminousWhispers(const TimeSystem& timeSystem, SettlementMetrics& metrics, Chronicle& chronicle) {
    AddFear(metrics, 5.0f);
    AddCorruption(metrics, 1.0f);

    chronicle.Add(timeSystem.GetDay(), timeSystem.GetSeasonName(), "Night",
                  "Whispers moved between the houses. Nobody admits hearing them first.");
}

void EventSystem::TriggerSuspiciousTracks(const TimeSystem& timeSystem, SettlementMetrics& metrics, Chronicle& chronicle) {
    AddFear(metrics, 2.0f);

    chronicle.Add(timeSystem.GetDay(), timeSystem.GetSeasonName(), "Night", "Suspicious tracks were found near the village edge at dawn.");
}

void EventSystem::TriggerFoodTheft(EntityManager& em, const TimeSystem& timeSystem, const ResourceRegistry& resourceReg,
                                   SettlementMetrics& metrics, Chronicle& chronicle) {
    std::string stolenItem;
    int stolenAmount = 0;

    const bool stolen = RemoveFoodFromAnyStorage(em, resourceReg, GetRandomValue(1, 4), stolenItem, stolenAmount);

    if (!stolen) {
        AddFear(metrics, 2.0f);

        chronicle.Add(timeSystem.GetDay(), timeSystem.GetSeasonName(), "Night",
                      "Something searched the stores, but found nothing worth stealing.");

        return;
    }

    AddFear(metrics, 4.0f);

    chronicle.Add(timeSystem.GetDay(), timeSystem.GetSeasonName(), "Night",
                  std::to_string(stolenAmount) + " " + stolenItem + " disappeared from storage during the night.");
}
