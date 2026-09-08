#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        log_quota.hpp
 * @brief       Die Rotations- und Aufbewahrungs-SSOT jeder log-schreibenden Binary
 * @description Grenzwerte und Zugriff auf <project-root>/logs/quota.conf: bei welcher Groesse eine
 *              Logdatei rotiert, wie viele Generationen bleiben, wie lange das Verzeichnis
 *              aufbewahrt und wie ein Durchlauf ueber dieses Verzeichnis begrenzt wird.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    process/computation
 * @created     2026-08-31
 * @modified    2026-08-31
 * @version     1.0.0
 *
 * WARUM DIESER ZWECK EINE EIGENE DATEI IST — GEMESSEN, NICHT GESCHAETZT
 *
 * Diese Erklaerungen standen bis 2026-08-31 in `log.hpp`. Dort trafen sie auf einen
 * Verbraucherkreis, den sie nichts angehen: `log.hpp` wird von 2224 Uebersetzungseinheiten
 * eingebunden, die Quota-Symbole von SECHS Dateien insgesamt — vier im Modul selbst
 * (internal/log_files.hpp, src/log_files.cpp, src/log_sys.cpp und log.hpp) und ZWEI ausserhalb:
 * tools/ase-edge-daemon/src/cli_backend/backend_paths.cpp und tools/ase-cli/src/logs/logs_cmd.cpp.
 *
 * DER PREIS WAR NICHT DIE GROESSE, SONDERN DIE KOPPLUNG: eine Aenderung an einer
 * Rotationsschranke uebersetzte 2224 Einheiten neu, obwohl sechs sie lesen. Der Schnitt folgt
 * deshalb dem EINSATZZWECK — wer eine Quota liest, bindet diese Datei ein; wer eine Zeile
 * schreibt, nicht.
 *
 * SPDLOG-FREI, UND DAS IST TEIL DER WIDMUNG: die Kommentare nennen das Verhalten der
 * darunterliegenden Bibliothek (eine Null laesst ihre rotierende Senke werfen), der Header selbst
 * bindet sie nicht ein und benutzt keinen ihrer Typen. Eine Schranke ist eine Zahl, keine Senke.
 *
 * ZUR NEBENLAEUFIGKEIT, weil die Checkliste danach fragt und eine Zusage ohne Rumpfpruefung
 * erfunden waere: die Lesefunktionen oeffnen quota.conf und geben Werte zurueck, sie halten
 * keinen Zustand. Der Bestand liest sie EINMAL beim Bau der Dateisenke, also vor dem Start
 * weiterer Threads; ein Mutex fehlt hier nicht, er hat keinen gemeinsamen Zustand zu schuetzen.
 * `set_log_quota` schreibt und ist damit ein Vorgang fuer die Konsole, nicht fuer den Betrieb.
 *
 * LAYER RULES:
 *   Layer 0 (Foundation): NO dependencies on other ASE modules (only std::)
 *   Layer 1 (Core):       May depend on Layer 0 only
 *
 * USAGE:
 *   #include <ase/log/log_quota.hpp>
 *   const auto quota = ase::log::log_quota();
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

#include <cstdint>
#include <string>

namespace ase::log {

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

}  // namespace ase::log
