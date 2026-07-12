/**
 * @file STVParser.cpp
 * @brief Implementation of the custom STV file parser.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "data/STVParser.hpp"

#include <fstream>
#include <iostream>

std::vector<STVBlock> STVParser::Parse(const std::string& filepath) {
    std::vector<STVBlock> blocks;
    std::ifstream file(filepath);

    if (!file.is_open()) {
        std::cerr << "[ERROR] STVParser could not open file: " << filepath << std::endl;
        return blocks;
    }

    std::string line;
    STVBlock currentBlock;
    bool readingBlock = false;

    while (std::getline(file, line)) {
        // Strip leading and trailing whitespaces/returns
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // Detect new block header, e.g., [VILLAGER]
        if (line[0] == '[' && line.back() == ']') {
            if (readingBlock && !currentBlock.id.empty()) {
                blocks.push_back(currentBlock);
            }
            currentBlock = STVBlock();
            currentBlock.id = line.substr(1, line.size() - 2);
            readingBlock = true;
            continue;
        }

        // Parse key-value pairs
        if (readingBlock) {
            size_t equalsPos = line.find('=');
            if (equalsPos != std::string::npos) {
                std::string key = line.substr(0, equalsPos);
                std::string value = line.substr(equalsPos + 1);

                // Clean spaces around the equals sign
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);

                currentBlock.properties[key] = value;
            }
        }
    }

    // Push the final block if it exists
    if (readingBlock && !currentBlock.id.empty()) {
        blocks.push_back(currentBlock);
    }

    std::cout << "[INFO] Parsed " << blocks.size() << " blocks from " << filepath << std::endl;
    return blocks;
}
