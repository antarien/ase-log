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
#include <ase/log/version.hpp>
#include <ase/log/log.hpp>
#include <cstring>

namespace ase::log {

/** Log configuration — set before adding LogModule to configure log file path.
 *  Usage:
 *    app.world().registry().ctx().emplace<LogConfig>("logs/engine.log");
 *    app.add_module<LogModule>();
 */
struct LogConfig {
    char log_file[256] = "logs/engine.log";
    char label[16] = "SERVER";
    uint32_t ringbuffer_size = 500;
    LogConfig() = default;
    explicit LogConfig(const char* path) { std::strncpy(log_file, path, 255); log_file[255] = '\0'; }
    LogConfig(const char* path, const char* lbl) {
        std::strncpy(log_file, path, 255); log_file[255] = '\0';
        std::strncpy(label, lbl, 15); label[15] = '\0';
    }
};

struct LogModule {
    static constexpr const char* name() { return "ase-log"; }
    static constexpr const char* version() { return MODULE_VERSION; }

    void build(ecs::App& app) {
        app.add_system<LogSystem>(ecs::Schedule::Initialization);
    }
};

}  // namespace ase::log
