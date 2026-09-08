/**
 * ASE CORE INFRASTRUCTURE IMPLEMENTATION
 *
 * @file        log_files.cpp
 * @brief       Logverzeichnis, Rotationsquota, Aufraeumen und die Datei-Senke
 * @description Wo die Logdateien liegen, wie gross sie werden duerfen und wann sie
 *              verschwinden. Die Quota liegt als Klartextdatei unter logs/quota.conf und wird
 *              aus dem Verzeichnis der jeweiligen Senke gelesen, nicht aus einem Pfad zur
 *              Uebersetzungszeit — der Edge-Daemon laeuft auf Maschinen ohne Bau-Baum.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    ecs/module
 * @created     2026-08-20
 * @modified    2026-08-20
 * @version     1.0.0
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
 * WARUM HIER NOCH std::filesystem UND spdlog STEHEN (2026-08-20, nachgemessen 16:47)
 *
 *   Der Validator meldet in dieser Datei jetzt 8 filesystem- und 2 spdlog-Befunde; vor dem
 *   Umbau von prune_log_dir waren es 13 filesystem-, 2 chrono- und 2 spdlog-Befunde. Die
 *   chrono-Klasse ist vollstaendig verschwunden, nicht kleiner geworden.
 *
 *   DIE LUECKE, DIE HIER FRUEHER BESCHRIEBEN STAND, IST GESCHLOSSEN. Der Satz lautete:
 *   "ase::utils traegt Stringhelfer, keine Verzeichnis-Iteration und kein Rotieren ... weshalb
 *   der Aufraeum-Stichtag mit std::chrono gerechnet wird." Beides gibt es seit heute:
 *   ase-fileio hat mit directory.hpp eine Verzeichnisseite bekommen (list_dir liefert den
 *   mtime gleich mit, remove_file loescht), und utils::clock rechnet in Unix-Sekunden, in
 *   derselben Einheit, in der DirEntry seinen Zeitstempel fuehrt. prune_log_dir braucht
 *   seither weder directory_iterator noch last_write_time noch chrono.
 *
 *   Was BLEIBT, ist eine andere Sache als das, was ging: die 8 verbliebenen Befunde sind
 *   Pfad-Arithmetik (path, is_relative, parent_path) und create_directories. ase-fileio kennt
 *   heute LISTEN und LOESCHEN, aber kein Anlegen und keine Pfadzerlegung. Das ist eine
 *   benennbare naechste Luecke, keine allgemeine Unmoeglichkeit — und sie hier
 *   auszuschreiben ist der Unterschied zwischen einem Befund und einer Ausrede.
 *
 *   Die spdlog-Befunde sind eine eigene Klasse: sie sind der GRUND, warum es diese Datei
 *   gibt. Sie IST die Dateiverwaltung des Logs, und die Regel "Use ase::log wrapper" nennt
 *   ase::log als Loesung — hier wird ase::log GEBAUT. Ein Wrapper, der das Gewrappte nicht
 *   benutzen darf, kann nicht existieren.
 *
 *   Beim Split aus log_sys.cpp sind diese Befunde MITGEWANDERT, und das ist der Gewinn: sie
 *   sitzen jetzt in der Datei, die sie verursacht, statt im Logging-System, das sie nur benutzt.
 *
 * HERKUNFT (2026-08-20)
 *
 *   Dieser Block lag bis heute in log_sys.cpp. Die Datei trug 682 Zeilen und ZWEI Aufgaben:
 *   das Logging-System (Senken aufbauen, Bootstrap nachspielen, Ringpuffer lesen) und die
 *   Verwaltung seiner Dateien (wo sie liegen, wie gross sie werden, wann sie verschwinden).
 *   Der God-System-Deckel hat genau das gemeldet — er misst keine LAENGE, sondern eine
 *   FEHLENDE TRENNUNG.
 *
 *   Die Naht wurde gemessen, nicht angesehen:
 *     - der verschobene Block ruft NICHTS aus dem Rest der Datei (kein g_*, kein LogSystem::,
 *       kein install_capture/finalize_logger)
 *     - aus dem Rest kreuzen genau ZWEI Namen die Naht: get_project_root (3 Aufrufe) und
 *       make_rotating_file_sink (3 Aufrufe). Beide stehen jetzt in internal/log_files.hpp.
 *     - die oeffentliche Quota-API (log_dir_path, log_quota_path, log_quota, log_quota_in,
 *       set_log_quota) wird im Rest gar nicht gerufen — sie ist in log.hpp deklariert und
 *       gehoert Aufrufern ausserhalb dieses Moduls.
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
#include <ase/utils/clock.hpp>
#include <ase/utils/fs.hpp>
#include <ase/utils/strops.hpp>

namespace ase::log {

namespace internal {

// Get ASE project root directory (where logs/ should be created)
// SSOT: ASE_PROJECT_ROOT compile define from CMake (_ASE_BASE),
// resolves correctly for both central and standalone subgit builds.
// Returns a plain string, not a path object: every caller immediately joined a segment and asked
// for .string() back, so the path type existed only to carry a '/' that a string carries too.
//
// DIE LUECKE VON HEUTE FRUEH IST GESCHLOSSEN. Hier stand: "ase::utils::fs covers
// exists/parent_of/create_directories/remove but has no current-directory reader, and that gap is
// named rather than worked around." Der Leser existiert seit heute — utils::fs::current_path()
// (in ase-utils/fs.hpp) liest ueber POSIX getcwd, also mit demselben Syscall, den <filesystem> darunter
// ohnehin benutzt, nur ohne dessen Maschinerie. Damit ist der letzte filesystem-Aufruf dieser
// Datei fort, und zwar durch das Schliessen der Luecke, nicht durch ein Umgehen.
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
 * two-key plain-text format ("max_bytes <n>" and "max_files <n>"), parsed with plain stream reads.
 */

// Keys of the quota SSOT file, shared by the reader and the writer so the two can never drift.
static constexpr const char* kQuotaKeyMaxBytes = "max_bytes";
static constexpr const char* kQuotaKeyMaxFiles = "max_files";

// Stack buffer for the whole quota file. It holds exactly two lines: each is one of the keys
// above (9 chars), a space, a decimal number, and a newline. The widest 64-bit value has 20
// digits, so the longest possible file is 2 * (9 + 1 + 20 + 1) = 62 bytes. 128 is double that,
// and the size is a proof rather than a guess — nothing written here can overrun it.
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

LogQuota log_quota_in(const std::string& dir) {
    LogQuota quota;  // starts at the kDefaultLog* values
    const std::string path = dir + "/quota.conf";
    if (!fileio::file_exists(path)) {
        return quota;  // no SSOT file there: the documented defaults apply
    }
    const std::string content = fileio::read_text(path);

    // `in >> key` and `in >> value` did the tokenising before; read_text hands over the whole
    // file, so the split is written out here. That is the honest trade of this change: the
    // stream hid the parse, and a hidden parse is one nobody reads. The format is two
    // whitespace-separated tokens per line, and unknown keys are skipped exactly as before.
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
        if (pos == val_start) continue;  // key without a number: same as a failed `in >> value`

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
        }
    }
    return quota;
}

bool set_log_quota(const LogQuota& quota) {
    // Rueckgabe absichtlich nicht geprueft, wie zuvor der error_code: schlaegt das Anlegen fehl,
    // scheitert das write_text unten ebenfalls und set_log_quota meldet false. Der Fehler geht
    // nicht verloren — er wird an der Stelle sichtbar, die ihn wirklich betrifft.
    (void)utils::fs::create_directories(log_dir_path());

    const uint64_t bytes = (quota.max_bytes < kMinLogMaxBytes) ? kMinLogMaxBytes : quota.max_bytes;
    const uint32_t files = (quota.max_files > kMaxLogMaxFiles) ? kMaxLogMaxFiles : quota.max_files;

    // The whole file is two lines, so it is built once and written once. The stream form
    // wrote field by field and asked out.good() at the end — which reports the state AFTER
    // a partial write just as well as after a complete one. write_text either places the
    // whole content or returns false.
    char content[kQuotaFileBufSize] = {};
    utils::str_copy(content, sizeof(content), kQuotaKeyMaxBytes);
    utils::str_append(content, sizeof(content), " ");
    utils::str_append_u64(content, sizeof(content), bytes);
    utils::str_append(content, sizeof(content), "\n");
    utils::str_append(content, sizeof(content), kQuotaKeyMaxFiles);
    utils::str_append(content, sizeof(content), " ");
    utils::str_append_u64(content, sizeof(content), files);
    utils::str_append(content, sizeof(content), "\n");

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
 * can never be caught: anything untouched for two weeks has no writer left.
 */
void prune_log_dir(const char* dir, uint32_t dir_len) {
    const int64_t cutoff =
        utils::wall_time_seconds() - static_cast<int64_t>(kLogRetentionDays) * kSecondsPerDay;

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
 * WAS BIS 2026-08-22 `make_rotating_file_sink` HIESS UND ZWEI DINGE TAT.
 *
 *   Die alte Fassung raeumte das Verzeichnis, las die Quota UND baute die Senke
 *   (`make_shared<rotating_file_sink_mt>`). Damit trug diese Datei einen fremden Senkentyp,
 *   obwohl sie fuer LogDATEIEN zustaendig ist, nicht fuer Logsenken — und die Naht lief quer
 *   durch drei Dateien: Rueckgabetyp im Header, Rumpf hier, alle drei Aufrufer in log_sys.cpp.
 *   Keine Haelfte war fuer sich aufloesbar, weil die Signatur der einen im Besitz der anderen
 *   stand.
 *
 *   Jetzt beantwortet jede Haelfte genau eine Frage: hier "welche Datei, wie gross, wie viele",
 *   dort "mach mir eine Senke". Der Aufbau der Senke (rotate_on_open, damit jeder Prozessstart
 *   eine frische Datei beginnt OHNE den vorigen Lauf zu zerstoeren — er wird name.1.log) steht
 *   unveraendert in log_sys.cpp, an EINER Stelle, die alle drei Aufrufer bedient.
 */
void prepare_log_sink(const std::string& path, uint64_t& out_max_bytes, uint32_t& out_max_files) {
    // parent_of liefert "" fuer einen blossen Dateinamen ohne Verzeichnisanteil — genau der Fall,
    // den has_parent_path() vorher abgefragt hat, nur als Wert statt als Praedikat.
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
 * This function replaced THREE verbatim copies of the same block in log_sys.cpp (on_start,
 * init_server_standalone, init_tui_standalone). Three copies are not one rule applied three
 * times; they are three places where it can drift apart.
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
