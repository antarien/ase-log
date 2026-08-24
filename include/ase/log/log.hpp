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
#include <ase/log/log_err_cat_gen.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <memory>
#include <source_location>
#include <string>
#include <type_traits>

namespace ase::log {

// ============================================================================
// LogSystem - The Logger as an ECS System
// ============================================================================

// Structured log entry for /api/logs endpoint (ringbuffer output)
// Level assumed when a ringbuffer line carries no parsable "[LVL]" field. INF is the only safe
// default: guessing lower would hide a warning behind a filter, guessing higher would raise every
// unparsable line to an alarm. The value is the index into the level table (0=trace..5=critical).
inline constexpr uint8_t kLogLevelInfo = 2u;

// Highest level index the tables in this header carry: 0 TRC, 1 DBG, 2 INF, 3 WRN, 4 ERR, 5 CRT.
// The underlying library knows one more value above CRT that means "logging off"; a line can never
// arrive carrying it, but capture_entry() clamps to this bound rather than trusting that, because
// the alternative is an out-of-range index into a caller's colour table.
inline constexpr uint8_t kLogLevelMax = 5u;

// Sentinel for recent_logs(since_seq): "I have nothing yet, give me the newest page." Any other
// value is a real sequence number and returns everything after it. A caller cannot mean sequence
// zero literally — the counter starts at one — so the sentinel costs no reachable value.
inline constexpr uint32_t kLogSeqAll = 0u;

// Stack buffer for a spdlog pattern assembled at startup. The longest pattern in the tree is the
// coloured server line (~55 chars of escapes and fields) plus LogConfig::label (declared
// `char label[16]` in log_module.hpp) plus the "] %v" tail — 128 leaves more than double the headroom, and the
// buffer lives for the two statements it takes to hand the pattern to spdlog.
inline constexpr uint32_t kLogPatternBufSize = 128u;

// Page size returned for a first poll (kLogSeqAll). The ringbuffer holds more than this; a fresh
// HTTP client asking for "recent" wants a screenful, not the whole buffer, and every later poll
// carries its sequence number and gets exactly the delta.
inline constexpr uint32_t kRecentLogsPageSize = 50u;

struct LogEntry {
    uint32_t seq = 0;           // monotonic sequence number
    uint8_t level = 0;          // spdlog level (0=trace..5=critical)
    char timestamp[16] = {};    // "HH:MM:SS.mmm"
    char system[32] = {};       // extracted from "[SystemName]" in message
    char message[512] = {};     // log message (truncated if too long)
};

/**
 * WO DER LOGGER LIEGT — UND WARUM NICHT MEHR IM SYSTEM (2026-08-22)
 *
 * Bis heute hielt LogSystem den Logger als statische Member `g_logger_`/`g_client_logger_`.
 * Ein ECS-System haelt keinen Zustand: die Goldene Regel sagt es, und der Validator meldet die
 * Folge — jeder Zugriff von aussen muss `LogSystem::logger()` rufen, und ein statischer
 * System-Aufruf ist STATIC_SYSTEM_CALLS_FORBIDDEN (4 Fundstellen in log_sys.cpp).
 *
 * Der Zustand liegt jetzt im LogResourceManager, dem von WRFL_ASE_FLYWEIGHT.md Section 1
 * vorgesehenen Halter. Er ist hier der einzig moegliche: `log::init` laeuft VOR `Kernel::build`,
 * also gibt es zum Zeitpunkt der Logger-Erzeugung keine Registry und damit kein `registry.ctx()`
 * (der Kopfkommentar von log_resource_manager.cpp fuehrt dieselbe Begruendung aus).
 *
 * ES IST EINE VORWAERTSDEKLARATION UND KEIN INCLUDE, mit Absicht: `internal/` bleibt intern.
 * Der Manager-Header wird von drei .cpp inkludiert und von keinem oeffentlichen Header — ihn
 * hier hereinzuziehen waere der bequeme Weg und wuerde die Kapselung fuer einen Zeilenersparnis
 * aufgeben.
 *
 * DIE ACCESSOREN BLEIBEN, WEIL 186 LESESTELLEN IN DIESER DATEI AN IHNEN HAENGEN. Sie geben
 * jetzt den Slot des Managers zurueck statt eines eigenen Speichers — EIN Speicher, kein Drift.
 * Wer sie eines Tages ganz entfernen will, muss die 186 Stellen mitziehen; das ist ein eigener
 * Zug und hat mit dieser Aenderung nichts zu tun.
 */
namespace internal {
[[nodiscard]] std::shared_ptr<spdlog::logger>& logger_slot();
[[nodiscard]] std::shared_ptr<spdlog::logger>& client_logger_slot();
}  // namespace internal

class LogSystem : public ecs::System {
public:
    /**
     * GELOESCHT 2026-08-22: explicit LogSystem(const std::string& name, const std::string&
     * log_file) — die zweite Haelfte einer Loeschung, deren Definition in log_sys.cpp schon
     * entfernt ist (dort steht der Grabstein mit derselben Begruendung). Der Konstruktor war
     * nicht bloss ungenutzt, sondern UNAUFRUFBAR: Systeme werden ueber das
     * add_system_with<LogSystem> in log_module.hpp angelegt, und
     * dieser Weg reicht keine Konstruktorargumente durch. LogSystem wird immer
     * default-konstruiert; log_file_ setzt on_start aus der LogConfig, logger_name_ bleibt bei
     * seiner Vorgabe. Eine Deklaration ohne Definition haette jeden Aufruf zu einem
     * Linkerfehler gemacht statt zu einem Uebersetzungsfehler.
     */
    LogSystem() = default;

    const char* name() const override { return "LogSystem"; }
    int priority() const override { return 0; }  // First system to run

    void on_start(ecs::Registry& registry) override;
    void on_stop(ecs::Registry& registry) override;
    void tick(ecs::Registry& registry, float dt) override;

    // Configuration (call before on_start)
    void set_name(const std::string& name) { logger_name_ = name; }
    void set_log_file(const std::string& path) { log_file_ = path; }

    // Access underlying loggers — the storage is the LogResourceManager, see the note above
    // this class. These stay as accessors only because 186 read sites in this header use them.
    static std::shared_ptr<spdlog::logger>& logger() { return internal::logger_slot(); }
    static std::shared_ptr<spdlog::logger>& client_logger() { return internal::client_logger_slot(); }
    static const std::string& log_path() { return g_log_path_; }

    // Recent logs from in-memory ringbuffer (for HTTP /api/logs endpoint)
    static ase::containers::Vector<LogEntry> recent_logs(uint32_t since_seq = 0);
    static uint32_t log_counter() { return g_log_counter_.load(); }

private:
    std::string logger_name_ = "ASE";
    std::string log_file_;

    // GELOESCHT 2026-08-22: g_logger_ / g_client_logger_. Der Logger liegt jetzt im
    // LogResourceManager (siehe die Notiz ueber dieser Klasse) — ein System haelt keinen
    // Zustand, und solange er hier lag, brauchte jede freie Funktion in log_sys.cpp einen
    // statischen LogSystem::-Aufruf, um an ihn heranzukommen.
    static std::string g_log_path_;
    static std::shared_ptr<spdlog::sinks::sink> g_ringbuffer_sink_;
    static std::atomic<uint32_t> g_log_counter_;
};

// ============================================================================
// Log rotation quota — SSOT for every log-writing binary
// ============================================================================

/**
 * Rotation quota shared by all tiers, the edge daemon and the operator CLI.
 *
 * The SSOT is the plain-text file <project-root>/logs/quota.conf, read once when a binary
 * builds its file sink. Every log file rotates at max_bytes and keeps max_files older
 * generations (world-9001.log, world-9001.1.log, ...), so no log can grow without bound in
 * production. A process started before a quota change keeps the quota it read at startup.
 */

// Quota applied when logs/quota.conf is absent. 50 MiB per file with 3 kept generations bounds
// one tier at 200 MiB, so five logging tiers cannot exceed 1 GiB no matter how long they run.
inline constexpr uint64_t kDefaultLogMaxBytes = 52428800ULL;  // 50 MiB
inline constexpr uint32_t kDefaultLogMaxFiles = 3u;

// Hard floor for a configured quota. A max_bytes of zero makes spdlog's rotating sink throw, and
// anything below one MiB would rotate mid-burst and shred a single stack trace across generations.
inline constexpr uint64_t kMinLogMaxBytes = 1048576ULL;  // 1 MiB

// Hard ceiling for kept generations. spdlog's rotating sink THROWS above 200000, and that throw
// happens inside the sink constructor during logger init — a place no ASE binary wraps in a catch,
// so it would reach std::terminate and every tier plus the operator console would die at startup.
// The bound is enforced in the SSOT itself (writer AND reader), so neither a mistyped console
// command nor a hand-edited quota.conf can turn a log setting into an unbootable stack. 100 kept
// generations is already far past any diagnostic need.
inline constexpr uint32_t kMaxLogMaxFiles = 100u;

// Retention for the log directory, swept when a binary builds its file sink. Size rotation bounds
// each STREAM; this bounds the NUMBER of streams, which grows by itself — one file per port a tier is
// ever started on, one per operator console run. Two weeks is far past any process lifetime, so a
// file a running binary still writes can never be caught by the sweep.
inline constexpr long kLogRetentionDays = 14;

// Seconds per day, so the retention cutoff can be computed in plain Unix seconds. utils::clock
// reports int64_t seconds rather than a clock type, and the sweep compares against DirEntry's
// st_mtime, which is the same unit — the two meet without a duration cast in between.
inline constexpr int64_t kSecondsPerDay = 86400;

// The sweep only ever deletes files ending in this suffix. Size counts the terminator, because
// str_equal is given the bound and stops at the NUL it finds within it.
inline constexpr const char* kLogFileSuffix    = ".log";
inline constexpr uint32_t    kLogFileSuffixLen = 5u;  // ".log" + '\0'

// Entries examined per directory listing. MEASURED 2026-08-20: the live log directory holds 24
// .log files. The population has two parts and only one of them grows — a fixed file per port a
// tier is ever started on (dist-9080 through dist-9093 is 14 of the 24), plus one per operator
// console run. 64 leaves room for 40 further console runs between two sweeps, and a sweep runs
// on every process start. Sized against the measurement, not against the type's maximum: a
// DirEntry is ~264 bytes, so this array is ~17 KiB of stack, and 256 entries would be ~68 KiB.
inline constexpr uint32_t kLogDirScanMax = 64u;

// If a listing comes back completely full, deleted files have freed slots and a further round can
// see entries the first one had no room for. Rounds stop as soon as a listing is short or deletes
// nothing, so this bound only caps a directory that keeps yielding full batches — it is a
// termination guard, not a work limit.
inline constexpr uint32_t kLogPruneMaxRounds = 4u;

struct LogQuota {
    uint64_t max_bytes = kDefaultLogMaxBytes;  // size at which the active file rotates
    uint32_t max_files = kDefaultLogMaxFiles;  // kept generations besides the active file
};

/** @brief Absolute path of the log directory (<project-root>/logs). */
[[nodiscard]] std::string log_dir_path();

/** @brief Absolute path of the quota SSOT (<project-root>/logs/quota.conf). */
[[nodiscard]] std::string log_quota_path();

/**
 * @brief Read the rotation quota from the SSOT file.
 * @return The configured quota, or the kDefaultLog* values when the file is absent or unreadable.
 */
[[nodiscard]] LogQuota log_quota();

/**
 * @brief Read the rotation quota from an EXPLICIT log directory.
 * @param dir Directory that holds the quota.conf to read.
 * @return The configured quota, or the kDefaultLog* values when the file is absent or unreadable.
 *
 * For binaries whose log directory is not the build tree's logs/. The edge daemon is downloaded as
 * a PREBUILT binary from the dist server; the customer never builds anything, so the
 * ASE_PROJECT_ROOT compiled into that binary is the BUILD MACHINE's path and exists nowhere on the
 * customer's disk. The daemon logs to <HOME>/.ase-edge/logs and must read its quota from THERE
 * (finding none and using the defaults), never from that foreign build path.
 */
[[nodiscard]] LogQuota log_quota_in(const std::string& dir);

/**
 * @brief Write the rotation quota to the SSOT file, creating the log directory if needed.
 * @param quota Values to persist; max_bytes is clamped up to kMinLogMaxBytes.
 * @return false when the file could not be written.
 */
bool set_log_quota(const LogQuota& quota);

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
 * @brief Callback that receives one fully-formatted log line (for a TUI log pane).
 * @param line   Pointer to the formatted line bytes (NOT null-terminated).
 * @param len    Number of bytes in the line.
 * @param level  spdlog level index (0 trace .. 5 critical) for optional per-level handling.
 * @param user   Opaque user pointer passed through from init_tui_standalone.
 *
 * The line is byte-identical to the tier console line (gray timestamp, colored 3-char level,
 * [ASE] [<label>] prefix). Invoked under the sink lock and possibly from a worker thread, so the
 * callback must only enqueue the line (never touch the terminal from here).
 */
using TuiLogCallback = void (*)(const char* line, uint32_t len, int level, void* user);

/**
 * @brief Initialize a standalone logger for a full-screen TUI tool (tools/ase-cli).
 * @param name     Logger name (e.g. "ase-cli")
 * @param label    Tier tag for the [ASE] [<label>] prefix (e.g. "CLI")
 * @param log_file Optional file path (relative resolves to project root; empty = no file)
 * @param callback Receives every formatted line for the caller's log pane
 * @param user     Opaque pointer handed back to the callback
 *
 * Builds a plain [LABEL][LVL] file sink (byte-identical to init_server_standalone's file sink) plus a
 * colored callback sink that feeds the caller's pane, and deliberately NO stdout console sink, so raw
 * ANSI never corrupts the alternate-screen TUI. Honours the 3-axis filter. Idempotent (no-op if a
 * logger already exists). Defined in log_sys.cpp (reuses the file-scope Colored/PlainLevelFlag flags).
 */
void init_tui_standalone(const std::string& name, const std::string& label, const std::string& log_file,
                         TuiLogCallback callback, void* user);

/**
 * @brief Install the capture-phase logger.
 *
 * Must be the FIRST call at the top of Kernel::build — it puts g_logger_
 * into a well-defined state (a single ringbuffer sink, no console) so every
 * log::* call made afterwards by kernel init, KernelEnvLdrSystem,
 * KernelCmdSystem, dlopen discovery and any pre-LogSystem::on_start code
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
 * @brief Divert every log line into a buffer, then hand the lines back one by one.
 *
 * WHY THIS EXISTS: two callers in ase-ecs (boot_logger.cpp, shutdown_sequence.cpp) print a
 * progress table to stdout and must not let a log line from some system's on_start tear a
 * row in half. Both solved it the same way and each carried its OWN copy: a private sink
 * class deriving from spdlog, a saved vector of the previous sinks, and a replay loop. Two
 * copies of one mechanism, in the module that owns neither the logger nor its sinks.
 *
 * The sinks belong to the logger, so the bracket belongs here.
 *
 * NO spdlog TYPE CROSSES THIS INTERFACE. The caller receives a level as a small integer and
 * the text through its OWN buffer. Handing out a sink pointer would have moved the coupling
 * into a different public signature instead of removing it — which is exactly the mistake
 * that a wrapper around a forbidden type makes while looking like a solution.
 *
 * Levels are the same small integers the rest of this header uses: 0 TRC, 1 DBG, 2 INF,
 * 3 WRN, 4 ERR, 5 CRT. Anything higher means "off" and is reported as CRT.
 *
 * Not reentrant and not nestable: a second capture_begin() while one is open returns false
 * and changes nothing. One progress table at a time is the only case that exists, and a
 * silent nested bracket would restore the wrong sinks on the inner close.
 *
 * @return true when the diversion is active. false means there is no logger yet — the caller
 *         may proceed, its log lines simply have nowhere to go, exactly as before.
 */
bool capture_begin();

/** Number of lines buffered since capture_begin(). 0 when no capture is open. */
[[nodiscard]] uint32_t capture_count();

/**
 * @brief Read one buffered line.
 *
 * @param index      0 .. capture_count()-1
 * @param out_level  receives the level (see above); untouched when the index is out of range
 * @param out_text   caller-owned buffer, always NUL-terminated when out_cap > 0
 * @param out_cap    capacity of out_text in bytes
 * @return the FULL length of the line, which may exceed out_cap-1 when it was truncated;
 *         0 when the index is out of range.
 *
 * The full length is returned rather than a bool so a caller can SEE a truncation instead of
 * silently printing a shortened line. A bool would have made "fits" and "was cut" look alike.
 */
uint32_t capture_entry(uint32_t index, uint8_t& out_level, char* out_text, uint32_t out_cap);

/**
 * @brief Replay the buffered lines through the sinks that were active at capture_begin().
 *
 * @return number of lines replayed.
 *
 * The diversion STAYS in place: replaying is not closing. Without this the caller could not do
 * it at all — while the bracket is open the logger's only sink is the buffer, so a log call
 * would land back in it, and after capture_end the buffer is gone. Only the bracket itself
 * holds both halves at once.
 *
 * THERE IS DELIBERATELY NO "skip the console" ARGUMENT, and the reason is a measurement rather
 * than a preference. boot_logger.cpp used to filter the console out of this replay with two
 * dynamic_casts against concrete sink classes of the logging library. Those casts were looking
 * for something that is not there: in the server path this module attaches a file sink, an
 * HTTP ringbuffer and a counting sink and NOTHING else (log_sys.cpp builds exactly two sinks,
 * both ringbuffers; finalize_logger_after_boot attaches exactly three, none of them a
 * console). The only console sink ase-log ever constructs lives in init_standalone, for CLI
 * tools that never run a boot table.
 *
 * So the filter removed nothing, and carrying it over would have moved a mechanism without an
 * effect into a second module — where the next reader would have had to work out all over
 * again what it guards against.
 *
 * IF A CONSOLE SINK IS EVER ATTACHED TO THE SERVER LOGGER, this decision has to be taken
 * again: replayed boot lines would then reach a terminal that already shows the progress
 * table. The place to notice it is here, not at a caller.
 */
uint32_t capture_replay();

/**
 * @brief Close the bracket and drop the buffer.
 *
 * @param restore_sinks  true puts the sinks that were active at capture_begin() back in
 *                       place; false leaves the logger without them.
 *
 * Both values are in use and neither is a shortcut: boot_logger restores, because the process
 * keeps running and every later log line has to reach console and file again.
 * shutdown_sequence does not, because it runs while the process is ending and has already
 * replayed the buffer to the terminal itself.
 *
 * Calling this without an open capture is a no-op.
 */
void capture_end(bool restore_sinks);

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

/**
 * WHY NINE WRAPPERS INSTEAD OF ONE PARAMETER PACK
 *
 * These wrap spdlog::format_string_t and capture the caller's source filename + function.
 * The constructor's __builtin_FILE()/__builtin_FUNCTION() defaults are evaluated at the CALL
 * SITE, which is what makes category-based filtering (file + lifecycle phase) work for
 * fmt-style calls. That capture is independent of consteval: a default argument is evaluated
 * where the call is written, so constexpr carries it just as well.
 *
 * One wrapper per arity, because a pack is forbidden and the format string has to be
 * parameterised over EXACTLY the argument types to keep fmt's compile-time check of the format
 * string against those types. Erasing the types into one value type would move that check to
 * run time, which is a capability loss and therefore not an option.
 *
 * THE WRAPPER IS NOT DECORATION — it is what makes the overload set safe. Taking
 * format_string_t directly and appending file/func as defaulted parameters compiles, and then
 * log::info("{} {}", "a", "b") silently binds to the ONE-argument overload with "b" as the
 * filename. Holding the defaults inside the wrapper's constructor makes that impossible.
 *
 * ARITY NINE: GEMESSEN 2026-08-22, the highest arity anywhere in the tree is 9
 * (replica_topo_swep_sys.cpp). Per function the measured maxima are info 8, warn 6, error 5,
 * debug 9; the other ten families have no caller at all. GESETZT is the uniform 9 for every
 * family — an uneven ceiling would be a trap, because adding one argument to a warn line would
 * break the build for a reason nobody can see at the call site. Needing a tenth means adding
 * one wrapper and one overload per level, and the compiler names the site.
 */
template<typename T0>
struct log_fmt1 {
    spdlog::format_string_t<T0> fmt;
    const char* file;
    const char* func;
    uint64_t loc_hash;  // constexpr-folded call-site key, see loc_key() above

    template<typename S>
        requires (!std::is_arithmetic_v<std::remove_cvref_t<S>>)
    constexpr log_fmt1(const S& s, const char* f = __builtin_FILE(),
                       const char* fn = __builtin_FUNCTION())
        : fmt(s), file(f), func(fn), loc_hash(loc_key(f, fn)) {}
};

template<typename T0, typename T1>
struct log_fmt2 {
    spdlog::format_string_t<T0, T1> fmt;
    const char* file;
    const char* func;
    uint64_t loc_hash;  // constexpr-folded call-site key, see loc_key() above

    template<typename S>
        requires (!std::is_arithmetic_v<std::remove_cvref_t<S>>)
    constexpr log_fmt2(const S& s, const char* f = __builtin_FILE(),
                       const char* fn = __builtin_FUNCTION())
        : fmt(s), file(f), func(fn), loc_hash(loc_key(f, fn)) {}
};

template<typename T0, typename T1, typename T2>
struct log_fmt3 {
    spdlog::format_string_t<T0, T1, T2> fmt;
    const char* file;
    const char* func;
    uint64_t loc_hash;  // constexpr-folded call-site key, see loc_key() above

    template<typename S>
        requires (!std::is_arithmetic_v<std::remove_cvref_t<S>>)
    constexpr log_fmt3(const S& s, const char* f = __builtin_FILE(),
                       const char* fn = __builtin_FUNCTION())
        : fmt(s), file(f), func(fn), loc_hash(loc_key(f, fn)) {}
};

template<typename T0, typename T1, typename T2, typename T3>
struct log_fmt4 {
    spdlog::format_string_t<T0, T1, T2, T3> fmt;
    const char* file;
    const char* func;
    uint64_t loc_hash;  // constexpr-folded call-site key, see loc_key() above

    template<typename S>
        requires (!std::is_arithmetic_v<std::remove_cvref_t<S>>)
    constexpr log_fmt4(const S& s, const char* f = __builtin_FILE(),
                       const char* fn = __builtin_FUNCTION())
        : fmt(s), file(f), func(fn), loc_hash(loc_key(f, fn)) {}
};

template<typename T0, typename T1, typename T2, typename T3, typename T4>
struct log_fmt5 {
    spdlog::format_string_t<T0, T1, T2, T3, T4> fmt;
    const char* file;
    const char* func;
    uint64_t loc_hash;  // constexpr-folded call-site key, see loc_key() above

    template<typename S>
        requires (!std::is_arithmetic_v<std::remove_cvref_t<S>>)
    constexpr log_fmt5(const S& s, const char* f = __builtin_FILE(),
                       const char* fn = __builtin_FUNCTION())
        : fmt(s), file(f), func(fn), loc_hash(loc_key(f, fn)) {}
};

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
struct log_fmt6 {
    spdlog::format_string_t<T0, T1, T2, T3, T4, T5> fmt;
    const char* file;
    const char* func;
    uint64_t loc_hash;  // constexpr-folded call-site key, see loc_key() above

    template<typename S>
        requires (!std::is_arithmetic_v<std::remove_cvref_t<S>>)
    constexpr log_fmt6(const S& s, const char* f = __builtin_FILE(),
                       const char* fn = __builtin_FUNCTION())
        : fmt(s), file(f), func(fn), loc_hash(loc_key(f, fn)) {}
};

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6>
struct log_fmt7 {
    spdlog::format_string_t<T0, T1, T2, T3, T4, T5, T6> fmt;
    const char* file;
    const char* func;
    uint64_t loc_hash;  // constexpr-folded call-site key, see loc_key() above

    template<typename S>
        requires (!std::is_arithmetic_v<std::remove_cvref_t<S>>)
    constexpr log_fmt7(const S& s, const char* f = __builtin_FILE(),
                       const char* fn = __builtin_FUNCTION())
        : fmt(s), file(f), func(fn), loc_hash(loc_key(f, fn)) {}
};

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7>
struct log_fmt8 {
    spdlog::format_string_t<T0, T1, T2, T3, T4, T5, T6, T7> fmt;
    const char* file;
    const char* func;
    uint64_t loc_hash;  // constexpr-folded call-site key, see loc_key() above

    template<typename S>
        requires (!std::is_arithmetic_v<std::remove_cvref_t<S>>)
    constexpr log_fmt8(const S& s, const char* f = __builtin_FILE(),
                       const char* fn = __builtin_FUNCTION())
        : fmt(s), file(f), func(fn), loc_hash(loc_key(f, fn)) {}
};

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7, typename T8>
struct log_fmt9 {
    spdlog::format_string_t<T0, T1, T2, T3, T4, T5, T6, T7, T8> fmt;
    const char* file;
    const char* func;
    uint64_t loc_hash;  // constexpr-folded call-site key, see loc_key() above

    template<typename S>
        requires (!std::is_arithmetic_v<std::remove_cvref_t<S>>)
    constexpr log_fmt9(const S& s, const char* f = __builtin_FILE(),
                       const char* fn = __builtin_FUNCTION())
        : fmt(s), file(f), func(fn), loc_hash(loc_key(f, fn)) {}
};

// Formatted logging (fmt style) with full category filtering.
// The log_fmtN wrappers capture the caller's filename in their constexpr constructor,
// so category blacklist AND whitelist work for all fmt-style log calls.
// Fast path: should_log_loc() = level + O(1) cached file+func filter (~12 cycles).
//
// std::type_identity_t on the wrapper's parameters is what stops the compiler from deducing
// T0..Tn from the FORMAT STRING; they are deduced from the arguments alone, and the wrapper is
// then built from the literal with exactly those types. Without it the first parameter would
// fight the rest for the deduction and every call with a plain literal would fail.
//
// The zero-argument case is NOT here: log::info("plain text") binds to the non-template
// const std::string& overload above. That asymmetry — non-template for zero arguments,
// template for one and more — IS the resolution mechanism, and it is why 4819 measured
// call sites keep working untouched.

template<typename T0>
inline void info(log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_loc(filter::LVL_INF, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger()) LogSystem::logger()->info(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void info(log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                 T0&& a0, T1&& a1) {
    if (!filter::should_log_loc(filter::LVL_INF, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void info(log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>> lf, T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_loc(filter::LVL_INF, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                  std::forward<T2>(a2));
}

template<typename T0, typename T1, typename T2, typename T3>
inline void info(log_fmt4<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3) {
    if (!filter::should_log_loc(filter::LVL_INF, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                  std::forward<T2>(a2), std::forward<T3>(a3));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4>
inline void info(log_fmt5<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>,
                          std::type_identity_t<T4>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4) {
    if (!filter::should_log_loc(filter::LVL_INF, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                  std::forward<T2>(a2), std::forward<T3>(a3),
                                  std::forward<T4>(a4));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
inline void info(log_fmt6<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>,
                          std::type_identity_t<T4>, std::type_identity_t<T5>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5) {
    if (!filter::should_log_loc(filter::LVL_INF, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                  std::forward<T2>(a2), std::forward<T3>(a3),
                                  std::forward<T4>(a4), std::forward<T5>(a5));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6>
inline void info(log_fmt7<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>,
                          std::type_identity_t<T4>, std::type_identity_t<T5>,
                          std::type_identity_t<T6>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6) {
    if (!filter::should_log_loc(filter::LVL_INF, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                  std::forward<T2>(a2), std::forward<T3>(a3),
                                  std::forward<T4>(a4), std::forward<T5>(a5),
                                  std::forward<T6>(a6));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7>
inline void info(log_fmt8<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>,
                          std::type_identity_t<T4>, std::type_identity_t<T5>,
                          std::type_identity_t<T6>, std::type_identity_t<T7>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7) {
    if (!filter::should_log_loc(filter::LVL_INF, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                  std::forward<T2>(a2), std::forward<T3>(a3),
                                  std::forward<T4>(a4), std::forward<T5>(a5),
                                  std::forward<T6>(a6), std::forward<T7>(a7));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7, typename T8>
inline void info(log_fmt9<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>,
                          std::type_identity_t<T4>, std::type_identity_t<T5>,
                          std::type_identity_t<T6>, std::type_identity_t<T7>,
                          std::type_identity_t<T8>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7,
                 T8&& a8) {
    if (!filter::should_log_loc(filter::LVL_INF, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                  std::forward<T2>(a2), std::forward<T3>(a3),
                                  std::forward<T4>(a4), std::forward<T5>(a5),
                                  std::forward<T6>(a6), std::forward<T7>(a7),
                                  std::forward<T8>(a8));
}

template<typename T0>
inline void warn(log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_loc(filter::LVL_WRN, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger()) LogSystem::logger()->warn(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void warn(log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                 T0&& a0, T1&& a1) {
    if (!filter::should_log_loc(filter::LVL_WRN, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void warn(log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>> lf, T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_loc(filter::LVL_WRN, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                  std::forward<T2>(a2));
}

template<typename T0, typename T1, typename T2, typename T3>
inline void warn(log_fmt4<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3) {
    if (!filter::should_log_loc(filter::LVL_WRN, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                  std::forward<T2>(a2), std::forward<T3>(a3));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4>
inline void warn(log_fmt5<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>,
                          std::type_identity_t<T4>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4) {
    if (!filter::should_log_loc(filter::LVL_WRN, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                  std::forward<T2>(a2), std::forward<T3>(a3),
                                  std::forward<T4>(a4));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
inline void warn(log_fmt6<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>,
                          std::type_identity_t<T4>, std::type_identity_t<T5>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5) {
    if (!filter::should_log_loc(filter::LVL_WRN, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                  std::forward<T2>(a2), std::forward<T3>(a3),
                                  std::forward<T4>(a4), std::forward<T5>(a5));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6>
inline void warn(log_fmt7<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>,
                          std::type_identity_t<T4>, std::type_identity_t<T5>,
                          std::type_identity_t<T6>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6) {
    if (!filter::should_log_loc(filter::LVL_WRN, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                  std::forward<T2>(a2), std::forward<T3>(a3),
                                  std::forward<T4>(a4), std::forward<T5>(a5),
                                  std::forward<T6>(a6));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7>
inline void warn(log_fmt8<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>,
                          std::type_identity_t<T4>, std::type_identity_t<T5>,
                          std::type_identity_t<T6>, std::type_identity_t<T7>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7) {
    if (!filter::should_log_loc(filter::LVL_WRN, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                  std::forward<T2>(a2), std::forward<T3>(a3),
                                  std::forward<T4>(a4), std::forward<T5>(a5),
                                  std::forward<T6>(a6), std::forward<T7>(a7));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7, typename T8>
inline void warn(log_fmt9<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>,
                          std::type_identity_t<T4>, std::type_identity_t<T5>,
                          std::type_identity_t<T6>, std::type_identity_t<T7>,
                          std::type_identity_t<T8>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7,
                 T8&& a8) {
    if (!filter::should_log_loc(filter::LVL_WRN, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                  std::forward<T2>(a2), std::forward<T3>(a3),
                                  std::forward<T4>(a4), std::forward<T5>(a5),
                                  std::forward<T6>(a6), std::forward<T7>(a7),
                                  std::forward<T8>(a8));
}

template<typename T0>
inline void error(log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_loc(filter::LVL_ERR, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger()) LogSystem::logger()->error(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void error(log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                  T0&& a0, T1&& a1) {
    if (!filter::should_log_loc(filter::LVL_ERR, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void error(log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>> lf, T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_loc(filter::LVL_ERR, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                   std::forward<T2>(a2));
}

template<typename T0, typename T1, typename T2, typename T3>
inline void error(log_fmt4<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3) {
    if (!filter::should_log_loc(filter::LVL_ERR, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                   std::forward<T2>(a2), std::forward<T3>(a3));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4>
inline void error(log_fmt5<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4) {
    if (!filter::should_log_loc(filter::LVL_ERR, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                   std::forward<T2>(a2), std::forward<T3>(a3),
                                   std::forward<T4>(a4));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
inline void error(log_fmt6<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5) {
    if (!filter::should_log_loc(filter::LVL_ERR, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                   std::forward<T2>(a2), std::forward<T3>(a3),
                                   std::forward<T4>(a4), std::forward<T5>(a5));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6>
inline void error(log_fmt7<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>,
                           std::type_identity_t<T6>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6) {
    if (!filter::should_log_loc(filter::LVL_ERR, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                   std::forward<T2>(a2), std::forward<T3>(a3),
                                   std::forward<T4>(a4), std::forward<T5>(a5),
                                   std::forward<T6>(a6));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7>
inline void error(log_fmt8<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>,
                           std::type_identity_t<T6>, std::type_identity_t<T7>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7) {
    if (!filter::should_log_loc(filter::LVL_ERR, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                   std::forward<T2>(a2), std::forward<T3>(a3),
                                   std::forward<T4>(a4), std::forward<T5>(a5),
                                   std::forward<T6>(a6), std::forward<T7>(a7));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7, typename T8>
inline void error(log_fmt9<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>,
                           std::type_identity_t<T6>, std::type_identity_t<T7>,
                           std::type_identity_t<T8>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7,
                  T8&& a8) {
    if (!filter::should_log_loc(filter::LVL_ERR, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                   std::forward<T2>(a2), std::forward<T3>(a3),
                                   std::forward<T4>(a4), std::forward<T5>(a5),
                                   std::forward<T6>(a6), std::forward<T7>(a7),
                                   std::forward<T8>(a8));
}

template<typename T0>
inline void debug(log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_loc(filter::LVL_DBG, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger()) LogSystem::logger()->debug(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void debug(log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                  T0&& a0, T1&& a1) {
    if (!filter::should_log_loc(filter::LVL_DBG, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void debug(log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>> lf, T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_loc(filter::LVL_DBG, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                   std::forward<T2>(a2));
}

template<typename T0, typename T1, typename T2, typename T3>
inline void debug(log_fmt4<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3) {
    if (!filter::should_log_loc(filter::LVL_DBG, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                   std::forward<T2>(a2), std::forward<T3>(a3));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4>
inline void debug(log_fmt5<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4) {
    if (!filter::should_log_loc(filter::LVL_DBG, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                   std::forward<T2>(a2), std::forward<T3>(a3),
                                   std::forward<T4>(a4));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
inline void debug(log_fmt6<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5) {
    if (!filter::should_log_loc(filter::LVL_DBG, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                   std::forward<T2>(a2), std::forward<T3>(a3),
                                   std::forward<T4>(a4), std::forward<T5>(a5));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6>
inline void debug(log_fmt7<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>,
                           std::type_identity_t<T6>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6) {
    if (!filter::should_log_loc(filter::LVL_DBG, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                   std::forward<T2>(a2), std::forward<T3>(a3),
                                   std::forward<T4>(a4), std::forward<T5>(a5),
                                   std::forward<T6>(a6));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7>
inline void debug(log_fmt8<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>,
                           std::type_identity_t<T6>, std::type_identity_t<T7>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7) {
    if (!filter::should_log_loc(filter::LVL_DBG, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                   std::forward<T2>(a2), std::forward<T3>(a3),
                                   std::forward<T4>(a4), std::forward<T5>(a5),
                                   std::forward<T6>(a6), std::forward<T7>(a7));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7, typename T8>
inline void debug(log_fmt9<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>,
                           std::type_identity_t<T6>, std::type_identity_t<T7>,
                           std::type_identity_t<T8>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7,
                  T8&& a8) {
    if (!filter::should_log_loc(filter::LVL_DBG, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                   std::forward<T2>(a2), std::forward<T3>(a3),
                                   std::forward<T4>(a4), std::forward<T5>(a5),
                                   std::forward<T6>(a6), std::forward<T7>(a7),
                                   std::forward<T8>(a8));
}

template<typename T0>
inline void trace(log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_loc(filter::LVL_TRC, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger()) LogSystem::logger()->trace(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void trace(log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                  T0&& a0, T1&& a1) {
    if (!filter::should_log_loc(filter::LVL_TRC, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->trace(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void trace(log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>> lf, T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_loc(filter::LVL_TRC, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->trace(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                   std::forward<T2>(a2));
}

template<typename T0, typename T1, typename T2, typename T3>
inline void trace(log_fmt4<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3) {
    if (!filter::should_log_loc(filter::LVL_TRC, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->trace(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                   std::forward<T2>(a2), std::forward<T3>(a3));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4>
inline void trace(log_fmt5<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4) {
    if (!filter::should_log_loc(filter::LVL_TRC, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->trace(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                   std::forward<T2>(a2), std::forward<T3>(a3),
                                   std::forward<T4>(a4));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
inline void trace(log_fmt6<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5) {
    if (!filter::should_log_loc(filter::LVL_TRC, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->trace(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                   std::forward<T2>(a2), std::forward<T3>(a3),
                                   std::forward<T4>(a4), std::forward<T5>(a5));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6>
inline void trace(log_fmt7<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>,
                           std::type_identity_t<T6>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6) {
    if (!filter::should_log_loc(filter::LVL_TRC, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->trace(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                   std::forward<T2>(a2), std::forward<T3>(a3),
                                   std::forward<T4>(a4), std::forward<T5>(a5),
                                   std::forward<T6>(a6));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7>
inline void trace(log_fmt8<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>,
                           std::type_identity_t<T6>, std::type_identity_t<T7>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7) {
    if (!filter::should_log_loc(filter::LVL_TRC, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->trace(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                   std::forward<T2>(a2), std::forward<T3>(a3),
                                   std::forward<T4>(a4), std::forward<T5>(a5),
                                   std::forward<T6>(a6), std::forward<T7>(a7));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7, typename T8>
inline void trace(log_fmt9<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>,
                           std::type_identity_t<T6>, std::type_identity_t<T7>,
                           std::type_identity_t<T8>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7,
                  T8&& a8) {
    if (!filter::should_log_loc(filter::LVL_TRC, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->trace(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                   std::forward<T2>(a2), std::forward<T3>(a3),
                                   std::forward<T4>(a4), std::forward<T5>(a5),
                                   std::forward<T6>(a6), std::forward<T7>(a7),
                                   std::forward<T8>(a8));
}

template<typename T0>
inline void critical(log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_loc(filter::LVL_CRT, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger()) LogSystem::logger()->critical(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void critical(log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                     T0&& a0, T1&& a1) {
    if (!filter::should_log_loc(filter::LVL_CRT, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->critical(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void critical(log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                              std::type_identity_t<T2>> lf, T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_loc(filter::LVL_CRT, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->critical(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                      std::forward<T2>(a2));
}

template<typename T0, typename T1, typename T2, typename T3>
inline void critical(log_fmt4<std::type_identity_t<T0>, std::type_identity_t<T1>,
                              std::type_identity_t<T2>, std::type_identity_t<T3>> lf,
                     T0&& a0, T1&& a1, T2&& a2, T3&& a3) {
    if (!filter::should_log_loc(filter::LVL_CRT, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->critical(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                      std::forward<T2>(a2), std::forward<T3>(a3));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4>
inline void critical(log_fmt5<std::type_identity_t<T0>, std::type_identity_t<T1>,
                              std::type_identity_t<T2>, std::type_identity_t<T3>,
                              std::type_identity_t<T4>> lf,
                     T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4) {
    if (!filter::should_log_loc(filter::LVL_CRT, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->critical(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                      std::forward<T2>(a2), std::forward<T3>(a3),
                                      std::forward<T4>(a4));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
inline void critical(log_fmt6<std::type_identity_t<T0>, std::type_identity_t<T1>,
                              std::type_identity_t<T2>, std::type_identity_t<T3>,
                              std::type_identity_t<T4>, std::type_identity_t<T5>> lf,
                     T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5) {
    if (!filter::should_log_loc(filter::LVL_CRT, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->critical(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                      std::forward<T2>(a2), std::forward<T3>(a3),
                                      std::forward<T4>(a4), std::forward<T5>(a5));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6>
inline void critical(log_fmt7<std::type_identity_t<T0>, std::type_identity_t<T1>,
                              std::type_identity_t<T2>, std::type_identity_t<T3>,
                              std::type_identity_t<T4>, std::type_identity_t<T5>,
                              std::type_identity_t<T6>> lf,
                     T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6) {
    if (!filter::should_log_loc(filter::LVL_CRT, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->critical(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                      std::forward<T2>(a2), std::forward<T3>(a3),
                                      std::forward<T4>(a4), std::forward<T5>(a5),
                                      std::forward<T6>(a6));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7>
inline void critical(log_fmt8<std::type_identity_t<T0>, std::type_identity_t<T1>,
                              std::type_identity_t<T2>, std::type_identity_t<T3>,
                              std::type_identity_t<T4>, std::type_identity_t<T5>,
                              std::type_identity_t<T6>, std::type_identity_t<T7>> lf,
                     T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7) {
    if (!filter::should_log_loc(filter::LVL_CRT, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->critical(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                      std::forward<T2>(a2), std::forward<T3>(a3),
                                      std::forward<T4>(a4), std::forward<T5>(a5),
                                      std::forward<T6>(a6), std::forward<T7>(a7));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7, typename T8>
inline void critical(log_fmt9<std::type_identity_t<T0>, std::type_identity_t<T1>,
                              std::type_identity_t<T2>, std::type_identity_t<T3>,
                              std::type_identity_t<T4>, std::type_identity_t<T5>,
                              std::type_identity_t<T6>, std::type_identity_t<T7>,
                              std::type_identity_t<T8>> lf,
                     T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7,
                     T8&& a8) {
    if (!filter::should_log_loc(filter::LVL_CRT, lf.file, lf.func, lf.loc_hash)) return;
    if (LogSystem::logger())
        LogSystem::logger()->critical(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                      std::forward<T2>(a2), std::forward<T3>(a3),
                                      std::forward<T4>(a4), std::forward<T5>(a5),
                                      std::forward<T6>(a6), std::forward<T7>(a7),
                                      std::forward<T8>(a8));
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
//
// ARITY THREE HERE, NOT NINE — and the difference is measured, not guessed:
// GEMESSEN 2026-08-22, the client_* family has ZERO callers in the whole tree (both with and
// without the log:: prefix, cross-checked against log::info which finds 464 files). There is no
// measured arity to cover. GESETZT is three as a reserve, the same shape ring_buffer.hpp chose
// for the same reason. Needing a fourth costs one overload written after this pattern, and the
// COMPILER names the call site — a missing overload cannot fail silently at run time.
// The six main levels above carry nine because there the demand is measured (debug reaches 9).

template<typename T0>
inline void client_info(uint64_t client_bit, log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_client_loc(filter::LVL_INF, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->info(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void client_info(uint64_t client_bit,
                        log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                        T0&& a0, T1&& a1) {
    if (!filter::should_log_client_loc(filter::LVL_INF, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void client_info(uint64_t client_bit,
                        log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                                 std::type_identity_t<T2>> lf,
                        T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_client_loc(filter::LVL_INF, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                         std::forward<T2>(a2));
}

template<typename T0>
inline void client_warn(uint64_t client_bit, log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_client_loc(filter::LVL_WRN, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->warn(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void client_warn(uint64_t client_bit,
                        log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                        T0&& a0, T1&& a1) {
    if (!filter::should_log_client_loc(filter::LVL_WRN, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void client_warn(uint64_t client_bit,
                        log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                                 std::type_identity_t<T2>> lf,
                        T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_client_loc(filter::LVL_WRN, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                         std::forward<T2>(a2));
}

template<typename T0>
inline void client_error(uint64_t client_bit, log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_client_loc(filter::LVL_ERR, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->error(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void client_error(uint64_t client_bit,
                         log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                         T0&& a0, T1&& a1) {
    if (!filter::should_log_client_loc(filter::LVL_ERR, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void client_error(uint64_t client_bit,
                         log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                                  std::type_identity_t<T2>> lf,
                         T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_client_loc(filter::LVL_ERR, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                          std::forward<T2>(a2));
}

template<typename T0>
inline void client_debug(uint64_t client_bit, log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_client_loc(filter::LVL_DBG, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->debug(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void client_debug(uint64_t client_bit,
                         log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                         T0&& a0, T1&& a1) {
    if (!filter::should_log_client_loc(filter::LVL_DBG, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void client_debug(uint64_t client_bit,
                         log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                                  std::type_identity_t<T2>> lf,
                         T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_client_loc(filter::LVL_DBG, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                          std::forward<T2>(a2));
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

// The rtc_* family needs NO log_fmtN wrapper: it does not filter by call site, so there is
// nothing to capture with __builtin_FILE(). The format string is taken directly, which keeps
// fmt's compile-time check of the string against T0..Tn exactly as before.
// Arity three, same reason as client_* above: GEMESSEN zero callers tree-wide, GESETZT three
// as a reserve. A fourth is one overload, and the compiler names the site that needs it.

template<typename T0>
inline void rtc_info(spdlog::format_string_t<T0> fmt, T0&& a0) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(fmt, std::forward<T0>(a0));
        LogSystem::client_logger()->info("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1>
inline void rtc_info(spdlog::format_string_t<T0, T1> fmt, T0&& a0, T1&& a1) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(fmt, std::forward<T0>(a0), std::forward<T1>(a1));
        LogSystem::client_logger()->info("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1, typename T2>
inline void rtc_info(spdlog::format_string_t<T0, T1, T2> fmt, T0&& a0, T1&& a1, T2&& a2) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                               std::forward<T2>(a2));
        LogSystem::client_logger()->info("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0>
inline void rtc_warn(spdlog::format_string_t<T0> fmt, T0&& a0) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(fmt, std::forward<T0>(a0));
        LogSystem::client_logger()->warn("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1>
inline void rtc_warn(spdlog::format_string_t<T0, T1> fmt, T0&& a0, T1&& a1) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(fmt, std::forward<T0>(a0), std::forward<T1>(a1));
        LogSystem::client_logger()->warn("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1, typename T2>
inline void rtc_warn(spdlog::format_string_t<T0, T1, T2> fmt, T0&& a0, T1&& a1, T2&& a2) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                               std::forward<T2>(a2));
        LogSystem::client_logger()->warn("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0>
inline void rtc_error(spdlog::format_string_t<T0> fmt, T0&& a0) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(fmt, std::forward<T0>(a0));
        LogSystem::client_logger()->error("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1>
inline void rtc_error(spdlog::format_string_t<T0, T1> fmt, T0&& a0, T1&& a1) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(fmt, std::forward<T0>(a0), std::forward<T1>(a1));
        LogSystem::client_logger()->error("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1, typename T2>
inline void rtc_error(spdlog::format_string_t<T0, T1, T2> fmt, T0&& a0, T1&& a1, T2&& a2) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                               std::forward<T2>(a2));
        LogSystem::client_logger()->error("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0>
inline void rtc_debug(spdlog::format_string_t<T0> fmt, T0&& a0) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(fmt, std::forward<T0>(a0));
        LogSystem::client_logger()->debug("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1>
inline void rtc_debug(spdlog::format_string_t<T0, T1> fmt, T0&& a0, T1&& a1) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(fmt, std::forward<T0>(a0), std::forward<T1>(a1));
        LogSystem::client_logger()->debug("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1, typename T2>
inline void rtc_debug(spdlog::format_string_t<T0, T1, T2> fmt, T0&& a0, T1&& a1, T2&& a2) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                               std::forward<T2>(a2));
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

// ============================================================================
// ERR::CAT und WRN::CAT kommen aus dem GENERATOR, nicht mehr von Hand aus dieser Datei.
// ============================================================================
//
// Konstanten, Tabellen und Zaehler sind durch static_assert aneinander gebunden. Standen sie
// von Hand hier, war JEDE neue Kategorie ein Schrieb an einer Datei mit rund 2000 Lesern, und
// jeder Zwischenstand zwischen zwei Edits ein Fenster, in dem der ganze Baum unbaubar ist.
// Gemessen 2026-08-23: ein fremder Bau starb bei 468 von 887 Zielen in genau so einem Fenster.
//
// Aendern will man eine Kategorie ab jetzt in core/ase-log/data/log_categories.json; den Rest
// schreibt core/ase-log/scripts/gen_log_cat.py. Ein Hilfetext oder ein Suffix kostet danach
// EINE neu uebersetzte Datei (log_err_cat_gen.cpp) statt des Baums. Eine NEUE Kategorie aendert
// den generierten Header und damit doch den Baum — das ist unvermeidbar, solange Aufrufer sie
// beim Namen nennen, und es ist der seltene Fall.
//
// DER INCLUDE STEHT OBEN BEI DEN UEBRIGEN, NICHT HIER. Der erste Versuch setzte ihn an genau
// diese Stelle, um den Schnitt auf einen Ort zu halten — das ist FALSCH und wurde sofort
// gemessen: an dieser Stelle steht die Datei bereits INNERHALB von `namespace ase::log`, ein
// Include dort verschachtelt den fremden Namensraum zu `ase::log::ase::log::ERR`, und clangd
// meldete es ueber acht Uebersetzungseinheiten als „No member named 'ERR' in namespace
// 'ase::log'". Ein Include gehoert vor die erste Namensraum-Oeffnung, ohne Ausnahme.


// Die Kategorie-Texte (Namen, short, hint, tail, Suffixe) und ihre Getter stehen in
// core/ase-log/src/log_err_cat_gen.cpp; die Deklarationen kommen aus log_err_cat_gen.hpp, der
// oben eingebunden wird. Jede kategorisierte Meldung entsteht hier als ZWEIZEILIGER Datensatz:
// die Kopfzeile endet mit dem SHORT (Essenz, Laengenobergrenze aus log_display.json, bricht
// nie um), und der TAIL haengt den hint als eigene, nur eingerueckte physische Zeile an. Das
// Paar ist EIN Log-Datensatz (ein Sink-Write): ein hint steht nie ohne seinen Kopf direkt
// darueber, deshalb traegt die hint-Zeile keine eigene Marke — sie waere die Wiederholung
// dessen, was der Kopf schon sagt. Jeder Kanal — Datei, tail, Konsole — traegt denselben
// Inhalt; eine Kategorie ohne hint (UNKNOWN) bleibt einzeilig.
//
// Der Suffix bleibt an der KATEGORIE und nicht an der Stelligkeit: HUB_GLOBAL_MISSING druckt
// weiterhin " (GLOBAL)", CONFIG_MISSING nichts.

// Categorized error logging for per-entity values
inline void error(uint32_t cat, const char* system, uint32_t owner, const char* value_id) {
    if (LogSystem::logger()) {
        LogSystem::logger()->error("[{}] {}: owner={}, value_id='{}'. {}{}",
            system, detail::get_cat_name(cat), owner, value_id, detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

// Categorized error logging for values WITHOUT an owner.
//
// Der Zusatz hinter der value_id kommt aus detail::get_cat_suffix() und damit aus der
// KATEGORIE, nicht aus dieser Ueberladung. HUB_GLOBAL_MISSING druckt weiterhin „ (GLOBAL)",
// zeichengleich zu vorher; CONFIG_MISSING druckt keinen Zusatz, weil eine Umgebungsvariable
// keine GLOBAL-Zeile des Hubs ist.
inline void error(uint32_t cat, const char* system, const char* value_id) {
    if (LogSystem::logger()) {
        LogSystem::logger()->error("[{}] {}: value_id='{}'{}. {}{}",
            system, detail::get_cat_name(cat), value_id, detail::get_cat_suffix(cat),
            detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

// Categorized error logging for component/entity issues (no value_id)
inline void error(uint32_t cat, const char* system, uint32_t entity) {
    if (LogSystem::logger()) {
        LogSystem::logger()->error("[{}] {}: entity={}. {}{}",
            system, detail::get_cat_name(cat), entity, detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

// ============================================================================
// Warning Categories (WRN::CAT) - DRY warning messages with auto-generated help
// ============================================================================
// Usage: log::warn(log::WRN::CAT::VALUE_OUT_OF_RANGE, "SystemName", owner, "VALUE_ID", value, min, max);
// Output: [WRN] [SystemName] VALUE_OUT_OF_RANGE: owner=123, value_id='VALUE_ID', value=1.5, range=[0.0,1.0]. value left its valid range
//             Check the value against the range it left. To clamp, reject or keep it is the caller's decision

// WRN::CAT steht ebenfalls in log_err_cat_gen.hpp, und beide Ebenen tragen denselben Bestand:
// jede Kategorie existiert als Fehler UND als Warnung, ausser wo die Abgrenzung sie
// ausschliesst (der Grund steht dann unter `_omitted` in log_categories.json). Ohne diese
// Spiegelung ist die Regel „die Ebene folgt der ZEILE, nie der Kategorie" unerfuellbar: eine
// Stelle, deren Kategorie es nur auf der anderen Ebene gibt, hat drei Auswege, und alle drei
// sind falsch — Ebene wechseln, Kategorie verbiegen, oder als freier String stehen bleiben.
//
// EINE KREUZUNG IST NICHT MEHR MOEGLICH, und zwar strukturell statt durch Nummernabstand.
// Der Wert einer Kategorie ist der FNV-1a-Hash von „<NAMENSRAUM>_<NAME>", also
// „WRN_HOST_OP_FAILED" gegen „ERR_HOST_OP_FAILED" — zwei verschiedene Zahlen. Der frueher
// noetige Fuellbereich (WRN 4..15) und der eigene Nummernbereich ab 16 sind damit
// gegenstandslos und ersatzlos entfallen.
//
// Und der Namensraum MUSS im gehashten String stehen: er trennt die Konstanten dort, wo sie
// GESCHRIEBEN werden, aber nichts trennt sie dort, wo der Wert NACHGESCHLAGEN wird. Waere nur
// der nackte Name gehasst, haetten ERR::CAT::HOST_OP_FAILED und WRN::CAT::HOST_OP_FAILED
// denselben Wert, und die WRN-Konstante an log::error druckte den ERR-Hilfetext.

// Die WRN-Nachschlagefunktionen stehen ebenfalls in log_err_cat_gen.cpp. Ein Hash, den die
// SSOT nicht kennt, faellt dort NICHT auf einen Eintrag zurueck, sondern bekommt eine LAUTE
// Marke: ein plausibler falscher Hilfetext ist teurer als ein sichtbar unbekannter, weil der
// Leser eine gueltige Kategorie sieht und nicht nachprueft.
//
// Und die Hilfetexte sagen dort weiterhin NICHTS ueber den Kontrollfluss zu. Der frueher hier
// stehende Satz („Fix: Value will be clamped to valid range") behauptete, was NACH der Meldung
// geschieht — das kann eine Kategorie nicht wissen: sie beschreibt den ZUSTAND eines Wertes,
// nicht die Reaktion des Aufrufers. Eine Bereichspruefung, die NICHT klemmt, fiel dadurch
// faelschlich durch.

// Categorized warning for out-of-range values
inline void warn(uint32_t cat, const char* system, uint32_t owner, const char* value_id, float value, float min, float max) {
    if (LogSystem::logger()) {
        LogSystem::logger()->warn("[{}] {}: owner={}, value_id='{}', value={}, range=[{},{}]. {}{}",
            system, detail::get_wrn_cat_name(cat), owner, value_id, value, min, max, detail::get_wrn_cat_short(cat), detail::get_wrn_cat_tail(cat));
    }
}

// Bereichsform OHNE Besitzer — fuer Stellen, an denen es keine Entity gibt: ein Rahmenfeld
// von der Leitung, eine Puffergrenze, ein Wert vor jeder Entity. Die owner-behaftete Form
// darueber verlangt einen Besitzer, und einen zu ERFINDEN ist teurer als der freie String, den
// diese Form ersetzt: `0` ist eine gueltige entt-Id, `types::InvalidEntityId` sagt „unbekannt"
// statt „gibt es nicht".
//
// DER SUFFIX IST ASYMMETRISCH, UND DAS IST ABSICHT — nicht zur Symmetrie „korrigieren": die
// ERR-Haelfte druckt `detail::get_cat_suffix(cat)`, die WRN-Haelfte nicht, und ein
// `get_wrn_cat_suffix` gibt es nicht. Der Zusatz ist das „ (GLOBAL)" von HUB_GLOBAL_MISSING,
// und GLOBAL heisst „kein Besitzer" — er gehoert deshalb genau dorthin, wo kein owner-Feld steht.
//
// EIN SOLLWERT wird hier als `min == max` gemeldet, nicht mit einer eigenen Form:
// `(value_id, ist, soll)` waere signaturgleich mit der Aggregatform und damit eine Redefinition
// — dieselbe Wand, die der Absatz am Ende dieses Namensraums fuer „zwei unabhaengige Zahlen ohne
// Besitzer" mit einer g++-Messung belegt. `range=[64,64]` ist wahr und lesbar: ein Bereich mit
// einem Element.
//
// EINE EINSEITIGE Schranke gehoert NICHT hierher, weder mit `max = 0` noch mit `FLT_MAX`; sie
// nimmt die Form ohne Grenzen. Drei Stellen haben das unabhaengig entschieden und begruendet:
//   replica_cell_rcv_sys.cpp  „eine erfundene Obergrenze waere eine Falschaussage in der
//                              strukturierten Form"
//   rsn_trg_reg_sys.cpp       „die Obergrenze gibt es nicht (jede Nummer >= 1 ist gueltig) —
//                              deshalb die Wertform und nicht die Bereichsform"
//   serial_jsn_sys.cpp        „die Bereichsgrenzen sind an dieser Stelle nicht als Zahl
//                              greifbar, deshalb die Wertform ohne min/max statt einer
//                              erfundenen Grenze"
// Diese drei Saetze stehen woertlich hier, damit der Leser die AUSSAGE pruefen kann, ohne sie
// glauben zu muessen. Ein Verweis, den man aufschlagen MUSS, wird nicht aufgeschlagen.
inline void error(uint32_t cat, const char* system, const char* value_id,
                  float value, float min, float max) {
    if (LogSystem::logger()) {
        LogSystem::logger()->error("[{}] {}: value_id='{}', value={}, range=[{},{}]{}. {}{}",
            system, detail::get_cat_name(cat), value_id, value, min, max,
            detail::get_cat_suffix(cat), detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

inline void warn(uint32_t cat, const char* system, const char* value_id,
                 float value, float min, float max) {
    if (LogSystem::logger()) {
        LogSystem::logger()->warn("[{}] {}: value_id='{}', value={}, range=[{},{}]. {}{}",
            system, detail::get_wrn_cat_name(cat), value_id, value, min, max,
            detail::get_wrn_cat_short(cat), detail::get_wrn_cat_tail(cat));
    }
}

// ERR-Haelfte der owner-behafteten Bereichsform. Bis 2026-08-23 gab es sie nur als `warn`, und
// eine Stelle, die einen Verbrauch gegen einen Deckel meldete, verlor dadurch ZWEI Dinge: die
// Untergrenze („X von Y" statt „X in [0,Y]") UND den Besitzer, weil die Aggregatform keinen
// owner-Slot hat. Der Nachbarzweig derselben Funktion behielt den Besitzer — der Unterschied
// zwischen beiden war nicht die Semantik, sondern die Verfuegbarkeit einer Form.
inline void error(uint32_t cat, const char* system, uint32_t owner, const char* value_id,
                  float value, float min, float max) {
    if (LogSystem::logger()) {
        LogSystem::logger()->error("[{}] {}: owner={}, value_id='{}', value={}, range=[{},{}]. {}{}",
            system, detail::get_cat_name(cat), owner, value_id, value, min, max,
            detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

// Categorized warning for negative values
inline void warn(uint32_t cat, const char* system, uint32_t owner, const char* value_id, float value) {
    if (LogSystem::logger()) {
        LogSystem::logger()->warn("[{}] {}: owner={}, value_id='{}', value={}. {}{}",
            system, detail::get_wrn_cat_name(cat), owner, value_id, value, detail::get_wrn_cat_short(cat), detail::get_wrn_cat_tail(cat));
    }
}

// Categorized warning WITHOUT a numeric value.
//
// Die beiden Ueberladungen darueber verlangen einen `float value` - fuer eine Warnung, deren
// Gegenstand KEINE Zahl ist, gab es bis hierher keine kategorisierte Form, und die Faelle
// landeten deshalb als freier String im Log. Gemessen an ase-pl-webserver: ein fehlender
// HTTP-Header und ein fehlgeschlagener HMAC haben nichts, was man in `value` einsetzen
// koennte, ohne etwas zu erfinden.
//
// KEIN `owner`, und das ist Absicht: der Ausloeser ist ein fremder Aufrufer, kein Hub-Owner.
// Ein owner-Feld an dieser Stelle waere eine Einladung, eine Entity-ID zu erfinden.
inline void warn(uint32_t cat, const char* system, const char* value_id) {
    if (LogSystem::logger()) {
        LogSystem::logger()->warn("[{}] {}: value_id='{}'. {}{}",
            system, detail::get_wrn_cat_name(cat), value_id, detail::get_wrn_cat_short(cat), detail::get_wrn_cat_tail(cat));
    }
}

// Categorized warning for an AGGREGATE over a set - a count against its population.
//
// Die beiden wert-tragenden Ueberladungen oben verlangen `uint32_t owner`. Eine Meldung, die
// ueber eine MENGE zaehlt, hat keinen: sie gehoert keiner Entity, sondern dem Durchlauf. Bis
// hierher blieben solche Zeilen als freier String stehen, weil jede kategorisierte Form einen
// Owner erfunden haette - und eine erfundene Entity-ID im Log ist teurer als der freie String,
// den sie ersetzt.
//
// ZWEI Zahlen und nicht eine, weil die AUSSAGE das Verhaeltnis ist: „3 von 512" und „3 von 4"
// sind derselbe Zaehlstand und zwei verschiedene Lagen. Bewusst NICHT die 7-Argument-Form mit
// `min`/`max` missbraucht: dort ist der zweite Wert eine BEREICHSGRENZE, hier eine
// GRUNDGESAMTHEIT. Wer die Population ins max-Feld schreibt, behauptet eine Grenze, wo eine
// Anzahl steht - eine Falschaussage in der strukturierten Form, und die wird von Werkzeugen
// gelesen und geglaubt, waehrend ein freier String nur von Menschen gelesen wird.
inline void warn(uint32_t cat, const char* system, const char* value_id, float count, float population) {
    if (LogSystem::logger()) {
        LogSystem::logger()->warn("[{}] {}: value_id='{}', count={} of {}. {}{}",
            system, detail::get_wrn_cat_name(cat), value_id, count, population,
            detail::get_wrn_cat_short(cat), detail::get_wrn_cat_tail(cat));
    }
}

// Dieselbe Aggregatform auf FEHLEREBENE. Die Begruendung oben gilt unveraendert - hier steht
// nur, warum es sie zweimal geben MUSS.
//
// Die Form existierte bis hierher nur unter WRN, und die passende Kategorie fuer den haeufigsten
// Fall (eine Bahn ist voll, N von M verworfen) nur unter ERR: zwei Haelften an entgegengesetzten
// Enden. Eine Stelle dazwischen hatte drei Wege, und alle drei waren verboten - die Ebene
// wechseln (Code an den Detektor anpassen), die Kategorie verbiegen (Falschaussage) oder als
// freier String stehen bleiben. Belegt an terrain_dlt_pub_sys.cpp und an
// capacity_orch_node_stat_sys.cpp (ase-pl-capacity-orch), wo der Dateikommentar die zwei Zahlen
// ausdruecklich als die Aussage benennt ("Names the REAL event ... Never silent").
//
// Der Unterschied zur WRN-Haelfte ist NICHT die Schwere, sondern was danach passiert: hier
// endet der Vorgang, dort laeuft er weiter. Wer das verwechselt, nimmt seine Zeile aus dem
// Strom, in dem sie gesucht wird.
inline void error(uint32_t cat, const char* system, const char* value_id, float count, float population) {
    if (LogSystem::logger()) {
        LogSystem::logger()->error("[{}] {}: value_id='{}', count={} of {}{}. {}{}",
            system, detail::get_cat_name(cat), value_id, count, population,
            detail::get_cat_suffix(cat), detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

// ============================================================================
// Ergaenzende Formen — ADDITIONEN. Keine bestehende Signatur ist angefasst.
// ============================================================================
//
// Jede traegt ihre ABGRENZUNG, nicht nur ihren Zweck: der Text hier ist das, was ein Aufrufer
// vor der Formwahl liest, und eine Form ohne Abgrenzung wird zum Sammelbecken.

// EIN Wert, KEIN Besitzer. Die haeufigste Luecke im Baum.
//
// Die ERR-Familie hatte bis hier DREI Formen und KEINE mit einem Wertfeld, waehrend die
// WRN-Familie drei von vier mit Wert kennt. Jede kategorisierte Fehlerzeile, die eine Zahl
// nennt, hatte deshalb nur zwei Auswege: die Zahl verlieren oder auf `warn` heruntergehen —
// und der zweite nimmt die Zeile aus dem Fehlerstrom, in dem sie gesucht wird.
//
// Gegen die 5-Argument-Form mit `owner` abgegrenzt: die hier ist fuer Stellen OHNE Entity —
// ein Dekodierhelfer, der vor jeder Entity laeuft, eine Puffergrenze, ein HTTP-Aufruf. Einen
// Owner zu erfinden waere teurer als der freie String, den die Form ersetzt: `0` ist eine
// gueltige entt-Id, die Zeile zeigte also auf eine fremde Entity.
inline void error(uint32_t cat, const char* system, const char* value_id, float value) {
    if (LogSystem::logger()) {
        LogSystem::logger()->error("[{}] {}: value_id='{}', value={}{}. {}{}",
            system, detail::get_cat_name(cat), value_id, value,
            detail::get_cat_suffix(cat), detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

inline void warn(uint32_t cat, const char* system, const char* value_id, float value) {
    if (LogSystem::logger()) {
        LogSystem::logger()->warn("[{}] {}: value_id='{}', value={}. {}{}",
            system, detail::get_wrn_cat_name(cat), value_id, value,
            detail::get_wrn_cat_short(cat), detail::get_wrn_cat_tail(cat));
    }
}

// EIN Wert NEBEN dem Besitzer, auf Fehlerebene — das Gegenstueck zur WRN-Form mit owner.
inline void error(uint32_t cat, const char* system, uint32_t owner, const char* value_id,
                  float value) {
    if (LogSystem::logger()) {
        LogSystem::logger()->error("[{}] {}: owner={}, value_id='{}', value={}. {}{}",
            system, detail::get_cat_name(cat), owner, value_id, value,
            detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

// Besitzer OHNE Wert auf WRN — die Spiegelluecke zur gleichnamigen ERR-Form.
// Fuer Warnungen, bei denen die Entity die Aussage traegt und es gar keinen Wert gibt:
// „Ausgangsschlange dieses Besitzers ist voll", „Zeile dieses Besitzers verworfen".
inline void warn(uint32_t cat, const char* system, uint32_t owner, const char* value_id) {
    if (LogSystem::logger()) {
        LogSystem::logger()->warn("[{}] {}: owner={}, value_id='{}'. {}{}",
            system, detail::get_wrn_cat_name(cat), owner, value_id,
            detail::get_wrn_cat_short(cat), detail::get_wrn_cat_tail(cat));
    }
}

// value_id + FREIER DETAILTEXT.
//
// `value_id` ist der FILTERSCHLUESSEL und bleibt je Aufrufstelle konstant; `detail` nimmt, was
// dort nicht hineinpasst — ein `dlerror()`, ein `strerror(errno)`, ein HTTP-Status, ein
// `e.what()`, auch drei Bezeichner in einem Satz. Die Zeile ist danach KATEGORISIERT, und das
// ist der ganze Unterschied zum freien String.
//
// Die Grenze: `detail` ist KEIN zweiter Filterschluessel. Was gefiltert werden soll, gehoert in
// `value_id`; ein wechselnder Wert dort macht jede Zeile zu einem eigenen Schluessel und
// zerstoert genau die Zaehlbarkeit, für die es die Kategorie gibt.
inline void error(uint32_t cat, const char* system, const char* value_id, const char* detail) {
    if (LogSystem::logger()) {
        LogSystem::logger()->error("[{}] {}: value_id='{}'{}, detail='{}'. {}{}",
            system, detail::get_cat_name(cat), value_id, detail::get_cat_suffix(cat),
            detail, detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

inline void warn(uint32_t cat, const char* system, const char* value_id, const char* detail) {
    if (LogSystem::logger()) {
        LogSystem::logger()->warn("[{}] {}: value_id='{}', detail='{}'. {}{}",
            system, detail::get_wrn_cat_name(cat), value_id, detail,
            detail::get_wrn_cat_short(cat), detail::get_wrn_cat_tail(cat));
    }
}

// Dieselbe Sache MIT Besitzer: wo die Zeile einer Entity gehoert UND einen freien Grund traegt.
// Der haeufige Fall ist eine abgewiesene Anfrage: die Entity sagt WELCHE, `detail` sagt WARUM.
inline void error(uint32_t cat, const char* system, uint32_t owner, const char* value_id,
                  const char* detail) {
    if (LogSystem::logger()) {
        LogSystem::logger()->error("[{}] {}: owner={}, value_id='{}', detail='{}'. {}{}",
            system, detail::get_cat_name(cat), owner, value_id, detail,
            detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

inline void warn(uint32_t cat, const char* system, uint32_t owner, const char* value_id,
                 const char* detail) {
    if (LogSystem::logger()) {
        LogSystem::logger()->warn("[{}] {}: owner={}, value_id='{}', detail='{}'. {}{}",
            system, detail::get_wrn_cat_name(cat), owner, value_id, detail,
            detail::get_wrn_cat_short(cat), detail::get_wrn_cat_tail(cat));
    }
}

// ZWEI identifizierende Zahlen.
//
// Beide benennen etwas, keine ist aus der anderen ableitbar: welche Anfrage verlorenging UND
// gegen welche Sammlung sie lief. Eine davon in `owner` zu stecken und die andere fallenzulassen
// waere kein Formatverlust, sondern ein INFORMATIONSverlust — danach ist die Zeile nicht mehr
// zuzuordnen.
//
// Die Grenze: eine Zahl, die je Aufrufstelle KONSTANT ist (ein Rahmentyp, eine feste Obergrenze),
// traegt in einer Logzeile keine Information und gehoert nicht hierher. Zwei IDENTIFIZIERENDE
// Zahlen sind etwas anderes als zwei Zahlen.
//
// Traegt zugleich eine 64-Bit-Kennung als hi/lo: beide Haelften in DERSELBEN Zeile, `value_id`
// mit dem Suffix `_hi`/`_lo` — sonst uebersetzt eine implizite Verengung auf uint32 still und
// das Log druckt einen abgeschnittenen Schluessel.
inline void error(uint32_t cat, const char* system, uint32_t owner, uint32_t subject,
                  const char* value_id) {
    if (LogSystem::logger()) {
        LogSystem::logger()->error("[{}] {}: owner={}, subject={}, value_id='{}'. {}{}",
            system, detail::get_cat_name(cat), owner, subject, value_id,
            detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

inline void warn(uint32_t cat, const char* system, uint32_t owner, uint32_t subject,
                 const char* value_id) {
    if (LogSystem::logger()) {
        LogSystem::logger()->warn("[{}] {}: owner={}, subject={}, value_id='{}'. {}{}",
            system, detail::get_wrn_cat_name(cat), owner, subject, value_id,
            detail::get_wrn_cat_short(cat), detail::get_wrn_cat_tail(cat));
    }
}

// KEINE Form `warn(cat, system, value_id, float a, float b)` — sie waere signaturgleich mit der
// Aggregatform oben und damit eine Redefinition, nicht eine Ueberladung (gemessen: g++ meldet
// „Redefinition"). Wer zwei unabhaengige Zahlen ohne Besitzer melden will, teilt die Zeile oder
// nimmt die Aggregatform NUR dann, wenn die zweite Zahl wirklich die Grundgesamtheit der ersten
// ist. Ein eigener Funktionsname waere der Ausweg; er ist bewusst nicht gebaut, solange die
// Entscheidung darueber nicht gefallen ist.

// Zwei identifizierende Zahlen UND ein Mengenpaar in EINER Zeile. Der Anlass ist eine Stelle,
// die den Fortschritt eines Vorgangs meldete und ihn nicht an seine Kennung binden konnte: die
// Aggregatform hat keinen Besitzer-Slot, „seen von total" stand ohne zu sagen, ZU WELCHEM
// Vorgang. Nur die zeitliche Nachbarschaft im Strom verband die zwei Zeilen, und die traegt nicht.
//
// EINE ZAHL, DIE IDENTIFIZIERT, IST ETWAS ANDERES ALS EINE, DIE MISST. Mengen duerfen `float`
// sein — sie sind gedeckelt, und ihre Rundung ist eine gerundete Menge. Kennungen nie: ein
// uint32-Hash ist ueber den ganzen Bereich gleichverteilt und verengt ueber 2^24 STILL, das Log
// druckt dann einen falschen, PLAUSIBLEN Wert. Deshalb stehen `owner` und `subject` als
// `uint32_t` und nicht als weitere Floats.
//
// EINE FORM FUER „zwei Kennungen UND einen dritten IDENTIFIZIERENDEN Wert" ist BEWUSST NICHT
// gebaut: ihre einzige Bedarfsstelle wurde verlustfrei anders geloest (die dritte Kennung in den
// owner-Slot, die 64-Bit-Kennung als Text im detail-Slot). Was bliebe, waere ein GEWINN —
// numerisch filterbar statt greppbar — und ein Gewinn ist keine Bedarfsstelle. Wer sie vermisst,
// nennt die Stelle, die sie braucht.
inline void error(uint32_t cat, const char* system, uint32_t owner, uint32_t subject,
                  const char* value_id, float count, float population) {
    if (LogSystem::logger()) {
        LogSystem::logger()->error("[{}] {}: owner={}, subject={}, value_id='{}', count={} of {}. {}{}",
            system, detail::get_cat_name(cat), owner, subject, value_id, count, population,
            detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

inline void warn(uint32_t cat, const char* system, uint32_t owner, uint32_t subject,
                 const char* value_id, float count, float population) {
    if (LogSystem::logger()) {
        LogSystem::logger()->warn("[{}] {}: owner={}, subject={}, value_id='{}', count={} of {}. {}{}",
            system, detail::get_wrn_cat_name(cat), owner, subject, value_id, count, population,
            detail::get_wrn_cat_short(cat), detail::get_wrn_cat_tail(cat));
    }
}

}  // namespace ase::log

#if defined(__GNUC__) && __GNUC__ >= 14
#pragma GCC diagnostic pop
#endif
