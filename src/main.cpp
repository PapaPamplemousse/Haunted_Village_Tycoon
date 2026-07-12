// ---------------------------------------------------------
// src/main.cpp
// ---------------------------------------------------------
/**
 * @file main.cpp
 * @brief Main entry point of the Haunted Village Tycoon application.
 * @author Hugo Reif Faudemer (PapaPamplemousse)
 */
#include "core/Application.hpp"

#include <iostream>

/**
 * @brief The main entry point of the application.
 */
int main() {
    std::cout << "Starting Haunted Village Tycoon..." << std::endl;

    Application app;
    app.Run();

    std::cout << "Engine shut down successfully." << std::endl;
    return 0;
}
