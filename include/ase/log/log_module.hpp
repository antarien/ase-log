#pragma once

/**
 * ASE MODULE DEFINITION
 *
 * @file        log_module.hpp
 * @brief       LogModule - registers the logging systems with the scheduler
 * @description Brings up logging for a running app: the sinks, the ring buffer
 *              and the console writer. LogConfig carries the destination and is
 *              placed in ctx() BEFORE the module is added, because the sinks are
 *              opened while the module builds.
 *
 *              Usage:
 *                app.world().registry().ctx().emplace<LogConfig>("logs/engine.log");
 *                app.add_module<LogModule>();
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @created     2026-01-09
 * @modified    2026-08-20
 * @version     1.1.0
 *
 * ECS MODULE/PLUGIN DEFINITION COMPLIANCE
 *
 * [ ] name() returns correct name (ase-{module} or ase-pl-{plugin})
 * [ ] build() registers all systems in correct schedules
 * [ ] Startup systems registered first (run once at start)
 * [ ] Initialization systems registered (entity creation)
 * [ ] Integration/Dynamics systems registered with run_after() ordering
 * [ ] Transmission systems registered (network sync)
 * [ ] Preservation systems registered (database writes)
 * [ ] Finalization systems registered (cleanup)
 * [ ] All system includes present
 * [ ] No circular dependencies
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
    char log_file[256] = "";   // Empty by default — KernelCmdSystem MUST populate.
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
        // LogSystem MUST run after KernelCmdSystem so LogConfig is populated
        // with the per-server path (logs/{server}-{port}.log) before on_start
        // reads it. Without this ordering all servers end up writing to the
        // same file.
        app.add_system_with<LogSystem>(ecs::Schedule::Initialization)
            .run_after("KernelCmdSystem");
    }
};

}  // namespace ase::log
