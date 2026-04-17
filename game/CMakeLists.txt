cmake_minimum_required(VERSION 3.24)
project(fps_game LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# --- Raylib ---------------------------------------------------------------
find_package(raylib 5.0 QUIET)
if(NOT raylib_FOUND)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
    set(RAYLIB_VERSION 5.5)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG        ${RAYLIB_VERSION}
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(raylib)
endif()

# --- enet (optional, required for fps_server) -----------------------------
find_path(ENET_INCLUDE_DIR enet/enet.h
    PATHS /opt/homebrew/include /usr/local/include /usr/include
)
find_library(ENET_LIBRARY
    NAMES enet
    PATHS /opt/homebrew/lib /usr/local/lib /usr/lib
)

# --- Shared simulation library -------------------------------------------
set(FPS_SIM_SOURCES
    src/ai/Bot.cpp
    src/ai/Nav.cpp
    src/core/Cli.cpp
    src/player/Player.cpp
    src/weapons/Smoke.cpp
    src/weapons/Weapon.cpp
    src/world/Map.cpp
)

add_library(fps_sim STATIC ${FPS_SIM_SOURCES})
target_include_directories(fps_sim PUBLIC src)
target_link_libraries(fps_sim PUBLIC raylib)

# --- fps_game client ------------------------------------------------------
set(FPS_CLIENT_SOURCES
    src/core/App.cpp
    src/render/Hud.cpp
    src/render/SoundBank.cpp
    src/main.cpp
)

add_executable(fps_game ${FPS_CLIENT_SOURCES})
target_link_libraries(fps_game PRIVATE fps_sim raylib)

if(ENET_INCLUDE_DIR AND ENET_LIBRARY)
    target_sources(fps_game PRIVATE src/net/NetClient.cpp)
    target_include_directories(fps_game PRIVATE ${ENET_INCLUDE_DIR})
    target_link_libraries(fps_game PRIVATE ${ENET_LIBRARY})
    target_compile_definitions(fps_game PRIVATE FPS_HAS_ENET=1)
endif()

# --- fps_server (headless) -----------------------------------------------
if(ENET_INCLUDE_DIR AND ENET_LIBRARY)
    add_executable(fps_server
        src/server_main.cpp
        src/net/NetServer.cpp
    )
    target_link_libraries(fps_server PRIVATE fps_sim ${ENET_LIBRARY})
    target_include_directories(fps_server PRIVATE ${ENET_INCLUDE_DIR})
    set_target_properties(fps_server PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY              ${CMAKE_BINARY_DIR}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG        ${CMAKE_BINARY_DIR}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE      ${CMAKE_BINARY_DIR}
    )
else()
    message(STATUS "enet not found - skipping fps_server target (run `brew install enet`)")
endif()

if(APPLE)
    target_link_libraries(fps_sim PUBLIC "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()
if(WIN32)
    target_link_libraries(fps_sim PUBLIC winmm ws2_32)
endif()

set_target_properties(fps_game PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY              ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_DEBUG        ${CMAKE_BINARY_DIR}
    RUNTIME_OUTPUT_DIRECTORY_RELEASE      ${CMAKE_BINARY_DIR}
)

add_custom_command(TARGET fps_game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/assets
)

if(MSVC)
    target_compile_options(fps_sim PRIVATE /W4 /permissive-)
    target_compile_options(fps_game PRIVATE /W4 /permissive-)
else()
    target_compile_options(fps_sim PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(fps_game PRIVATE -Wall -Wextra -Wpedantic)
endif()
