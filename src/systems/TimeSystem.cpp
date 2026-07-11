#include "systems/TimeSystem.hpp"

#include "core/Config.hpp"

int TimeSystem::GetSeasonIndex() const {
    return ((m_day - 1) / DAYS_PER_SEASON) % SEASON_COUNT;
}

int TimeSystem::GetDayInSeason() const {
    return ((m_day - 1) % DAYS_PER_SEASON) + 1;
}

const char* TimeSystem::GetSeasonName() const {
    switch (GetSeasonIndex()) {
        case 0:
            return "Spring";
        case 1:
            return "Summer";
        case 2:
            return "Autumn";
        case 3:
            return "Winter";
        default:
            return "Unknown";
    }
}

void TimeSystem::ApplySeasonAging(EntityManager& em) {
    for (size_t i = 0; i < em.active.size(); ++i) {
        if (!em.active[i] || !em.hasTag[i]) {
            continue;
        }

        // Only humans age with seasons for now.
        // Cannibals, eldritch horrors, animals, etc. can get their own aging rules later.
        if (em.tags[i].category == "humanoid") {
            em.tags[i].age += 1;
        }
    }
}

void TimeSystem::Update(float deltaTime, EntityManager& em) {
    // 1. Update global clock.
    m_hour += (deltaTime * Config::TIME_SCALE) / 60.0f;

    while (m_hour >= 24.0f) {
        m_hour -= 24.0f;
        m_day++;

        // A season starts every 5 in-game days.
        // Day 1 = Spring 1/5
        // Day 6 = Summer 1/5
        // Day 11 = Autumn 1/5
        // Day 16 = Winter 1/5
        // Day 21 = Spring 1/5 again
        if (GetDayInSeason() == 1) {
            ApplySeasonAging(em);
        }
    }

    // 2. Process passive needs.
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
