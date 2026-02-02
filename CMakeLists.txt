cmake_minimum_required(VERSION 3.20)

#[[
ASE CORE BUILD CONFIGURATION

@file        CMakeLists.txt
@brief       Build configuration for ase-log
@description Logging - ECS-based logging system with spdlog backend.
             Provides log::info, log::warn, log::error macros.

@module      ase-log
@layer       1 (Core)
@created     2025-12-15
@modified    2026-01-30
@version     00.00.20.00020 [init]

CMAKE MODULE COMPLIANCE

[ ] cmake_minimum_required(VERSION 3.20) as FIRST line
[ ] project() with VERSION and LANGUAGES CXX
[ ] message(STATUS) for configuration output
[ ] add_library() with STATIC and EXPLICIT file list
[ ] add_library(ase::{module} ALIAS ...) for namespace
[ ] target_include_directories() with PUBLIC and generator expressions
[ ] target_compile_features() with cxx_std_20
[ ] target_link_libraries() with PUBLIC/PRIVATE separation
[ ] include(GNUInstallDirs) for installation
[ ] install(DIRECTORY include/ ...) for headers
]]

# Parse local VERSION (SSOT) if not building from root
if(NOT DEFINED ASE_LOG_VERSION)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
        message(FATAL_ERROR "VERSION file not found: ${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
    endif()
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _VERSION_LINES)
    foreach(_LINE ${_VERSION_LINES})
        if(_LINE MATCHES "^ASE_LOG=(.*)$")
            set(ASE_LOG_VERSION "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    if(NOT DEFINED ASE_LOG_VERSION)
        message(FATAL_ERROR "ASE_LOG version not found in VERSION file")
    endif()
endif()

project(ase-log VERSION ${ASE_LOG_VERSION} LANGUAGES CXX)

# Generate version.hpp from template (SSOT)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/ase/log/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/ase/log/version.hpp"
    @ONLY
)

# ==============================================================================
# CODE GENERATION: Log Categories from Taxonomy
# ==============================================================================
# Runs every build but only writes if content changed (fast: read + compare).
# No file(GLOB) - script internally scans taxonomy directory.

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_custom_target(ase-log-codegen ALL
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_log_cat.py"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Checking log categories (regenerates only if taxonomy changed)..."
)

# ==============================================================================
# BUILD CONFIGURATION
# ==============================================================================

message(STATUS "Configuring ${PROJECT_NAME} v${PROJECT_VERSION}")

add_library(${PROJECT_NAME} STATIC
    #[[
    CORE
    Logging system implementation.
    ]]
    src/log_system.cpp
    src/log_cat_gen.cpp
)
add_library(ase::log ALIAS ${PROJECT_NAME})

# Ensure codegen runs before compiling (checks taxonomy, regenerates if needed)
add_dependencies(${PROJECT_NAME} ase-log-codegen)

# Track module definition header - forces rebuild when module registration changes
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ase/log/log_module.hpp
    PROPERTIES HEADER_FILE_ONLY TRUE
)
target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ase/log/log_module.hpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ase::ecs         # Layer 1 - System base class
        spdlog::spdlog   # External - Logging backend
)
#[[ VERBOTEN: No Layer 3 dependencies! ]]

include(GNUInstallDirs)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(ASE_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()
