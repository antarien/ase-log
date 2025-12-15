cmake_minimum_required(VERSION 3.20)

project(ase-log
    VERSION 0.1.0
    DESCRIPTION "ASE Logging Library - spdlog wrapper"
    LANGUAGES CXX
)

# Header-only library wrapping spdlog
add_library(ase-log INTERFACE)
add_library(ase::log ALIAS ase-log)

target_include_directories(ase-log
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(ase-log INTERFACE cxx_std_20)

# Link to spdlog
target_link_libraries(ase-log INTERFACE spdlog::spdlog)

# Install
install(TARGETS ase-log
    EXPORT ase-log-targets
)

install(DIRECTORY include/ase
    DESTINATION include
)
