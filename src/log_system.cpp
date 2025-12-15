#include <ase/log/log.hpp>
#include <ase/ecs/system_registry.hpp>
#include <spdlog/pattern_formatter.h>
#include <filesystem>

namespace ase::log {

// Custom flag for colored 3-character log level (matching ecs.cpp boot_log)
class ColoredLevelFlag : public spdlog::custom_flag_formatter {
public:
    void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override {
        static const char* levels[] = {"Trc", "Dbg", "Inf", "Wrn", "Err", "Crt", "Off"};
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
        static const char* levels[] = {"Trc", "Dbg", "Inf", "Wrn", "Err", "Crt", "Off"};
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
std::shared_ptr<spdlog::logger> LogSystem::g_logger_ = nullptr;
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

    // Console: format matching ecs.cpp boot_log
    // Format: "[2025-01-15 18:32:45.123] [Inf] [ASE] message"
    auto console_formatter = std::make_unique<spdlog::pattern_formatter>();
    console_formatter->add_flag<ColoredLevelFlag>('*');
    console_formatter->set_pattern("\x1b[38;5;242m[%Y-%m-%d %H:%M:%S.%e]\x1b[0m [%*] [ASE] %v");

    // File: plain text (same format, no colors)
    auto file_formatter = std::make_unique<spdlog::pattern_formatter>();
    file_formatter->add_flag<PlainLevelFlag>('#');
    file_formatter->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%#] [ASE] %v");

    // Create sinks with muted colors
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_color(spdlog::level::trace, "\033[38;5;243m");    // dark gray
    console_sink->set_color(spdlog::level::debug, "\033[38;5;67m");     // muted blue
    console_sink->set_color(spdlog::level::info, "\033[38;5;71m");      // muted green
    console_sink->set_color(spdlog::level::warn, "\033[38;5;179m");     // muted yellow
    console_sink->set_color(spdlog::level::err, "\033[38;5;167m");      // muted red
    console_sink->set_color(spdlog::level::critical, "\033[38;5;168m"); // muted magenta
    console_sink->set_formatter(std::move(console_formatter));

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_, true);
    file_sink->set_formatter(std::move(file_formatter));

    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    g_logger_ = std::make_shared<spdlog::logger>(logger_name_, sinks.begin(), sinks.end());
    g_logger_->set_level(spdlog::level::debug);
    g_logger_->flush_on(spdlog::level::trace);

    spdlog::register_logger(g_logger_);

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

// Auto-register in Foundation phase (first!)
AUTO_REGISTER_SYSTEM(
    LogSystem,
    ecs::SystemPhase::Foundation,
    (std::vector<std::string>{})
)

}  // namespace ase::log
