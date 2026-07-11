#pragma once

#include <raylib.h>

/**
 * @class LightingSystem
 * @brief Handles visual day/night lighting overlay based on time and season.
 *
 * This system is purely visual. It does not drive gameplay logic.
 */
class LightingSystem {
public:
    LightingSystem() = default;

    void RenderOverlay(float hour, int seasonIndex) const;

    const char* GetDayPhaseName(float hour, int seasonIndex) const;

private:
    struct DaylightProfile {
        float sunriseStart = 6.0f;
        float sunriseEnd = 7.0f;
        float sunsetStart = 18.0f;
        float sunsetEnd = 20.0f;
        unsigned char nightAlpha = 140;
    };

    DaylightProfile GetDaylightProfile(int seasonIndex) const;

    Color GetOverlayColor(float hour, int seasonIndex) const;

    Color LerpColor(Color from, Color to, float t) const;

    unsigned char LerpChannel(unsigned char from, unsigned char to, float t) const;
};
