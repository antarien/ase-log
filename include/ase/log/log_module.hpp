#pragma once

/**
 * LogModule - Bevy-style module for logging initialization
 *
 * Usage:
 *   ecs::App()
 *       .add_module<LogModule>()
 *       .run();
 */

#include <ase/ecs/app.hpp>
#include <ase/log/log.hpp>

namespace ase::log {

struct LogModule {
    void build(ecs::App& app) {
        app.add_system<LogSystem>(ecs::Schedule::Startup);
    }
};

}  // namespace ase::log
