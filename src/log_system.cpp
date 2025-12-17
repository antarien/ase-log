#include <ase/log/log.hpp>
#include <ase/ecs/schedule_registry.hpp>
#include <spdlog/pattern_formatter.h>
#include <filesystem>

namespace ase::log {

// Custom flag for colored 3-character log level (matching ecs.cpp boot_log)
class ColoredLevelFlag : public spdlog::custom_flag_formatter {
public:
    void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override {
        static const char* levels[] = {"TRC", "DBG", "INF", "WRN", "ERR", "CRT", "OFF"};
        static const char* colors[] = {
            "\x1b[38;5;243m", // trace - dark gray
            "\x1b[38;5;67m",  // debug - muted blue
            "\x1b[38;5;71m",  // info - muted green
            "\x1b[38;5;179m", // warn - muted yellow
            "\x1b[38;5;167m", // error - muted red
            "\x1b[38;5;168m", // critical - muted magenta
            "\x1b[0m"         // off
        };
        auto idx = static_cast<size_t>(msg.level);
        if (idx < sizeof(levels) / sizeof(levels[0])) {
            dest.append(std::string_view(colors[idx]));
            dest.append(std::string_view(levels[idx]));
            dest.append(std::string_view("\x1b[0m"));
        }
    }

    [[nodiscard]] std::unique_ptr<custom_flag_formatter> clone() const override {
        return std::make_unique<ColoredLevelFlag>();
    }
};

// Plain level for file output (no colors)
class PlainLevelFlag : public spdlog::custom_flag_formatter {
public:
    void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override {
        static const char* levels[] = {"TRC", "DBG", "INF", "WRN", "ERR", "CRT", "OFF"};
        auto idx = static_cast<size_t>(msg.level);
        if (idx < sizeof(levels) / sizeof(levels[0])) {
            dest.append(std::string_view(levels[idx]));
        }
    }

    [[nodiscard]] std::unique_ptr<custom_flag_formatter> clone() const override {
        return std::make_unique<PlainLevelFlag>();
    }
};

// Custom flag for muted gray timestamp with brackets
class GrayTimestampFlag : public spdlog::custom_flag_formatter {
public:
    void format(const spdlog::details::log_msg& msg, const std::tm& tm, spdlog::memory_buf_t& dest) override {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            msg.time.time_since_epoch()).count() % 1000;
        char buf[64];
        snprintf(buf, sizeof(buf), "\033[38;5;242m[%04d-%02d-%02d %02d:%02d:%02d.%03d]\033[0m",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<int>(ms));
        dest.append(std::string_view(buf));
    }

    [[nodiscard]] std::unique_ptr<custom_flag_formatter> clone() const override {
        return std::make_unique<GrayTimestampFlag>();
    }
};

// Static member definitions
std::shared_ptr<spdlog::logger> LogSystem::g_logger_ = nullptr;        // Server logger with [SERVER] prefix
std::shared_ptr<spdlog::logger> LogSystem::g_client_logger_ = nullptr; // Client logger without [SERVER] prefix
std::string LogSystem::g_log_path_;

LogSystem::LogSystem(const std::string& name, const std::string& log_file)
    : logger_name_(name)
    , log_file_(log_file)
{}

void LogSystem::on_start(ecs::Registry& /*registry*/) {
    if (g_logger_) return;

    // Create logs directory
    std::filesystem::path log_path(log_file_);
    if (log_path.has_parent_path()) {
        std::filesystem::create_directories(log_path.parent_path());
    }
    g_log_path_ = log_file_;

    // Truncate log file on startup
    if (std::filesystem::exists(log_file_)) {
        std::filesystem::resize_file(log_file_, 0);
    }

    // === SERVER LOGGER (with [SERVER] prefix) ===
    // Console: "[2025-01-15 18:32:45.123] [Inf] [ASE] [SERVER] message"
    auto server_console_formatter = std::make_unique<spdlog::pattern_formatter>();
    server_console_formatter->add_flag<ColoredLevelFlag>('*');
    server_console_formatter->set_pattern("\x1b[38;5;242m[%Y-%m-%d %H:%M:%S.%e]\x1b[0m [%*] [ASE] [SERVER] %v");

    // File: plain text
    auto server_file_formatter = std::make_unique<spdlog::pattern_formatter>();
    server_file_formatter->add_flag<PlainLevelFlag>('#');
    server_file_formatter->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%#] [ASE] [SERVER] %v");

    auto server_console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    server_console_sink->set_color(spdlog::level::trace, "\033[38;5;243m");
    server_console_sink->set_color(spdlog::level::debug, "\033[38;5;67m");
    server_console_sink->set_color(spdlog::level::info, "\033[38;5;71m");
    server_console_sink->set_color(spdlog::level::warn, "\033[38;5;179m");
    server_console_sink->set_color(spdlog::level::err, "\033[38;5;167m");
    server_console_sink->set_color(spdlog::level::critical, "\033[38;5;168m");
    server_console_sink->set_formatter(std::move(server_console_formatter));

    auto server_file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_, true);
    server_file_sink->set_formatter(std::move(server_file_formatter));

    std::vector<spdlog::sink_ptr> server_sinks{server_console_sink, server_file_sink};
    g_logger_ = std::make_shared<spdlog::logger>(logger_name_, server_sinks.begin(), server_sinks.end());
    g_logger_->set_level(spdlog::level::debug);
    g_logger_->flush_on(spdlog::level::trace);
    spdlog::register_logger(g_logger_);

    // === CLIENT LOGGER (without [SERVER] - client logs have their own prefix) ===
    // Console: "[2025-01-15 18:32:45.123] [Inf] [ASE] message"
    auto client_console_formatter = std::make_unique<spdlog::pattern_formatter>();
    client_console_formatter->add_flag<ColoredLevelFlag>('*');
    client_console_formatter->set_pattern("\x1b[38;5;242m[%Y-%m-%d %H:%M:%S.%e]\x1b[0m [%*] [ASE] %v");

    // File: plain text
    auto client_file_formatter = std::make_unique<spdlog::pattern_formatter>();
    client_file_formatter->add_flag<PlainLevelFlag>('#');
    client_file_formatter->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%#] [ASE] %v");

    auto client_console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    client_console_sink->set_color(spdlog::level::trace, "\033[38;5;243m");
    client_console_sink->set_color(spdlog::level::debug, "\033[38;5;67m");
    client_console_sink->set_color(spdlog::level::info, "\033[38;5;71m");
    client_console_sink->set_color(spdlog::level::warn, "\033[38;5;179m");
    client_console_sink->set_color(spdlog::level::err, "\033[38;5;167m");
    client_console_sink->set_color(spdlog::level::critical, "\033[38;5;168m");
    client_console_sink->set_formatter(std::move(client_console_formatter));

    // Client logger shares file sink but with different formatter - need separate sink
    auto client_file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_, false); // append
    client_file_sink->set_formatter(std::move(client_file_formatter));

    std::vector<spdlog::sink_ptr> client_sinks{client_console_sink, client_file_sink};
    g_client_logger_ = std::make_shared<spdlog::logger>("client", client_sinks.begin(), client_sinks.end());
    g_client_logger_->set_level(spdlog::level::debug);
    g_client_logger_->flush_on(spdlog::level::trace);
    spdlog::register_logger(g_client_logger_);

    g_logger_->info("LogSystem started");
}

void LogSystem::on_stop(ecs::Registry& /*registry*/) {
    if (g_logger_) {
        g_logger_->flush();
        spdlog::drop_all();
        g_logger_.reset();
    }
}

void LogSystem::tick(ecs::Registry& /*registry*/, float /*dt*/) {
    // Könnte später für Batch-Logging bei Millionen Logs genutzt werden
}

// Register in Startup schedule (first!)
REGISTER_SYSTEM(LogSystem)
    .in_schedule(ecs::Schedule::Startup)
    .with_priority(0);

}  // namespace ase::log
