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
    static constexpr const char* name() { return "ase-log"; }

    void build(ecs::App& app) {
        app.add_system<LogSystem>(ecs::Schedule::Initialization);
    }
};

}  // namespace ase::log
