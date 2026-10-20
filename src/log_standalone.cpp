/**
 * ASE CORE INFRASTRUCTURE IMPLEMENTATION
 *
 * @file        log_standalone.cpp
 * @brief       Logger fuer Programme OHNE ECS: der Edge-Daemon und die TUI
 * @description Zwei Einstiege, die einen fertigen Logger bauen, ohne dass es einen Kernel, eine
 *              Registry oder ein System gibt. Beide spiegeln das Dateiformat von
 *              LogSystem::on_start byteweise, damit kundenseitige Werkzeuge dieselben Zeilen
 *              schreiben wie engine, replica und world.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    process/computation
 * @usecase     usecase:100361 - open the log sinks
 * @created     2026-08-30
 * @modified    2026-10-11
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
 * WARUM ES DIESE DATEI GIBT
 *
 *   Der Abnehmer trennt sie sauber vom Rest: LogSystem bekommt eine Registry und laeuft in
 *   Schedule::Initialization; diese Funktionen laufen in Prozessen, in denen es beides NIE gibt
 *   — tools/ase-edge-daemon und tools/ase-cli.
 *
 *   Das ist keine Laengenfrage. Ein Logger, der aus einer LogConfig in der Registry gebaut wird,
 *   und einer, der aus drei Argumenten einer freien Funktion entsteht, beantworten dieselbe Frage
 *   fuer zwei verschiedene Welten.
 *
 * ZWEI SENKEN, ZWEI ORTE — UND DER GRUND STEHT NICHT HIER
 *
 *   Die rotierende Datei-Senke bauen beide Funktionen ueber internal::make_rotating_file_sink;
 *   sie liegt in internal/log_resource_manager.hpp, weil on_start sie ebenfalls braucht und eine
 *   Funktion im anonymen Namensraum keine Dateigrenze ueberschreitet. Die Rueckruf-Senke der TUI
 *   baut internal::make_callback_sink aus derselben Fabrik.
 */

#include <ase/log/log.hpp>
#include <ase/log/log_lifecycle.hpp>  // die Deklarationen der drei Einstiege dieser Datei

#include <ase/log/internal/log_files.hpp>
#include <ase/log/internal/log_resource_manager.hpp>
#include <ase/containers/vector.hpp>

#include <spdlog/sinks/callback_sink.h>

#include <cstdint>
#include <memory>
#include <string>

namespace ase::log {

using internal::make_file_formatter;
using internal::make_rotating_file_sink;
using internal::make_tui_callback_formatter;
using internal::make_tui_file_formatter;
using internal::resolve_log_path;

// KEINE EIGENE SENKENKLASSE: eine Senke, die je Zeile GENAU EINE Sache tut, braucht dafuer keine
// eigene Klasse, sondern einen Rueckruf. `spdlog::sinks::callback_sink_mt` ist genau das.
//
// GEMESSEN, und es ist die Hausform des ganzen Baums: es gibt KEINE EINZIGE Ableitung von einer
// Fremdbibliothek. ase-network haengt seine Rueckrufe an libdatachannel (onMessage/onOpen) ohne
// eine Klasse abzuleiten, ase-websocket und ase-mongodb ebenso.
//
// Die Sperre bleibt: `callback_sink_mt` ist selbst die _mt-Fassung und haelt dieselbe Sperre wie
// `base_sink<std::mutex>`. ase-log ist der EINZIGE Weg, aus einem fremden Thread zu loggen; die
// Sperre steht dort, wo die Bibliothek sie ohnehin fuehrt.

// DIE RUECKRUF-SENKE baut internal::make_callback_sink; die Begruendung, warum sie ihren
// Formatierer selbst haelt statt ihn der Basisklasse zu ueberlassen, steht bei der Funktion.
//
// DER GRUND IST DIE WIDMUNG, nicht die Laenge: diese Datei sagt, WELCHE Senken ein Programm
// ohne ECS-Welt bekommt. WIE eine Senke gebaut wird, sagt die Senken-Fabrik — dieselbe, aus der
// die rotierende Datei-Senke kommt.

// Standalone server-style logger for non-ECS binaries (edge daemon). Mirrors the LogSystem::on_start
// file formatter byte-for-byte so customer-side tools log identically to engine/replica/world,
// but with no capture-ring/CountingSink/HTTP-ring (no /api/logs consumer) and no registry dependency.
//
// EIN SCHREIBWEG, WIE BEI JEDEM TIER (Betreiber-Entscheid 2026-08-11): das Logverzeichnis ist das
// Ziel, der rotierende Datei-Sink der einzige Schreiber, eine Konsole LIEST die Datei per tail. Ein
// stdout_color_sink als zweite Ausgabe daneben waere dieselbe Parallelklasse, die als
// systemd-append 83 GB Phantom geschrieben hat. Die Datei ist deshalb PFLICHT, nicht Option:
// ohne Pfad haette dieses Werkzeug gar kein Ziel und waere still (verbotener stiller Bereich).
void init_server_standalone(const std::string& name, const std::string& label, const std::string& log_file) {
    // EIN Zugriff auf den Logger-Platz, nicht vier: die Referenz zeigt auf denselben Zeiger, und
    // lesbar wird sie durch den Namen, nicht durch die Wiederholung.
    auto& active_logger = internal::logger_slot();
    if (active_logger) return;  // idempotent — matches inline init() guard

    if (log_file.empty()) {
        // Kein Ziel = keine Stimme. Ein Werkzeug ohne Logpfad ist ein Aufrufer-Fehler und wird
        // hier sichtbar, statt still zu verschwinden.
        //
        // WARUM HIER EINE SENKE ENTSTEHT UND SOFORT WIEDER VERSCHWINDET, statt dass eine Zeile
        // an stderr geht: ein direkter Ausgabeaufruf laeuft am Kategoriesystem vorbei — er
        // erscheint in keiner --log-Auswahl, landet in keiner Logdatei und ist im Betrieb genau
        // dann unsichtbar, wenn man ihn braucht.
        //
        // EIN BLOSSES log::error WAERE ABER SCHLIMMER, und das ist gemessen, nicht vermutet:
        // JEDE error-Ueberladung in log.hpp steht hinter `if (internal::logger_slot())`. An
        // dieser Stelle ist der Slot GARANTIERT leer — die Funktion kehrt wenige Zeilen weiter
        // oben zurueck, falls er es nicht ist. Die Meldung waere also spurlos verschwunden, und
        // eine fehlende Zeile sieht wie ein ruhiger Normalfall aus. Das ist der teurere
        // Fehlermodus, nicht der billigere.
        //
        // Deshalb: eine Konsolensenke fuer genau diese eine Zeile, danach den Slot wieder
        // raeumen. Das Raeumen ist kein Aufraeumen, sondern PFLICHT — bliebe der Notlogger
        // stehen, wiese der Idempotenz-Waechter oben einen SPAETEREN Aufruf mit gueltigem Pfad
        // ab, und das Werkzeug schriebe fuer den Rest seines Laufs in eine Konsole statt in
        // seine Datei. Der Betreiber-Entscheid "ein Schreibweg" bleibt dadurch unberuehrt: er
        // regelt den Normalbetrieb, und hier gibt es gerade kein Ziel, das er regeln koennte.
        //
        // CONFIG_MISSING und nicht RESOURCE_UNAVAIL: der Pfad wurde nie GESETZT. Er ist nicht
        // vorhanden-aber-unbrauchbar, und es hat auch niemand vergeblich nach ihm gefragt.
        internal::install_logger(name, internal::make_console_sink("%v"), kLogLevelTrace);
        log::error(log::ERR::CAT::CONFIG_MISSING, "ase-log", "log_file");
        internal::logger_slot().reset();
        return;
    }

    internal::SinkSet sinks;
    {
        // NO truncate: the rotating sink opens with rotate_on_open, turning the previous run into
        // name.1.log instead of erasing it. Emptying the file first would disable that rotation,
        // because it only fires on a non-empty file.
        //
        // VOLLES FARB-FORMAT: die Konsole ist ein tail auf diese Datei, die Farben sind
        // Systemsprache - identisch zum Tier-Datei-Sink in LogSystem::on_start. "Identisch" ist
        // keine Zusage, sondern dieselbe Funktion: beide rufen internal::make_file_formatter.
        auto file_sink = make_rotating_file_sink(resolve_log_path(log_file));
        file_sink->set_formatter(make_file_formatter(label.c_str()));
        sinks.add(file_sink);
    }

    // Beide Stufen auf TRC: was geschrieben wird, und ab wann es den Puffer verlaesst. Ein
    // Werkzeug, das abstuerzt, naehme sonst genau die letzten Zeilen mit, die den Absturz
    // erklaeren. Die Begruendung steht einmal bei install_logger und wird hier nicht wiederholt.
    //
    // DER ABLEGEWEG: install_logger ruft store_logger, also haelt der ResourceManager mit —
    // sonst bliebe sein Fach auf diesem Pfad DAUERHAFT leer, denn LogSystem::on_start laeuft in
    // einem CLI-Werkzeug NIE, es gibt keinen Kernel. Er tut es unter dem Mutex des Managers statt
    // ueber die nackte Slot-Referenz.
    //
    // DIE FEHLENDE SYMMETRIE ZUM SERVERPFAD IST ABSICHT, KEIN VERSAEUMNIS: der Server braucht
    // clear_all(), weil er lange laeuft und Teilsysteme neu startet — dort waere ein gehaltener
    // shared_ptr eine verlaengerte Lebensdauer im Betrieb. Ein CLI-Werkzeug laeuft EINMAL und
    // endet; das Prozessende IST der Aufraeumweg, und das Betriebssystem gibt alles frei.
    // Hier ein on_stop nachzutragen ist deshalb nicht moeglich und auch nicht noetig.
    internal::install_logger(name, sinks, kLogLevelTrace, kLogLevelTrace);
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

    internal::SinkSet sinks;

    if (!log_file.empty()) {
        // NO truncate: the rotating sink opens with rotate_on_open, turning the previous run into
        // name.1.log instead of erasing it. Emptying the file first would disable that rotation,
        // because it only fires on a non-empty file.
        auto file_sink = make_rotating_file_sink(resolve_log_path(log_file));
        file_sink->set_formatter(make_tui_file_formatter(label.c_str()));
        sinks.add(file_sink);
    }

    // Warum der Rueckruf-Formatierer OHNE eigenes Zeilenende gebaut wird, steht bei
    // internal::make_tui_callback_formatter und wird hier nicht wiederholt.
    sinks.add(internal::make_callback_sink(callback, user, make_tui_callback_formatter(label.c_str())));

    // Wie im Serverpfad daneben: install_logger ruft store_logger, der ResourceManager haelt also
    // mit — sonst bliebe sein Fach auf diesem Pfad DAUERHAFT leer, denn LogSystem::on_start laeuft
    // in einem CLI-Werkzeug NIE, es gibt keinen Kernel. Die Ablage ist EIN Aufruf: install_logger
    // legt den Logger ab, und kein zweiter store_logger folgt ihm.
    //
    // DIE FEHLENDE SYMMETRIE ZUM SERVERPFAD IST ABSICHT, KEIN VERSAEUMNIS: der Server braucht
    // clear_all(), weil er lange laeuft und Teilsysteme neu startet — dort waere ein gehaltener
    // shared_ptr eine verlaengerte Lebensdauer im Betrieb. Ein CLI-Werkzeug laeuft EINMAL und
    // endet; das Prozessende IST der Aufraeumweg, und das Betriebssystem gibt alles frei.
    // Hier ein on_stop nachzutragen ist deshalb nicht moeglich und auch nicht noetig.
    internal::install_logger(name, sinks, kLogLevelTrace, kLogLevelTrace);
}

/*
 * DER DRITTE UND EINFACHSTE EINSTIEG.
 *
 * WARUM GENAU DIESE DATEI, und die Antwort ist die WIDMUNG, nicht die Bequemlichkeit: ihr Kopf
 * sagt "Logger fuer Programme OHNE ECS". Das ist wortgleich der Zweck von `init` — der eigene
 * Doxygen-Text nennt ihn "for CLI tools that don't have an ECS World". Die drei Einstiege
 * gehoeren zusammen; sie unterscheiden sich nur im Format, das sie aufsetzen.
 *
 * `init` und `shutdown` sind NICHT inline in log.hpp: dort machten sie den Header, den fast jede
 * Uebersetzungseinheit einbindet, von der Bibliothek abhaengig — wegen zweier Funktionen, die
 * eine Handvoll Programme rufen. `init` bleibt idempotent ueber den Logger-Test, schreibt das
 * Muster "%v" und setzt seine Stufe; `shutdown` leert und loest den Slot.
 */

/**
 * @brief Initialize logger for CLI tools (no ECS required)
 * @param name Logger name (e.g., "ase-codegen")
 *
 * Use this for CLI tools that don't have an ECS World.
 * For ECS-based apps, LogModule registers LogSystem, whose on_start opens the sinks.
 */
void init(const std::string& name) {
    if (internal::logger_slot()) return;  // Already initialized

    // Die Senke kommt aus der Fabrik, statt hier gebaut zu werden. Das ist die Widmung dieser
    // Datei: sie sagt, WELCHEN Logger ein Programm ohne ECS-Welt bekommt — nicht, WIE eine Senke
    // konstruiert wird. Das Muster "%v" ist die nackte Zeile ohne Zeitstempel und Stufe, weil ein
    // CLI-Werkzeug seine Ausgabe selbst formatiert.
    auto console_sink = internal::make_console_sink("%v");

    // Die Stufe geht als ZAHL hinein (log.hpp's kLogLevelInfo, ein uint8_t). Diese Datei nennt
    // damit keinen Typ der Logbibliothek — weder die Senke, noch den Logger, noch die
    // Stufenkonstante. Das ist die Widmung, nicht das Vermeiden eines Befundes: sie sagt, WELCHEN
    // Logger ein Programm ohne ECS-Welt bekommt; WIE einer entsteht, sagt der Manager, dem der
    // Slot gehoert.
    internal::install_logger(name, console_sink, kLogLevelInfo);
}

/**
 * @brief Shutdown logger (for CLI tools)
 */
void shutdown() {
    if (internal::logger_slot()) {
        internal::logger_slot()->flush();
        internal::logger_slot().reset();
    }
}

}  // namespace ase::log
