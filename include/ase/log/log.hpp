#pragma once

// GCC14 has a false positive -Wdangling-reference warning with spdlog/fmt
// See: https://github.com/fmtlib/fmt/issues/3415
#if defined(__GNUC__) && __GNUC__ >= 14
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-reference"
#endif

/**
 * ASE LogSystem - ECS-based Logging System
 *
 * The logger IS an ECS System. It initializes on_start() and shuts down on_stop().
 * Runs in Foundation phase (first to start, last to stop).
 *
 * Usage:
 *   // LogSystem is auto-registered, just use SystemRegistry::create_all_systems()
 *   ase::ecs::World world;
 *   ase::ecs::SystemRegistry::create_all_systems(world);
 *   world.start();  // LogSystem initializes here
 *
 *   // Then log anywhere:
 *   ase::log::info("Server started on port {}", 8080);
 *
 * Tail logs: tail -f logs/antares.log
 */

#include <ase/ecs/ecs.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <string>
#include <string_view>

namespace ase::log {

// ============================================================================
// LogSystem - The Logger as an ECS System
// ============================================================================

class LogSystem : public ecs::System {
public:
    LogSystem() = default;
    explicit LogSystem(const std::string& name, const std::string& log_file = "logs/antares.log");

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

private:
    std::string logger_name_ = "ASE";
    std::string log_file_ = "logs/antares.log";

    static std::shared_ptr<spdlog::logger> g_logger_;         // Server logger with [SERVER] prefix
    static std::shared_ptr<spdlog::logger> g_client_logger_;  // Client logger without [SERVER] prefix
    static std::string g_log_path_;
};

// ============================================================================
// Global Logging Functions (static, use LogSystem's logger)
// ============================================================================

// Simple string logging
// Use std::string to avoid GCC14 -Wdangling-reference false positive with spdlog/fmt
inline void info(const std::string& msg) {
    if (LogSystem::logger()) LogSystem::logger()->info("{}", msg);
}

inline void warn(const std::string& msg) {
    if (LogSystem::logger()) LogSystem::logger()->warn("{}", msg);
}

inline void error(const std::string& msg) {
    if (LogSystem::logger()) LogSystem::logger()->error("{}", msg);
}

inline void debug(const std::string& msg) {
    if (LogSystem::logger()) LogSystem::logger()->debug("{}", msg);
}

inline void trace(const std::string& msg) {
    if (LogSystem::logger()) LogSystem::logger()->trace("{}", msg);
}

inline void critical(const std::string& msg) {
    if (LogSystem::logger()) LogSystem::logger()->critical("{}", msg);
}

// Formatted logging (fmt style)
template<typename... Args>
inline void info(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    if (LogSystem::logger()) LogSystem::logger()->info(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void warn(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    if (LogSystem::logger()) LogSystem::logger()->warn(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void error(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    if (LogSystem::logger()) LogSystem::logger()->error(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void debug(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    if (LogSystem::logger()) LogSystem::logger()->debug(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void trace(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    if (LogSystem::logger()) LogSystem::logger()->trace(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void critical(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    if (LogSystem::logger()) LogSystem::logger()->critical(fmt, std::forward<Args>(args)...);
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
