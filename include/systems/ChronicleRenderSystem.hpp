#pragma once

#include "core/Chronicle.hpp"
#include "core/SettlementMetrics.hpp"

/**
 * @class ChronicleRenderSystem
 * @brief Draws settlement metrics and recent chronicle entries on the HUD.
 */
class ChronicleRenderSystem {
public:
    ChronicleRenderSystem() = default;

    void Render(const Chronicle& chronicle, const SettlementMetrics& metrics) const;
};
