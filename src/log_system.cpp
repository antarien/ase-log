#include <ase/log/log.hpp>
#include <ase/ecs/system_registry.hpp>
#include <spdlog/pattern_formatter.h>
#include <filesystem>

namespace ase::log {

// Custom flag for 3-character log level: Trc, Dbg, Inf, Wrn, Err, Crt
class ShortLevelFlag : public spdlog::custom_flag_formatter {
public:
    void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override {
        static const char* short_levels[] = {"Trc", "Dbg", "Inf", "Wrn", "Err", "Crt", "Off"};
        auto level_idx = static_cast<size_t>(msg.level);
        if (level_idx < sizeof(short_levels) / sizeof(short_levels[0])) {
            dest.append(std::string_view(short_levels[level_idx]));
        }
    }

    [[nodiscard]] std::unique_ptr<custom_flag_formatter> clone() const override {
        return std::make_unique<ShortLevelFlag>();
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

    // Create custom formatter with 3-char level (%* = custom flag)
    auto console_formatter = std::make_unique<spdlog::pattern_formatter>();
    console_formatter->add_flag<ShortLevelFlag>('*');
    console_formatter->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%*%$] [%n] %v");

    auto file_formatter = std::make_unique<spdlog::pattern_formatter>();
    file_formatter->add_flag<ShortLevelFlag>('*');
    file_formatter->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%*] [%n] %v");

    // Create sinks: console (colored) + file
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_formatter(std::move(console_formatter));

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_, true);
    file_sink->set_formatter(std::move(file_formatter));

    // Create logger with both sinks
    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    g_logger_ = std::make_shared<spdlog::logger>(logger_name_, sinks.begin(), sinks.end());
    g_logger_->set_level(spdlog::level::debug);
    g_logger_->flush_on(spdlog::level::trace);

    spdlog::register_logger(g_logger_);

    g_logger_->info("=== ANTARES SIMULATION ENGINE ===");
    g_logger_->info("[Phase: Foundation] LogSystem started");
}

void LogSystem::on_stop(ecs::Registry& /*registry*/) {
    if (g_logger_) {
        g_logger_->info("[Phase: Foundation] LogSystem stopped");
        g_logger_->info("=== ENGINE SHUTDOWN ===");
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
