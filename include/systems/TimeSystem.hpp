#pragma once
#include "ecs/EntityManager.hpp"

/**
 * @class TimeSystem
 * @brief Manages the global game clock and passive over-time mechanics (like hunger decay).
 */
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

private:
    int m_day = 1;
    float m_hour = 8.0f;            // Start the game at 08:00 AM
    const float TIME_SCALE = 20.0f; // 1 real second = 20 in-game minutes
};
