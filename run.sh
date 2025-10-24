#!/bin/bash

# Define the main executable from the Makefile
EXECUTABLE="bin/containment_tycoon"

# --- Utility Functions ---

# Function to pause execution and wait for a key press
wait_for_key() {
    echo "-------------------------"
    read -n 1 -s -r -p "Press any key to continue..."
    echo
}

# Function to handle compilation
compile() {
    echo "--- Compiling Project ---"
    # Execute make all and capture exit status
    make all
    if [ $? -eq 0 ]; then
        echo "✅ Compilation successful!"
        return 0 # Success
    else
        echo "❌ Compilation failed."
        return 1 # Failure
    fi
}

# Function to handle running the game
run_game() {
    if [ -f "$EXECUTABLE" ]; then
        echo "--- Running Game ---"
        # Use exec to replace the shell process with the game process,
        # which can be useful for proper signal handling, but here we just call it directly.
        ./$EXECUTABLE
        return 0 # Success
    else
        echo "❌ Error: The executable '$EXECUTABLE' was not found."
        echo "   Please compile the project first."
        return 1 # Failure
    fi
}

# Function to handle cleaning the project
clean_project() {
    echo "--- Cleaning Project ---"
    make clean
    if [ $? -eq 0 ]; then
        echo "✅ Clean successful."
        return 0 # Success
    else
        echo "❌ Clean failed."
        return 1 # Failure
    fi
}

# --- Main Menu Logic ---

# Function to display the menu
show_menu() {
    clear
    echo "======================================="
    echo "       Containment Tycoon Menu         "
    echo "======================================="
    echo "1) 🌟 Clean, Recompile and Run (make clean && make all && ./$EXECUTABLE)"
    echo "2) 🔨 Clean and Recompile (make clean && make all)"
    echo "3) 🚀 Run Game ($EXECUTABLE)"
    echo "4) 🔄 Recompile and Run (make all && $EXECUTABLE)"
    echo "5) 🧹 Clean Project (make clean)"
    echo "6) 🚪 Exit"
    echo "======================================="
    echo -n "Please enter your choice [1-6]: "
}

# Main loop
while true; do
    show_menu
    read choice
    echo

    case $choice in
        # 1) Clean, Recompile and Run
        1)
            # We call clean_project, then compile. If compile is successful, then we run.
            clean_project
            if compile; then
                # Only run if compilation was successful
                run_game
            fi
            wait_for_key
            ;;

        # 2) Clean and Recompile
        2)
            clean_project
            compile
            wait_for_key
            ;;

        # 3) Run Game
        3)
            run_game
            wait_for_key
            ;;

        # 4) Recompile and Run
        4)
            if compile; then
                # Only run if compilation was successful
                run_game
            fi
            wait_for_key
            ;;

        # 5) Clean Project
        5)
            clean_project
            wait_for_key
            ;;

        # 6) Exit
        6)
            echo "👋 Exiting script. Goodbye!"
            break
            ;;

        # Invalid choice
        *)
            echo "⚠️ Invalid choice '$choice'. Please enter a number between 1 and 6."
            wait_for_key
            ;;
    esac
done
