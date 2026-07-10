#include "systems/TimeSystem.hpp"

void TimeSystem::Update(float deltaTime, EntityManager& em) {
    // 1. Update Global Clock
    m_hour += (deltaTime * TIME_SCALE) / 60.0f;
    if (m_hour >= 24.0f) {
        m_hour -= 24.0f;
        m_day++;
    }

    // 2. Process passive Needs (Hunger)
    for (size_t i = 0; i < em.active.size(); ++i) {
        if (!em.active[i] || !em.hasNeeds[i])
            continue;

        // Entities lose 0.5 hunger per real-time second
        em.needs[i].hunger -= deltaTime * 0.5f;

        if (em.needs[i].hunger < 0.0f) {
            em.needs[i].hunger = 0.0f;
            // TODO later: Apply starvation damage to HealthComponent
        }
    }
}
