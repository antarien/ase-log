#pragma once

/**
 * ASE ECS SYSTEM HEADER
 *
 * @file        log_sys.hpp
 * @brief       LogSystem - brings up and tears down the logging sinks of a running app
 * @description Declares the system LogModule registers in Initialization: on_start builds the file
 *              sink, the HTTP ring and the counting sink from LogConfig and parks them at the
 *              LogResourceManager, on_stop flushes and releases every logger and sink in a
 *              controlled descent. The loggers themselves live in the manager, reached through
 *              internal::logger_slot() and internal::client_logger_slot() in log.hpp.
 *
 *              A header of its own, not part of log.hpp: every unit that logs includes log.hpp,
 *              and the system that opens the sinks is named only by LogModule and its own
 *              implementation. Keeping it here keeps the ECS base class and every change to this
 *              declaration out of the units that only write a line.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    process/computation
 * @schedule    Initialization
 * @usecase     usecase:100361 - open the log sinks
 * @created     2026-10-11
 * @modified    2026-10-11
 * @version     1.0.0
 *
 * ECS SYSTEM HEADER COMPLIANCE
 *
 * [ ] STATELESS - No member variables
 * [ ] Views created on demand, not stored
 * [ ] NO direct calls to other systems
 * [ ] Communication only via Components
 * [ ] Helpers in anonymous namespace (in .cpp, NOT static functions!)
 * [ ] Math functions from ase-math (Layer 0)
 * [ ] NO file-level static/constexpr (constants → types.hpp)
 * [ ] Registered in Module with correct Schedule
 * [ ] Filename matches convention
 * [ ] Class name derived from filename
 * [ ] ALL THREE METHODS DECLARED: on_start, tick, on_stop
 */

#include <ase/ecs/system.hpp>

namespace ase::log {

/**
 * @brief LogSystem - the sinks of a running app, opened once and closed once
 *
 * @schedule Initialization - after KernelCmdSystem, which fills LogConfig (log_module.hpp)
 * @reads    LogConfig from registry.ctx()
 * @writes   the loggers and sinks held by internal::log_resources(), nothing in the registry
 * @depends  KernelCmdSystem (run_after in LogModule::build)
 */
class LogSystem : public ecs::System {
public:
    const char* name() const override { return "LogSystem"; }
    int priority() const override { return 0; }

    void on_start(ecs::Registry& registry) override;
    void tick(ecs::Registry& registry, float dt) override;
    void on_stop(ecs::Registry& registry) override;
};

}  // namespace ase::log
