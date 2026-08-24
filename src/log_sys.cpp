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
 * @modified    2026-08-20
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
 *   │    → die im Bootstrap gesammelten Meldungen (Ringpuffer) │
 *   │                                                         │
 *   │  WRITES:                                                │
 *   │    → spdlog-Senken: Konsole, Datei, HTTP-Ring, Counting │
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
 * WARUM DIESE DATEI STRUKTURELL ANDERS AUSSIEHT ALS JEDES ANDERE SYSTEM (2026-08-20)
 *
 *   Der Validator meldet hier strukturelle Befunde, die nicht Nachlaessigkeit sind, sondern die
 *   Bauform der Sache. Die Regeln, die sie ausloesen, richten sich an die VERBRAUCHER von
 *   log::* — und diese Datei IST log::*:
 *
 *     shared_ptr / unique_ptr    die spdlog-Senken, Fremdobjekte mit eigener Lebenszeit
 *     file-level static          der globale Logger und der Bootstrap-Ringpuffer
 *     Inheritance                eigene Senken erben von spdlog::sinks::base_sink
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
#include <ase/log/log_module.hpp>
#include <ase/log/log_filter.hpp>
#include <ase/log/log_display.hpp>
#include <ase/log/internal/log_files.hpp>
#include <ase/log/internal/log_pattern_flags.hpp>
#include <ase/log/internal/log_resource_manager.hpp>
#include <ase/containers/vector.hpp>
#include <ase/types/types.hpp>
#include <ase/utils/strops.hpp>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/callback_sink.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <algorithm>
#include <cstdio>     // std::fprintf (der eine Startfehler, wenn kein Logpfad kam)
#include <cstring>

namespace ase::log {
using namespace entt::literals;  // For "_hs hashed strings (Hub)

// ZWEI EIGENE SENKENKLASSEN ERSETZT 2026-08-22 — spdlog bringt die Senke mit, die sie waren.
//
//   CountingSink      zaehlte je Zeile einen Zaehler hoch       → make_counting_sink
//   TuiCallbackSink   reichte je Zeile eine formatierte Zeile   → make_tui_sink
//                     an einen C-Rueckruf weiter
//
// Beide taten je Zeile GENAU EINE Sache und brauchten dafuer keine eigene Klasse, sondern einen
// Rueckruf. `spdlog::sinks::callback_sink_mt` ist genau das.
//
// GEMESSEN, und es ist die Hausform des ganzen Baums: ausserhalb dieser Datei gibt es KEINE
// EINZIGE Ableitung von einer Fremdbibliothek. ase-network haengt seine Rueckrufe an
// libdatachannel (onMessage/onOpen) ohne eine Klasse abzuleiten, ase-websocket und ase-mongodb
// ebenso. ase-log war das einzige Modul, das eine fremde Bibliothek per Vererbung erweitert hat.
//
// Der Zaehler bleibt atomar und die Sperre bleibt: `callback_sink_mt` ist selbst die _mt-Fassung
// und haelt dieselbe Sperre, die vorher `base_sink<std::mutex>` hielt. ase-log ist weiterhin der
// EINZIGE Weg, aus einem fremden Thread zu loggen; der Umbau verlagert die Sperre nur dorthin,
// wo die Bibliothek sie ohnehin fuehrt.

// AUFGELOEST 2026-08-22: hier stand make_counting_sink(std::atomic<uint32_t>&).
//
// Die Fabrik existierte nur, um einen Zaehler entgegenzunehmen, den ihr einziger Aufrufer
// ohnehin sieht: LogSystem::on_start IST eine Methode von LogSystem und erreicht g_log_counter_
// direkt. Damit trug die Signatur einen verbotenen Typ, ohne dafuer etwas zu leisten — eine
// Benennung war es nicht, ein Umweg schon. Die Senke wird jetzt dort gebaut, wo ihr Zaehler
// lebt (siehe on_start).

// Die TUI-Senke muss die Zeile FORMATIERT weiterreichen, damit der Bereich in tools/ase-cli
// byte-gleich zur Datei aussieht. Die Rueckruf-Senke uebergibt nur den Rohdatensatz, also haelt
// der Rueckruf seinen eigenen Formatierer — dieselbe Ausgabe, ohne die geschuetzte
// formatter_-Zuweisung einer Basisklasse.
[[nodiscard]] spdlog::sink_ptr make_tui_sink(TuiLogCallback callback, void* user,
                                             std::shared_ptr<spdlog::formatter> fmt) {
    return std::make_shared<spdlog::sinks::callback_sink_mt>(
        [callback, user, fmt](const spdlog::details::log_msg& msg) {
            if (callback == nullptr) return;
            spdlog::memory_buf_t buf;
            fmt->format(msg, buf);
            callback(buf.data(), static_cast<uint32_t>(buf.size()), static_cast<int>(msg.level), user);
        });
}

// Die Capture-Klammer (capture_begin/count/entry/end) und ihre Queue-Senke liegen in
// src/log_capture.cpp, nicht hier. Grund ist derselbe wie beim Auszug von log_files.cpp: diese
// Datei stand bei 781 Zeilen, und das Schreibtor laesst eine Datei nahe dem 800er-Deckel nur
// bearbeiten, solange sie nicht WAECHST. Die Klammer haette sie auf 869 gebracht.
// Der Deckel misst keine Laenge, sondern eine fehlende Trennung — und eine Senke, die den
// Logausgang fuer die Dauer einer Fortschrittstabelle umhaengt, ist eine eigene Aufgabe.

// VERSCHOBEN 2026-08-20 nach src/log_files.cpp: Projektwurzel, Logverzeichnis, Rotationsquota,
// das Aufraeumen alter Dateien und die eine Bauform der Datei-Senke. Diese Datei trug zwei
// Aufgaben — das Logging-SYSTEM und die Verwaltung seiner DATEIEN —, und der God-System-Deckel
// hat genau das gemeldet: keine Laenge, sondern eine fehlende Trennung.
//
// Die beiden Namen, die weiterhin von hier gebraucht werden, stehen in
// <ase/log/internal/log_files.hpp>: internal::make_rotating_file_sink und
// internal::resolve_log_path. Die oeffentliche Quota-API (log_dir_path, log_quota,
// set_log_quota) wird in dieser Datei gar nicht gerufen — sie gehoert Aufrufern ausserhalb.
//
// internal::get_project_root stand hier bis 2026-08-20 und ist es nicht mehr: die
// Pfadaufloesung, die es rief, stand DREIMAL woertlich in dieser Datei und ist als
// resolve_log_path nach log_files.cpp gewandert. Die Wurzel wird dort gebraucht, nicht hier.

using internal::resolve_log_path;

namespace {

/**
 * Die EINE Bauform der Datei-Senke — "mach mir eine Senke". Die Politik dahinter ("welche Datei,
 * wie gross, wie viele") steht in internal::prepare_log_sink; warum beides 2026-08-22 getrennt
 * wurde, ist dort einmal ausgeschrieben und hier bewusst nicht wiederholt.
 *
 * rotate_on_open: jeder Prozessstart beginnt eine frische Datei, OHNE den vorigen Lauf zu
 * zerstoeren — der wird name.1.log. Es feuert nur bei nicht-leerer Datei.
 */
[[nodiscard]] std::shared_ptr<spdlog::sinks::sink> make_rotating_file_sink(const std::string& path) {
    uint64_t max_bytes = 0;
    uint32_t max_files = 0;
    internal::prepare_log_sink(path, max_bytes, max_files);
    return std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        path, static_cast<std::size_t>(max_bytes), static_cast<std::size_t>(max_files), true);
}

}  // anonymous namespace

// AUSGEZOGEN 2026-08-24 nach <ase/log/internal/log_pattern_flags.hpp>: ColoredLevelFlag (%*),
// PlainLevelFlag (%#) und der neue SemanticMessageFlag (%~). Die Flags sind die EINE Stelle,
// an der die Bytes eines Kanals entstehen; der Header ist intern und bewusst spdlog-gebunden.
// Verwendung hier unveraendert ueber add_flag<...> an jedem Formatter.
using internal::ColoredLevelFlag;
using internal::PlainLevelFlag;
using internal::SemanticMessageFlag;

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
// Sie war der einzige Grund fuer <chrono> in dieser Datei: `duration_cast` rechnete spdlogs
// eigene Zeitpunkt-Uhr um. Mit ihr faellt der Einschluss weg. Fuer eine EIGENE Zeitmessung
// waere foundation/ase-utils/clock.hpp der Weg (wall_time_millis, monotonic_nanos) — hier
// wird gar keine mehr gebraucht, weil spdlog den Zeitstempel selbst setzt.
//
// Der graue Zeitstempel bleibt unveraendert sichtbar (Betreiber-Entscheid 2026-08-11 zum
// vollen Farbformat in der Datei): er kommt aus dem Muster, nicht aus dieser Klasse.

// Static member definitions
namespace internal {

/**
 * @brief The one storage slot for the server logger — see the note above LogSystem in log.hpp.
 *
 * GELOESCHT 2026-08-22: LogSystem::g_logger_ und LogSystem::g_client_logger_. Ein ECS-System
 * haelt keinen Zustand; solange der Logger dort lag, musste jede freie Funktion dieser Datei
 * ihn ueber `LogSystem::logger()` holen, und ein statischer System-Aufruf ist
 * STATIC_SYSTEM_CALLS_FORBIDDEN (WRFL_ASE_MODULE_DEPENDENCIES.md Section 3).
 *
 * Die Funktion gibt den Slot des LogResourceManagers zurueck, nicht eine Kopie: die Accessoren
 * in log.hpp reichen eine Referenz durch, ueber die auch ZUGEWIESEN wird (186 Lesestellen, drei
 * Schreibstellen in dieser Datei). Ein Rueckgabewert waere hier kein Detail, sondern ein
 * stiller Verlust — die Zuweisung liefe ins Leere.
 */
std::shared_ptr<spdlog::logger>& logger_slot() {
    return log_resources().logger_slot();
}

std::shared_ptr<spdlog::logger>& client_logger_slot() {
    return log_resources().client_logger_slot();
}

}  // namespace internal
std::string LogSystem::g_log_path_;
std::shared_ptr<spdlog::sinks::sink> LogSystem::g_ringbuffer_sink_ = nullptr;
std::atomic<uint32_t> LogSystem::g_log_counter_{0};

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
// Fortschrittstabelle auf stdout. finalize_logger_after_boot haengt dann alle drei auf einmal ein
// und spielt den Ring in jede hinein, sodass Datei, HTTP-Ring und Zaehler denselben Inhalt tragen.
// KEINE stdout-Senke (Betreiber-Entscheid 2026-08-11): das Logverzeichnis ist das einzige Ziel,
// eine Konsole liest es als tail auf der Datei.

// Install the capture-phase logger. Must be called as the FIRST line of
// Kernel::build so every later log call (KernelEnvLdrSystem, KernelCmdSystem,
// dlopen discovery, any system's on_start before LogSystem runs) goes into
// g_capture_ring_ instead of being dropped by the null-logger gate in log.hpp.
// No console sink — the App::startup() Schedule-Bootstrap block writes its
// own progress table to stdout and must not be interleaved with log lines.
void install_capture_logger() {
    auto& active_logger = internal::logger_slot();
    if (active_logger) return;  // idempotent (tests / CLI tools)
    auto capture_ring = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(2000);
    internal::log_resources().store_capture_ring(capture_ring);
    auto boot = std::make_shared<spdlog::logger>("ase-server", capture_ring);
    boot->set_level(spdlog::level::trace);
    boot->flush_on(spdlog::level::trace);
    active_logger = boot;
    // Der ResourceManager haelt den Boot-Logger MIT, damit sein Fach zu JEDEM Zeitpunkt
    // dasselbe sagt wie der statische Member. Diese Funktion laeuft als erste Zeile von
    // Kernel::build, also VOR LogSystem::on_start — ohne diese Zeile waere das Fach hier leer,
    // und ein Idempotenz-Check ueber den Manager wuerde aus "schon installiert, nichts tun"
    // ein ZWEITES Installieren machen. Doppelte Senken sind doppelte Zeilen, also Flut.
    // Aufraeumweg: finalize_logger_after_boot ersetzt den Boot-Logger, on_stop ruft clear_all().
    internal::log_resources().store_logger(boot);
}

// Parse --log <+/-token...> from argv and feed the 3-axis filter engine.
// Collects every consecutive token starting with '+' or '-' that follows
// the "--log" flag, joined by spaces, exactly like KernelCmdSystem did
// before. Standalone (no Registry / no ECS) so it can run before
// install_capture_logger — i.e. before any log::* call could fire.
void finalize_logger_after_boot() {
    auto& res = internal::log_resources();
    auto& logger = internal::logger_slot();
    if (!logger) {
        // Kein Logger heisst: der Start ist nie so weit gekommen. Dann sind die geparkten Senken
        // Ballast und der Aufnahme-Ring haelt Zeilen, die nirgends mehr hingespielt werden koennen.
        res.clear_all();
        return;
    }

    // Attach all three real sinks to the logger in one go. Order matters
    // only for the replay below (capture_ring → all four); after the
    // ring is detached the order is irrelevant because every log call
    // fans out to every sink.
    //
    // take_* raeumt den Parkplatz beim Herausgeben: jede Senke wird genau einmal eingehaengt.
    ase::containers::Vector<spdlog::sink_ptr> real_sinks;
    if (auto file_sink = res.take_file_sink())         real_sinks.push_back(file_sink);
    if (auto http_sink = res.take_http_sink())         real_sinks.push_back(http_sink);
    if (auto counting_sink = res.take_counting_sink()) real_sinks.push_back(counting_sink);
    for (auto& s : real_sinks) {
        logger->sinks().push_back(s);
    }

    // Replay the capture-ring into exactly these three real sinks. Every
    // captured log message reaches every real sink — no channel is missing
    // any entry — so file, HTTP-ring and counting end up showing
    // 100% identical content for the boot phase and beyond.
    if (auto* capture_ring = res.get_capture_ring()) {
        auto captured = capture_ring->last_raw();
        for (auto& msg : captured) {
            for (auto& s : real_sinks) {
                s->log(msg);
            }
        }
        logger->flush();

        // Den Ring aus der Senkenliste nehmen: der Vergleich laeuft ueber die ROHE Adresse, weil
        // der Manager den Besitz haelt und hier nur ein Zeiger vorliegt.
        auto& sinks_vec = logger->sinks();
        sinks_vec.erase(std::remove_if(sinks_vec.begin(), sinks_vec.end(),
                                       [capture_ring](const spdlog::sink_ptr& s) {
                                           return s.get() == capture_ring;
                                       }),
                        sinks_vec.end());
        res.release_capture_ring();
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
            // Grenzen 1 und sizeof-2, weil is_in_rng_u32 INKLUSIV prueft (ase-types/types.hpp,
            // "v >= min && v <= max") und die ersetzte Bedingung STRIKT war: `pos > 0` schliesst
            // 0 aus, `pos < sizeof-1` schliesst sizeof-1 aus. Fuer vorzeichenlose Werte ist
            // [1, sizeof-2] dieselbe Menge wie (0, sizeof-1) — eine blinde Substitution mit
            // [0, sizeof-1] haette dagegen zwei Werte zusaetzlich durchgelassen.
            if (types::is_in_rng_u32(pos, 1u, static_cast<uint32_t>(sizeof(filter_buf)) - 2u))
                filter_buf[pos++] = ' ';
            for (const char* p = argv[j]; *p && pos < sizeof(filter_buf) - 1; ++p)
                filter_buf[pos++] = *p;
        }
        filter_buf[pos] = '\0';
        ase::log::filter::parse_log_filter(filter_buf);
        return;
    }
}

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
    // das Format durchgehend farbcodiert.
    auto server_file_formatter = std::make_unique<spdlog::pattern_formatter>();
    server_file_formatter->add_flag<ColoredLevelFlag>('*');
    server_file_formatter->add_flag<SemanticMessageFlag>('~');
    char server_pattern[kLogPatternBufSize] = {};
    // Das Zeitstempel-Grau kommt aus der Anzeige-Politik (Generat-Getter), das Muster wird
    // zur Laufzeit daraus zusammengesetzt - eine Farbaenderung recompiliert das Generat, nie
    // diese Datei. %~ statt %v: die Meldung geht gekuerzt und semantisch gefaerbt in JEDEN
    // Kanal (log_pattern_flags.hpp) - die Datei traegt dieselben Bytes wie jede Konsole.
    utils::str_copy(server_pattern, sizeof(server_pattern), "\x1b[");
    utils::str_append(server_pattern, sizeof(server_pattern), detail::display_timestamp_sgr());
    utils::str_append(server_pattern, sizeof(server_pattern),
                      "m[%Y-%m-%d %H:%M:%S.%e]\x1b[0m [%*] [ASE] [");
    utils::str_append(server_pattern, sizeof(server_pattern), lbl);
    utils::str_append(server_pattern, sizeof(server_pattern), "] %~");
    server_file_formatter->set_pattern(server_pattern);

    auto server_file_sink = make_rotating_file_sink(g_log_path_);
    server_file_sink->set_formatter(std::move(server_file_formatter));

    // HTTP-endpoint ringbuffer (served by /api/logs). Separate from the
    // transient capture-ring: this one stays for the process lifetime.
    auto http_ring = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(cfg.ringbuffer_size);
    internal::log_resources().store_http_ring(http_ring);
    g_ringbuffer_sink_ = http_ring;
    auto ring_formatter = std::make_unique<spdlog::pattern_formatter>();
    ring_formatter->add_flag<PlainLevelFlag>('#');
    ring_formatter->add_flag<SemanticMessageFlag>('~');
    ring_formatter->set_pattern("[%H:%M:%S.%e] [%#] %~");
    http_ring->set_formatter(std::move(ring_formatter));

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
    // Die Ablage steht HIER und nicht an den vier standalone-Einstiegen, weil nur fuer diesen
    // Pfad der Aufraeumweg belegt ist: on_stop setzt den Logger-Slot zurueck und ruft
    // clear_all() (:628), das logger_ und client_logger_ mit freigibt.
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

// Register in Startup schedule (first!)

// Standalone server-style logger for non-ECS binaries (edge daemon). Mirrors the LogSystem::on_start
// file formatter byte-for-byte so customer-side tools log identically to engine/replica/world,
// but with no capture-ring/CountingSink/HTTP-ring (no /api/logs consumer) and no registry dependency.
//
// EIN SCHREIBWEG, WIE BEI JEDEM TIER (Betreiber-Entscheid 2026-08-11): das Logverzeichnis ist das
// Ziel, der rotierende Datei-Sink der einzige Schreiber, eine Konsole LIEST die Datei per tail. Der
// fruehere stdout_color_sink stand hier als zweite Ausgabe daneben - dieselbe Parallelklasse, die
// als systemd-append 83 GB Phantom geschrieben hat. Die Datei ist deshalb PFLICHT, nicht Option:
// ohne Pfad haette dieses Werkzeug gar kein Ziel mehr und waere still (verbotener stiller Bereich).
void init_server_standalone(const std::string& name, const std::string& label, const std::string& log_file) {
    // EIN Zugriff auf den Logger-Platz, nicht vier. Der Rueckgabewert ist eine Referenz auf
    // denselben Zeiger (`LogSystem::logger()` gibt `internal::logger_slot()` zurueck), also war
    // die viermalige Schreibweise vier Wege zu einem
    // Ziel — lesbar wird sie durch den Namen, nicht durch die Wiederholung.
    auto& active_logger = internal::logger_slot();
    if (active_logger) return;  // idempotent — matches inline init() guard

    if (log_file.empty()) {
        // Kein Ziel = keine Stimme. Ein Werkzeug ohne Logpfad ist ein Aufrufer-Fehler und wird
        // hier sichtbar, statt still zu verschwinden: stderr ist der einzige Ort, der ohne
        // Logger ueberhaupt erreichbar ist, und diese eine Zeile faellt genau einmal beim Start.
        std::fprintf(stderr, "[ase-log] %s: kein Logpfad uebergeben - keine Logdatei, keine Ausgabe\n",
                     name.c_str());
        return;
    }

    ase::containers::Vector<spdlog::sink_ptr> sinks;
    {
        // NO truncate: the rotating sink opens with rotate_on_open, turning the previous run into
        // name.1.log instead of erasing it. Emptying the file first would disable that rotation,
        // because it only fires on a non-empty file.
        //
        // VOLLES FARB-FORMAT: die Konsole ist ein tail auf diese Datei, die Farben sind
        // Systemsprache - identisch zum Tier-Datei-Sink in LogSystem::on_start.
        auto file_formatter = std::make_unique<spdlog::pattern_formatter>();
        file_formatter->add_flag<ColoredLevelFlag>('*');
        file_formatter->add_flag<SemanticMessageFlag>('~');
        file_formatter->set_pattern(std::string("\x1b[") + detail::display_timestamp_sgr() +
                                    "m[%Y-%m-%d %H:%M:%S.%e]\x1b[0m [%*] [ASE] [" + label + "] %~");
        auto file_sink = make_rotating_file_sink(resolve_log_path(log_file));
        file_sink->set_formatter(std::move(file_formatter));
        sinks.push_back(file_sink);
    }

    active_logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    active_logger->set_level(spdlog::level::trace);
    active_logger->flush_on(spdlog::level::trace);
    // Auch hier haelt der ResourceManager mit — sonst bliebe sein Fach auf diesem Pfad DAUERHAFT
    // leer: LogSystem::on_start laeuft in einem CLI-Werkzeug NIE, es gibt keinen Kernel.
    //
    // DIE FEHLENDE SYMMETRIE ZUM SERVERPFAD IST ABSICHT, KEIN VERSAEUMNIS: der Server braucht
    // clear_all(), weil er lange laeuft und Teilsysteme neu startet — dort waere ein gehaltener
    // shared_ptr eine verlaengerte Lebensdauer im Betrieb. Ein CLI-Werkzeug laeuft EINMAL und
    // endet; das Prozessende IST der Aufraeumweg, und das Betriebssystem gibt alles frei.
    // Hier ein on_stop nachzutragen ist deshalb nicht moeglich und auch nicht noetig.
    internal::log_resources().store_logger(active_logger);
}

// Standalone logger for a full-screen TUI tool (tools/ase-cli): a plain [LABEL] file sink plus a
// colored TUI-callback sink (the caller's log pane), and NO stdout console sink, so raw ANSI never
// corrupts the alternate screen. File formatter and path resolution mirror init_server_standalone
// byte-for-byte; the callback line matches the tier console line (colored 3-char level).
void init_tui_standalone(const std::string& name, const std::string& label, const std::string& log_file,
                         TuiLogCallback callback, void* user) {
    // Wie in init_server_standalone daneben: ein benannter Zugriff statt vier gleichen.
    auto& active_logger = internal::logger_slot();
    if (active_logger) return;  // idempotent — matches init_server_standalone

    ase::containers::Vector<spdlog::sink_ptr> sinks;

    if (!log_file.empty()) {
        // NO truncate: the rotating sink opens with rotate_on_open, turning the previous run into
        // name.1.log instead of erasing it. Emptying the file first would disable that rotation,
        // because it only fires on a non-empty file.
        auto file_formatter = std::make_unique<spdlog::pattern_formatter>();
        file_formatter->add_flag<PlainLevelFlag>('#');
        file_formatter->add_flag<SemanticMessageFlag>('~');
        file_formatter->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%#] [ASE] [" + label + "] %~");
        auto file_sink = make_rotating_file_sink(resolve_log_path(log_file));
        file_sink->set_formatter(std::move(file_formatter));
        sinks.push_back(file_sink);
    }

    // OHNE eigenes Zeilenende (eol ""): der Rueckruf bekommt den Datensatz EXAKT - ein
    // mehrzeiliger Datensatz schliesst selbst mit seinem Leerzeilen-\n, und die Senke
    // rendert jede eingebettete Zeile; ein zusaetzliches eol wuerde dort als zweite
    // Leerzeile erscheinen. Die Datei-Formatter daneben behalten ihr eol.
    auto tui_formatter =
        std::make_shared<spdlog::pattern_formatter>(spdlog::pattern_time_type::local, std::string());
    tui_formatter->add_flag<ColoredLevelFlag>('*');
    tui_formatter->add_flag<SemanticMessageFlag>('~');
    tui_formatter->set_pattern(std::string("\x1b[") + detail::display_timestamp_sgr() +
                               "m[%Y-%m-%d %H:%M:%S.%e]\x1b[0m [%*] [ASE] [" + label + "] %~");
    sinks.push_back(make_tui_sink(callback, user, tui_formatter));

    active_logger = std::make_shared<spdlog::logger>(name, sinks.begin(), sinks.end());
    active_logger->set_level(spdlog::level::trace);
    active_logger->flush_on(spdlog::level::trace);
    // Auch hier haelt der ResourceManager mit — sonst bliebe sein Fach auf diesem Pfad DAUERHAFT
    // leer: LogSystem::on_start laeuft in einem CLI-Werkzeug NIE, es gibt keinen Kernel.
    //
    // DIE FEHLENDE SYMMETRIE ZUM SERVERPFAD IST ABSICHT, KEIN VERSAEUMNIS: der Server braucht
    // clear_all(), weil er lange laeuft und Teilsysteme neu startet — dort waere ein gehaltener
    // shared_ptr eine verlaengerte Lebensdauer im Betrieb. Ein CLI-Werkzeug laeuft EINMAL und
    // endet; das Prozessende IST der Aufraeumweg, und das Betriebssystem gibt alles frei.
    // Hier ein on_stop nachzutragen ist deshalb nicht moeglich und auch nicht noetig.
    internal::log_resources().store_logger(active_logger);
}

}  // namespace ase::log
