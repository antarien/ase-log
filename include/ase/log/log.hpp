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

}  // namespace ase::log

#if defined(__GNUC__) && __GNUC__ >= 14
#pragma GCC diagnostic pop
#endif
