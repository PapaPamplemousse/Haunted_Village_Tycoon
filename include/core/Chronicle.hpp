/**
 * @file Chronicle.hpp
 * @brief Stores important simulation events and history.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once

#include <string>
#include <vector>

struct ChronicleEntry {
    int day = 1;
    std::string season;
    std::string phase;
    std::string message;
};

/**
 * @class Chronicle
 * @brief Stores important simulation events.
 */
class Chronicle {
public:
    void Add(int day, const std::string& season, const std::string& phase, const std::string& message);

    const std::vector<ChronicleEntry>& GetEntries() const {
        return m_entries;
    }

private:
    std::vector<ChronicleEntry> m_entries;
};
