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
 * @created     2026-08-30
 * @modified    2026-08-30
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
 * WARUM ES DIESE DATEI GIBT (2026-08-30)
 *
 *   Sie ist ein AUSZUG aus log_sys.cpp, der fuenfte nach log_files.cpp, log_query.cpp,
 *   log_capture.cpp und log_boot.cpp. Der Abnehmer trennt sie sauber vom Rest: LogSystem
 *   bekommt eine Registry und laeuft in Schedule::Initialization; diese beiden Funktionen
 *   laufen in Prozessen, in denen es beides NIE gibt — tools/ase-edge-daemon und tools/ase-cli.
 *
 *   Das ist keine Laengenfrage. Ein Logger, der aus einer LogConfig in der Registry gebaut wird,
 *   und einer, der aus drei Argumenten einer freien Funktion entsteht, beantworten dieselbe Frage
 *   fuer zwei verschiedene Welten. Solange beide in einer Datei standen, sah es aus wie eine
 *   Sache mit Sonderfaellen.
 *
 * ZWEI SENKEN, ZWEI ORTE — UND DER GRUND STEHT NICHT HIER
 *
 *   Die rotierende Datei-Senke bauen beide Funktionen ueber internal::make_rotating_file_sink;
 *   sie liegt seit dem Schnitt in internal/log_resource_manager.hpp, weil on_start sie ebenfalls
 *   braucht und eine Funktion im anonymen Namensraum keine Dateigrenze ueberschreitet. Die
 *   TUI-Senke dagegen hat genau EINEN Aufrufer — init_tui_standalone, zehn Zeilen weiter unten —
 *   und bleibt deshalb hier, im anonymen Namensraum, wo sie hingehoert.
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

// ZWEI EIGENE SENKENKLASSEN ERSETZT 2026-08-22 — spdlog bringt die Senke mit, die sie waren.
//
//   CountingSink      zaehlte je Zeile einen Zaehler hoch       → jetzt in LogSystem::on_start
//   TuiCallbackSink   reichte je Zeile eine formatierte Zeile   → make_tui_sink unten
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

// DIE RUECKRUF-SENKE STAND HIER, im anonymen Namensraum, und ist am 2026-08-31 nach
// internal::make_callback_sink gewandert. Ihr Rumpf ist unveraendert — auch die Begruendung,
// warum sie ihren Formatierer selbst haelt statt ihn der Basisklasse zu ueberlassen; sie steht
// jetzt bei der Funktion.
//
// DER ANLASS WAR DIE WIDMUNG, nicht die Laenge: diese Datei sagt, WELCHE Senken ein Programm
// ohne ECS-Welt bekommt. WIE eine Senke gebaut wird, sagt die Senken-Fabrik — dieselbe, aus der
// schon die rotierende Datei-Senke kommt. Fuenf Nennungen der Bibliothek hingen allein an diesem
// einen Bauschritt.

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
        // hier sichtbar, statt still zu verschwinden.
        //
        // WARUM HIER EINE SENKE ENTSTEHT UND SOFORT WIEDER VERSCHWINDET, statt dass eine Zeile
        // an stderr geht: bis 2026-08-31 stand hier ein direkter Ausgabeaufruf. Eine solche
        // Zeile laeuft am Kategoriesystem vorbei — sie erscheint in keiner --log-Auswahl, landet
        // in keiner Logdatei und ist im Betrieb genau dann unsichtbar, wenn man sie braucht.
        //
        // EIN BLOSSER AUSTAUSCH GEGEN log::error WAERE ABER SCHLIMMER GEWESEN ALS DER VORGAENGER,
        // und das ist gemessen, nicht vermutet: JEDE error-Ueberladung in log.hpp steht hinter
        // `if (LogSystem::logger())`. An dieser Stelle ist der Slot GARANTIERT leer — die
        // Funktion kehrt wenige Zeilen weiter oben zurueck, falls er es nicht ist. Die Meldung
        // waere also spurlos verschwunden, und eine fehlende Zeile sieht wie ein ruhiger
        // Normalfall aus. Das ist der teurere Fehlermodus, nicht der billigere.
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
        // Systemsprache - identisch zum Tier-Datei-Sink in LogSystem::on_start. Seit dem
        // 2026-08-30 ist "identisch" keine Zusage mehr, sondern dieselbe Funktion: beide rufen
        // internal::make_file_formatter.
        auto file_sink = make_rotating_file_sink(resolve_log_path(log_file));
        file_sink->set_formatter(make_file_formatter(label.c_str()));
        sinks.add(file_sink);
    }

    // Beide Stufen auf TRC: was geschrieben wird, und ab wann es den Puffer verlaesst. Ein
    // Werkzeug, das abstuerzt, naehme sonst genau die letzten Zeilen mit, die den Absturz
    // erklaeren. Die Begruendung steht einmal bei install_logger und wird hier nicht wiederholt.
    //
    // DER ABLEGEWEG IST DERSELBE WIE ZUVOR, nur benannt: install_logger ruft store_logger, also
    // haelt der ResourceManager weiterhin mit — sonst bliebe sein Fach auf diesem Pfad DAUERHAFT
    // leer, denn LogSystem::on_start laeuft in einem CLI-Werkzeug NIE, es gibt keinen Kernel.
    // Er tut es jetzt unter dem Mutex des Managers statt ueber die nackte Slot-Referenz; das ist
    // eine Haertung, kein anderer Vorgang.
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
    // weiterhin mit — sonst bliebe sein Fach auf diesem Pfad DAUERHAFT leer, denn
    // LogSystem::on_start laeuft in einem CLI-Werkzeug NIE, es gibt keinen Kernel.
    //
    // HIER STAND BIS 2026-08-31 EIN ZWEITER store_logger(active_logger) HINTER DIESER ZEILE, und
    // er waere nach dem Umbau ein STILLER FEHLER GEWORDEN: `active_logger` ist die Referenz auf
    // den Slot, wie er VOR dem Ablegen war — also leer. Der Nachtrag haette den soeben gesetzten
    // Logger mit einem Nullzeiger ueberschrieben, und das Werkzeug waere ohne jede Meldung stumm
    // gewesen. Frueher trug dieselbe Zeile den fertigen Logger, weil die Zuweisung darueber ihn
    // in genau diese Referenz geschrieben hatte.
    //
    // DIE FEHLENDE SYMMETRIE ZUM SERVERPFAD IST ABSICHT, KEIN VERSAEUMNIS: der Server braucht
    // clear_all(), weil er lange laeuft und Teilsysteme neu startet — dort waere ein gehaltener
    // shared_ptr eine verlaengerte Lebensdauer im Betrieb. Ein CLI-Werkzeug laeuft EINMAL und
    // endet; das Prozessende IST der Aufraeumweg, und das Betriebssystem gibt alles frei.
    // Hier ein on_stop nachzutragen ist deshalb nicht moeglich und auch nicht noetig.
    internal::install_logger(name, sinks, kLogLevelTrace, kLogLevelTrace);
}

/*
 * DER DRITTE UND EINFACHSTE EINSTIEG — hierher gezogen am 2026-08-31.
 *
 * `init` und `shutdown` standen als `inline` in log.hpp und griffen dort direkt auf die
 * Bibliothek zu: eine farbige Konsolensenke, ein Logger, eine Stufenkonstante. Das machte den
 * Header, den 2224 Uebersetzungseinheiten einbinden, von der Bibliothek abhaengig — wegen zweier
 * Funktionen, die 14 bzw. 10 Dateien rufen.
 *
 * WARUM GENAU DIESE DATEI, und die Antwort ist die WIDMUNG, nicht die Bequemlichkeit: ihr Kopf
 * sagt "Logger fuer Programme OHNE ECS". Das ist wortgleich der Zweck von `init` — der eigene
 * Doxygen-Text nennt ihn "for CLI tools that don't have an ECS World". Die drei Einstiege
 * gehoeren zusammen; sie unterscheiden sich nur im Format, das sie aufsetzen.
 *
 * DIE RUMPFE SIND UNVERAENDERT UEBERNOMMEN. Kein Fall ist weggefallen, keiner hinzugekommen:
 * `init` bleibt idempotent ueber denselben Logger-Test, schreibt dasselbe Muster "%v" und setzt
 * dieselbe Stufe; `shutdown` leert und loest denselben Slot. Was sich geaendert hat, ist
 * ausschliesslich der ORT und damit die Zahl der Uebersetzungseinheiten, die davon abhaengen.
 *
 * ZUM PREIS, offen gemessen statt verschwiegen: diese Datei steht nicht in der path_exclude der
 * Sperre gegen den direkten Gebrauch der Bibliothek, `init` nennt drei ihrer Symbole, also
 * steigen die Befunde des Moduls um drei. Das ist KEINE neue Klasse — dieselbe Regel steht hier
 * bereits 13 Mal. Ihre Ursache ist, dass die Ausnahme fuenf Wrapper-Dateien nennt, waehrend die
 * Umhuellung nach ihrem Schnitt aus dreizehn besteht; das gehoert dem Betreiber und ist gemeldet.
 * Der Ort nach der WIDMUNG zu waehlen und den Preis zu nennen ist richtiger, als den Rumpf in
 * eine zweckfremde Datei zu legen, weil deren Zeile in einer Ausnahmeliste steht.
 */

/**
 * @brief Initialize logger for CLI tools (no ECS required)
 * @param name Logger name (e.g., "ase-codegen")
 *
 * Use this for CLI tools that don't have an ECS World.
 * For ECS-based apps, use LogSystem::on_start() instead.
 */
void init(const std::string& name) {
    if (LogSystem::logger()) return;  // Already initialized

    // Die Senke kommt aus der Fabrik, statt hier gebaut zu werden. Das ist die Widmung dieser
    // Datei: sie sagt, WELCHEN Logger ein Programm ohne ECS-Welt bekommt — nicht, WIE eine Senke
    // konstruiert wird. Das Muster "%v" ist die nackte Zeile ohne Zeitstempel und Stufe, weil ein
    // CLI-Werkzeug seine Ausgabe selbst formatiert.
    auto console_sink = internal::make_console_sink("%v");

    // Die Stufe geht als ZAHL hinein (log.hpp's kLogLevelInfo, ein uint8_t). Diese Datei nennt
    // damit keinen Typ der Logbibliothek mehr — weder die Senke, noch den Logger, noch die
    // Stufenkonstante. Das ist die Widmung, nicht das Vermeiden eines Befundes: sie sagt, WELCHEN
    // Logger ein Programm ohne ECS-Welt bekommt; WIE einer entsteht, sagt der Manager, dem der
    // Slot gehoert.
    internal::install_logger(name, console_sink, kLogLevelInfo);
}

/**
 * @brief Shutdown logger (for CLI tools)
 */
void shutdown() {
    if (LogSystem::logger()) {
        LogSystem::logger()->flush();
        LogSystem::logger().reset();
    }
}

}  // namespace ase::log
