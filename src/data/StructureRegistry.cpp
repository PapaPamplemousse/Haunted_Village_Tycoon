#include "data/StructureRegistry.hpp"

#include "data/STVParser.hpp"

#include <iostream>
#include <sstream>

bool StructureRegistry::LoadFromSTV(const std::string& filepath) {
    auto blocks = STVParser::Parse(filepath);
    if (blocks.empty())
        return false;

    for (const auto& block : blocks) {
        StructureDef def;
        def.id = block.id;

        if (block.properties.count("name"))
            def.name = block.properties.at("name");
        if (block.properties.count("min_area"))
            def.minArea = std::stoi(block.properties.at("min_area"));
        if (block.properties.count("max_area"))
            def.maxArea = std::stoi(block.properties.at("max_area"));

        if (block.properties.count("requirements")) {
            std::stringstream ss(block.properties.at("requirements"));
            std::string reqToken;
            while (std::getline(ss, reqToken, ',')) {
                std::stringstream pairSS(reqToken);
                std::string reqId, reqCount;
                if (std::getline(pairSS, reqId, ':') && std::getline(pairSS, reqCount)) {
                    reqId.erase(0, reqId.find_first_not_of(" \t"));
                    reqId.erase(reqId.find_last_not_of(" \t") + 1);
                    def.requirements[reqId] = std::stoi(reqCount);
                }
            }
        }

        if (block.properties.count("granted_buffs")) {
            std::stringstream ss(block.properties.at("granted_buffs"));
            std::string buff;
            while (std::getline(ss, buff, ',')) {
                buff.erase(0, buff.find_first_not_of(" \t"));
                buff.erase(buff.find_last_not_of(" \t") + 1);
                def.grantedBuffs.push_back(buff);
            }
        }

        if (block.properties.count("job_slots")) {
            std::stringstream ss(block.properties.at("job_slots"));
            std::string jobToken;
            while (std::getline(ss, jobToken, ',')) {
                std::stringstream pairSS(jobToken);
                std::string jobId, jobCount;
                if (std::getline(pairSS, jobId, ':') && std::getline(pairSS, jobCount)) {
                    jobId.erase(0, jobId.find_first_not_of(" \t"));
                    jobId.erase(jobId.find_last_not_of(" \t") + 1);
                    def.jobSlots[jobId] = std::stoi(jobCount);
                }
            }
        }

        m_templates[block.id] = def;
    }
    std::cout << "[INFO] Loaded " << m_templates.size() << " structure definitions." << std::endl;
    return true;
}
