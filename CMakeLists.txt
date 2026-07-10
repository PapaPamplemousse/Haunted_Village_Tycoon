cmake_minimum_required(VERSION 3.14)
project(HauntedVillageTycoon CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build." FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif()

set(SUPPORT_WAYLAND ON CACHE BOOL "" FORCE)
set(SUPPORT_X11 OFF CACHE BOOL "" FORCE)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE include)

include(FetchContent)
FetchContent_Declare(
    raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(raylib)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib)

if(ENABLE_DEV_MODE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DEV_MODE)
endif()
