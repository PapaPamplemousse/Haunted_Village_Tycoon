/**
 * @file STVParser.hpp
 * @brief Utility class for reading custom .stv files and extracting structured data blocks.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#pragma once
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @struct STVBlock
 * @brief Represents a single parsed block from an .stv file.
 */
struct STVBlock {
    std::string id;                                          // E.g., "VILLAGER"
    std::unordered_map<std::string, std::string> properties; // Key-value pairs inside the block
};

/**
 * @class STVParser
 * @brief Utility class responsible for reading .stv files and extracting structured blocks.
 * This ensures file I/O and string parsing logic is written only once.
 */
class STVParser {
public:
    /**
     * @brief Parses the given file into a list of STVBlocks.
     * @param filepath The relative path to the .stv file.
     * @return std::vector<STVBlock> The extracted blocks. Returns an empty vector on failure.
     */
    static std::vector<STVBlock> Parse(const std::string& filepath);
};
