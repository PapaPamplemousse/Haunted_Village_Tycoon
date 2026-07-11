#include "systems/LightingSystem.hpp"

#include <algorithm>

unsigned char LightingSystem::LerpChannel(unsigned char from, unsigned char to, float t) const {
    t = std::clamp(t, 0.0f, 1.0f);

    const int a = static_cast<int>(from);
    const int b = static_cast<int>(to);

    return static_cast<unsigned char>(a + static_cast<int>((b - a) * t));
}

Color LightingSystem::LerpColor(Color from, Color to, float t) const {
    return {LerpChannel(from.r, to.r, t), LerpChannel(from.g, to.g, t), LerpChannel(from.b, to.b, t), LerpChannel(from.a, to.a, t)};
}

LightingSystem::DaylightProfile LightingSystem::GetDaylightProfile(int seasonIndex) const {
    switch (seasonIndex) {
        case 0: // Spring
            return {5.5f, 7.0f, 18.5f, 20.0f, 135};

        case 1: // Summer
            return {4.5f, 6.0f, 20.0f, 21.5f, 115};

        case 2: // Autumn
            return {6.0f, 7.5f, 17.5f, 19.0f, 150};

        case 3: // Winter
            return {7.0f, 8.5f, 16.5f, 18.0f, 175};

        default:
            return {6.0f, 7.0f, 18.0f, 20.0f, 140};
    }
}

Color LightingSystem::GetOverlayColor(float hour, int seasonIndex) const {
    const DaylightProfile profile = GetDaylightProfile(seasonIndex);

    const Color noOverlay = {0, 0, 0, 0};
    const Color nightOverlay = {5, 10, 35, profile.nightAlpha};
    const Color sunriseOverlay = {255, 170, 80, 60};
    const Color sunsetOverlay = {255, 110, 55, 75};

    // Full night.
    if (hour >= profile.sunsetEnd || hour < profile.sunriseStart) {
        return nightOverlay;
    }

    // Sunrise transition.
    if (hour >= profile.sunriseStart && hour < profile.sunriseEnd) {
        const float t = (hour - profile.sunriseStart) / (profile.sunriseEnd - profile.sunriseStart);
        return LerpColor(nightOverlay, sunriseOverlay, t);
    }

    // Full day.
    if (hour >= profile.sunriseEnd && hour < profile.sunsetStart) {
        return noOverlay;
    }

    // Sunset transition.
    if (hour >= profile.sunsetStart && hour < profile.sunsetEnd) {
        const float t = (hour - profile.sunsetStart) / (profile.sunsetEnd - profile.sunsetStart);
        return LerpColor(sunsetOverlay, nightOverlay, t);
    }

    return noOverlay;
}

const char* LightingSystem::GetDayPhaseName(float hour, int seasonIndex) const {
    const DaylightProfile profile = GetDaylightProfile(seasonIndex);

    if (hour >= profile.sunsetEnd || hour < profile.sunriseStart) {
        return "Night";
    }

    if (hour >= profile.sunriseStart && hour < profile.sunriseEnd) {
        return "Sunrise";
    }

    if (hour >= profile.sunriseEnd && hour < profile.sunsetStart) {
        return "Day";
    }

    if (hour >= profile.sunsetStart && hour < profile.sunsetEnd) {
        return "Sunset";
    }

    return "Day";
}

void LightingSystem::RenderOverlay(float hour, int seasonIndex) const {
    const Color overlay = GetOverlayColor(hour, seasonIndex);

    if (overlay.a <= 0) {
        return;
    }

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), overlay);
}
