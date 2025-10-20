#!/bin/bash

# Define the main executable from the Makefile
EXECUTABLE="bin/containment_tycoon"

# Function to display the menu
show_menu() {
    clear
    echo "======================================="
    echo "         Containment Tycoon Menu       "
    echo "======================================="
    echo "1) 🔨 Clean and Recompile (make clean && make all)"
    echo "2) 🚀 Run Game ($EXECUTABLE)"
    echo "3) 🔄 Recompile and Run (make all && $EXECUTABLE)"
    echo "4) 🧹 Clean Project (make clean)"
    echo "5) 🚪 Exit"
    echo "======================================="
    echo -n "Please enter your choice [1-5]: "
}

# Function to handle compilation
compile() {
    echo "--- Compiling Project ---"
    make all
    if [ $? -eq 0 ]; then
        echo "✅ Compilation successful!"
    else
        echo "❌ Compilation failed."
    fi
    echo "-------------------------"
    read -n 1 -s -r -p "Press any key to continue..."
}

# Function to handle running the game
run_game() {
    if [ -f "$EXECUTABLE" ]; then
        echo "--- Running Game ---"
        ./$EXECUTABLE
    else
        echo "❌ Error: The executable '$EXECUTABLE' was not found."
        echo "   Please compile the project first (Option 1 or 3)."
    fi
    read -n 1 -s -r -p "Press any key to continue..."
}

# Function to handle cleaning the project
clean_project() {
    echo "--- Cleaning Project ---"
    make clean
    if [ $? -eq 0 ]; then
        echo "✅ Clean successful."
    else
        echo "❌ Clean failed."
    fi
    echo "------------------------"
    read -n 1 -s -r -p "Press any key to continue..."
}

# Main loop
while true; do
    show_menu
    read choice
    echo

    case $choice in
        1)  # Clean and Recompile
            clean_project
            compile
            ;;
        2)  # Run Game
            run_game
            ;;
        3)  # Recompile and Run
            compile
            if [ -f "$EXECUTABLE" ]; then
                run_game
            fi
            ;;
        4)  # Clean Project
            clean_project
            ;;
        5)  # Exit
            echo "👋 Exiting script. Goodbye!"
            break
            ;;
        *)
            echo "⚠️ Invalid choice '$choice'. Please enter a number between 1 and 5."
            read -n 1 -s -r -p "Press any key to continue..."
            ;;
    esac
done
