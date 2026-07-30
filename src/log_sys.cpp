#include <ase/log/log.hpp>
#include <ase/log/log_module.hpp>
#include <ase/log/log_filter.hpp>
#include <ase/containers/vector.hpp>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <algorithm>
#include <chrono>     // std::chrono::hours (retention cutoff)
#include <cstring>
#include <filesystem>
#include <fstream>

namespace ase::log {

// Counting sink — increments atomic counter for every log message (sequence numbers for /api/logs)
class CountingSink : public spdlog::sinks::base_sink<std::mutex> {
public:
    explicit CountingSink(std::atomic<uint32_t>& counter) : counter_(counter) {}
protected:
    void sink_it_(const spdlog::details::log_msg& /*msg*/) override { counter_.fetch_add(1, std::memory_order_relaxed); }
    void flush_() override {}
private:
    std::atomic<uint32_t>& counter_;
};

// TUI callback sink — forwards each formatted line to a C callback (tools/ase-cli's log pane).
// Same base_sink<std::mutex> shape as CountingSink; formats through the sink's own formatter so the
// pane text is byte-identical to the console/file line.
class TuiCallbackSink : public spdlog::sinks::base_sink<std::mutex> {
public:
    TuiCallbackSink(TuiLogCallback callback, void* user) : callback_(callback), user_(user) {}
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        if (callback_ == nullptr) return;
        spdlog::memory_buf_t buf;
        formatter_->format(msg, buf);
        callback_(buf.data(), static_cast<uint32_t>(buf.size()), static_cast<int>(msg.level), user_);
    }
    void flush_() override {}
private:
    TuiLogCallback callback_;
    void* user_;
};

// Get ASE project root directory (where logs/ should be created)
// SSOT: ASE_PROJECT_ROOT compile define from CMake (_ASE_BASE),
// resolves correctly for both central and standalone subgit builds.
static std::filesystem::path get_project_root() {
#ifdef ASE_PROJECT_ROOT
    return std::filesystem::path(ASE_PROJECT_ROOT);
#else
    // Fallback: current working directory
    return std::filesystem::current_path();
#endif
}

/**
 * Rotation quota, SSOT file <project-root>/logs/quota.conf.
 *
 * Every file sink in this translation unit is a rotating sink fed by these values, so no log file
 * of any tier, of the edge daemon or of the operator CLI can grow without bound. The file is a
 * two-key plain-text format ("max_bytes <n>" and "max_files <n>"), parsed with plain stream reads.
 */

// Keys of the quota SSOT file, shared by the reader and the writer so the two can never drift.
static constexpr const char* kQuotaKeyMaxBytes = "max_bytes";
static constexpr const char* kQuotaKeyMaxFiles = "max_files";

std::string log_dir_path() {
    return (get_project_root() / "logs").string();
}

std::string log_quota_path() {
    return (get_project_root() / "logs" / "quota.conf").string();
}

LogQuota log_quota() {
    return log_quota_in(log_dir_path());
}

LogQuota log_quota_in(const std::string& dir) {
    LogQuota quota;  // starts at the kDefaultLog* values
    std::ifstream in(dir + "/quota.conf");
    if (!in.is_open()) {
        return quota;  // no SSOT file there: the documented defaults apply
    }
    std::string key;
    while (in >> key) {
        if (key == kQuotaKeyMaxBytes) {
            uint64_t value = 0;
            if (in >> value && value >= kMinLogMaxBytes) {
                quota.max_bytes = value;
            }
        } else if (key == kQuotaKeyMaxFiles) {
            uint32_t value = 0;
            if (in >> value) {
                // Clamped on READ as well: a hand-edited quota.conf must never be able to push
                // max_files past what the rotating sink accepts, because that throw would abort
                // startup for every binary that reads this file.
                quota.max_files = (value > kMaxLogMaxFiles) ? kMaxLogMaxFiles : value;
            }
        }
    }
    return quota;
}

bool set_log_quota(const LogQuota& quota) {
    std::error_code ec;
    std::filesystem::create_directories(log_dir_path(), ec);
    std::ofstream out(log_quota_path(), std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    const uint64_t bytes = (quota.max_bytes < kMinLogMaxBytes) ? kMinLogMaxBytes : quota.max_bytes;
    const uint32_t files = (quota.max_files > kMaxLogMaxFiles) ? kMaxLogMaxFiles : quota.max_files;
    out << kQuotaKeyMaxBytes << ' ' << bytes << '\n'
        << kQuotaKeyMaxFiles << ' ' << files << '\n';
    return out.good();
}

namespace {

/**
 * Retention sweep over a log directory, run when a binary builds its file sink.
 *
 * Size rotation bounds each log STREAM, but it cannot bound the NUMBER of streams, and that number
 * grows on its own: one file per port a tier is ever started on (dist-9080 through dist-9093 all
 * exist), plus one per operator console run (cli-<pid>.log). Without this sweep the directory grows
 * without end even though no single file does.
 *
 * The cutoff is deliberately far past any process lifetime, so a file a running binary still writes
 * can never be caught: anything untouched for two weeks has no writer left.
 */
void prune_log_dir(const std::filesystem::path& dir) {
    std::error_code ec;
    const auto cutoff = std::filesystem::file_time_type::clock::now() -
                        std::chrono::hours(24 * kLogRetentionDays);
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) {
            return;
        }
        if (entry.path().extension() != ".log" || !entry.is_regular_file(ec)) {
            continue;
        }
        std::error_code time_ec;
        const auto written = std::filesystem::last_write_time(entry.path(), time_ec);
        if (time_ec || written >= cutoff) {
            continue;
        }
        std::error_code remove_ec;
        std::filesystem::remove(entry.path(), remove_ec);
    }
}

/**
 * Build the one file-sink shape used everywhere: rotate at the configured size, keep the configured
 * number of generations, and rotate once on open so each process start begins a fresh file WITHOUT
 * destroying the previous run (the old content becomes name.1.log). rotate_on_open only fires when
 * the existing file is non-empty, so a restart on an untouched file adds no empty generation.
 *
 * The quota is read from the sink's OWN directory, not from a compile-time path: the edge daemon
 * runs as a prebuilt binary on machines where the build tree does not exist, and it must find its
 * own quota (or none, and use the defaults) rather than probing a foreign path.
 */
std::shared_ptr<spdlog::sinks::sink> make_rotating_file_sink(const std::string& path) {
    const std::filesystem::path file_path(path);
    const std::string dir = file_path.has_parent_path() ? file_path.parent_path().string()
                                                        : log_dir_path();
    prune_log_dir(dir);
    const LogQuota quota = log_quota_in(dir);
    return std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        path, static_cast<std::size_t>(quota.max_bytes), static_cast<std::size_t>(quota.max_files), true);
}

}  // namespace

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
std::shared_ptr<spdlog::sinks::sink> LogSystem::g_ringbuffer_sink_ = nullptr;
std::atomic<uint32_t> LogSystem::g_log_counter_{0};

// File-scope: typed pointer for ringbuffer access (avoids ringbuffer_sink.h in header)
static std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> g_ring_typed_ = nullptr;

// Capture ringbuffer — holds every log call made between install_capture_logger()
// (first line of Kernel::build) and finalize_logger_after_boot (end of
// Schedule-Bootstrap block). One logger, sinks mutated at runtime.
static std::shared_ptr<spdlog::sinks::ringbuffer_sink_mt> g_capture_ring_ = nullptr;

// All real sinks are BUILT in LogSystem::on_start but NOT attached to the
// logger yet. During the Schedule-Bootstrap block the logger holds ONLY
// the capture_ring, so every log line (including those from every system's
// on_start) goes into the same single destination — no channel is live, no
// channel misses entries, no interleaving with the stdout boot progress
// table. finalize_logger_after_boot then attaches all four real sinks in
// one go and replays the capture_ring into all of them so every sink
// (console, file, HTTP-ring, counting) contains 100% identical content.
static std::shared_ptr<spdlog::sinks::sink> g_pending_console_sink_ = nullptr;
static std::shared_ptr<spdlog::sinks::sink> g_pending_file_sink_    = nullptr;
static std::shared_ptr<spdlog::sinks::sink> g_pending_http_ring_    = nullptr;
static std::shared_ptr<spdlog::sinks::sink> g_pending_counting_     = nullptr;

// Install the capture-phase logger. Must be called as the FIRST line of
// Kernel::build so every later log call (KernelEnvLdrSystem, KernelCliSystem,
// dlopen discovery, any system's on_start before LogSystem runs) goes into
// g_capture_ring_ instead of being dropped by the null-logger gate in log.hpp.
// No console sink — the App::startup() Schedule-Bootstrap block writes its
// own progress table to stdout and must not be interleaved with log lines.
void install_capture_logger() {
    if (LogSystem::logger()) return;  // idempotent (tests / CLI tools)
    g_capture_ring_ = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(2000);
    auto boot = std::make_shared<spdlog::logger>("ase-server", g_capture_ring_);
    boot->set_level(spdlog::level::trace);
    boot->flush_on(spdlog::level::trace);
    LogSystem::logger() = boot;
}

// Parse --log <+/-token...> from argv and feed the 3-axis filter engine.
// Collects every consecutive token starting with '+' or '-' that follows
// the "--log" flag, joined by spaces, exactly like KernelCliSystem did
// before. Standalone (no Registry / no ECS) so it can run before
// install_capture_logger — i.e. before any log::* call could fire.
void finalize_logger_after_boot() {
    auto& logger = LogSystem::logger();
    if (!logger) {
        g_capture_ring_.reset();
        g_pending_console_sink_.reset();
        g_pending_file_sink_.reset();
        g_pending_http_ring_.reset();
        g_pending_counting_.reset();
        return;
    }

    // Attach all four real sinks to the logger in one go. Order matters
    // only for the replay below (capture_ring → all four); after the
    // ring is detached the order is irrelevant because every log call
    // fans out to every sink.
    ase::containers::Vector<spdlog::sink_ptr> real_sinks;
    if (g_pending_console_sink_) { real_sinks.push_back(g_pending_console_sink_); g_pending_console_sink_.reset(); }
    if (g_pending_file_sink_)    { real_sinks.push_back(g_pending_file_sink_);    g_pending_file_sink_.reset(); }
    if (g_pending_http_ring_)    { real_sinks.push_back(g_pending_http_ring_);    g_pending_http_ring_.reset(); }
    if (g_pending_counting_)     { real_sinks.push_back(g_pending_counting_);     g_pending_counting_.reset(); }
    for (auto& s : real_sinks) {
        logger->sinks().push_back(s);
    }

    // Replay the capture-ring into exactly these four real sinks. Every
    // captured log message reaches every real sink — no channel is missing
    // any entry — so console, file, HTTP-ring and counting end up showing
    // 100% identical content for the boot phase and beyond.
    if (g_capture_ring_) {
        auto captured = g_capture_ring_->last_raw();
        for (auto& msg : captured) {
            for (auto& s : real_sinks) {
                s->log(msg);
            }
        }
        logger->flush();

        auto& sinks_vec = logger->sinks();
        sinks_vec.erase(std::remove(sinks_vec.begin(), sinks_vec.end(), g_capture_ring_), sinks_vec.end());
        g_capture_ring_.reset();
    }
}

void parse_cli_filter_from_argv(int argc, char* argv[]) {
    if (!argv) return;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (!a || a[0] != '-' || a[1] != '-' || std::strcmp(a + 2, "log") != 0) continue;
        char filter_buf[512] = {};
        uint32_t pos = 0;
        for (int j = i + 1; j < argc && argv[j] && (argv[j][0] == '+' || argv[j][0] == '-'); ++j) {
            if (pos > 0 && pos < sizeof(filter_buf) - 1) filter_buf[pos++] = ' ';
            for (const char* p = argv[j]; *p && pos < sizeof(filter_buf) - 1; ++p)
                filter_buf[pos++] = *p;
        }
        filter_buf[pos] = '\0';
        ase::log::filter::parse_log_filter(filter_buf);
        return;
    }
}

LogSystem::LogSystem(const std::string& name, const std::string& log_file)
    : logger_name_(name)
    , log_file_(log_file)
{}

void LogSystem::on_start(ecs::Registry& registry) {
    // LogConfig is authoritative — set by KernelCliSystem from argv[0] +
    // ASE_HTTP_PORT. Guaranteed present because LogSystem runs after
    // KernelCliSystem via run_after in LogModule::build. No fallback — a
    // missing LogConfig is a boot bug and must fail fast (entt-assert).
    auto& cfg = registry.ctx().get<LogConfig>();
    std::string lbl = cfg.label[0] != '\0' ? std::string(cfg.label) : std::string("SERVER");
    log_file_ = cfg.log_file;

    // Calculate absolute log path relative to project root (not cwd).
    std::filesystem::path absolute_log_path;
    if (std::filesystem::path(log_file_).is_relative()) {
        absolute_log_path = get_project_root() / log_file_;
    } else {
        absolute_log_path = log_file_;
    }
    if (absolute_log_path.has_parent_path()) {
        std::filesystem::create_directories(absolute_log_path.parent_path());
    }
    g_log_path_ = absolute_log_path.string();
    // NO truncate here. The rotating sink below opens with rotate_on_open, which turns the previous
    // run into <name>.1.log instead of erasing it. Emptying the file first would make that rotation
    // a no-op (it only fires on a non-empty file) and would destroy the previous run's evidence on
    // every restart — including the crash that caused the restart.

    // Build server sinks — console, per-server file, HTTP-ringbuffer, counting.
    auto server_console_formatter = std::make_unique<spdlog::pattern_formatter>();
    server_console_formatter->add_flag<ColoredLevelFlag>('*');
    server_console_formatter->set_pattern("\x1b[38;5;242m[%Y-%m-%d %H:%M:%S.%e]\x1b[0m [%*] [ASE] [" + lbl + "] %v");

    auto server_file_formatter = std::make_unique<spdlog::pattern_formatter>();
    server_file_formatter->add_flag<PlainLevelFlag>('#');
    server_file_formatter->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%#] [ASE] [" + lbl + "] %v");

    auto server_console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    server_console_sink->set_color(spdlog::level::trace, "\033[38;5;243m");
    server_console_sink->set_color(spdlog::level::debug, "\033[38;5;67m");
    server_console_sink->set_color(spdlog::level::info, "\033[38;5;71m");
    server_console_sink->set_color(spdlog::level::warn, "\033[38;5;179m");
    server_console_sink->set_color(spdlog::level::err, "\033[38;5;167m");
    server_console_sink->set_color(spdlog::level::critical, "\033[38;5;168m");
    server_console_sink->set_formatter(std::move(server_console_formatter));

    auto server_file_sink = make_rotating_file_sink(absolute_log_path.string());
    server_file_sink->set_formatter(std::move(server_file_formatter));

    // HTTP-endpoint ringbuffer (served by /api/logs). Separate from the
    // transient capture-ring: this one stays for the process lifetime.
    g_ring_typed_ = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(cfg.ringbuffer_size);
    g_ringbuffer_sink_ = g_ring_typed_;
    auto ring_formatter = std::make_unique<spdlog::pattern_formatter>();
    ring_formatter->add_flag<PlainLevelFlag>('#');
    ring_formatter->set_pattern("[%H:%M:%S.%e] [%#] %v");
    g_ringbuffer_sink_->set_formatter(std::move(ring_formatter));

    auto counting_sink = std::make_shared<CountingSink>(g_log_counter_);

    // Park ALL real sinks. The logger keeps only the capture_ring during
    // the Schedule-Bootstrap block — one single capture, no channel is
    // live yet. finalize_logger_after_boot attaches all four at once and
    // replays the capture_ring into each of them, so every sink ends up
    // 100% identical.
    g_pending_console_sink_ = server_console_sink;
    g_pending_file_sink_    = server_file_sink;
    g_pending_http_ring_    = g_ringbuffer_sink_;
    g_pending_counting_     = counting_sink;

    if (!g_logger_) {
        // Safety net for callers that never invoked install_capture_logger
        // (unit tests etc.): create a minimal logger on the real sinks.
        ase::containers::Vector<spdlog::sink_ptr> all{server_console_sink, server_file_sink, g_ringbuffer_sink_, counting_sink};
        g_logger_ = std::make_shared<spdlog::logger>(logger_name_, all.begin(), all.end());
    }
    // else: leave the logger alone — it currently has [capture_ring] only,
    // exactly right for the boot phase.
    g_logger_->set_level(spdlog::level::trace);
    g_logger_->flush_on(spdlog::level::trace);
    spdlog::register_logger(g_logger_);

    // Client logger — dedicated pattern without the [LABEL] prefix. No
    // capture-phase; client logs can only arrive after the network layer is up.
    auto client_console_formatter = std::make_unique<spdlog::pattern_formatter>();
    client_console_formatter->add_flag<ColoredLevelFlag>('*');
    client_console_formatter->set_pattern("\x1b[38;5;242m[%Y-%m-%d %H:%M:%S.%e]\x1b[0m [%*] [ASE] %v");

    auto client_console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    client_console_sink->set_color(spdlog::level::trace, "\033[38;5;243m");
    client_console_sink->set_color(spdlog::level::debug, "\033[38;5;67m");
    client_console_sink->set_color(spdlog::level::info, "\033[38;5;71m");
    client_console_sink->set_color(spdlog::level::warn, "\033[38;5;179m");
    client_console_sink->set_color(spdlog::level::err, "\033[38;5;167m");
    client_console_sink->set_color(spdlog::level::critical, "\033[38;5;168m");
    client_console_sink->set_formatter(std::move(client_console_formatter));

    // The client logger SHARES the server's rotating file sink instead of opening a second handle on
    // the same path. Two rotating sinks on one file would rename each other's generations and write
    // through independent offsets, corrupting both. Sharing gives one writer, one rotation state and
    // one lock; the client console sink above keeps its own [LABEL]-free pattern.
    ase::containers::Vector<spdlog::sink_ptr> client_sinks{client_console_sink, server_file_sink};
    g_client_logger_ = std::make_shared<spdlog::logger>("client", client_sinks.begin(), client_sinks.end());
    g_client_logger_->set_level(spdlog::level::debug);
    g_client_logger_->flush_on(spdlog::level::trace);
    spdlog::register_logger(g_client_logger_);
}

// Parse ringbuffer formatted string "[HH:MM:SS.mmm] [LVL] [SystemName] message" → LogEntry
static LogEntry parse_ring_line(const std::string& line, uint32_t seq) {
    static const char* level_names[] = {"TRC", "DBG", "INF", "WRN", "ERR", "CRT"};
    LogEntry entry{};
    entry.seq = seq;
    entry.level = 2;  // default INF

    std::string_view sv(line);

    // [HH:MM:SS.mmm]
    if (sv.size() > 14 && sv[0] == '[') {
        auto ts_end = sv.find(']', 1);
        if (ts_end != std::string_view::npos && ts_end < sizeof(entry.timestamp)) {
            std::memcpy(entry.timestamp, sv.data() + 1, ts_end - 1);
            entry.timestamp[ts_end - 1] = '\0';
            sv.remove_prefix(ts_end + 1);
            if (!sv.empty() && sv[0] == ' ') sv.remove_prefix(1);
        }
    }

    // [LVL]
    if (sv.size() > 4 && sv[0] == '[') {
        auto lvl_end = sv.find(']', 1);
        if (lvl_end != std::string_view::npos) {
            auto lvl_str = sv.substr(1, lvl_end - 1);
            for (uint8_t i = 0; i < 6; ++i) {
                if (lvl_str == level_names[i]) { entry.level = i; break; }
            }
            sv.remove_prefix(lvl_end + 1);
            if (!sv.empty() && sv[0] == ' ') sv.remove_prefix(1);
        }
    }

    // [SystemName] (optional, first bracket in remaining text)
    if (sv.size() > 2 && sv[0] == '[') {
        auto sys_end = sv.find(']', 1);
        if (sys_end != std::string_view::npos) {
            auto len = sys_end - 1;
            if (len > 0 && len < sizeof(entry.system)) {
                std::memcpy(entry.system, sv.data() + 1, len);
                entry.system[len] = '\0';
            }
            sv.remove_prefix(sys_end + 1);
            if (!sv.empty() && sv[0] == ' ') sv.remove_prefix(1);
        }
    }

    // Remaining = message
    auto msg_len = (sv.size() < sizeof(entry.message) - 1) ? sv.size() : sizeof(entry.message) - 1;
    std::memcpy(entry.message, sv.data(), msg_len);
    entry.message[msg_len] = '\0';

    return entry;
}

ase::containers::Vector<LogEntry> LogSystem::recent_logs(uint32_t since_seq) {
    if (!g_ring_typed_) return {};
    auto all = g_ring_typed_->last_formatted();
    uint32_t seq = g_log_counter_.load();
    uint32_t total = static_cast<uint32_t>(all.size());

    if (total == 0) return {};

    // Determine which lines to return
    uint32_t count;
    if (since_seq == 0) {
        count = (total > 50) ? 50 : total;
    } else if (since_seq >= seq) {
        return {};
    } else {
        count = seq - since_seq;
        if (count > total) count = total;
    }

    // Parse formatted strings to LogEntry, assign sequence numbers
    ase::containers::Vector<LogEntry> result;
    result.reserve(count);
    uint32_t start_seq = seq - count + 1;
    for (uint32_t i = total - count; i < total; ++i) {
        result.push_back(parse_ring_line(all[i], start_seq + (i - (total - count))));
    }
    return result;
}

void LogSystem::on_stop(ecs::Registry& /*registry*/) {
    if (g_logger_) {
        g_logger_->flush();
    }
    if (g_client_logger_) {
        g_client_logger_->flush();
    }
    // Drop every logger from spdlog's global registry, then release ALL static sink/logger
    // shared_ptrs HERE (controlled downstrap), so none survive into the C++ static-destruction
    // phase at process exit. If any lingered, its exit-time destructor would race spdlog's own
    // static registry destructor across translation units and free the same spdlog logger/sink
    // twice — the "double free or corruption (!prev)" abort in __run_exit_handlers (valgrind-
    // confirmed: blocks alloc'd in LogSystem::on_start, freed twice at exit). Resetting only
    // g_logger_ (as before) left g_client_logger_ + every sink static dangling to exit.
    // spdlog::shutdown() is a superset of drop_all(): it also resets the periodic flusher and
    // the global thread pool, and tears down the registry singleton's internal state. drop_all()
    // alone left spdlog-owned static state (a 45-byte logger-name block, valgrind-confirmed) to be
    // freed in the C++ static-destruction phase at exit, racing the registry's own static dtor →
    // the last remaining double-free. Releasing it HERE, in the controlled downstrap, removes it.
    spdlog::shutdown();
    g_logger_.reset();
    g_client_logger_.reset();
    g_ringbuffer_sink_.reset();
    g_ring_typed_.reset();
    g_capture_ring_.reset();
    g_pending_console_sink_.reset();
    g_pending_file_sink_.reset();
    g_pending_http_ring_.reset();
    g_pending_counting_.reset();

    // Release the static std::string heap buffer too. ase-log is a STATIC lib embedded in the
    // server binary AND in every dlopen'd .module (via ase::ecs → ase::log), so g_log_path_ has
    // one merged instance but a per-translation-unit __cxa_atexit destructor registration. At
    // process exit those destructors all run on the SAME merged string and free its buffer more
    // than once ("double free or corruption" in __run_exit_handlers; the 45-byte block is this
    // path). Emptying + shrinking it HERE (once, in the controlled downstrap) leaves every
    // exit-time destructor a no-op on an empty string. Same reasoning as the sink/logger resets
    // above — clear the merged static while the engine still runs, never at static destruction.
    g_log_path_.clear();
    g_log_path_.shrink_to_fit();
}

void LogSystem::tick(ecs::Registry& /*registry*/, float /*dt*/) {
    // Könnte später für Batch-Logging bei Millionen Logs genutzt werden
}

// Register in Startup schedule (first!)

// Standalone server-style logger for non-ECS binaries (edge daemon). Mirrors the LogSystem::on_start
// console+file formatters byte-for-byte so customer-side tools log identically to engine/replica/world,
// but with no capture-ring/CountingSink/HTTP-ring (no /api/logs consumer) and no registry dependency.
void init_server_standalone(const std::string& name, const std::string& label, const std::string& log_file) {
    if (LogSystem::logger()) return;  // idempotent — matches inline init() guard

    auto console_formatter = std::make_unique<spdlog::pattern_formatter>();
    console_formatter->add_flag<ColoredLevelFlag>('*');
    console_formatter->set_pattern("\x1b[38;5;242m[%Y-%m-%d %H:%M:%S.%e]\x1b[0m [%*] [ASE] [" + label + "] %v");

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_color(spdlog::level::trace, "\033[38;5;243m");
    console_sink->set_color(spdlog::level::debug, "\033[38;5;67m");
    console_sink->set_color(spdlog::level::info, "\033[38;5;71m");
    console_sink->set_color(spdlog::level::warn, "\033[38;5;179m");
    console_sink->set_color(spdlog::level::err, "\033[38;5;167m");
    console_sink->set_color(spdlog::level::critical, "\033[38;5;168m");
    console_sink->set_formatter(std::move(console_formatter));

    ase::containers::Vector<spdlog::sink_ptr> sinks{console_sink};

    if (!log_file.empty()) {
        std::filesystem::path absolute_log_path;
        if (std::filesystem::path(log_file).is_relative()) {
            absolute_log_path = get_project_root() / log_file;
        } else {
            absolute_log_path = log_file;
        }
        if (absolute_log_path.has_parent_path()) {
            std::filesystem::create_directories(absolute_log_path.parent_path());
        }
        // NO truncate: the rotating sink opens with rotate_on_open, turning the previous run into
        // name.1.log instead of erasing it. Emptying the file first would disable that rotation,
        // because it only fires on a non-empty file.
        auto file_formatter = std::make_unique<spdlog::pattern_formatter>();
        file_formatter->add_flag<PlainLevelFlag>('#');
        file_formatter->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%#] [ASE] [" + label + "] %v");
        auto file_sink = make_rotating_file_sink(absolute_log_path.string());
        file_sink->set_formatter(std::move(file_formatter));
        sinks.push_back(file_sink);
    }

    LogSystem::logger() = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    LogSystem::logger()->set_level(spdlog::level::trace);
    LogSystem::logger()->flush_on(spdlog::level::trace);
}

// Standalone logger for a full-screen TUI tool (tools/ase-cli): a plain [LABEL] file sink plus a
// colored TUI-callback sink (the caller's log pane), and NO stdout console sink, so raw ANSI never
// corrupts the alternate screen. File formatter and path resolution mirror init_server_standalone
// byte-for-byte; the callback line matches the tier console line (colored 3-char level).
void init_tui_standalone(const std::string& name, const std::string& label, const std::string& log_file,
                         TuiLogCallback callback, void* user) {
    if (LogSystem::logger()) return;  // idempotent — matches init_server_standalone

    ase::containers::Vector<spdlog::sink_ptr> sinks;

    if (!log_file.empty()) {
        std::filesystem::path absolute_log_path;
        if (std::filesystem::path(log_file).is_relative()) {
            absolute_log_path = get_project_root() / log_file;
        } else {
            absolute_log_path = log_file;
        }
        if (absolute_log_path.has_parent_path()) {
            std::filesystem::create_directories(absolute_log_path.parent_path());
        }
        // NO truncate: the rotating sink opens with rotate_on_open, turning the previous run into
        // name.1.log instead of erasing it. Emptying the file first would disable that rotation,
        // because it only fires on a non-empty file.
        auto file_formatter = std::make_unique<spdlog::pattern_formatter>();
        file_formatter->add_flag<PlainLevelFlag>('#');
        file_formatter->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%#] [ASE] [" + label + "] %v");
        auto file_sink = make_rotating_file_sink(absolute_log_path.string());
        file_sink->set_formatter(std::move(file_formatter));
        sinks.push_back(file_sink);
    }

    auto tui_formatter = std::make_unique<spdlog::pattern_formatter>();
    tui_formatter->add_flag<ColoredLevelFlag>('*');
    tui_formatter->set_pattern("\x1b[38;5;242m[%Y-%m-%d %H:%M:%S.%e]\x1b[0m [%*] [ASE] [" + label + "] %v");
    auto tui_sink = std::make_shared<TuiCallbackSink>(callback, user);
    tui_sink->set_formatter(std::move(tui_formatter));
    sinks.push_back(tui_sink);

    LogSystem::logger() = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    LogSystem::logger()->set_level(spdlog::level::trace);
    LogSystem::logger()->flush_on(spdlog::level::trace);
}

}  // namespace ase::log
