#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <string_view>
#include <filesystem>

namespace ase::log {

inline std::shared_ptr<spdlog::logger> g_logger = nullptr;
inline std::string g_log_path;

/**
 * Initialize the logging system
 *
 * @param name Logger name (shown in console output)
 * @param log_file Path to log file (default: logs/antares.log)
 *
 * Log format: [YYYY-MM-DD HH:MM:SS.mmm] [level] [name] message
 * File is truncated on start for fresh logs each run.
 *
 * Usage:
 *   ase::log::init("ASE-Server");
 *   ase::log::info("Server started on port {}", 8080);
 *
 * Tail logs in parallel:
 *   tail -f logs/antares.log
 */
inline void init(const std::string& name = "ASE", const std::string& log_file = "logs/antares.log") {
    if (g_logger) return;

    // Create logs directory if it doesn't exist
    std::filesystem::path log_path(log_file);
    if (log_path.has_parent_path()) {
        std::filesystem::create_directories(log_path.parent_path());
    }
    g_log_path = log_file;

    // Create sinks: console (colored) + file
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, true); // truncate
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");

    // Create logger with both sinks
    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    g_logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    g_logger->set_level(spdlog::level::debug);

    // Flush immediately on every log (for tail -f support)
    g_logger->flush_on(spdlog::level::trace);

    // Register globally
    spdlog::register_logger(g_logger);

    // Write startup marker
    g_logger->info("=== ANTARES SIMULATION ENGINE STARTED ===");
}

inline void shutdown() {
    if (g_logger) {
        g_logger->info("=== ANTARES SIMULATION ENGINE STOPPED ===");
        g_logger->flush();
        spdlog::drop_all();
        g_logger.reset();
    }
}

/**
 * Get the log file path
 */
inline const std::string& log_path() {
    return g_log_path;
}

// Simple string logging
inline void info(std::string_view msg) {
    if (g_logger) g_logger->info("{}", msg);
}

inline void warn(std::string_view msg) {
    if (g_logger) g_logger->warn("{}", msg);
}

inline void error(std::string_view msg) {
    if (g_logger) g_logger->error("{}", msg);
}

inline void debug(std::string_view msg) {
    if (g_logger) g_logger->debug("{}", msg);
}

inline void trace(std::string_view msg) {
    if (g_logger) g_logger->trace("{}", msg);
}

inline void critical(std::string_view msg) {
    if (g_logger) g_logger->critical("{}", msg);
}

// Formatted logging (fmt style)
template<typename... Args>
inline void info(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    if (g_logger) g_logger->info(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void warn(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    if (g_logger) g_logger->warn(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void error(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    if (g_logger) g_logger->error(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void debug(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    if (g_logger) g_logger->debug(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void trace(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    if (g_logger) g_logger->trace(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void critical(spdlog::format_string_t<Args...> fmt, Args&&... args) {
    if (g_logger) g_logger->critical(fmt, std::forward<Args>(args)...);
}

inline void set_level(spdlog::level::level_enum level) {
    if (g_logger) g_logger->set_level(level);
}

/**
 * Force flush all pending log messages
 */
inline void flush() {
    if (g_logger) g_logger->flush();
}

}  // namespace ase::log
