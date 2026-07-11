#pragma once

#include "data/BehaviorRegistry.hpp"
#include "data/ProfessionRegistry.hpp"
#include "ecs/EntityManager.hpp"

class ProfessionSystem {
public:
    void Update(float deltaTime, EntityManager& em, const ProfessionRegistry& profReg, const BehaviorRegistry& behReg);

private:
    float m_updateAccumulator = 0.0f;
};
