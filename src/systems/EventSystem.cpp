#include "systems/EventSystem.hpp"

void EventSystem::Update(EntityManager& em, const TimeSystem& timeSystem, const ResourceRegistry& resourceReg, SettlementMetrics& metrics,
                         Chronicle& chronicle) {
    HandleSeasonEvents(timeSystem, metrics, chronicle);

    HandleNightEvents(em, timeSystem, resourceReg, metrics, chronicle);

    HandleDawnEvents(timeSystem, metrics, chronicle);

    m_lastProcessedDay = timeSystem.GetDay();
}
