#include "systems/TimeSystem.hpp"

#include "core/Config.hpp"

int TimeSystem::GetSeasonIndex() const {
    return ((m_day - 1) / DAYS_PER_SEASON) % SEASON_COUNT;
}

int TimeSystem::GetSeasonNumber() const {
    return ((m_day - 1) / DAYS_PER_SEASON) + 1;
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

void TimeSystem::Update(float deltaTime, EntityManager& em) {
    m_hour += (deltaTime * Config::TIME_SCALE) / 60.0f;

    while (m_hour >= 24.0f) {
        m_hour -= 24.0f;
        m_day++;
    }

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

        const bool isResting = em.hasBehavior[i] && em.behaviors[i].currentTask == "resting";

        if (!isResting) {
            needs.fatigue += Config::FATIGUE_GAIN_PER_SECOND * deltaTime;

            if (needs.fatigue > needs.maxFatigue) {
                needs.fatigue = needs.maxFatigue;
            }
        }
    }
}
