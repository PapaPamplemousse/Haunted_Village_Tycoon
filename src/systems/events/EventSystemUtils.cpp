/**
 * @file EventSystemUtils.cpp
 * @brief Helper functions for parsing day phases used by the event system.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/EventSystem.hpp"

EventDayPhase EventSystem::GetCurrentPhase(float hour) const {
    if (hour >= 5.0f && hour < 7.0f) {
        return EventDayPhase::Dawn;
    }

    if (hour >= 7.0f && hour < 18.0f) {
        return EventDayPhase::Day;
    }

    if (hour >= 18.0f && hour < 20.0f) {
        return EventDayPhase::Dusk;
    }

    return EventDayPhase::Night;
}

const char* EventSystem::PhaseToString(EventDayPhase phase) const {
    switch (phase) {
        case EventDayPhase::Dawn:
            return "Dawn";
        case EventDayPhase::Day:
            return "Day";
        case EventDayPhase::Dusk:
            return "Dusk";
        case EventDayPhase::Night:
            return "Night";
        default:
            return "Unknown";
    }
}
