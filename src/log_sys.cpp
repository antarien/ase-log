/**
 * ASE ECS SYSTEM IMPLEMENTATION
 * @design      DSGN_021
 * @file        log_sys.cpp
 * @brief       LogSystem - brings up and tears down the logging sinks
 * @module      ase-log
 * @layer       1 (Core)
 * @category    process/computation
 * @schedule    Initialization
 * @usecase     usecase:100361 - open the log sinks
 * @created     2026-01-09
 * @modified    2026-10-11
 * @version     1.2.0
 *
 * CAUSAL CHAIN (CAUSA_LOG_SINKS: die Senken entstehen und vergehen)
 *
 *   [Kernel startet, LogModule::build hat registriert]
 *          │
 *          │ on_start() einmal, Schedule::Initialization
 *          ▼
 *   ┌─────────────────────────────────────────────────────────┐
 *   │  THIS SYSTEM: LogSystem                                 │
 *   │                                                         │
 *   │  READS:                                                 │
 *   │    → LogConfig (log_module.hpp, Pfad und Quota)         │
 *   │                                                         │
 *   │  WRITES:                                                │
 *   │    → spdlog-Senken: Datei, HTTP-Ring, Counting          │
 *   │    → nichts in der Registry                             │
 *   └─────────────────────────────────────────────────────────┘
 *          │
 *          ▼
 *   [jedes log::info/warn/error im ganzen Baum erreicht seine Senke]
 *
 * HUB Pattern (N/A - No Hub reads/writes)
 *
 * READS (from Hub):
 *   (none)
 *
 * WRITES (to Hub):
 *   (none)
 *
 * FLYWEIGHT PATTERN (spdlog-Senken als shared_ptr, nicht in Components)
 *
 *   Die Senken sind Fremdobjekte aus spdlog und leben im LogResourceManager, nicht in der
 *   Registry. Sie muessen VOR dem ersten log-Aufruf existieren und NACH dem letzten noch — also
 *   ausserhalb jeder Entity-Lebenszeit.
 *
 * WO DIE NACHBARN DIESES SYSTEMS STEHEN
 *
 *     src/log_boot.cpp        install_capture_logger, finalize_logger_after_boot,
 *                             parse_cli_filter_from_argv — der UEBERGANG vom "es gibt noch
 *                             keinen Logger" zum fertigen Zustand. Laeuft VOR jedem System und
 *                             sieht nie eine Registry.
 *     src/log_standalone.cpp  init_server_standalone, init_tui_standalone — derselbe Logger
 *                             fuer Programme, in denen es KEINEN Kernel gibt (ase-edge-daemon,
 *                             ase-cli).
 *     internal/               make_rotating_file_sink — die eine Bauform der Datei-Senke, die
 *      log_resource_manager   on_start und beide Standalone-Einstiege brauchen, dazu die
 *                             Logger-Slots (internal::logger_slot, client_logger_slot): ein
 *                             Durchreicher gehoert zu dem, wohin er reicht.
 *     src/log_query.cpp       recent_logs — der LESEZUGRIFF auf den Ring.
 *
 *   Diese Datei traegt den Systemvertrag: on_start baut die drei echten Senken und PARKT sie am
 *   ResourceManager, on_stop raeumt im kontrollierten Abstieg ab. Die andere Haelfte der
 *   Park-Logik — das Abholen und Einspielen — steht in log_boot.cpp, und der Kanal zwischen
 *   beiden ist der Manager, nicht eine gemeinsame Datei.
 *
 * WARUM DIESE DATEI STRUKTURELL ANDERS AUSSIEHT ALS JEDES ANDERE SYSTEM
 *
 *   Der Validator meldet hier strukturelle Befunde, die nicht Nachlaessigkeit sind, sondern die
 *   Bauform der Sache. Die Regeln, die sie ausloesen, richten sich an die VERBRAUCHER von
 *   log::* — und diese Datei IST log::*:
 *
 *     shared_ptr / unique_ptr    die spdlog-Senken, Fremdobjekte mit eigener Lebenszeit
 *     atomic / Mutex             ase-log ist der EINZIGE Weg, aus einem fremden Thread zu
 *                                loggen; ohne Sperre waere jede Callback-Meldung ein Datenrennen.
 *                                Beides liegt im LogResourceManager, nicht in diesem System.
 *     "Static System calls"      `log::error(...)` IST dieser statische Aufruf
 *
 *   Die Gegenprobe ist gemacht: log_sys.cpp ist ein echtes ECS-System (on_start/tick/on_stop,
 *   Layer 1, Schedule Initialization), die Dateityp-Einordnung des Validators stimmt. Es ist
 *   also KEINE Prueflueckenmeldung, sondern ein Regelkonflikt an einer einzigen Stelle des
 *   Baums — derselbe wie bei core/ase-ecs/src/internal/boot_logger.cpp, das aus demselben Grund
 *   cout benutzt: es ist die Konsolenausgabe, bevor es eine Konsolensenke gibt.
 *
 *   Diese Befunde werden NICHT einzeln umgebaut. Wer sie raeumt, nimmt dem Baum sein Logging.
 *
 * ECS SYSTEM IMPLEMENTATION COMPLIANCE
 *
 * [ ] Layer dependencies checked (only depend on lower layers)
 * [ ] Existing functions checked (ase-math, ase-utils, ase-containers)
 * [ ] Abbreviations defined in types.hpp or documentation
 * [ ] types.hpp created with all constants and enums
 * [ ] STATELESS? No member variables?
 * [ ] Views created on demand, not stored?
 * [ ] NO direct calls to other systems?
 * [ ] Communication only via Components?
 * [ ] Helpers in anonymous namespace (NOT static!)?
 * [ ] Math functions from ase-math (Layer 0)?
 * [ ] NO file-level static/constexpr?
 * [ ] Registered in Module with correct Schedule?
 * [ ] Filename matches convention?
 * [ ] Class name derived correctly from filename?
 * [ ] Using Deferred Deletion Pattern? (Tag + Batch Destroy)
 * [ ] NO destroy() on other entities during iteration?
 * [ ] Cleanup System in Schedule::Conclusion?
 * [ ] NO local arrays/vectors for collection?
 * [ ] Safe deletion (first collect, then delete)?
 * [ ] Not deleting other entities during iteration?
 * [ ] Not invalidating references during iteration?
 * [ ] 1 File = 1 System?
 * [ ] Folder structure matches convention?
 * [ ] components/, systems/, src/ have IDENTICAL subfolder structure?
 * [ ] Layer dependencies respected (no upward dependencies)?
 * [ ] NO inline nlohmann::json + .dump() in broadcast systems?
 * [ ] Serializer functions in anonymous namespace?
 * [ ] *NetBctReqSystem + *NetBctSndSystem pattern?
 * [ ] Math functions from ase-math? (lerp, clamp, noise)
 * [ ] Containers from ase-containers? (RingBuffer)
 * [ ] Types from ase-types? (Result, Option)
 * [ ] Utils from ase-utils? (UUID, hash)
 * [ ] No duplicate functionality across modules?
 * [ ] ONLY primitive types: int, float, uint32_t, bool, etc.
 * [ ] ONLY ase-math for math (NO std::min, std::max, std::clamp!)
 * [ ] ONLY ase-containers for containers (NO std::vector, std::map, std::unordered_map!)
 * [ ] ONLY ase-types for Result/Option (NO std::optional, std::expected!)
 * [ ] std:: FORBIDDEN except: <cstdint>, <cmath> basics, <cassert>
 * [ ] NO ARRAYS! (use Entity-per-Item + Tags!)
 * [ ] CAUSAL CHAIN documented (Input → Processing → Output)
 * [ ] HUB Pattern documented (READS/WRITES)
 * [ ] hub::get() for reads
 * [ ] hub::set() for writes
 * [ ] Method order: on_start → tick → on_stop
 * [ ] ALL THREE METHODS implemented
 * [ ] on_start/on_stop: log::debug with system name
 * [ ] log::warn() if value EXISTS but invalid (e.g., health < 0, temp > 1000)
 * [ ] log::error() for EVERY NOT_FOUND check (see ase-log/log.hpp ERR::CAT::*)
 * [ ] Unused params: (void)dt; or commented parameter name
 * [ ] NO switch/case statements? (use Tag-filtered Views or lookup tables!)
 * [ ] NO if-else chains for type dispatch? (use separate Systems per type!)
 * [ ] NO instanceof/dynamic_cast checks? (use Tags for entity classification!)
 * [ ] NO factory patterns with type enums? (use Component composition!)
 * [ ] NO inheritance hierarchies? (use Component composition!)
 * [ ] NO virtual dispatch for game logic? (only ecs::System base class allowed!)
 * [ ] NO singleton patterns? (use Manager Tags on entities!)
 * [ ] NO state machines with switch? (use Tag-based state + separate Systems!)
 * [ ] ALL behavior driven by Component DATA, not hardcoded logic?
 * [ ] NO hardcoded entity types? (types defined by Component composition!)
 * [ ] NO hardcoded processing order? (order via Schedule + run_after!)
 * [ ] NO hardcoded value ranges? (ranges in types.hpp constants!)
 * [ ] NO hardcoded special cases? (special cases = Tags + dedicated Systems!)
 * [ ] Formulas use Component fields, not magic numbers?
 * [ ] New behavior = new Component + new System, NOT if-else in existing code?
 * [ ] NO `find_*()` with View/Query? (use DUAL-PATTERN)
 * [ ] NO `check_*()`/`has_*()`/`is_*()` with View/Query? (use DUAL-PATTERN)
 * [ ] NO `get_*()` with View/Query? (use DUAL-PATTERN)
 * [ ] NO struct in namespace {}? (use Component)
 * [ ] NO collect-then-process? (use single-pass)
 * [ ] NO View/Query in Helper? (only pure math)
 * [ ] NO `bool has_*` for type categories in Components? (use Tags!)
 * [ ] NO `bool is_*` for type categories in Components? (use Tags!)
 * [ ] NO `uint8_t *_type` field with if-chain dispatch? (use Tag-filtered Views!)
 * [ ] Type determined by Tag composition, not boolean field?
 * [ ] N-item support via Entity-per-Item + Tags, not type booleans?
 * [ ] Tag-filtered Views per type, not if-chain in single loop?
 * [ ] NO Entity-per-Character pattern when loading strings?
 * [ ] String loading uses char[N] fixed arrays or Pointer Pattern?
 * [ ] String hashing via entt::hashed_string for lookup keys?
 * [ ] String data stored as single attribute, not per-character entities?
 * [ ] NO std::shared_ptr in Components? (use Flyweight Pattern!)
 * [ ] NO void* in Components? (use Flyweight Pattern!)
 * [ ] NO static std::unordered_map for resource storage? (use ResourceManager via ctx!)
 * [ ] External resources (shared_ptr, handles) accessed via registry.ctx().get<ResourceManager&>()?
 * [ ] ResourceManager registered in on_start() via registry.ctx().emplace<ResourceManager&>()?
 * [ ] Components store ONLY uint32_t IDs referencing external resources?
 */

// INCLUDES - ONLY THESE ARE ALLOWED!
// FORBIDDEN: <vector>, <map>, <unordered_map>, <optional>, <algorithm>
// ALLOWED:   <cstdint>, <cmath>, <cassert>, ase-* headers

// Own header FIRST
#include <ase/log/systems/log_sys.hpp>

#include <ase/log/log.hpp>
#include <ase/log/log_lifecycle.hpp>  // install_capture_logger, finalize_logger_after_boot
#include <ase/log/log_module.hpp>
// Die Rotations- und Aufbewahrungs-SSOT.
#include <ase/log/log_quota.hpp>
#include <ase/log/internal/log_files.hpp>
#include <ase/log/internal/log_resource_manager.hpp>
#include <ase/containers/vector.hpp>
// DIE REGISTRY DER BIBLIOTHEK, hier SELBST eingebunden: diese Datei ruft register_logger und
// shutdown. log.hpp zieht den Dachheader nicht — die Zeile steht bei der EINEN Datei, die die
// Registry wirklich braucht, nicht in jeder Uebersetzungseinheit, die eine Zeile schreibt.
#include <spdlog/spdlog.h>
#include <spdlog/sinks/callback_sink.h>
#include <spdlog/sinks/ringbuffer_sink.h>

// Die Formatierer samt ihren Musterflaggen, <ase/log/log_display.hpp> und <ase/utils/strops.hpp>
// stehen in internal/log_resource_manager.cpp bei den Fabriken selbst; diese Datei ruft nur
// make_file_formatter und make_ring_formatter.

namespace ase::log {
using namespace entt::literals;  // For "_hs hashed strings (Hub)

// Die Capture-Klammer (capture_begin/count/entry/end) und ihre Queue-Senke liegen in
// src/log_capture.cpp, nicht hier: eine Senke, die den Logausgang fuer die Dauer einer
// Fortschrittstabelle umhaengt, ist eine eigene Aufgabe.

// Projektwurzel, Logverzeichnis, Rotationsquota, das Aufraeumen alter Dateien und die Politik
// der Datei-Senke stehen in src/log_files.cpp.
//
// DER NAME, DER VON HIER GEBRAUCHT WIRD, IST internal::resolve_log_path, und er steht in
// <ase/log/internal/log_files.hpp>. internal::make_rotating_file_sink steht in
// <ase/log/internal/log_resource_manager.hpp>.
//
// Die oeffentliche Quota-API (log_dir_path, log_quota, set_log_quota) wird in dieser Datei gar
// nicht gerufen — sie gehoert Aufrufern ausserhalb.

using internal::make_rotating_file_sink;
using internal::resolve_log_path;

// Die Musterflaggen ColoredLevelFlag (%*), PlainLevelFlag (%#) und SemanticMessageFlag (%~)
// stehen in src/log_capture.cpp. Sie sind die EINE Stelle, an der die Bytes eines Kanals
// entstehen, und bewusst spdlog-gebunden — die Bibliothek verlangt fuer eine Musterflagge
// Vererbung UND clone(), beides ist ohne sie nicht zu haben. Die Formatierer, die sie anhaengen,
// stehen bei ihnen — vier Fabriken fuer vier Kanaele.

/**
 * Anonymous namespace for helper FUNCTIONS (NOT static!)
 * IMPORTANT: Use anonymous namespace, NOT static keyword!
 *   ✅ namespace { void helper() {...} }   // CORRECT
 *   ❌ static void helper() {...}          // WRONG!
 * NO STRUCTS HERE! Structs = Data = Components!
 */
namespace {

}  // namespace

// DER TYPISIERTE HTTP-RING, DER AUFNAHME-RING DER STARTPHASE UND DIE DREI GEPARKTEN SENKEN liegen
// im ResourceManager. Sie halten Fremdobjekte mit eigener Lebenszeit — genau der Fall, fuer den
// WRFL_ASE_FLYWEIGHT.md Section 1 den ResourceManager vorsieht: der Besitz lebt ausserhalb, der
// Zugriff laeuft ueber eine Fassade. Erreichbar ueber internal::log_resources(). Dasselbe gilt
// fuer den Zeilenzaehler: das System haelt keinen Zustand.
//
// DIE PARK-LOGIK steht am Manager: on_start BAUT alle drei echten Senken, haengt sie aber NICHT
// ein. Waehrend des Startblocks traegt der Logger NUR den Aufnahme-Ring, sodass jede Zeile — auch
// die aus dem on_start jedes anderen Systems — in dasselbe eine Ziel faellt: kein Kanal ist halb
// offen, keiner verliert Eintraege, nichts mischt sich in die Fortschrittstabelle auf stdout.
// finalize_logger_after_boot (src/log_boot.cpp) haengt dann alle drei auf einmal ein und spielt
// den Ring in jede hinein, sodass Datei, HTTP-Ring und Zaehler denselben Inhalt tragen.
// KEINE stdout-Senke (Betreiber-Entscheid 2026-08-11): das Logverzeichnis ist das einzige Ziel,
// eine Konsole liest es als tail auf der Datei.

// SYSTEM IMPLEMENTATION (ORDER: on_start → tick → on_stop)
// ALL THREE METHODS MUST BE IMPLEMENTED - NO EXCEPTIONS!

void LogSystem::on_start(ecs::Registry& registry) {
    // LogConfig is authoritative — set by KernelCmdSystem from argv[0] +
    // ASE_HTTP_PORT. Guaranteed present because LogSystem runs after
    // KernelCmdSystem via run_after in LogModule::build. No fallback — a
    // missing LogConfig is a boot bug and must fail fast (entt-assert).
    auto& cfg = registry.ctx().get<LogConfig>();
    // cfg.label is a char[] already; it is carried as a plain pointer, and the pattern is
    // assembled by internal::make_file_formatter. The std::string appears only where spdlog's
    // set_pattern demands one, at the library boundary.
    // DAS ETIKETT BLEIBT EIN ZEIGER, und das ist kein Detail: ein std::string an dieser Stelle
    // erzeugte HIER eine Umwandlung — STD_STRING_FORBIDDEN, an genau der Stelle, an der ein
    // Stack-Puffer sie vermeiden soll. Die Fabrik nimmt einen const char* und bildet den
    // std::string erst an der Bibliotheksgrenze, wo spdlog::set_pattern ihn ohnehin verlangt.
    const char* lbl = cfg.label[0] != '\0' ? cfg.label : "SERVER";

    // The log path is resolved relative to the project root (not cwd) and goes straight into the
    // sink that opens it; no copy of it outlives this call.
    // NO truncate here. The rotating sink below opens with rotate_on_open, which turns the previous
    // run into <name>.1.log instead of erasing it. Emptying the file first would make that rotation
    // a no-op (it only fires on a non-empty file) and would destroy the previous run's evidence on
    // every restart — including the crash that caused the restart.

    // Build server sinks — per-server file, HTTP-ringbuffer, counting.
    //
    // KEIN STDOUT-SINK (Betreiber-Entscheid 2026-08-11): alle Logs leben AUSSCHLIESSLICH im
    // Logverzeichnis, rotiert vom EINEN Datei-Sink. Eine Parallelausgabe auf stdout bog unter
    // systemd mit StandardOutput=append denselben Strom in eine zweite, rotationslose Datei - der
    // Deskriptor folgte beim spdlog-Rename dem Inode in die .1/.2/.3 und schrieb dort 83 GB
    // Phantom (gemessen 2026-08-11, Platte 100%). Eine Konsole liest das Log als tail auf der
    // Datei - dieselben Bytes, ein Schreibweg.
    // VOLLES FARB-FORMAT IN DER DATEI (Betreiber-Entscheid 2026-08-11): die Konsole IST ein
    // tail auf diese Datei, und die Farben sind Systemsprache - Level-Farbe und grauer
    // Zeitstempel gehoeren deshalb hierher. Die Modul-Inline-Farben (Teil von %v) stehen
    // ohnehin in der Datei; das Format ist durchgehend farbcodiert. Die Bytes selbst stehen an
    // EINER Stelle: internal::make_file_formatter, dieselbe, aus der auch der Edge-Daemon seinen
    // Formatierer bekommt.
    auto server_file_sink = make_rotating_file_sink(resolve_log_path(cfg.log_file));
    server_file_sink->set_formatter(internal::make_file_formatter(lbl));

    // HTTP-endpoint ringbuffer (served by /api/logs). Separate from the
    // transient capture-ring: this one stays for the process lifetime.
    auto http_ring = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(cfg.ringbuffer_size);
    internal::log_resources().store_http_ring(http_ring);
    http_ring->set_formatter(internal::make_ring_formatter());

    // Zaehl-Senke: eine Zeile, ein Hochzaehlen. OHNE Einfangen — der Zaehler liegt im
    // ResourceManager, und internal::log_resources() erreicht ihn ohne Registry und ohne
    // Capture. callback_sink_mt bringt dieselbe Sperre mit, die vorher base_sink<std::mutex>
    // hielt; der Zaehler bleibt atomar und ueberlebt die Senke.
    auto counting_sink = std::make_shared<spdlog::sinks::callback_sink_mt>(
        [](const spdlog::details::log_msg&) {
            internal::log_resources().increment_counter();
        });

    // Park ALL real sinks. The logger keeps only the capture_ring during
    // the Schedule-Bootstrap block — one single capture, no channel is
    // live yet. finalize_logger_after_boot attaches all four at once and
    // replays the capture_ring into each of them, so every sink ends up
    // 100% identical.
    auto& res = internal::log_resources();
    res.park_file_sink(server_file_sink);
    res.park_http_sink(http_ring);
    res.park_counting_sink(counting_sink);

    if (!internal::logger_slot()) {
        // Safety net for callers that never invoked install_capture_logger
        // (unit tests etc.): create a minimal logger on the real sinks.
        ase::containers::Vector<spdlog::sink_ptr> all{server_file_sink, http_ring, counting_sink};
        internal::logger_slot() = std::make_shared<spdlog::logger>("ASE", all.begin(), all.end());
    }
    // else: leave the logger alone — it currently has [capture_ring] only,
    // exactly right for the boot phase.
    internal::logger_slot()->set_level(spdlog::level::trace);
    internal::logger_slot()->flush_on(spdlog::level::trace);
    spdlog::register_logger(internal::logger_slot());
    // Der ResourceManager haelt den Logger MIT. Ohne diese Ablage liefert get_logger() dauerhaft
    // nullptr, und eine Ablage ohne Schreiber meldet sich nie: kein Tor sieht sie, kein Compiler,
    // und der erste Leser haelt das nullptr fuer "noch nicht initialisiert".
    // Die Ablage steht HIER und nicht an den Standalone-Einstiegen, weil nur fuer diesen
    // Pfad der Aufraeumweg belegt ist: on_stop setzt den Logger-Slot zurueck und ruft
    // clear_all(), das logger_ und client_logger_ mit freigibt.
    res.store_logger(internal::logger_slot());

    // Client logger — no stdout sink here either (Betreiber-Entscheid 2026-08-11, ein
    // Schreibweg). The client logger SHARES the server's rotating file sink instead of opening a
    // second handle on the same path. Two rotating sinks on one file would rename each other's
    // generations and write through independent offsets, corrupting both. Sharing gives one
    // writer, one rotation state and one lock.
    ase::containers::Vector<spdlog::sink_ptr> client_sinks{server_file_sink};
    internal::client_logger_slot() = std::make_shared<spdlog::logger>("client", client_sinks.begin(), client_sinks.end());
    internal::client_logger_slot()->set_level(spdlog::level::debug);
    internal::client_logger_slot()->flush_on(spdlog::level::trace);
    spdlog::register_logger(internal::client_logger_slot());
    res.store_client_logger(internal::client_logger_slot());

    // LAST line of on_start, not the first: this system BUILDS the logger it logs
    // through. Every sink above must exist before the first call, otherwise the
    // line goes to the null-logger gate in log.hpp and is silently dropped.
    log::debug("[LogSystem] Started");
}

// Diese Datei traegt den AUFBAU des Loggings — Senken, Logger, Formatierer, Lebenszyklus. Der
// LESEZUGRIFF auf den Ring ("welche Zeilen hat es geschrieben") ist die zweite Frage und steht in
// src/log_query.cpp (recent_logs).

void LogSystem::tick(ecs::Registry& /*registry*/, float /*dt*/) {
    // Kein Arbeitsschritt je Tick: die Senken arbeiten im Aufruf von log::*, nicht im Takt.
}

void LogSystem::on_stop(ecs::Registry& /*registry*/) {
    // FIRST line here, mirroring the LAST line of on_start: this method tears the
    // logger down. After spdlog::shutdown() below there is no sink left to write to,
    // so a closing line placed at the end would be dropped instead of recorded.
    log::debug("[LogSystem] Stopped");

    if (internal::logger_slot()) {
        internal::logger_slot()->flush();
    }
    if (internal::client_logger_slot()) {
        internal::client_logger_slot()->flush();
    }
    // Drop every logger from spdlog's global registry, then release ALL sink/logger shared_ptrs
    // HERE (controlled downstrap), so none survive into the C++ static-destruction phase at
    // process exit. If any lingered, its exit-time destructor would race spdlog's own static
    // registry destructor across translation units and free the same spdlog logger/sink twice —
    // the "double free or corruption (!prev)" abort in __run_exit_handlers (valgrind-confirmed:
    // blocks alloc'd in LogSystem::on_start, freed twice at exit). Resetting only the server
    // logger would leave the client logger and every sink to the exit phase.
    // spdlog::shutdown() is a superset of drop_all(): it also resets the periodic flusher and
    // the global thread pool, and tears down the registry singleton's internal state. drop_all()
    // alone leaves spdlog-owned static state (a 45-byte logger-name block, valgrind-confirmed) to
    // be freed in the C++ static-destruction phase at exit, racing the registry's own static
    // dtor. Releasing it HERE, in the controlled downstrap, removes it.
    spdlog::shutdown();
    internal::logger_slot().reset();
    internal::client_logger_slot().reset();
    // Der Manager haelt die uebrigen Zeiger (HTTP-Ring, Aufnahme-Ring, drei geparkte Senken).
    // Er wird HIER geleert, aus demselben Grund wie die Slots darueber: im kontrollierten
    // Abstieg, nicht in der statischen Zerstoerung.
    internal::log_resources().clear_all();
}

}  // namespace ase::log
