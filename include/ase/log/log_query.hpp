#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        log_query.hpp
 * @brief       The read side of the log ring: recent_logs and the entry it returns
 * @description Declares the query that answers /api/logs: the lines of the in-memory ring after a
 *              sequence number, parsed back into LogEntry records. It is a header of its own for
 *              the same reason as log_quota.hpp: log.hpp stands in nearly every unit of every tier,
 *              and the query is the only part of the log that needs a container. A unit that
 *              writes a line does not see it.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @design      DSGN_021
 * @category    error/logging
 * @usecase     usecase:100363 - read recent log lines
 * @created     2026-10-11
 * @modified    2026-10-11
 * @version     1.0.0
 *
 * LAYER RULES:
 *   Layer 0 (Foundation): NO dependencies on other ASE modules (only std::)
 *   Layer 1 (Core):       May depend on Layer 0 only
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

#include <ase/containers/vector.hpp>

#include <cstdint>

namespace ase::log {

// THE RING ENTRY AND ITS PAGING

// Sentinel for recent_logs(since_seq): "I have nothing yet, give me the newest page." Any other
// value is a real sequence number and returns everything after it. A caller cannot mean sequence
// zero literally — the counter starts at one — so the sentinel costs no reachable value.
inline constexpr uint32_t kLogSeqAll = 0u;

// Page size returned for a first poll (kLogSeqAll). The ringbuffer holds more than this; a fresh
// HTTP client asking for "recent" wants a screenful, not the whole buffer, and every later poll
// carries its sequence number and gets exactly the delta.
inline constexpr uint32_t kRecentLogsPageSize = 50u;

// Structured log entry for /api/logs endpoint (ringbuffer output)
struct LogEntry {
    uint32_t seq = 0;           // monotonic sequence number
    uint8_t level = 0;          // spdlog level (0=trace..5=critical)
    char timestamp[16] = {};    // "HH:MM:SS.mmm"
    char system[32] = {};       // extracted from "[SystemName]" in message
    char message[512] = {};     // log message (truncated if too long)
};

/**
 * @brief The lines of the in-memory ring after a sequence number, newest page first
 * @param since_seq kLogSeqAll for the newest page, otherwise the last sequence number seen
 * @return the parsed ring lines after since_seq, each with its own sequence number
 *
 * Reads the HTTP ring the LogResourceManager holds; defined in src/log_query.cpp.
 */
[[nodiscard]] ase::containers::Vector<LogEntry> recent_logs(uint32_t since_seq = kLogSeqAll);

}  // namespace ase::log
