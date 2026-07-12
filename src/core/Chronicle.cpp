/**
 * @file Chronicle.cpp
 * @brief Implementation of the Chronicle event logger.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Chronicle.hpp"

#include <iostream>

void Chronicle::Add(int day, const std::string& season, const std::string& phase, const std::string& message) {
    m_entries.push_back({day, season, phase, message});

    std::cout << "[CHRONICLE] Day " << day << " - " << season << " - " << phase << ": " << message << std::endl;
}
