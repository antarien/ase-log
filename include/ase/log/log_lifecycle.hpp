#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        log_lifecycle.hpp
 * @brief       Aufsetzen, Umlenken und Beenden des Loggers — fuer Binaries ohne ECS-Welt
 * @description Die vier Einstiege, mit denen ein Programm ohne Registry einen Logger bekommt
 *              (Konsole, Server-Format, TUI-Rueckruf), die Capture-Klammer, mit der ein Aufrufer
 *              den Zeilenstrom voruebergehend in einen Puffer umlenkt, der Filter-Einstieg aus
 *              argv und das Beenden.
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
 * Diese Deklarationen standen bis 2026-08-31 in `log.hpp`, und dort trafen sie auf einen
 * Verbraucherkreis, den sie nichts angehen: `log.hpp` wird von 2224 Uebersetzungseinheiten
 * eingebunden. Den Lebenszyklus dagegen ruft, wer ein Programm STARTET oder BEENDET —
 * `log::init` aus 14 Dateien, `log::shutdown` aus 10, die Capture-Klammer aus 3.
 *
 * DER PREIS WAR DIE KOPPLUNG, NICHT DIE GROESSE: eine Aenderung an der Capture-Klammer
 * uebersetzte 2224 Einheiten neu, obwohl drei sie rufen. Der Schnitt folgt dem EINSATZZWECK —
 * wer eine Zeile SCHREIBT, bindet log.hpp ein; wer einen Logger AUFSETZT, diese Datei.
 *
 * KEINE INLINE-DEFINITION, UND DAS IST DER GRUND, WARUM DIESE DATEI DIE BIBLIOTHEK NICHT NENNT.
 * `init` und `shutdown` waren inline und griffen dafuer auf `LogSystem::logger()` zu — `init`
 * nannte dabei die farbige Konsolensenke, den Logger-Typ und die Stufenkonstante der Bibliothek
 * DIREKT, in einem Header, den der ganze Baum sieht. Beide sind jetzt Deklarationen; ihre Rumpfe
 * stehen unveraendert bei ihren Geschwistern in log_standalone.cpp. Damit traegt weder diese
 * Datei noch ihr Verbraucherkreis einen Fremdtyp, den er nicht braucht.
 *
 * EIN HINWEIS FUER JEDEN, DER HIER SPAETER SCHREIBT: die Sperre gegen den direkten Gebrauch der
 * Bibliothek greift auch in KOMMENTAREN — ihr Muster ist der qualifizierte Name, nicht der
 * Aufruf. Dieser Absatz umschreibt die drei Typen deshalb, statt sie zu zitieren. Wer sie hier
 * beim Namen nennt, erzeugt einen Befund in einer Datei, die sonst keinen haette.
 *
 * DIE UMSTELLUNG FUEGT KEINE LINKKANTE HINZU, gemessen: `internal::logger_slot()` ist in log.hpp
 * nur DEKLARIERT und ausserhalb definiert. Wer `log::info` ruft, ruft sie — also linkt jeder
 * Aufrufer bereits gegen diese Bibliothek, und eine nicht-inline `init` verlangt nichts Neues.
 *
 * LAYER RULES:
 *   Layer 0 (Foundation): NO dependencies on other ASE modules (only std::)
 *   Layer 1 (Core):       May depend on Layer 0 only
 *
 * USAGE:
 *   #include <ase/log/log_lifecycle.hpp>
 *   ase::log::init("ase-codegen");
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
// CLI Initialization (for tools without ECS World)
// ============================================================================

/**
 * @brief Initialize logger for CLI tools (no ECS required)
 * @param name Logger name (e.g., "ase-codegen")
 *
 * Use this for CLI tools that don't have an ECS World.
 * For ECS-based apps, use LogSystem::on_start() instead.
 *
 * Idempotent: returns immediately when a logger already exists. Defined in log_sys.cpp — der
 * Rumpf baut eine Konsolensenke der Bibliothek, und die gehoert nicht in einen Header.
 */
void init(const std::string& name);

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
 * Defined in log_standalone.cpp; the bytes come from internal::make_file_formatter.
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
 * logger already exists). Defined in log_standalone.cpp; bytes from internal::make_tui_*_formatter.
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
 *
 * Flushes and releases the logger slot. Defined in log_sys.cpp — der Rumpf fasst den
 * Bibliothekszeiger an, und der gehoert nicht in einen Header.
 */
void shutdown();

}  // namespace ase::log
