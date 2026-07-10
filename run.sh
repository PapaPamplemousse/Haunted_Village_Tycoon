#!/bin/bash

BUILD_DIR="build"
EXECUTABLE="bin/HauntedVillageTycoon"

# Fonction pour attendre une touche
wait_for_key() {
    echo "-------------------------"
    read -n 1 -s -r -p "Press any key to continue..."
    echo
}

# Initialisation de CMake avec Ninja si le dossier build n'existe pas
init_cmake() {
    if [ ! -d "$BUILD_DIR" ]; then
        echo " Configuring project with Ninja..."
        cmake -B "$BUILD_DIR" -G Ninja
    fi
}

# Fonction de compilation ultra-rapide via Ninja
compile() {
    init_cmake
    echo "--- Compiling Project with Ninja ---"
    cmake --build "$BUILD_DIR"
    return $?
}

run_game() {
    if [ -f "$EXECUTABLE" ]; then
        echo "--- Running Game ---"
        ./$EXECUTABLE
        return 0
    else
        echo " Error: Executable '$EXECUTABLE' not found. Please compile first."
        return 1
    fi
}

clean_project() {
    echo "--- Cleaning Project ---"
    rm -rf "$BUILD_DIR" bin
    echo " Clean successful."
}

show_menu() {
    clear
    echo "======================================="
    echo "       Haunted Village Tycoon Menu     "
    echo "======================================="
    echo "1)  Clean, Recompile and Run"
    echo "2)  Clean and Recompile"
    echo "3)  Run Game"
    echo "4)  Recompile and Run (Fast Ninja+ccache)"
    echo "5)  Clean Project"
    echo "6)  Exit"
    echo "======================================="
    echo -n "Please enter your choice [1-6]: "
}

while true; do
    show_menu
    read choice
    echo

    case $choice in
        1)
            clean_project
            if compile; then run_game; fi
            wait_for_key
            ;;
        2)
            clean_project
            compile
            wait_for_key
            ;;
        3)
            run_game
            wait_for_key
            ;;
        4)
            if compile; then run_game; fi
            wait_for_key
            ;;
        5)
            clean_project
            wait_for_key
            ;;
        6)
            echo " Exiting script. Goodbye!"
            break
            ;;
        *)
            echo " Error: Invalid choice '$choice'."
            wait_for_key
            ;;
    esac
done
