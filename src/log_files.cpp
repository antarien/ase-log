/**
 * ASE CORE INFRASTRUCTURE IMPLEMENTATION
 *
 * @file        log_files.cpp
 * @brief       Logverzeichnis, Rotationsquota, Aufraeumen und die Datei-Senke
 * @description Wo die Logdateien liegen, wie gross sie werden duerfen und wann sie
 *              verschwinden. Die Quota liegt als Klartextdatei unter logs/quota.conf und wird
 *              aus dem Verzeichnis der jeweiligen Senke gelesen, nicht aus einem Pfad zur
 *              Uebersetzungszeit — der Edge-Daemon laeuft auf Maschinen ohne Bau-Baum. Dieselbe
 *              Datei traegt die Zeilenzahl des Aufnahme-Rings der Startphase.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    ecs/module
 * @usecase     usecase:100361 - open the log sinks
 * @created     2026-08-20
 * @modified    2026-10-13
 * @version     1.1.0
 *
 * CORE INFRASTRUCTURE IMPLEMENTATION COMPLIANCE
 *
 * [ ] NOT an ECS System implementation
 * [ ] Layer dependencies correct (L0: no ASE deps, L1: L0 only)
 * [ ] Own header included FIRST
 * [ ] No global mutable state
 * [ ] No static initialization order fiasco
 * [ ] Thread-safe implementations (pure or mutex-protected)
 * [ ] All error conditions handled
 * [ ] No exceptions thrown (use Result<T> pattern)
 * [ ] Implementation details in anonymous namespace
 * [ ] No inline implementations of template specializations here
 * [ ] Platform-specific code isolated and documented
 * [ ] Performance-critical code profiled and optimized
 *
 * WARUM HIER std::filesystem UND spdlog STEHEN
 *
 *   Die Verzeichnisseite laeuft ueber ase-fileio (list_dir liefert den mtime gleich mit,
 *   remove_file loescht), und utils::clock rechnet in Unix-Sekunden, in derselben Einheit, in der
 *   DirEntry seinen Zeitstempel fuehrt. Der Aufraeum-Stichtag braucht deshalb weder
 *   directory_iterator noch last_write_time noch chrono, und die eine UTC-Tageslaenge kommt aus
 *   ase-utils (SECONDS_PER_UTC_DAY).
 *
 *   Was an filesystem-Befunden bleibt, ist Pfad-Arithmetik (path, is_relative, parent_path) und
 *   create_directories. ase-fileio kennt LISTEN und LOESCHEN, aber kein Anlegen und keine
 *   Pfadzerlegung. Das ist eine benennbare Luecke, keine allgemeine Unmoeglichkeit — und sie hier
 *   auszuschreiben ist der Unterschied zwischen einem Befund und einer Ausrede.
 *
 *   Die spdlog-Befunde sind eine eigene Klasse: sie sind der GRUND, warum es diese Datei
 *   gibt. Sie IST die Dateiverwaltung des Logs, und die Regel "Use ase::log wrapper" nennt
 *   ase::log als Loesung — hier wird ase::log GEBAUT. Ein Wrapper, der das Gewrappte nicht
 *   benutzen darf, kann nicht existieren. Die Befunde sitzen damit in der Datei, die sie
 *   verursacht, statt im Logging-System, das sie nur benutzt.
 *
 * DIE NAHT ZU log_sys.cpp
 *
 *   log_sys.cpp traegt das Logging-System (Senken aufbauen, Bootstrap nachspielen, Ringpuffer
 *   lesen), diese Datei die Verwaltung seiner Dateien (wo sie liegen, wie gross sie werden, wann
 *   sie verschwinden). Die Naht ist gemessen, nicht angesehen:
 *     - dieser Block ruft NICHTS aus log_sys.cpp (kein g_*, kein LogSystem::, kein
 *       install_capture/finalize_logger)
 *     - aus log_sys.cpp kreuzen genau ZWEI Namen die Naht: get_project_root und
 *       prepare_log_sink. Beide stehen in internal/log_files.hpp.
 *     - die oeffentliche Quota-API (log_dir_path, log_quota_path, log_quota, log_quota_in,
 *       set_log_quota) wird in log_sys.cpp gar nicht gerufen — sie gehoert Aufrufern ausserhalb
 *       dieses Moduls.
 *
 *   Eine einseitige Kante also, und die einzige Ordnung, die sie braucht, ist die des
 *   Uebersetzers: log_sys.cpp inkludiert den internen Header.
 */

#include <ase/log/internal/log_files.hpp>

// Die Rotations- und Aufbewahrungs-SSOT. Direkt eingebunden statt ueber den eigenen Header
// mitgenommen: wer einen Typ benutzt, nennt die Datei, die ihn deklariert.
#include <ase/log/log_quota.hpp>

#include <ase/log/log.hpp>

#include <spdlog/sinks/rotating_file_sink.h>

#include <ase/fileio/directory.hpp>
#include <ase/fileio/text_reader.hpp>
#include <ase/fileio/text_writer.hpp>
// Wanduhr in Unix-Sekunden und die eine UTC-Tageslaenge (L0)
#include <ase/utils/clock.hpp>
#include <ase/utils/fs.hpp>
#include <ase/utils/strops.hpp>

namespace ase::log {

namespace internal {

// Get ASE project root directory (where logs/ should be created)
// SSOT: ASE_PROJECT_ROOT compile define from CMake (_ASE_BASE),
// resolves correctly for both central and standalone subgit builds.
// Returns a plain string, not a path object: every caller immediately joins a segment and asks
// for .string() back, so a path type would exist only to carry a '/' that a string carries too.
//
// Der Rueckfall liest das Arbeitsverzeichnis ueber utils::fs::current_path() (ase-utils/fs.hpp),
// also ueber POSIX getcwd — denselben Syscall, den <filesystem> darunter ohnehin benutzt, nur ohne
// dessen Maschinerie.
std::string get_project_root() {
#ifdef ASE_PROJECT_ROOT
    return std::string(ASE_PROJECT_ROOT);
#else
    // Fallback: current working directory
    return utils::fs::current_path();
#endif
}

}  // namespace internal

/**
 * Rotation quota, SSOT file <project-root>/logs/quota.conf.
 *
 * Every file sink in this translation unit is a rotating sink fed by these values, so no log file
 * of any tier, of the edge daemon or of the operator CLI can grow without bound. The file is a
 * plain-text format of "<key> <n>" lines: "max_bytes" and "max_files" for the rotation, and
 * "boot_lines" for the capture ring of the startup phase (capture_ring_lines() in
 * internal/log_files.hpp), so that number changes with a file edit instead of a build.
 */

// Keys of the quota SSOT file, shared by the reader and the writer so the two can never drift.
static constexpr const char* kQuotaKeyMaxBytes  = "max_bytes";
static constexpr const char* kQuotaKeyMaxFiles  = "max_files";
static constexpr const char* kQuotaKeyBootLines = "boot_lines";

// Stack buffer for the whole quota file. It holds at most three lines, each a key above, a space,
// a decimal number and a newline. max_bytes is 64-bit (20 digits), max_files and boot_lines are
// 32-bit (10 digits), so the longest possible file is (9 + 1 + 20 + 1) + (9 + 1 + 10 + 1) +
// (10 + 1 + 10 + 1) = 74 bytes. 128 covers it, and the size is a proof rather than a guess —
// nothing written here can overrun it.
static constexpr uint32_t kQuotaFileBufSize = 128u;

std::string log_dir_path() {
    return internal::get_project_root() + "/logs";
}

std::string log_quota_path() {
    return internal::get_project_root() + "/logs/quota.conf";
}

LogQuota log_quota() {
    return log_quota_in(log_dir_path());
}

namespace {

/**
 * Reads <dir>/quota.conf in ONE pass: the two rotation keys and boot_lines. One tokenizer for all
 * three keys, so a key added later is never read by a second loop that drifts from this one.
 * Without the file the documented defaults stay; unknown keys are skipped.
 */
void read_quota_file(const std::string& dir, LogQuota& quota, uint32_t& boot_lines,
                     bool& boot_lines_set) {
    quota = LogQuota{};  // starts at the kDefaultLog* values
    boot_lines = internal::kDefaultCaptureRingLines;
    boot_lines_set = false;
    const std::string path = dir + "/quota.conf";
    if (!fileio::file_exists(path)) {
        return;  // no SSOT file there: the documented defaults apply
    }
    const std::string content = fileio::read_text(path);

    // read_text hands over the whole file, so the split is written out here: a stream would hide
    // the parse, and a hidden parse is one nobody reads. The format is two whitespace-separated
    // tokens per line, and unknown keys are skipped.
    size_t pos = 0;
    while (pos < content.size()) {
        while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\n' ||
                                        content[pos] == '\r' || content[pos] == '\t')) ++pos;
        const size_t key_start = pos;
        while (pos < content.size() && content[pos] != ' ' && content[pos] != '\n' &&
               content[pos] != '\r' && content[pos] != '\t') ++pos;
        if (pos == key_start) break;
        const std::string key = content.substr(key_start, pos - key_start);

        while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\t')) ++pos;
        const size_t val_start = pos;
        uint64_t value = 0;
        while (pos < content.size() && content[pos] >= '0' && content[pos] <= '9') {
            value = value * 10u + static_cast<uint64_t>(content[pos] - '0');
            ++pos;
        }
        if (pos == val_start) continue;  // key without a number: the line carries no value

        if (key == kQuotaKeyMaxBytes) {
            if (value >= kMinLogMaxBytes) {
                quota.max_bytes = value;
            }
        } else if (key == kQuotaKeyMaxFiles) {
            // Clamped on READ as well: a hand-edited quota.conf must never be able to push
            // max_files past what the rotating sink accepts, because that throw would abort
            // startup for every binary that reads this file.
            const uint32_t files = static_cast<uint32_t>(value);
            quota.max_files = (files > kMaxLogMaxFiles) ? kMaxLogMaxFiles : files;
        } else if (key == kQuotaKeyBootLines) {
            // Clamped on READ like max_files: the ring allocates every slot when it is built, so
            // a hand-edited value must stay inside kMinCaptureRingLines and kMaxCaptureRingLines.
            if (value < internal::kMinCaptureRingLines) {
                boot_lines = internal::kMinCaptureRingLines;
            } else if (value > internal::kMaxCaptureRingLines) {
                boot_lines = internal::kMaxCaptureRingLines;
            } else {
                boot_lines = static_cast<uint32_t>(value);
            }
            boot_lines_set = true;
        }
    }
}

}  // namespace

LogQuota log_quota_in(const std::string& dir) {
    LogQuota quota;
    uint32_t boot_lines = 0u;
    bool boot_lines_set = false;
    read_quota_file(dir, quota, boot_lines, boot_lines_set);
    return quota;
}

namespace internal {

uint32_t capture_ring_lines() {
    LogQuota quota;
    uint32_t boot_lines = 0u;
    bool boot_lines_set = false;
    read_quota_file(log_dir_path(), quota, boot_lines, boot_lines_set);
    return boot_lines;
}

}  // namespace internal

bool set_log_quota(const LogQuota& quota) {
    // Rueckgabe absichtlich nicht geprueft: schlaegt das Anlegen fehl, scheitert das write_text
    // unten ebenfalls und set_log_quota meldet false. Der Fehler geht nicht verloren — er wird an
    // der Stelle sichtbar, die ihn wirklich betrifft.
    (void)utils::fs::create_directories(log_dir_path());

    const uint64_t bytes = (quota.max_bytes < kMinLogMaxBytes) ? kMinLogMaxBytes : quota.max_bytes;
    const uint32_t files = (quota.max_files > kMaxLogMaxFiles) ? kMaxLogMaxFiles : quota.max_files;

    // boot_lines lives in the same file but is not set here: writing the rotation quota must not
    // silently drop an operator's boot_lines. If the key stands in the file, it is written back
    // unchanged.
    LogQuota current;
    uint32_t boot_lines = 0u;
    bool boot_lines_set = false;
    read_quota_file(log_dir_path(), current, boot_lines, boot_lines_set);

    // The whole file is at most three lines, so it is built once and written once. A field-by-field
    // stream asked at the end reports the state AFTER a partial write just as well as after a
    // complete one; write_text either places the whole content or returns false.
    char content[kQuotaFileBufSize] = {};
    utils::str_copy(content, sizeof(content), kQuotaKeyMaxBytes);
    utils::str_append(content, sizeof(content), " ");
    utils::str_append_u64(content, sizeof(content), bytes);
    utils::str_append(content, sizeof(content), "\n");
    utils::str_append(content, sizeof(content), kQuotaKeyMaxFiles);
    utils::str_append(content, sizeof(content), " ");
    utils::str_append_u64(content, sizeof(content), files);
    utils::str_append(content, sizeof(content), "\n");
    if (boot_lines_set) {
        utils::str_append(content, sizeof(content), kQuotaKeyBootLines);
        utils::str_append(content, sizeof(content), " ");
        utils::str_append_u64(content, sizeof(content), boot_lines);
        utils::str_append(content, sizeof(content), "\n");
    }

    return fileio::write_text(log_quota_path(), content);
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
 * can never be caught: anything untouched for two weeks has no writer left. The day length is the
 * one UTC day of ase-utils (utils::SECONDS_PER_UTC_DAY).
 */
void prune_log_dir(const char* dir, uint32_t dir_len) {
    const int64_t cutoff = utils::wall_time_seconds() -
                           static_cast<int64_t>(kLogRetentionDays) * utils::SECONDS_PER_UTC_DAY;

    fileio::DirEntry entries[kLogDirScanMax];
    char             full[fileio::DIR_PATH_MAX];

    // Rounds, because a listing reports at most kLogDirScanMax entries and says so by coming back
    // full. Deleting frees slots, so a further listing can see what the previous one had no room
    // for. A short listing means everything was seen; a round that deletes nothing means further
    // rounds would report the same entries again.
    for (uint32_t round = 0u; round < kLogPruneMaxRounds; ++round) {
        const uint32_t found = fileio::list_dir(dir, dir_len, entries, kLogDirScanMax);

        uint32_t removed = 0u;
        for (uint32_t i = 0u; i < found; ++i) {
            const fileio::DirEntry& entry = entries[i];
            if (!entry.is_regular) {
                continue;
            }

            // Suffix, not "extension": the sweep must not touch quota.conf or a rotated
            // name.1.log — the latter ends in .log and IS a generation this sweep may retire.
            const uint32_t name_len = utils::str_len(entry.name, fileio::DIR_ENTRY_NAME_MAX);
            const int32_t  dot      = utils::str_rfind(entry.name, name_len, '.');
            if (dot < 0 ||
                !utils::str_equal(entry.name + dot, kLogFileSuffix, kLogFileSuffixLen)) {
                continue;
            }

            // list_dir skips any entry whose stat() failed rather than reporting it with a zero
            // timestamp, so an unreadable file never arrives here looking like 1970 — the one
            // shape that would make a retention sweep delete exactly what it could not inspect.
            if (entry.modified_secs >= cutoff) {
                continue;
            }

            utils::str_path(full, sizeof(full), dir, entry.name, nullptr, nullptr);
            if (fileio::remove_file(full, utils::str_len(full, fileio::DIR_PATH_MAX))) {
                ++removed;
            }
        }

        if (found < kLogDirScanMax || removed == 0u) {
            break;
        }
    }
}

}  // namespace

namespace internal {

/**
 * Prepare the directory of a log file and report the rotation limits that apply there.
 *
 * The quota is read from the file's OWN directory, not from a compile-time path: the edge daemon
 * runs as a prebuilt binary on machines where the build tree does not exist, and it must find its
 * own quota (or none, and use the defaults) rather than probing a foreign path.
 *
 * JEDE HAELFTE BEANTWORTET GENAU EINE FRAGE: hier "welche Datei, wie gross, wie viele", in
 * log_sys.cpp "mach mir eine Senke". Diese Datei ist fuer LogDATEIEN zustaendig, nicht fuer
 * Logsenken, und traegt deshalb keinen Senkentyp. Der Aufbau der Senke (rotate_on_open, damit
 * jeder Prozessstart eine frische Datei beginnt OHNE den vorigen Lauf zu zerstoeren — er wird
 * name.1.log) steht in log_sys.cpp, an EINER Stelle, die alle drei Aufrufer bedient.
 */
void prepare_log_sink(const std::string& path, uint64_t& out_max_bytes, uint32_t& out_max_files) {
    // parent_of liefert "" fuer einen blossen Dateinamen ohne Verzeichnisanteil — dann gilt das
    // Logverzeichnis des Projekts.
    const std::string parent = utils::fs::parent_of(path);
    const std::string dir    = parent.empty() ? log_dir_path() : parent;
    prune_log_dir(dir.c_str(), static_cast<uint32_t>(dir.size()));
    const LogQuota quota = log_quota_in(dir);
    out_max_bytes = quota.max_bytes;
    out_max_files = quota.max_files;
}

/**
 * Resolve a log file name to an absolute path and make sure its directory exists.
 *
 * A RELATIVE name resolves against the project root, NOT the working directory: a tier started
 * from a different directory would otherwise write its log somewhere else than the tier next to
 * it, and the console would tail a file that never fills. An ABSOLUTE name is passed through —
 * the edge daemon receives its path from the caller and runs where no build tree exists.
 *
 * ONE function for the three callers in log_sys.cpp (on_start, init_server_standalone,
 * init_tui_standalone). Three copies would not be one rule applied three times; they would be
 * three places where it can drift apart.
 */
std::string resolve_log_path(const std::string& log_file) {
    // Absolut heisst auf POSIX genau eines: der Pfad beginnt mit '/'. is_relative() prueft nichts
    // anderes, solange keine Windows-Laufwerksbuchstaben im Spiel sind, und ASE baut POSIX.
    const bool absolut = !log_file.empty() && log_file[0] == '/';

    const std::string absolute_path = absolut ? log_file : get_project_root() + "/" + log_file;

    const std::string parent = utils::fs::parent_of(absolute_path);
    if (!parent.empty()) {
        (void)utils::fs::create_directories(parent);
    }
    return absolute_path;
}

}  // namespace internal

}  // namespace ase::log
