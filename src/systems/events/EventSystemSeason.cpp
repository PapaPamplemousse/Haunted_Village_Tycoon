#include "systems/EventSystem.hpp"

void EventSystem::HandleSeasonEvents(const TimeSystem& timeSystem, SettlementMetrics& metrics, Chronicle& chronicle) {
    const int currentSeasonIndex = timeSystem.GetSeasonIndex();

    if (m_lastProcessedSeasonIndex == currentSeasonIndex) {
        return;
    }

    m_lastProcessedSeasonIndex = currentSeasonIndex;

    TriggerSeasonChanged(timeSystem, metrics, chronicle);
}

void EventSystem::TriggerSeasonChanged(const TimeSystem& timeSystem, SettlementMetrics& metrics, Chronicle& chronicle) {
    const int seasonIndex = timeSystem.GetSeasonIndex();

    std::string message;

    switch (seasonIndex) {
        case 0:
            message = "Spring begins. The mud is optimistic.";
            break;
        case 1:
            message = "Summer begins. The days grow long, and excuses grow short.";
            break;
        case 2:
            message = "Autumn begins. The village counts its stores twice.";
            break;
        case 3:
            message = "Winter begins. The cold has excellent bookkeeping.";
            AddFear(metrics, 3.0f);
            break;
        default:
            message = "A new season begins.";
            break;
    }

    chronicle.Add(timeSystem.GetDay(), timeSystem.GetSeasonName(), "Season", message);
}

void EventSystem::HandleDawnEvents(const TimeSystem& timeSystem, SettlementMetrics& metrics, Chronicle& chronicle) {
    const EventDayPhase phase = GetCurrentPhase(timeSystem.GetHour());

    if (phase != EventDayPhase::Dawn) {
        return;
    }

    if (m_lastDawnEventDay == timeSystem.GetDay()) {
        return;
    }

    m_lastDawnEventDay = timeSystem.GetDay();

    if (metrics.fear > 60.0f) {
        chronicle.Add(timeSystem.GetDay(), timeSystem.GetSeasonName(), "Dawn",
                      "The village wakes tense. Even the roosters sound like witnesses.");
    } else {
        chronicle.Add(timeSystem.GetDay(), timeSystem.GetSeasonName(), "Dawn",
                      "Dawn breaks. The village pretends this was always guaranteed.");
    }
}
