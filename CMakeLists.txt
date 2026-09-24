# ===================================
# Velocity-Next - CMake superproject
# ===================================
# Project structure:
#   cmake/            - CMake modules (msvc-toolchain.cmake, etc.)
#   Velocity/         - Main GUI application
#   XboxInternals/    - Xbox file format library
#   externals/vcpkg/  - vcpkg package manager (Git submodule; provides Botan)
#   out/build/        - Generated build outputs (not in source control)
#   specs/            - Feature specifications and documentation
cmake_minimum_required(VERSION 3.21)

# Prevent in-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "
In-source builds are not allowed!

Please create a separate build directory and run CMake from there:

  cmake -B out/build/your-preset
  cmake --build out/build/your-preset

Or use a preset:

  cmake --preset windows-mingw-release
  cmake --build --preset windows-mingw-release

To clean up, remove: CMakeCache.txt and CMakeFiles/
")
endif()

project(VelocityNext VERSION 0.2.0.0 LANGUAGES CXX)

# Display build configuration for debugging
message(STATUS "Generator: ${CMAKE_GENERATOR}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
if(CMAKE_GENERATOR_PLATFORM)
    message(STATUS "Platform: ${CMAKE_GENERATOR_PLATFORM}")
endif()

# Export compilation database for clang-tidy / clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Use folders in IDE (Visual Studio) for cleaner grouping
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Build System Strategy:
# - All presets use the Ninja generator (including MSVC) for consistency across platforms
# - This provides faster builds and uniform behavior between MinGW/MSVC/GCC/Clang
# - If you prefer the Visual Studio generator for IDE integration:
#   * Use: cmake -G "Visual Studio 17 2022" -A x64 -S . -B build
#   * Note: VS generator uses multi-config (Debug/Release in same tree)
#   * Presets use single-config (separate directories per build type)

# Platform-specific settings
if(APPLE)
    set(PLATFORM_BUILD_NAME "OSX" CACHE STRING "Platform build name")
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_BUILD_NAME "Unix" CACHE STRING "Platform build name") 
elseif(WIN32)
    set(PLATFORM_BUILD_NAME "Win" CACHE STRING "Platform build name")
endif()

# Enforce C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Create an INTERFACE library to hold common compile options
add_library(velocity_compiler_flags INTERFACE)

# Ensure MSVC reports correct __cplusplus value for C++20 compliance
if(MSVC)
    target_compile_options(velocity_compiler_flags INTERFACE /Zc:__cplusplus)
endif()

# All Velocity targets (XboxInternals, Velocity) must link against velocity_compiler_flags
# to inherit common compile options (e.g., MSVC /Zc:__cplusplus for C++20 compliance)
# Example: target_link_libraries(MyTarget PRIVATE velocity_compiler_flags)

# ------------------------------
# Options
# ------------------------------
# XboxInternals library build configuration:
# - Default: BUILD_XBOXINTERNALS_SHARED=ON (shared library only)
# - Both ON: Builds both shared and static variants
# - Both OFF: Automatically enables STATIC to ensure library is built
option(BUILD_XBOXINTERNALS_SHARED "Build XboxInternals as a shared library" ON)
option(BUILD_XBOXINTERNALS_STATIC "Also build XboxInternals as a static library" OFF)

# Ensure at least one library type is built
if(NOT BUILD_XBOXINTERNALS_SHARED AND NOT BUILD_XBOXINTERNALS_STATIC)
    message(STATUS "Neither SHARED nor STATIC selected for XboxInternals; defaulting to STATIC")
    set(BUILD_XBOXINTERNALS_STATIC ON)
endif()

# Qt setup
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ------------------------------
# Qt Discovery
# ------------------------------
set(QT_MIN_VERSION "6.7.3")

# Make cmake/ available for SideloadQt.cmake
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(SideloadQt)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Xml Widgets Network Concurrent)

message(STATUS "Found Qt6 ${Qt6Core_VERSION} at: ${Qt6Core_DIR}")

# ====================================================================
# BOTAN INTEGRATION (via vcpkg)
# ====================================================================
# Botan is provided as a regular vcpkg dependency (see vcpkg.json). The
# vcpkg toolchain file is wired up in CMakePresets.json and resolves the
# externals/vcpkg submodule automatically, bootstrapping vcpkg on first
# configure if needed.
# ====================================================================

find_package(Botan 3 CONFIG REQUIRED)

# ====================================================================
# BUILD CONFIGURATION SUMMARY
# ====================================================================
# Display a comprehensive summary of the build configuration
# ====================================================================

# Capture configuration start time
string(TIMESTAMP CONFIG_START_TIME "%s")

# Detect compiler name and version
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    set(COMPILER_NAME "GCC")
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE COMPILER_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(COMPILER_NAME "MSVC")
    set(COMPILER_VERSION ${MSVC_VERSION})
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(COMPILER_NAME "Clang")
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} --version OUTPUT_VARIABLE CLANG_VERSION_OUTPUT)
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" COMPILER_VERSION "${CLANG_VERSION_OUTPUT}")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
    set(COMPILER_NAME "Apple Clang")
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} --version OUTPUT_VARIABLE CLANG_VERSION_OUTPUT)
    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" COMPILER_VERSION "${CLANG_VERSION_OUTPUT}")
else()
    set(COMPILER_NAME ${CMAKE_CXX_COMPILER_ID})
    set(COMPILER_VERSION "unknown")
endif()

# Detect platform name
if(WIN32)
    set(PLATFORM_NAME "Windows")
elseif(APPLE)
    set(PLATFORM_NAME "macOS")
elseif(UNIX)
    set(PLATFORM_NAME "Linux")
else()
    set(PLATFORM_NAME ${CMAKE_SYSTEM_NAME})
endif()

# Build library types
set(LIBRARY_TYPES "")
if(BUILD_XBOXINTERNALS_SHARED)
    list(APPEND LIBRARY_TYPES "Shared")
endif()
if(BUILD_XBOXINTERNALS_STATIC)
    list(APPEND LIBRARY_TYPES "Static")
endif()
string(REPLACE ";" ", " LIBRARY_TYPES_STR "${LIBRARY_TYPES}")

# Check for ccache (compiler cache for faster rebuilds)
find_program(CCACHE_EXECUTABLE ccache)
if(CCACHE_EXECUTABLE)
    set(CCACHE_STATUS "✓ detected (faster rebuilds)")
else()
    set(CCACHE_STATUS "not found")
endif()

# Detect CPU core count for parallel build hints
cmake_host_system_information(RESULT CPU_CORES QUERY NUMBER_OF_LOGICAL_CORES)
if(CPU_CORES)
    set(PARALLEL_BUILD_HINT "-j${CPU_CORES}")
else()
    set(PARALLEL_BUILD_HINT "-j4")
    set(CPU_CORES "unknown")
endif()

message(STATUS "")
message(STATUS "═══════════════════════════════════════════════════════")
message(STATUS "             Build Configuration Summary")
message(STATUS "═══════════════════════════════════════════════════════")
message(STATUS "  Platform:  ${PLATFORM_NAME} (${COMPILER_NAME} ${COMPILER_VERSION})")
message(STATUS "  Generator: ${CMAKE_GENERATOR}")
message(STATUS "  Build:     ${CMAKE_BUILD_TYPE}")
message(STATUS "───────────────────────────────────────────────────────")
message(STATUS "  Qt:        ${Qt6Core_VERSION}   ✓ (requires ${QT_MIN_VERSION}+)")
message(STATUS "  Botan:     ${Botan_VERSION}   ✓ (via vcpkg)")
message(STATUS "  CMake:     ${CMAKE_VERSION}  ✓ (requires ${CMAKE_MINIMUM_REQUIRED_VERSION}+)")
message(STATUS "───────────────────────────────────────────────────────")
message(STATUS "  XboxInternals: ${LIBRARY_TYPES_STR}")
message(STATUS "  Output:        ${CMAKE_BINARY_DIR}")
message(STATUS "  Targets:       VelocityNext, XboxInternals")
message(STATUS "───────────────────────────────────────────────────────")
message(STATUS "  Ccache:        ${CCACHE_STATUS}")
message(STATUS "  CPU Cores:     ${CPU_CORES} (use ${PARALLEL_BUILD_HINT} for parallel builds)")
message(STATUS "═══════════════════════════════════════════════════════")

# Calculate and display configuration time
string(TIMESTAMP CONFIG_END_TIME "%s")
math(EXPR CONFIG_DURATION "${CONFIG_END_TIME} - ${CONFIG_START_TIME}")
message(STATUS "Configuration completed in ${CONFIG_DURATION} seconds")
message(STATUS "")

# Display helpful next steps
message(STATUS "Next steps:")
# Detect preset name from binary directory path
string(REGEX MATCH "out/build/([^/]+)$" _ "${CMAKE_BINARY_DIR}")
if(CMAKE_MATCH_1)
    message(STATUS "  Build:   cmake --build --preset ${CMAKE_MATCH_1}")
else()
    message(STATUS "  Build:   cmake --build ${CMAKE_BINARY_DIR}")
endif()
if(WIN32)
    message(STATUS "  Run:     ${CMAKE_BINARY_DIR}/VelocityNext/VelocityNext.exe")
elseif(APPLE)
    message(STATUS "  Run:     ${CMAKE_BINARY_DIR}/VelocityNext/VelocityNext.app/Contents/MacOS/VelocityNext")
else()
    message(STATUS "  Run:     ${CMAKE_BINARY_DIR}/VelocityNext/VelocityNext")
endif()
message(STATUS "")

# ------------------------------
# Subdirectories
# ------------------------------
add_subdirectory(XboxInternals)
add_subdirectory(Velocity VelocityNext)