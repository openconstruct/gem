cmake_minimum_required(VERSION 3.14) # FetchContent improved significantly after 3.11, 3.14 is safer
project(GeminiFileEditor CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# --- FetchContent Setup ---
include(FetchContent)

# --- nlohmann/json ---
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.3 # Use a specific stable tag
)
# nlohmann_json is header-only, MakeAvailable adds include directories
FetchContent_MakeAvailable(nlohmann_json)

# --- cpr (C++ Requests) ---
# Note: cpr requires libcurl to be installed on the system
FetchContent_Declare(
  cpr
  GIT_REPOSITORY https://github.com/libcpr/cpr.git
  GIT_TAG 1.10.5 # Use a specific stable tag
  # Optional: If cpr's CMakeLists has options you want to set:
  # CMAKE_ARGS -DCPR_USE_SYSTEM_CURL=ON # Ensure it uses system curl
  #            -DBUILD_SHARED_LIBS=OFF   # Build as static library (often easier)
  #            -DCPR_ENABLE_TESTING=OFF # Don't build cpr tests
)
# Setting FetchContent properties *before* MakeAvailable
# Force static library build if desired (avoids potential DLL issues on Windows)
set(CPR_FORCE_USE_SYSTEM_CURL ON CACHE BOOL "Force CPR to use system Curl" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build CPR as a static library" FORCE)
set(CPR_ENABLE_TESTING OFF CACHE BOOL "Disable building CPR tests" FORCE)

FetchContent_MakeAvailable(cpr)

# --- Main Application ---
add_executable(gem main.cpp)

# --- Linking ---
# Link against the targets provided by FetchContent
# nlohmann_json::nlohmann_json (interface target for header-only)
# cpr::cpr (target created by cpr's CMakeLists.txt)
target_link_libraries(gem PRIVATE nlohmann_json::nlohmann_json cpr::cpr)

# CMake should find system Curl via find_package(CURL) called *within* cpr's CMakeLists.
# If CMake struggles to find Curl on your system (especially Windows/macOS),
# you might need to help it by setting CMAKE_PREFIX_PATH
# Example: cmake -B build -DCMAKE_PREFIX_PATH=/path/to/curl/installation
# Or using environment variables as shown for Homebrew earlier.

message(STATUS "Using nlohmann_json source dir: ${nlohmann_json_SOURCE_DIR}")
message(STATUS "Using cpr source dir: ${cpr_SOURCE_DIR}")
