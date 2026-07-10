#pragma once
#include "data/BehaviorRegistry.hpp"
#include "data/ProfessionRegistry.hpp"
#include "ecs/EntityManager.hpp"

class ProfessionSystem {
public:
    void Update(EntityManager& em, const ProfessionRegistry& profReg, const BehaviorRegistry& behReg);
};
