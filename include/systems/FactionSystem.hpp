/**
 * @file FactionSystem.hpp
 * @brief Updates faction/religion affiliation and social effects.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include "core/SettlementMetrics.hpp"
#include "data/FactionRegistry.hpp"
#include "ecs/EntityManager.hpp"

class FactionSystem {
public:
    FactionSystem() = default;

    void AssignMissingFactions(EntityManager& em, const FactionRegistry& factionReg);
    void Update(float deltaTime, EntityManager& em, const FactionRegistry& factionReg, const SettlementMetrics& metrics);

private:
    float m_updateAccumulator = 0.0f;
};
