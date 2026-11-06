#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        log_config.hpp
 * @brief       LogConfig - destination and label of the log sinks of a running app
 * @description The data a server parks in ctx() before the sinks open: the log file path, the
 *              [LABEL] of its lines and the size of the ring buffer. KernelCmdSystem fills it,
 *              LogSystem::on_start reads it and the shutdown view prints its label.
 *
 *              A HEADER OF ITS OWN, BECAUSE THE DATA NEEDS NO ECS. Beside LogModule it would hang
 *              on ase/ecs/app.hpp; ase-ecs reads this label in shutdown_sequence.cpp and must not
 *              depend on the part of ase-log that is built on ase-ecs. The cut follows the
 *              consumers, as in log_quota.hpp. Only the standard headers stand below.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    structure/datatype
 * @usecase     usecase:100361 - open the log sinks
 * @actor       Entity Registry Steward (ENTIREGSTE)
 * @created     2026-10-31
 * @modified    2026-10-31
 * @version     1.0.0
 *
 * CORE INFRASTRUCTURE COMPLIANCE
 *
 * [ ] NOT an ECS Component or System
 * [ ] Layer dependencies correct (L0: no ASE deps, L1: L0 only)
 * [ ] No global mutable state (constexpr/const only)
 * [ ] No singletons or static mutable variables
 * [ ] Thread-safe by design (pure functions or explicit mutex)
 * [ ] All public functions documented with @brief, @param, @return
 * [ ] constexpr where possible (compile-time evaluation)
 * [ ] noexcept where possible (no-throw guarantee)
 * [ ] [[nodiscard]] on functions returning values
 * [ ] No magic numbers (use named constants)
 * [ ] No implicit conversions (use explicit constructors)
 * [ ] Header-only OR header+cpp pattern (not mixed)
 * [ ] Include guards via #pragma once
 * [ ] Namespace matches module: ase::{module}
 * [ ] No circular dependencies
 * [ ] No macros (except include guards) - use constexpr/templates
 * [ ] API stable (changes require version bump)
 */

#include <cstdint>
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

}  // namespace ase::log
