/**
 * @file main.c
 * @brief Entry point that starts the Haunted Village Tycoon application loop.
 */

#include "app.h"

int main(void)
{
    // Run the high-level application loop defined in the core module.
    app_run();

    // Indicate successful termination to the operating system.
    return 0;
}
