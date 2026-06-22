#pragma once

// GCC14 has a false positive -Wdangling-reference warning with spdlog/fmt
// See: https://github.com/fmtlib/fmt/issues/3415
#if defined(__GNUC__) && __GNUC__ >= 14
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-reference"
#endif

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        log.hpp
 * @brief       ECS-based logging system with CLI support
 * @description Provides logging infrastructure for both ECS-based applications
 *              and CLI tools. LogSystem is an ECS System that initializes on_start().
 *              CLI tools can use init() for standalone initialization.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    error/logging
 * @created     2024-01-01
 * @modified    2025-01-21
 * @version     2.0.0
 *
 * LAYER RULES:
 *   Layer 0 (Foundation): NO dependencies on other ASE modules (only std::)
 *   Layer 1 (Core):       May depend on Layer 0 only
 *
 * USAGE:
 *   // ECS-based apps (LogSystem auto-initializes):
 *   ase::ecs::World world;
 *   world.start();  // LogSystem initializes here
 *   ase::log::info("Server started on port {}", 8090);
 *
 *   // CLI tools (manual initialization):
 *   ase::log::init("ase-codegen");
 *   ase::log::info("Processing module: {}", module_name);
 *   ase::log::shutdown();
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

#include <ase/ecs/system.hpp>
#include <ase/containers/vector.hpp>
#include <ase/log/log_filter.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>

namespace ase::log {

// ============================================================================
// LogSystem - The Logger as an ECS System
// ============================================================================

// Structured log entry for /api/logs endpoint (ringbuffer output)
struct LogEntry {
    uint32_t seq = 0;           // monotonic sequence number
    uint8_t level = 0;          // spdlog level (0=trace..5=critical)
    char timestamp[16] = {};    // "HH:MM:SS.mmm"
    char system[32] = {};       // extracted from "[SystemName]" in message
    char message[512] = {};     // log message (truncated if too long)
};

class LogSystem : public ecs::System {
public:
    LogSystem() = default;
    explicit LogSystem(const std::string& name, const std::string& log_file = "");

    const char* name() const override { return "LogSystem"; }
    int priority() const override { return 0; }  // First system to run

    void on_start(ecs::Registry& registry) override;
    void on_stop(ecs::Registry& registry) override;
    void tick(ecs::Registry& registry, float dt) override;

    // Configuration (call before on_start)
    void set_name(const std::string& name) { logger_name_ = name; }
    void set_log_file(const std::string& path) { log_file_ = path; }

    // Access underlying loggers
    static std::shared_ptr<spdlog::logger>& logger() { return g_logger_; }
    static std::shared_ptr<spdlog::logger>& client_logger() { return g_client_logger_; }
    static const std::string& log_path() { return g_log_path_; }

    // Recent logs from in-memory ringbuffer (for HTTP /api/logs endpoint)
    static ase::containers::Vector<LogEntry> recent_logs(uint32_t since_seq = 0);
    static uint32_t log_counter() { return g_log_counter_.load(); }

private:
    std::string logger_name_ = "ASE";
    std::string log_file_;

    static std::shared_ptr<spdlog::logger> g_logger_;         // Server logger with [SERVER] prefix
    static std::shared_ptr<spdlog::logger> g_client_logger_;  // Client logger without [SERVER] prefix
    static std::string g_log_path_;
    static std::shared_ptr<spdlog::sinks::sink> g_ringbuffer_sink_;
    static std::atomic<uint32_t> g_log_counter_;
};

// ============================================================================
// CLI Initialization (for tools without ECS World)
// ============================================================================

/**
 * @brief Initialize logger for CLI tools (no ECS required)
 * @param name Logger name (e.g., "ase-codegen")
 *
 * Use this for CLI tools that don't have an ECS World.
 * For ECS-based apps, use LogSystem::on_start() instead.
 */
inline void init(const std::string& name) {
    if (LogSystem::logger()) return;  // Already initialized

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("%v");  // Simple output for CLI

    LogSystem::logger() = std::make_shared<spdlog::logger>(name, console_sink);
    LogSystem::logger()->set_level(spdlog::level::info);
}

/**
 * @brief Initialize a server-style logger for a standalone (non-ECS) binary.
 * @param name     Logger name (e.g. "ase-edge")
 * @param label    Tier tag for the [ASE] [<label>] prefix (e.g. "EDGE")
 * @param log_file Optional file path (relative resolves to project root; empty = console only)
 *
 * Emits the SAME uniform console format as the ECS-based servers —
 * [ts] [LVL] [ASE] [<label>] message — but without ECS/registry/kernel/capture-ring, so
 * customer-side tools (the edge daemon) log identically to engine/replica/world. Honours the
 * 3-axis filter set by parse_cli_filter_from_argv. Idempotent (no-op if a logger already exists).
 * Defined in log_sys.cpp (it reuses the file-scope Colored/PlainLevelFlag formatters).
 */
void init_server_standalone(const std::string& name, const std::string& label, const std::string& log_file = "");

/**
 * @brief Install the capture-phase logger.
 *
 * Must be the FIRST call at the top of Kernel::build — it puts g_logger_
 * into a well-defined state (a single ringbuffer sink, no console) so every
 * log::* call made afterwards by kernel init, KernelEnvLdrSystem,
 * KernelCliSystem, dlopen discovery and any pre-LogSystem::on_start code
 * is captured instead of silently dropped by the null-logger gate.
 *
 * LogSystem::on_start later attaches the real sinks (console, per-server
 * file, HTTP-endpoint ringbuffer, CountingSink) to the SAME logger, drains
 * the captured entries into those new sinks (so they appear in the final
 * log file with the correct [LABEL] and [DBG|INF|WRN|ERR|CRT|TRC] format),
 * and removes the capture ring. One logger across the whole process.
 */
void install_capture_logger();

/**
 * @brief Parse the --log argument from argv and feed it into the 3-axis
 *        filter engine (level/category/phase).
 *
 * Must run BEFORE install_capture_logger so that even capture-phase calls
 * respect the user's filter. Pure global state mutation — no Registry,
 * no logger dependency.
 */
void parse_cli_filter_from_argv(int argc, char* argv[]);

/**
 * @brief Finalize the logger after the Schedule-Bootstrap block.
 *
 * During the boot block the logger runs with only {capture-ring, file,
 * HTTP-ring, counting} — the console sink is deliberately withheld so
 * log lines from every system's on_start cannot interleave into the
 * boot progress table on stdout. This call:
 *   1. attaches the previously parked console sink to g_logger_
 *   2. replays the capture-ring into every attached sink (console + file
 *      + HTTP-ring + counting) so early log lines appear on BOTH stdout
 *      and in logs/{server}-{port}.log with the correct [LABEL] format
 *   3. detaches and drops the capture ring
 *
 * Idempotent: second call is a no-op.
 */
void finalize_logger_after_boot();

/**
 * @brief Shutdown logger (for CLI tools)
 */
inline void shutdown() {
    if (LogSystem::logger()) {
        LogSystem::logger()->flush();
        LogSystem::logger().reset();
    }
}

// ============================================================================
// Global Logging Functions (static, use LogSystem's logger)
// ============================================================================

// Simple string logging with compile-time category filtering
// source_location default param captures CALL SITE, not this file
inline void info(const std::string& msg,
                 const std::source_location& loc = std::source_location::current()) {
    if (!should_log_loc(level::info, loc)) return;
    if (LogSystem::logger()) LogSystem::logger()->info("{}", msg);
}

inline void warn(const std::string& msg,
                 const std::source_location& loc = std::source_location::current()) {
    if (!should_log_loc(level::warn, loc)) return;
    if (LogSystem::logger()) LogSystem::logger()->warn("{}", msg);
}

inline void error(const std::string& msg,
                  const std::source_location& loc = std::source_location::current()) {
    if (!should_log_loc(level::error, loc)) return;
    if (LogSystem::logger()) LogSystem::logger()->error("{}", msg);
}

inline void debug(const std::string& msg,
                  const std::source_location& loc = std::source_location::current()) {
    if (!should_log_loc(level::debug, loc)) return;
    if (LogSystem::logger()) LogSystem::logger()->debug("{}", msg);
}

inline void trace(const std::string& msg,
                  const std::source_location& loc = std::source_location::current()) {
    if (!should_log_loc(level::trace, loc)) return;
    if (LogSystem::logger()) LogSystem::logger()->trace("{}", msg);
}

inline void critical(const std::string& msg,
                     const std::source_location& loc = std::source_location::current()) {
    if (!should_log_loc(level::critical, loc)) return;
    if (LogSystem::logger()) LogSystem::logger()->critical("{}", msg);
}

// ============================================================================
// Format string wrapper that captures caller's filename via __builtin_FILE()
// ============================================================================

/// Wraps spdlog::format_string_t and captures the caller's source filename + function.
/// The consteval constructor with __builtin_FILE()/__builtin_FUNCTION() defaults capture
/// the CALL SITE, enabling category-based filtering (file + lifecycle phase) for fmt-style calls.
template<typename... Args>
struct log_fmt {
    spdlog::format_string_t<Args...> fmt;
    const char* file;
    const char* func;

    template<typename S>
        requires (!std::is_arithmetic_v<std::remove_cvref_t<S>>)
    consteval log_fmt(const S& s, const char* f = __builtin_FILE(),
                      const char* fn = __builtin_FUNCTION())
        : fmt(s), file(f), func(fn) {}
};

// Formatted logging (fmt style) with full category filtering.
// log_fmt captures the caller's filename in its consteval constructor,
// so category blacklist AND whitelist work for all fmt-style log calls.
// Fast path: should_log_loc() = level + O(1) cached file+func filter (~12 cycles).
template<typename... Args>
inline void info(log_fmt<std::type_identity_t<Args>...> lf, Args&&... args) {
    if (!filter::should_log_loc(filter::LVL_INF, lf.file, lf.func)) return;
    if (LogSystem::logger()) LogSystem::logger()->info(lf.fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void warn(log_fmt<std::type_identity_t<Args>...> lf, Args&&... args) {
    if (!filter::should_log_loc(filter::LVL_WRN, lf.file, lf.func)) return;
    if (LogSystem::logger()) LogSystem::logger()->warn(lf.fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void error(log_fmt<std::type_identity_t<Args>...> lf, Args&&... args) {
    if (!filter::should_log_loc(filter::LVL_ERR, lf.file, lf.func)) return;
    if (LogSystem::logger()) LogSystem::logger()->error(lf.fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void debug(log_fmt<std::type_identity_t<Args>...> lf, Args&&... args) {
    if (!filter::should_log_loc(filter::LVL_DBG, lf.file, lf.func)) return;
    if (LogSystem::logger()) LogSystem::logger()->debug(lf.fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void trace(log_fmt<std::type_identity_t<Args>...> lf, Args&&... args) {
    if (!filter::should_log_loc(filter::LVL_TRC, lf.file, lf.func)) return;
    if (LogSystem::logger()) LogSystem::logger()->trace(lf.fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void critical(log_fmt<std::type_identity_t<Args>...> lf, Args&&... args) {
    if (!filter::should_log_loc(filter::LVL_CRT, lf.file, lf.func)) return;
    if (LogSystem::logger()) LogSystem::logger()->critical(lf.fmt, std::forward<Args>(args)...);
}

inline void set_level(spdlog::level::level_enum level) {
    if (LogSystem::logger()) LogSystem::logger()->set_level(level);
}

inline void flush() {
    if (LogSystem::logger()) LogSystem::logger()->flush();
}

// ============================================================================
// Client Logging Functions (uses client_logger without [SERVER] prefix)
// For forwarding browser console logs via RTC
// ============================================================================

// Unfiltered client logging (backward compatible, no client filter applied)
inline void client_info(const std::string& msg) {
    if (LogSystem::client_logger()) LogSystem::client_logger()->info("{}", msg);
}

inline void client_warn(const std::string& msg) {
    if (LogSystem::client_logger()) LogSystem::client_logger()->warn("{}", msg);
}

inline void client_error(const std::string& msg) {
    if (LogSystem::client_logger()) LogSystem::client_logger()->error("{}", msg);
}

inline void client_debug(const std::string& msg) {
    if (LogSystem::client_logger()) LogSystem::client_logger()->debug("{}", msg);
}

// Filtered client logging with client_bit (use 1ULL << client_id)
// Respects level filter, category filter, AND client filter (+CLT:01, -CLT)
inline void client_info(uint64_t client_bit, const std::string& msg,
                        const std::source_location& loc = std::source_location::current()) {
    if (!should_log_client(level::info, client_bit, loc)) return;
    if (LogSystem::client_logger()) LogSystem::client_logger()->info("{}", msg);
}

inline void client_warn(uint64_t client_bit, const std::string& msg,
                        const std::source_location& loc = std::source_location::current()) {
    if (!should_log_client(level::warn, client_bit, loc)) return;
    if (LogSystem::client_logger()) LogSystem::client_logger()->warn("{}", msg);
}

inline void client_error(uint64_t client_bit, const std::string& msg,
                         const std::source_location& loc = std::source_location::current()) {
    if (!should_log_client(level::error, client_bit, loc)) return;
    if (LogSystem::client_logger()) LogSystem::client_logger()->error("{}", msg);
}

inline void client_debug(uint64_t client_bit, const std::string& msg,
                         const std::source_location& loc = std::source_location::current()) {
    if (!should_log_client(level::debug, client_bit, loc)) return;
    if (LogSystem::client_logger()) LogSystem::client_logger()->debug("{}", msg);
}

// Filtered client logging with fmt-style formatting
// Full filtering: level + O(1) cached category + client filter
template<typename... Args>
inline void client_info(uint64_t client_bit, log_fmt<std::type_identity_t<Args>...> lf, Args&&... args) {
    if (!filter::should_log_client_loc(filter::LVL_INF, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger()) LogSystem::client_logger()->info(lf.fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void client_warn(uint64_t client_bit, log_fmt<std::type_identity_t<Args>...> lf, Args&&... args) {
    if (!filter::should_log_client_loc(filter::LVL_WRN, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger()) LogSystem::client_logger()->warn(lf.fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void client_error(uint64_t client_bit, log_fmt<std::type_identity_t<Args>...> lf, Args&&... args) {
    if (!filter::should_log_client_loc(filter::LVL_ERR, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger()) LogSystem::client_logger()->error(lf.fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void client_debug(uint64_t client_bit, log_fmt<std::type_identity_t<Args>...> lf, Args&&... args) {
    if (!filter::should_log_client_loc(filter::LVL_DBG, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger()) LogSystem::client_logger()->debug(lf.fmt, std::forward<Args>(args)...);
}

// ============================================================================
// RTC Server Logging Functions (uses client_logger with [SERVER] [RTC] prefix)
// For RTC-related server logs: [ASE] [SERVER] [RTC] message
// ============================================================================

inline void rtc_info(const std::string& msg) {
    if (LogSystem::client_logger()) LogSystem::client_logger()->info("[SERVER] [RTC] {}", msg);
}

inline void rtc_warn(const std::string& msg) {
    if (LogSystem::client_logger()) LogSystem::client_logger()->warn("[SERVER] [RTC] {}", msg);
}

inline void rtc_error(const std::string& msg) {
    if (LogSystem::client_logger()) LogSystem::client_logger()->error("[SERVER] [RTC] {}", msg);
}

inline void rtc_debug(const std::string& msg) {
    if (LogSystem::client_logger()) LogSystem::client_logger()->debug("[SERVER] [RTC] {}", msg);
}

template<typename... Args>
inline void rtc_info(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(fmt, std::forward<Args>(args)...);
        LogSystem::client_logger()->info("[SERVER] [RTC] {}", msg);
    }
}

template<typename... Args>
inline void rtc_warn(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(fmt, std::forward<Args>(args)...);
        LogSystem::client_logger()->warn("[SERVER] [RTC] {}", msg);
    }
}

template<typename... Args>
inline void rtc_error(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(fmt, std::forward<Args>(args)...);
        LogSystem::client_logger()->error("[SERVER] [RTC] {}", msg);
    }
}

template<typename... Args>
inline void rtc_debug(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(fmt, std::forward<Args>(args)...);
        LogSystem::client_logger()->debug("[SERVER] [RTC] {}", msg);
    }
}

// ============================================================================
// Error Categories (ERR::CAT) - DRY error messages with auto-generated help
// ============================================================================
// Usage: log::error(log::ERR::CAT::HUB_NOT_FOUND, "SystemName", owner, "VALUE_ID");
// Output: [ERR] [SystemName] HUB_NOT_FOUND: owner=123, value_id='VALUE_ID'
//         Check: 1) IniSystem created hub entity in on_start()
//                2) Spawn created hub values for entity
//                3) Correct owner ID, 4) No typo in value_id

namespace ERR {
namespace CAT {
    constexpr uint8_t HUB_NOT_FOUND = 1;       // Hub entity doesn't exist for owner+value_id
    constexpr uint8_t HUB_GLOBAL_MISSING = 2;  // GLOBAL hub value missing
    constexpr uint8_t COMPONENT_MISSING = 3;   // Required component not on entity
    constexpr uint8_t INVALID_ENTITY = 4;      // Entity ID is invalid or destroyed
    constexpr uint8_t SCHEDULE_ORDER = 5;      // System runs before its dependency
}  // namespace CAT
}  // namespace ERR

// Category-specific error messages (SSOT - defined once here)
namespace detail {
    inline const char* get_cat_name(uint8_t cat) {
        switch (cat) {
            case ERR::CAT::HUB_NOT_FOUND:      return "HUB_NOT_FOUND";
            case ERR::CAT::HUB_GLOBAL_MISSING: return "HUB_GLOBAL_MISSING";
            case ERR::CAT::COMPONENT_MISSING:  return "COMPONENT_MISSING";
            case ERR::CAT::INVALID_ENTITY:     return "INVALID_ENTITY";
            case ERR::CAT::SCHEDULE_ORDER:     return "SCHEDULE_ORDER";
            default: return "UNKNOWN";
        }
    }

    inline const char* get_cat_help(uint8_t cat) {
        switch (cat) {
            case ERR::CAT::HUB_NOT_FOUND:
                return "Check: 1) IniSystem created hub entity in on_start(), "
                       "2) Spawn created hub values for entity, "
                       "3) Correct owner ID, 4) No typo in value_id";
            case ERR::CAT::HUB_GLOBAL_MISSING:
                return "Check: 1) Source module IniSystem created GLOBAL hub value, "
                       "2) Source module is loaded, 3) No typo in value_id";
            case ERR::CAT::COMPONENT_MISSING:
                return "Check: 1) Entity was spawned with required components, "
                       "2) Component not removed by another system";
            case ERR::CAT::INVALID_ENTITY:
                return "Check: 1) Entity not destroyed, "
                       "2) Correct entity ID stored, 3) No dangling reference";
            case ERR::CAT::SCHEDULE_ORDER:
                return "Check: 1) Producer system has lower priority number, "
                       "2) .run_after() constraint in module definition";
            default:
                return "";
        }
    }
}  // namespace detail

// Categorized error logging for per-entity values
inline void error(uint8_t cat, const char* system, uint32_t owner, const char* value_id) {
    if (LogSystem::logger()) {
        LogSystem::logger()->error("[{}] {}: owner={}, value_id='{}'. {}",
            system, detail::get_cat_name(cat), owner, value_id, detail::get_cat_help(cat));
    }
}

// Categorized error logging for GLOBAL values (no owner)
inline void error(uint8_t cat, const char* system, const char* value_id) {
    if (LogSystem::logger()) {
        LogSystem::logger()->error("[{}] {}: value_id='{}' (GLOBAL). {}",
            system, detail::get_cat_name(cat), value_id, detail::get_cat_help(cat));
    }
}

// Categorized error logging for component/entity issues (no value_id)
inline void error(uint8_t cat, const char* system, uint32_t entity) {
    if (LogSystem::logger()) {
        LogSystem::logger()->error("[{}] {}: entity={}. {}",
            system, detail::get_cat_name(cat), entity, detail::get_cat_help(cat));
    }
}

// ============================================================================
// Warning Categories (WRN::CAT) - DRY warning messages with auto-generated help
// ============================================================================
// Usage: log::warn(log::WRN::CAT::VALUE_OUT_OF_RANGE, "SystemName", owner, "VALUE_ID", value, min, max);
// Output: [WRN] [SystemName] VALUE_OUT_OF_RANGE: owner=123, value_id='VALUE_ID', value=1.5, range=[0.0,1.0]
//         Fix: Value will be clamped to valid range

namespace WRN {
namespace CAT {
    constexpr uint8_t VALUE_OUT_OF_RANGE = 1;   // Value exists but outside valid range
    constexpr uint8_t VALUE_NEGATIVE = 2;       // Value exists but negative (should be >= 0)
    constexpr uint8_t VALUE_INVALID = 3;        // Value exists but semantically invalid
}  // namespace CAT
}  // namespace WRN

namespace detail {
    inline const char* get_wrn_cat_name(uint8_t cat) {
        switch (cat) {
            case WRN::CAT::VALUE_OUT_OF_RANGE: return "VALUE_OUT_OF_RANGE";
            case WRN::CAT::VALUE_NEGATIVE:     return "VALUE_NEGATIVE";
            case WRN::CAT::VALUE_INVALID:      return "VALUE_INVALID";
            default: return "UNKNOWN";
        }
    }

    inline const char* get_wrn_cat_help(uint8_t cat) {
        switch (cat) {
            case WRN::CAT::VALUE_OUT_OF_RANGE:
                return "Fix: Value will be clamped to valid range";
            case WRN::CAT::VALUE_NEGATIVE:
                return "Fix: Value will be set to 0";
            case WRN::CAT::VALUE_INVALID:
                return "Fix: Value will be set to default";
            default:
                return "";
        }
    }
}  // namespace detail

// Categorized warning for out-of-range values
inline void warn(uint8_t cat, const char* system, uint32_t owner, const char* value_id, float value, float min, float max) {
    if (LogSystem::logger()) {
        LogSystem::logger()->warn("[{}] {}: owner={}, value_id='{}', value={}, range=[{},{}]. {}",
            system, detail::get_wrn_cat_name(cat), owner, value_id, value, min, max, detail::get_wrn_cat_help(cat));
    }
}

// Categorized warning for negative values
inline void warn(uint8_t cat, const char* system, uint32_t owner, const char* value_id, float value) {
    if (LogSystem::logger()) {
        LogSystem::logger()->warn("[{}] {}: owner={}, value_id='{}', value={}. {}",
            system, detail::get_wrn_cat_name(cat), owner, value_id, value, detail::get_wrn_cat_help(cat));
    }
}

}  // namespace ase::log

#if defined(__GNUC__) && __GNUC__ >= 14
#pragma GCC diagnostic pop
#endif
