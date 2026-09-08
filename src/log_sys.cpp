/**
 * ASE ECS SYSTEM IMPLEMENTATION
 *
 * @file        log_sys.cpp
 * @brief       LogSystem - brings up and tears down the logging sinks
 * @module      ase-log
 * @layer       1 (Core)
 * @category    process/computation
 * @schedule    Initialization
 * @created     2026-01-09
 * @modified    2026-08-30
 * @version     1.1.0
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
 *   Die Senken sind Fremdobjekte aus spdlog und leben in Dateibereichs-Zeigern, nicht in der
 *   Registry. Sie muessen VOR dem ersten log-Aufruf existieren und NACH dem letzten noch — also
 *   ausserhalb jeder Entity-Lebenszeit.
 *
 * WAS AM 2026-08-30 AUS DIESER DATEI GEZOGEN WURDE, UND WARUM DER ABNEHMER ES ENTSCHIED
 *
 *   Sie stand bei 767 Zeilen im Band und trug drei Dinge, die verschiedene Programme benutzen:
 *
 *     src/log_boot.cpp        install_capture_logger, finalize_logger_after_boot,
 *                             parse_cli_filter_from_argv — der UEBERGANG vom "es gibt noch
 *                             keinen Logger" zum fertigen Zustand. Laeuft VOR jedem System und
 *                             sieht nie eine Registry.
 *     src/log_standalone.cpp  init_server_standalone, init_tui_standalone — derselbe Logger
 *                             fuer Programme, in denen es KEINEN Kernel gibt (ase-edge-daemon,
 *                             ase-cli).
 *     internal/               make_rotating_file_sink — die eine Bauform der Datei-Senke, die
 *      log_resource_manager   on_start und beide Standalone-Einstiege brauchen. Sie stand im
 *                             anonymen Namensraum und konnte deshalb keine Dateigrenze
 *                             ueberschreiten; kopieren waere die zweite Wahrheit gewesen.
 *
 *   Die Logger-Slots (internal::logger_slot, client_logger_slot) sind mit demselben Schnitt nach
 *   src/log_resource_manager.cpp gegangen: sie reichen nur den Slot des Managers durch, und ein
 *   Durchreicher gehoert zu dem, wohin er reicht.
 *
 *   Was BLEIBT, ist der Systemvertrag: on_start baut die drei echten Senken und PARKT sie am
 *   ResourceManager, on_stop raeumt im kontrollierten Abstieg ab. Die andere Haelfte der
 *   Park-Logik — das Abholen und Einspielen — steht in log_boot.cpp, und der Kanal zwischen
 *   beiden ist der Manager, nicht eine gemeinsame Datei.
 *
 * WARUM DIESE DATEI STRUKTURELL ANDERS AUSSIEHT ALS JEDES ANDERE SYSTEM (2026-08-20)
 *
 *   Der Validator meldet hier strukturelle Befunde, die nicht Nachlaessigkeit sind, sondern die
 *   Bauform der Sache. Die Regeln, die sie ausloesen, richten sich an die VERBRAUCHER von
 *   log::* — und diese Datei IST log::*:
 *
 *     shared_ptr / unique_ptr    die spdlog-Senken, Fremdobjekte mit eigener Lebenszeit
 *     file-level static          der globale Logger und der Bootstrap-Ringpuffer
 *     atomic / Mutex             ase-log ist der EINZIGE Weg, aus einem fremden Thread zu
 *                                loggen; ohne Sperre waere jede Callback-Meldung ein Datenrennen
 *     Zeitbibliothek             ERLEDIGT 2026-08-22, und der Weg dorthin ist lehrreich: hier
 *                                stand zweimal eine Begruendung, warum die Umrechnung eines
 *                                fremden Zeitstempels unvermeidbar sei. Beide Fassungen waren
 *                                richtig ueber die Umrechnung und falsch ueber die Frage —
 *                                denn die Klasse, die sie brauchte, war NIE registriert. Das
 *                                Muster erzeugte denselben Zeitstempel laengst selbst.
 *                                Eine Begruendung kann in jedem Detail stimmen und trotzdem
 *                                die falsche Frage beantworten: nicht "wie rechne ich das um",
 *                                sondern "rechnet das ueberhaupt noch jemand".
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

#include <ase/log/log.hpp>
#include <ase/log/log_lifecycle.hpp>  // install_capture_logger, finalize_logger_after_boot
#include <ase/log/log_module.hpp>
// Die Rotations- und Aufbewahrungs-SSOT, seit 2026-08-31 ein eigener Header.
#include <ase/log/log_quota.hpp>
#include <ase/log/internal/log_files.hpp>
#include <ase/log/internal/log_resource_manager.hpp>
#include <ase/containers/vector.hpp>
// DIE REGISTRY DER BIBLIOTHEK, hier SELBST eingebunden seit 2026-08-31. Diese Datei ruft
// register_logger und shutdown; bis dahin bekam sie beide GESCHENKT, weil log.hpp den
// Dachheader zog. Der ist dort entfallen — und dieselbe Zeile, die 2224 Uebersetzungseinheiten
// die Registry aufdraengte, steht jetzt bei der EINEN, die sie wirklich braucht.
#include <spdlog/spdlog.h>
#include <spdlog/sinks/callback_sink.h>
#include <spdlog/sinks/ringbuffer_sink.h>

// WAS MIT DEN FORMATIERERN WEGGEFALLEN IST, gemessen und nicht geraten: <ase/log/log_display.hpp>
// (detail::display_timestamp_sgr), die drei Musterflaggen, <ase/utils/strops.hpp>
// (str_copy/str_append) und <spdlog/pattern_formatter.h>. Alle vier stehen seit 2026-08-31 in
// internal/log_resource_manager.cpp, zusammen mit den Fabriken selbst — die Zwischenstationen
// internal/log_format und internal/log_sinks gibt es nicht mehr: sie gaben den Bibliothekstyp in
// ihrer oeffentlichen Signatur zurueck und EXPORTIERTEN spdlog damit aus dem Wrapper heraus.

namespace ase::log {
using namespace entt::literals;  // For "_hs hashed strings (Hub)

// Die Capture-Klammer (capture_begin/count/entry/end) und ihre Queue-Senke liegen in
// src/log_capture.cpp, nicht hier. Grund ist derselbe wie beim Auszug von log_files.cpp: diese
// Datei stand bei 781 Zeilen, und das Schreibtor laesst eine Datei nahe dem 800er-Deckel nur
// bearbeiten, solange sie nicht WAECHST. Die Klammer haette sie auf 869 gebracht.
// Der Deckel misst keine Laenge, sondern eine fehlende Trennung — und eine Senke, die den
// Logausgang fuer die Dauer einer Fortschrittstabelle umhaengt, ist eine eigene Aufgabe.

// VERSCHOBEN 2026-08-20 nach src/log_files.cpp: Projektwurzel, Logverzeichnis, Rotationsquota,
// das Aufraeumen alter Dateien und die Politik der Datei-Senke. Diese Datei trug zwei
// Aufgaben — das Logging-SYSTEM und die Verwaltung seiner DATEIEN —, und der God-System-Deckel
// hat genau das gemeldet: keine Laenge, sondern eine fehlende Trennung.
//
// DER NAME, DER WEITERHIN VON HIER GEBRAUCHT WIRD, IST internal::resolve_log_path, und er steht
// in <ase/log/internal/log_files.hpp>. Bis 2026-08-30 nannte diese Stelle daneben ein zweites
// Mal internal::make_rotating_file_sink als dort liegend — das war FALSCH und ist gemessen:
// log_files.hpp:83 sagt selbst, in der Vergangenheitsform, "hier stand make_rotating_file_sink",
// aufgeloest am 2026-08-22 in prepare_log_sink, das nur noch die Politik traegt. Der Kopf nannte
// also einen Namen an einem Ort, an dem es ihn nicht gab, waehrend eine gleichnamige LOKALE
// Fassung zwanzig Zeilen weiter unten stand. Seit dem Schnitt gibt es den Namen wirklich, in
// <ase/log/internal/log_resource_manager.hpp>, die lokale Fassung ist dorthin gewandert.
//
// Die oeffentliche Quota-API (log_dir_path, log_quota, set_log_quota) wird in dieser Datei gar
// nicht gerufen — sie gehoert Aufrufern ausserhalb.

using internal::make_rotating_file_sink;
using internal::resolve_log_path;

// AUSGEZOGEN 2026-08-24, seit 2026-08-31 in src/log_resource_manager.cpp: ColoredLevelFlag (%*),
// PlainLevelFlag (%#) und der neue SemanticMessageFlag (%~). Die Flags sind die EINE Stelle,
// an der die Bytes eines Kanals entstehen; sie sind bewusst spdlog-gebunden — die Bibliothek
// verlangt fuer eine Musterflagge Vererbung UND clone(), beides ist ohne sie nicht zu haben.
//
// SEIT DEM 2026-08-30 WERDEN SIE HIER NICHT MEHR GENANNT: die Formatierer, die sie anhaengen,
// stehen bei ihnen — vier Fabriken fuer vier Kanaele, mit vier Aufrufstellen in drei Dateien.
// Diese Datei ruft nur noch make_file_formatter und make_ring_formatter.

// GELOESCHT 2026-08-22: GrayTimestampFlag.
//
// Die Klasse baute den grauen Zeitstempel Feld fuer Feld nach und wurde NIE registriert —
// gemessen ueber den ganzen Baum, null `add_flag<GrayTimestampFlag>`, waehrend die beiden
// Nachbarn ColoredLevelFlag und PlainLevelFlag auf fuenf Registrierungen kommen.
//
// Ihre Ausgabe erzeugen die Muster selbst, und zwar Byte fuer Byte dieselbe:
//     Klasse:  "\033[38;5;242m[%04d-%02d-%02d %02d:%02d:%02d.%03d]\033[0m"
//     Muster:  "\x1b[38;5;242m[%Y-%m-%d %H:%M:%S.%e]\x1b[0m"
// \033 und \x1b sind dasselbe Zeichen; %e IST die Millisekunde. Der Farbwert 242 steht in
// beiden. Die Klasse wurde also durch das Muster ERSETZT und nur nicht mitgeloescht.
//
// Der graue Zeitstempel bleibt unveraendert sichtbar (Betreiber-Entscheid 2026-08-11 zum
// vollen Farbformat in der Datei): er kommt aus dem Muster, nicht aus dieser Klasse.

// Static member definitions
std::string LogSystem::g_log_path_;
std::shared_ptr<spdlog::sinks::sink> LogSystem::g_ringbuffer_sink_ = nullptr;
std::atomic<uint32_t> LogSystem::g_log_counter_{0};

/**
 * Anonymous namespace for helper FUNCTIONS (NOT static!)
 * NO STRUCTS HERE! Structs = Data = Components!
 *
 * LEER SEIT DEM SCHNITT VOM 2026-08-30, und die Leere ist eine Aussage: die beiden Helfer dieser
 * Datei hatten MEHR Aufrufer als diese Datei. make_rotating_file_sink wird von on_start und von
 * beiden Standalone-Einstiegen gebraucht, der Formatierbau von vier Stellen in drei Dateien —
 * eine Funktion im anonymen Namensraum ueberschreitet aber keine Dateigrenze. Beide stehen jetzt
 * in internal/log_resource_manager.hpp und werden von hier nur gerufen.
 */
namespace {

}  // namespace

/**
 * Anonymous namespace for helper FUNCTIONS (NOT static!)
 * IMPORTANT: Use anonymous namespace, NOT static keyword!
 *   ✅ namespace { void helper() {...} }   // CORRECT
 *   ❌ static void helper() {...}          // WRONG!
 * NO STRUCTS HERE! Structs = Data = Components!
 */
// FUENF DATEIBEREICHS-ZEIGER SIND 2026-08-22 IN DEN RESOURCEMANAGER GEWANDERT.
//
// Hier standen der typisierte HTTP-Ring, der Aufnahme-Ring der Startphase und die drei geparkten
// Senken. Sie halten Fremdobjekte mit eigener Lebenszeit — genau der Fall, fuer den
// WRFL_ASE_FLYWEIGHT.md Section 1 den ResourceManager vorsieht: der Besitz lebt ausserhalb, der
// Zugriff laeuft ueber eine Fassade. Erreichbar ueber internal::log_resources().
//
// DIE PARK-LOGIK BLEIBT WORTGLEICH DIESELBE, sie steht jetzt am Manager: on_start BAUT alle drei
// echten Senken, haengt sie aber NICHT ein. Waehrend des Startblocks traegt der Logger NUR den
// Aufnahme-Ring, sodass jede Zeile — auch die aus dem on_start jedes anderen Systems — in dasselbe
// eine Ziel faellt: kein Kanal ist halb offen, keiner verliert Eintraege, nichts mischt sich in die
// Fortschrittstabelle auf stdout. finalize_logger_after_boot (src/log_boot.cpp) haengt dann alle
// drei auf einmal ein und spielt den Ring in jede hinein, sodass Datei, HTTP-Ring und Zaehler
// denselben Inhalt tragen.
// KEINE stdout-Senke (Betreiber-Entscheid 2026-08-11): das Logverzeichnis ist das einzige Ziel,
// eine Konsole liest es als tail auf der Datei.

// GELOESCHT 2026-08-22: LogSystem::LogSystem(const std::string&, const std::string&) — UNAUFRUFBAR.
// `App::add_system(Schedule)` nimmt nur den Schedule, es gibt keinen Weg, einem System
// Konstruktorargumente mitzugeben; das `add_system_with<LogSystem>` in log_module.hpp baut den
// Default. Beide Felder werden ohnehin anders gesetzt (log_file_ aus der LogConfig in on_start,
// logger_name_ bleibt "ASE").
// Von 1576 System-Headern im Baum trug GENAU DIESER einen Konstruktor mit Argumenten — der
// vermutete Torfehler war der einzige Ausreisser des Baums. Die Deklaration in log.hpp samt der
// aufruferlosen set_name/set_log_file gehoert einer anderen Sitzung und ist ihr gemeldet.

// SYSTEM IMPLEMENTATION (ORDER: on_start → tick → on_stop)
// ALL THREE METHODS MUST BE IMPLEMENTED - NO EXCEPTIONS!

void LogSystem::on_start(ecs::Registry& registry) {
    // LogConfig is authoritative — set by KernelCmdSystem from argv[0] +
    // ASE_HTTP_PORT. Guaranteed present because LogSystem runs after
    // KernelCmdSystem via run_after in LogModule::build. No fallback — a
    // missing LogConfig is a boot bug and must fail fast (entt-assert).
    auto& cfg = registry.ctx().get<LogConfig>();
    // cfg.label is a char[] already; the old code built a std::string from it only to
    // concatenate it into the pattern below. The label is now carried as a plain pointer
    // and the pattern assembled in a stack buffer with ase::utils::strops - no heap
    // allocation for a string that lives for two statements. The std::string appears only
    // where spdlog's set_pattern demands one, at the library boundary.
    // DAS ETIKETT BLEIBT EIN ZEIGER, und das ist kein Detail: die erste Fassung des Schnitts vom
    // 2026-08-30 gab internal::make_file_formatter einen std::string und erzeugte damit HIER eine
    // Umwandlung — STD_STRING_FORBIDDEN, an genau der Stelle, an der der Vorgaenger einen
    // Stack-Puffer benutzt hatte, um sie zu vermeiden. Die Fabrik nimmt jetzt einen const char*
    // und bildet den std::string erst an der Bibliotheksgrenze, wo spdlog::set_pattern ihn
    // ohnehin verlangt. Was sich gegenueber dem Vorgaenger aendert, ist nur der ORT der
    // Musterbildung, nicht ihr Typ.
    const char* lbl = cfg.label[0] != '\0' ? cfg.label : "SERVER";
    log_file_ = cfg.log_file;

    // Calculate absolute log path relative to project root (not cwd).
    g_log_path_ = resolve_log_path(log_file_);
    // NO truncate here. The rotating sink below opens with rotate_on_open, which turns the previous
    // run into <name>.1.log instead of erasing it. Emptying the file first would make that rotation
    // a no-op (it only fires on a non-empty file) and would destroy the previous run's evidence on
    // every restart — including the crash that caused the restart.

    // Build server sinks — per-server file, HTTP-ringbuffer, counting.
    //
    // KEIN STDOUT-SINK (Betreiber-Entscheid 2026-08-11): alle Logs leben AUSSCHLIESSLICH im
    // Logverzeichnis, rotiert vom EINEN Datei-Sink. Der fruehere stdout_color_sink war eine nie
    // entfernte Parallelausgabe: unter systemd bog StandardOutput=append denselben Strom in eine
    // zweite, rotationslose Datei - der Deskriptor folgte beim spdlog-Rename dem Inode in die
    // .1/.2/.3 und schrieb dort 83 GB Phantom (gemessen 2026-08-11, Platte 100%). Eine Konsole
    // liest das Log als tail auf der Datei - dieselben Bytes, ein Schreibweg.
    // VOLLES FARB-FORMAT IN DER DATEI (Betreiber-Entscheid 2026-08-11): die Konsole IST ein
    // tail auf diese Datei, und die Farben sind Systemsprache - Level-Farbe und grauer
    // Zeitstempel gehoeren deshalb hierher, sonst saehe das Fenster anders aus als zuvor.
    // Die Modul-Inline-Farben (Teil von %v) standen ohnehin immer in der Datei; jetzt ist
    // das Format durchgehend farbcodiert. Die Bytes selbst stehen seit dem 2026-08-30 an EINER
    // Stelle: internal::make_file_formatter, dieselbe, aus der auch der Edge-Daemon seinen
    // Formatierer bekommt.
    auto server_file_sink = make_rotating_file_sink(g_log_path_);
    server_file_sink->set_formatter(internal::make_file_formatter(lbl));

    // HTTP-endpoint ringbuffer (served by /api/logs). Separate from the
    // transient capture-ring: this one stays for the process lifetime.
    auto http_ring = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(cfg.ringbuffer_size);
    internal::log_resources().store_http_ring(http_ring);
    g_ringbuffer_sink_ = http_ring;
    http_ring->set_formatter(internal::make_ring_formatter());

    // Zaehl-Senke: eine Zeile, ein Hochzaehlen. OHNE Einfangen — g_log_counter_ ist ein
    // statisches Member dieser Klasse, hat also statische Lebensdauer und ist im Lambda ohne
    // Capture erreichbar. callback_sink_mt bringt dieselbe Sperre mit, die vorher
    // base_sink<std::mutex> hielt; der Zaehler bleibt atomar und ueberlebt die Senke.
    auto counting_sink = std::make_shared<spdlog::sinks::callback_sink_mt>(
        [](const spdlog::details::log_msg&) {
            g_log_counter_.fetch_add(1, std::memory_order_relaxed);
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
        ase::containers::Vector<spdlog::sink_ptr> all{server_file_sink, g_ringbuffer_sink_, counting_sink};
        internal::logger_slot() = std::make_shared<spdlog::logger>(logger_name_, all.begin(), all.end());
    }
    // else: leave the logger alone — it currently has [capture_ring] only,
    // exactly right for the boot phase.
    internal::logger_slot()->set_level(spdlog::level::trace);
    internal::logger_slot()->flush_on(spdlog::level::trace);
    spdlog::register_logger(internal::logger_slot());
    // Der ResourceManager haelt den Logger MIT — bis 2026-08-22 hatte er ein Logger-Fach
    // (store_logger/get_logger) ohne jeden Schreiber, weshalb get_logger() dauerhaft nullptr
    // lieferte. Eine Ablage ohne Schreiber meldet sich nie: kein Tor sieht sie, kein Compiler,
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

// AUSGEZOGEN 2026-08-22 nach src/log_query.cpp: parse_ring_line und LogSystem::recent_logs.
//
// Diese Datei traegt den AUFBAU des Loggings — Senken, Logger, Formatierer, Lebenszyklus. Der
// LESEZUGRIFF auf den Ring ("welche Zeilen hat es geschrieben") ist die zweite Frage, und der
// God-System-Deckel hat sie gemeldet: nicht als Laengenproblem, sondern als fehlende Trennung.
//
// Die Regel sagt dazu, was die Reihenfolge sein muss — "split FIRST, fix findings SECOND" —,
// weil die vorgeschriebenen Reparaturen Zeilen HINZUFUEGEN und eine uebervolle Datei deshalb
// beide Fronten gleichzeitig blockiert. Genau das ist hier passiert: die Aufloesung der
// Zaehl-Fabrik scheiterte am Deckel, nicht an der Sache.

void LogSystem::tick(ecs::Registry& /*registry*/, float /*dt*/) {
    // Könnte später für Batch-Logging bei Millionen Logs genutzt werden
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
    // Drop every logger from spdlog's global registry, then release ALL static sink/logger
    // shared_ptrs HERE (controlled downstrap), so none survive into the C++ static-destruction
    // phase at process exit. If any lingered, its exit-time destructor would race spdlog's own
    // static registry destructor across translation units and free the same spdlog logger/sink
    // twice — the "double free or corruption (!prev)" abort in __run_exit_handlers (valgrind-
    // confirmed: blocks alloc'd in LogSystem::on_start, freed twice at exit). Resetting only
    // the server logger alone (as before) left the client logger + every sink static dangling to exit.
    // spdlog::shutdown() is a superset of drop_all(): it also resets the periodic flusher and
    // the global thread pool, and tears down the registry singleton's internal state. drop_all()
    // alone left spdlog-owned static state (a 45-byte logger-name block, valgrind-confirmed) to be
    // freed in the C++ static-destruction phase at exit, racing the registry's own static dtor →
    // the last remaining double-free. Releasing it HERE, in the controlled downstrap, removes it.
    spdlog::shutdown();
    internal::logger_slot().reset();
    internal::client_logger_slot().reset();
    g_ringbuffer_sink_.reset();
    // Der Manager haelt seit 2026-08-22 die uebrigen fuenf Zeiger (HTTP-Ring, Aufnahme-Ring, drei
    // geparkte Senken). Er wird HIER geleert, aus demselben Grund wie die drei darueber: im
    // kontrollierten Abstieg, nicht in der statischen Zerstoerung.
    internal::log_resources().clear_all();

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

}  // namespace ase::log
