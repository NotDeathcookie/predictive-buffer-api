cmake_minimum_required(VERSION 3.21)
# Geode v5 requires C++23 (see the v5 migration guide). The original project's CMake said 20.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

project(predictive-buffer-api VERSION 0.1.0)

option(PBUF_NO_PLATFORMER_DETECT "Do not call GJGameLevel::isPlatformer() (unverified API)" OFF)

# Engine core: no Geode dependency (also built standalone by tools/build_and_test.sh).
set(PBUF_CORE_SOURCES src/core/Engine.cpp src/core/Scheduler.cpp)
set(PBUF_GEODE_SOURCES
    src/geode/main.cpp
    src/geode/Hooks.cpp
    src/geode/GeodeBackend.cpp
    src/geode/Runtime.cpp
    src/geode/ApiExports.cpp)

# One shared library, sources compiled straight into it. NOT a static library: Geode's
# $modify hook registrations are static initialisers, which a linker may drop from an
# unreferenced static-library object.
add_library(${PROJECT_NAME} SHARED ${PBUF_CORE_SOURCES} ${PBUF_GEODE_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE include src/core src/geode)
if (PBUF_NO_PLATFORMER_DETECT)
    target_compile_definitions(${PROJECT_NAME} PRIVATE PBUF_NO_PLATFORMER_DETECT)
endif()

if (NOT DEFINED ENV{GEODE_SDK})
    message(FATAL_ERROR "Unable to find Geode SDK! Please define GEODE_SDK environment variable to point to Geode")
else()
    message(STATUS "Found Geode: $ENV{GEODE_SDK}")
endif()

add_subdirectory($ENV{GEODE_SDK} ${CMAKE_CURRENT_BINARY_DIR}/geode)

setup_geode_mod(${PROJECT_NAME})
