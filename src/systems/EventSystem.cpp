/**
 * @file EventSystem.cpp
 * @brief Main event system loop implementation.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "systems/EventSystem.hpp"

void EventSystem::Update(EntityManager& em, EntityRegistry& entityReg, const NameRegistry& nameReg, const BehaviorRegistry& behaviorReg,
                         const EventRuleRegistry& eventRuleReg, const WorldMap& worldMap, const TileRegistry& tileReg,
                         const TimeSystem& timeSystem, const ResourceRegistry& resourceReg, SettlementMetrics& metrics,
                         Chronicle& chronicle) {
    HandleSeasonEvents(timeSystem, metrics, chronicle);

    HandleNightEvents(em, entityReg, nameReg, behaviorReg, eventRuleReg, worldMap, tileReg, timeSystem, resourceReg, metrics, chronicle);

    HandleDawnEvents(timeSystem, metrics, chronicle);

    m_lastProcessedDay = timeSystem.GetDay();
}
