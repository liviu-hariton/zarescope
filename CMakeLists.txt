# SPDX-License-Identifier: GPL-3.0-or-later
#
# Zarescope — RTL-SDR reception in one window: live spectrum, aircraft
# (ADS-B) and Meteor-M weather satellites.
#
# Layout:
#   cmake/        helper modules: warnings, pinned third-party dependencies,
#                 the macOS Info.plist template
#   src/ui/theme  design tokens (pure C++, tested without a window) and the
#                 code that turns them into a Dear ImGui style
#   src/ui        the Dear ImGui screens
#   src/platform  small interfaces (Renderer, AppPaths) with one folder per
#                 operating system behind them; only macOS exists in v1
#   src/app       main() and the frame loop
#   tests         doctest unit tests; none of them needs an RTL-SDR stick
#
# Build:  cmake -S . -B build -G Ninja && cmake --build build
# Test:   ctest --test-dir build --output-on-failure

cmake_minimum_required(VERSION 3.25)

# v1 is built for Apple Silicon only (docs/DECISIONS.md). These must be set
# before project() so the compiler checks use them.
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Target CPU architecture")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.0" CACHE STRING "Oldest macOS the app runs on")
endif()

project(Zarescope
    VERSION 0.9.0
    DESCRIPTION "RTL-SDR reception: live spectrum, aircraft and weather satellites"
    HOMEPAGE_URL "https://zarescope.eu"
    LANGUAGES C CXX)

if(APPLE)
    # Objective-C++ (.mm) is used only under src/platform/macos.
    enable_language(OBJCXX)
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Build out of the source tree: cmake -S . -B build -G Ninja")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(Warnings)
include(Dependencies)

# Files the app reads at run time. The bundle gets copies (src/app); the tests
# read the originals in the source tree.
set(ZARESCOPE_TOKENS_FILE "${CMAKE_CURRENT_SOURCE_DIR}/design/tokens/zarescope.tokens.json")
set(ZARESCOPE_FONTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts")

add_subdirectory(src)

option(ZARESCOPE_BUILD_TESTS "Build the unit tests" ON)
if(ZARESCOPE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Developer tools (not part of the app bundle).
add_subdirectory(tools)
