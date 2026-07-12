#pragma once

#include "ecs/EntityManager.hpp"

class TimeSystem {
public:
    TimeSystem() = default;

    void Update(float deltaTime, EntityManager& em);

    int GetDay() const {
        return m_day;
    }

    float GetHour() const {
        return m_hour;
    }

    int GetSeasonIndex() const;
    int GetSeasonNumber() const;
    int GetDayInSeason() const;
    const char* GetSeasonName() const;

private:
    static constexpr int DAYS_PER_SEASON = 5;
    static constexpr int SEASON_COUNT = 4;

    int m_day = 1;
    float m_hour = 8.0f;
};
