/**
 * @file EventSystemNight.cpp
 * @brief Logic for triggering data-driven night-time events.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Config.hpp"
#include "systems/EventSystem.hpp"

#include <algorithm>
#include <cmath>
#include <raylib.h>

namespace {

int WorldToTile(float worldCoord) {
    return static_cast<int>(std::floor(worldCoord / Config::TILE_SIZE));
}

Vector2 TileToWorldCenter(int tileX, int tileY) {
    return {tileX * Config::TILE_SIZE + Config::TILE_SIZE * 0.5f, tileY * Config::TILE_SIZE + Config::TILE_SIZE * 0.5f};
}

bool IsEntityOnTile(const EntityManager& em, int tileX, int tileY) {
    for (EntityID entity = 0; entity < em.active.size(); ++entity) {
        if (!em.active[entity] || !em.hasTransform[entity]) {
            continue;
        }

        const int ex = WorldToTile(em.transforms[entity].position.x);
        const int ey = WorldToTile(em.transforms[entity].position.y);

        if (ex == tileX && ey == tileY) {
            return true;
        }
    }

    return false;
}

bool IsBlockedTileById(const TileRegistry& tileReg, int tileId) {
    const int voidTile = tileReg.GetTileIdByString("VOID");
    const int lavaTile = tileReg.GetTileIdByString("LAVA");

    if (voidTile >= 0 && tileId == voidTile) {
        return true;
    }

    if (lavaTile >= 0 && tileId == lavaTile) {
        return true;
    }

    return tileId < 0;
}

} // namespace

void EventSystem::HandleNightEvents(EntityManager& em, EntityRegistry& entityReg, const NameRegistry& nameReg,
                                    const BehaviorRegistry& behaviorReg, const EventRuleRegistry& eventRuleReg, const WorldMap& worldMap,
                                    const TileRegistry& tileReg, const TimeSystem& timeSystem, const ResourceRegistry& resourceReg,
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

    const EntityID villageId = FindPrimaryVillage(em);
    const int population = villageId != static_cast<EntityID>(-1) ? CountVillagePopulation(em, villageId) : 0;

    for (const EventRuleDef& rule : eventRuleReg.GetRules()) {
        if (rule.phase != "night") {
            continue;
        }

        if (!IsRuleEligible(rule, timeSystem, population)) {
            continue;
        }

        if (IsRuleOnCooldown(rule, timeSystem)) {
            continue;
        }

        const float chance = ComputeRuleChance(rule, timeSystem, population);
        const int roll = GetRandomValue(1, 100);

        if (static_cast<float>(roll) > chance) {
            continue;
        }

        if (TryTriggerEventRule(em, entityReg, nameReg, behaviorReg, rule, worldMap, tileReg, timeSystem, resourceReg, metrics,
                                chronicle)) {
            MarkRuleTriggered(rule, timeSystem);
            return;
        }
    }
}

bool EventSystem::TryTriggerEventRule(EntityManager& em, EntityRegistry& entityReg, const NameRegistry& nameReg,
                                      const BehaviorRegistry& behaviorReg, const EventRuleDef& rule, const WorldMap& worldMap,
                                      const TileRegistry& tileReg, const TimeSystem& timeSystem, const ResourceRegistry& resourceReg,
                                      SettlementMetrics& metrics, Chronicle& chronicle) {
    if (rule.type == "spawn_wave") {
        return TriggerSpawnWaveRule(em, entityReg, nameReg, behaviorReg, rule, worldMap, tileReg, timeSystem, metrics, chronicle);
    }

    if (rule.type == "food_theft") {
        return TriggerFoodTheftRule(em, rule, timeSystem, resourceReg, metrics, chronicle);
    }

    if (rule.type == "settlement_effect") {
        return TriggerSettlementEffectRule(rule, timeSystem, metrics, chronicle);
    }

    if (rule.type == "chronicle") {
        chronicle.Add(timeSystem.GetDay(), timeSystem.GetSeasonName(), PhaseToString(GetCurrentPhase(timeSystem.GetHour())), rule.message);
        return true;
    }

    return false;
}

bool EventSystem::IsRuleEligible(const EventRuleDef& rule, const TimeSystem& timeSystem, int population) const {
    if (population < rule.minPopulation) {
        return false;
    }

    if (timeSystem.GetSeasonNumber() < rule.minSeasonNumber) {
        return false;
    }

    if (!rule.allowedSeasonIndexes.empty()) {
        const int currentSeason = timeSystem.GetSeasonIndex();

        if (std::find(rule.allowedSeasonIndexes.begin(), rule.allowedSeasonIndexes.end(), currentSeason) ==
            rule.allowedSeasonIndexes.end()) {
            return false;
        }
    }

    return true;
}

float EventSystem::ComputeRuleChance(const EventRuleDef& rule, const TimeSystem& timeSystem, int population) const {
    float chance = rule.baseChance;
    chance += static_cast<float>(population) * rule.populationChanceFactor;
    chance += static_cast<float>(timeSystem.GetSeasonNumber()) * rule.seasonChanceFactor;

    return std::clamp(chance, 0.0f, 100.0f);
}

bool EventSystem::IsRuleOnCooldown(const EventRuleDef& rule, const TimeSystem& timeSystem) const {
    auto it = m_lastTriggeredDayByRule.find(rule.id);

    if (it == m_lastTriggeredDayByRule.end()) {
        return false;
    }

    return timeSystem.GetDay() - it->second < rule.cooldownDays;
}

void EventSystem::MarkRuleTriggered(const EventRuleDef& rule, const TimeSystem& timeSystem) {
    m_lastTriggeredDayByRule[rule.id] = timeSystem.GetDay();
}

bool EventSystem::TriggerSettlementEffectRule(const EventRuleDef& rule, const TimeSystem& timeSystem, SettlementMetrics& metrics,
                                              Chronicle& chronicle) {
    if (rule.fear > 0.0f) {
        AddFear(metrics, rule.fear);
    }

    if (rule.corruption > 0.0f) {
        AddCorruption(metrics, rule.corruption);
    }

    chronicle.Add(timeSystem.GetDay(), timeSystem.GetSeasonName(), PhaseToString(GetCurrentPhase(timeSystem.GetHour())), rule.message);

    return true;
}

bool EventSystem::TriggerFoodTheftRule(EntityManager& em, const EventRuleDef& rule, const TimeSystem& timeSystem,
                                       const ResourceRegistry& resourceReg, SettlementMetrics& metrics, Chronicle& chronicle) {
    const int amountToSteal = GetRandomValue(rule.foodAmount.min, rule.foodAmount.max);

    std::string stolenItem;
    int stolenAmount = 0;

    const bool stolen = RemoveFoodFromAnyStorage(em, resourceReg, amountToSteal, stolenItem, stolenAmount);

    if (!stolen) {
        if (rule.fear > 0.0f) {
            AddFear(metrics, std::max(1.0f, rule.fear * 0.5f));
        }

        chronicle.Add(timeSystem.GetDay(), timeSystem.GetSeasonName(), PhaseToString(GetCurrentPhase(timeSystem.GetHour())),
                      rule.emptyMessage.empty() ? "Something searched the stores, but found nothing worth stealing." : rule.emptyMessage);

        return true;
    }

    if (rule.fear > 0.0f) {
        AddFear(metrics, rule.fear);
    }

    if (rule.corruption > 0.0f) {
        AddCorruption(metrics, rule.corruption);
    }

    chronicle.Add(timeSystem.GetDay(), timeSystem.GetSeasonName(), PhaseToString(GetCurrentPhase(timeSystem.GetHour())),
                  FormatEventMessage(rule.message, 0, stolenItem, stolenAmount));

    return true;
}
