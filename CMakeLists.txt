cmake_minimum_required(VERSION 3.20)

project(ase-log
    VERSION 0.1.0
    DESCRIPTION "ASE LogSystem - ECS-based Logging"
    LANGUAGES CXX
)

# Static library (LogSystem is an ECS System)
add_library(ase-log STATIC
    src/log_system.cpp
)
add_library(ase::log ALIAS ase-log)

target_include_directories(ase-log
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(ase-log PUBLIC cxx_std_20)

# Dependencies: ase-ecs (for System base) + spdlog
target_link_libraries(ase-log
    PUBLIC
        ase::ecs
        spdlog::spdlog
)
