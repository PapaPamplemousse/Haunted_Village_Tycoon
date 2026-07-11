#include "systems/TimeSystem.hpp"

#include "core/Config.hpp"

void TimeSystem::Update(float deltaTime, EntityManager& em) {
    // 1. Update global clock
    m_hour += (deltaTime * TIME_SCALE) / 60.0f;

    if (m_hour >= 24.0f) {
        m_hour -= 24.0f;
        m_day++;
    }

    // 2. Process passive needs
    for (size_t i = 0; i < em.active.size(); ++i) {
        if (!em.active[i] || !em.hasNeeds[i]) {
            continue;
        }

        auto& needs = em.needs[i];

        needs.hunger -= Config::HUNGER_DECAY_PER_SECOND * deltaTime;

        if (needs.hunger < 0.0f) {
            needs.hunger = 0.0f;
        }

        if (needs.hunger <= 0.0f) {
            em.DestroyEntity(i);
            continue;
        }
    }
}
