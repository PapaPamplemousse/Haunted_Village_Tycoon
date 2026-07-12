/**
 * @file SettlementMetrics.hpp
 * @brief Global settlement-level social and supernatural meters (Faith, Fear, Corruption, etc.).
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

/**
 * @struct SettlementMetrics
 * @brief Global settlement-level social and supernatural meters.
 */
struct SettlementMetrics {
    float faith = 50.0f;
    float fear = 0.0f;
    float corruption = 0.0f;
    float reputation = 0.0f;
};
