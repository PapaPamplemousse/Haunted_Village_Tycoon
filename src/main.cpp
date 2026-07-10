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
