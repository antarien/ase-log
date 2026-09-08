#pragma once

/**
 * ASE RESOURCE MANAGER (NOT A COMPONENT!)
 *
 * @file        log_resource_manager.hpp
 * @brief       LogResourceManager - Flyweight Pattern fuer die spdlog-Senken und -Logger
 * @description Haelt die Besitzzeiger auf Logger und Senken AUSSERHALB des Systems, BAUT sie und
 *              haengt sie ein. Wer eine Senke oder einen Formatierer braucht, ruft hier — statt
 *              den Typ der Bibliothek selbst zu nennen.
 *
 * FLYWEIGHT PATTERN
 * - Der Manager haelt die eigentlichen Objekte, nicht das System
 * - Die Zugriffsmethoden geben rohe Zeiger heraus, nie den Besitz
 * - Erreichbar ueber log_resources(), NICHT ueber registry.ctx()
 *
 * WARUM NICHT UEBER registry.ctx(), wie das Template es vorsieht
 *
 *   Das Template setzt eine Registry voraus. ase-log hat keine, und zwar aus drei gemessenen
 *   Gruenden: `log::init` laeuft VOR Kernel::build, der Logger wird statisch herausgegeben
 *   (`LogSystem::logger()` gibt `internal::logger_slot()` zurueck, beides in log.hpp), und
 *   core/ase-convert loggt ohne jede Registry — seine erste `log::error`-Zeile steht dort
 *   sogar VOR dem eigenen `log::init`.
 *   Ein ctx()-Zugriff waere in allen drei Faellen ein Griff ins Leere.
 *
 *   Die Bauform bleibt deshalb dieselbe — Besitz ausserhalb des Systems, Zugriff ueber eine
 *   Fassade —, nur der Weg zur Fassade ist ein funktionslokaler static statt der Registry. Das
 *   ist derselbe Mechanismus, mit dem der C++-Standard die Lebensdauer garantiert, bevor main()
 *   laeuft, und er traegt genau die drei Faelle oben.
 *
 * WARUM DIE FABRIKEN HIER STEHEN UND NICHT IN EIGENEN DATEIEN (2026-08-31)
 *
 *   Sie standen einen Tag lang in internal/log_sinks.hpp mit ihrer .cpp und in
 *   internal/log_format.hpp mit ihrer .cpp, und diese Dateien trugen ihre eigene Begruendung
 *   dafuer: "ein eigener interner Header, der den fremden Typ nennen DARF, weil er nichts
 *   anderes tut. Er ist `internal/` und damit kein oeffentlicher Vertrag."
 *
 *   DIESE BEGRUENDUNG WAR FALSCH, und zwar nachweisbar: die Ausnahme von SPDLOG_DIRECT_FORBIDDEN
 *   ist NAMENTLICH, nicht nach Verzeichnis. Sie nennt fuenf Dateien — log.hpp, log_sys.cpp,
 *   diesen Header, die zugehoerige .cpp und log_capture.cpp. Ein Header steht dort nicht, nur
 *   weil er unter `internal/` liegt. Der Meldetext der Regel sagt es woertlich: "the wrapper is
 *   complete, a new file reaching for spdlog is not building it."
 *
 *   UND DER FEHLER WAR NICHT DER SCHNITT, SONDERN SEINE RICHTUNG. Die Fabriken gaben
 *   `std::shared_ptr` auf eine Senke und `std::unique_ptr` auf einen Formatierer in der
 *   OEFFENTLICHEN Signatur zurueck — sie EXPORTIERTEN die Bibliothek, statt sie zu kapseln. Ein
 *   Wrapper, dessen Header den gewrappten Typ herausreicht, ist an dieser Stelle keiner mehr.
 *   Hier ist derselbe Rueckgabetyp richtig: dieser Header IST die Innenseite.
 *
 *   Die Widmung stimmt ebenfalls, und sie ist der eigentliche Grund: der Manager HAELT die
 *   Senken (park_ und take_), er HAENGT sie ein (install_logger, attach_sinks) — dass er sie
 *   nicht auch BAUT, war die Luecke, nicht die Ordnung. Bauen, Halten und Einhaengen derselben
 *   Sache gehoeren an eine Stelle.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    ecs/module
 * @created     2026-08-22
 * @modified    2026-08-31
 * @version     1.1.0
 *
 * ECS RESOURCE MANAGER HEADER COMPLIANCE
 *
 * [ ] NOT a Component - lives outside ECS registry
 * [ ] Accessed via registry.ctx().get<ResourceManager&>()
 * [ ] Private maps for resource storage
 * [ ] Components store ONLY uint32_t IDs - N/A (no component references these)
 * [ ] Header contains ONLY declarations (implementations in .cpp!)
 * [ ] Thread-safe method signatures (const where possible)
 * [ ] store_*(), get_*(), remove_*(), has_*() method pattern
 * [ ] clear_all() for shutdown cleanup
 * [ ] Private mutex for thread safety
 * [ ] Private storage for the resources
 * [ ] NO inline implementations (prevents mass rebuilds!)
 */

// Der Behaelter von SinkSet. ase-containers ist Layer 0 und damit von hier aus erlaubt; die
// Standardbibliothek waere es an dieser Stelle nicht.
#include <ase/containers/vector.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

// Forward declaration - no need to include the heavy sink headers here. Die Senkentypen selbst
// stehen ausschliesslich in log_sys.cpp und in der zugehoerigen .cpp; dieser Header nennt sie nur.
namespace spdlog {
class logger;
class formatter;
namespace sinks {
class sink;
template <typename Mutex>
class ringbuffer_sink;
}  // namespace sinks
}  // namespace spdlog

namespace ase::log::internal {

class LogResourceManager {
public:
    /** Der aktive Server-Logger. Nullzeiger, solange keiner aufgebaut ist. */
    [[nodiscard]] spdlog::logger* get_logger() const;
    void store_logger(std::shared_ptr<spdlog::logger> logger);

    /** Der Client-Logger (ohne [SERVER]-Praefix, teilt sich die Datei-Senke des Servers). */
    [[nodiscard]] spdlog::logger* get_client_logger() const;
    void store_client_logger(std::shared_ptr<spdlog::logger> logger);

    /**
     * Der Speicherplatz selbst, als Referenz — der einzige Halter des Loggers seit 2026-08-22.
     *
     * WARUM ES DIE BEIDEN NEBEN get_logger()/store_logger() GIBT: die Accessoren
     * LogSystem::logger()/client_logger() geben eine Referenz zurueck, ueber die im Baum auch
     * ZUGEWIESEN wird (`LogSystem::logger() = boot;`). Ein Zeiger oder eine Kopie kann das nicht
     * abbilden — die Zuweisung liefe ins Leere, und zwar lautlos.
     *
     * SIE SPERREN NICHT, UND DAS IST KEINE NACHLAESSIGKEIT: eine herausgereichte Referenz kann
     * nicht geschuetzt werden, egal was die Methode tut. Der Vorgaengerzustand war ein blanker
     * statischer Member ohne jeden Schutz (LogSystem::g_logger_) — hier wird also nichts
     * schwaecher. Wer den Logger nur LIEST, nimmt get_logger(): der sperrt.
     */
    [[nodiscard]] std::shared_ptr<spdlog::logger>& logger_slot();
    [[nodiscard]] std::shared_ptr<spdlog::logger>& client_logger_slot();

    /**
     * Der Ringpuffer, den /api/logs liest — TYPISIERT gehalten, nicht als blanke Senke.
     *
     * Der Leser braucht last_formatted(), und das kennt nur der Ringpuffer-Typ. Ihn als sink zu
     * halten und beim Lesen zurueckzuverwandeln waere ein Typwechsel ohne Not; der Manager haelt
     * deshalb, was er wirklich haelt.
     */
    using RingSink = spdlog::sinks::ringbuffer_sink<std::mutex>;
    [[nodiscard]] RingSink* get_http_ring() const;
    void store_http_ring(std::shared_ptr<RingSink> ring);

    /**
     * Der Aufnahme-Ring der Startphase.
     *
     * Haelt jede Zeile zwischen install_capture_logger (erste Zeile von Kernel::build) und
     * finalize_logger_after_boot. Danach wird er zurueckgespielt und freigegeben — er ist der
     * einzige Grund, warum keine Startmeldung verlorengeht, waehrend noch keine Senke lebt.
     */
    [[nodiscard]] RingSink* get_capture_ring() const;
    void store_capture_ring(std::shared_ptr<RingSink> ring);
    void release_capture_ring();

    /**
     * Die drei geparkten Senken.
     *
     * on_start BAUT sie, haengt sie aber noch nicht ein: waehrend des Startblocks traegt der
     * Logger NUR den Aufnahme-Ring, damit keine Zeile in einen halb fertigen Kanal faellt.
     * finalize_logger_after_boot haengt alle drei auf einmal ein und spielt den Ring in jede
     * hinein, sodass Datei, HTTP-Ring und Zaehler denselben Inhalt tragen.
     *
     * Ein take_ gibt die Senke heraus UND gibt den Platz frei: sie wird genau einmal eingehaengt.
     */
    void park_file_sink(std::shared_ptr<spdlog::sinks::sink> sink);
    void park_http_sink(std::shared_ptr<spdlog::sinks::sink> sink);
    void park_counting_sink(std::shared_ptr<spdlog::sinks::sink> sink);
    [[nodiscard]] std::shared_ptr<spdlog::sinks::sink> take_file_sink();
    [[nodiscard]] std::shared_ptr<spdlog::sinks::sink> take_http_sink();
    [[nodiscard]] std::shared_ptr<spdlog::sinks::sink> take_counting_sink();

    /**
     * Fortlaufende Nummer jeder Logzeile.
     *
     * Die Zaehlung passiert IM Manager, nicht ueber einen herausgegebenen Zeiger: ase-log ist
     * der einzige Weg, aus einem fremden Thread zu loggen, also faellt das Hochzaehlen aus
     * beliebigen Threads an. Ein roher Zeiger auf den Zaehler wuerde genau die Atomaritaet
     * verlieren, die ihn richtig macht — deshalb zwei Methoden statt eines Slots.
     */
    void increment_counter();
    [[nodiscard]] uint32_t log_counter() const;

    /** Alles freigeben - aus LogSystem::on_stop, vor der statischen Zerstoerung. */
    void clear_all();

private:
    // Direkte Member wie im Template (Section 1), nicht hinter einem Zeiger versteckt: die
    // Forward-Deklarationen oben genuegen, weil shared_ptr keinen vollstaendigen Typ braucht.
    mutable std::mutex mutex_;
    std::shared_ptr<spdlog::logger> logger_;
    std::shared_ptr<spdlog::logger> client_logger_;
    std::shared_ptr<RingSink> http_ring_;
    std::shared_ptr<RingSink> capture_ring_;
    std::shared_ptr<spdlog::sinks::sink> pending_file_;
    std::shared_ptr<spdlog::sinks::sink> pending_http_;
    std::shared_ptr<spdlog::sinks::sink> pending_counting_;
    std::atomic<uint32_t> counter_{0};
};

/**
 * Die eine Fassade auf den Manager.
 *
 * Funktionslokaler static: der C++-Standard garantiert den Aufbau beim ersten Zugriff und
 * gewaehrleistet ihn threadsicher, ohne dass eine Registry existieren muss. Genau das braucht
 * ase-log, weil es vor dem Kernel laeuft.
 */
[[nodiscard]] LogResourceManager& log_resources();

/**
 * @brief Baut die rotierende Datei-Senke fuer einen Logpfad
 * @param path Pfad der Logdatei, bereits aufgeloest (siehe resolve_log_path)
 * @return Die fertige Senke, Groesse und Generationenzahl aus prepare_log_sink
 *
 * rotate_on_open: jeder Prozessstart beginnt eine frische Datei, OHNE den vorigen Lauf zu
 * zerstoeren — der wird name.1.log. Es feuert nur bei nicht-leerer Datei.
 *
 * DIE POLITIK DAHINTER (welche Datei, wie gross, wie viele Generationen) steht in
 * internal::prepare_log_sink und wird hier nur gerufen. Die Bauform stand bis 2026-08-30 im
 * ANONYMEN Namensraum von log_sys.cpp und hatte dort drei Aufrufer: LogSystem::on_start,
 * init_server_standalone und init_tui_standalone. Eine Funktion im anonymen Namensraum kann
 * keine Dateigrenze ueberschreiten, und sie zu KOPIEREN waere die zweite Wahrheit ueber denselben
 * Schritt gewesen: zwei Stellen, an denen steht, wann eine Logdatei rotiert, und die driften.
 *
 * EIN TOTER VERWEIS, DER DABEI AUFFIEL UND HIER SEINEN GRABSTEIN BEHAELT: log_sys.cpp trug bis
 * 2026-08-30 die Zeile "internal::make_rotating_file_sink und internal::resolve_log_path stehen
 * in <ase/log/internal/log_files.hpp>". Gemessen: nur der zweite Name steht dort. Der erste wurde
 * am 2026-08-22 in `prepare_log_sink` aufgeloest, das ausschliesslich die Politik traegt —
 * log_files.hpp:83 sagt es selbst, in der Vergangenheit ("hier stand `make_rotating_file_sink`").
 */
[[nodiscard]] std::shared_ptr<spdlog::sinks::sink> make_rotating_file_sink(const std::string& path);

/**
 * @brief Baut die farbige Konsolensenke fuer ein Programm ohne ECS-Welt
 * @param pattern Ausgabemuster der Bibliothek, z.B. "%v" fuer die nackte Zeile
 * @return Die fertige Senke, bereits auf das Muster gesetzt
 *
 * Zweiter Einstieg derselben Widmung wie make_rotating_file_sink darueber: WER EINE SENKE
 * BRAUCHT, RUFT HIER, statt sie selbst zu konstruieren. Das ist keine Bequemlichkeit, sondern
 * die Trennung, fuer die es diese Fabriken gibt — die Bauform einer Senke steht an EINER Stelle,
 * und die Aufrufer nennen keinen Typ der Bibliothek mehr.
 *
 * Sie schreibt auf stdout und faerbt nach Stufe. Fuer eine TUI ist sie deshalb FALSCH: rohe
 * ANSI-Sequenzen zerlegen einen Alternativschirm. Dieser Fall hat seine eigene Senke im
 * Rueckruf von init_tui_standalone, und das bleibt so.
 */
[[nodiscard]] std::shared_ptr<spdlog::sinks::sink> make_console_sink(const std::string& pattern);

/**
 * @brief Baut die Rueckruf-Senke, die jede fertig formatierte Zeile an einen Aufrufer reicht
 * @param callback Empfaengt die Zeile; ein Nullzeiger macht die Senke still, nicht ungueltig
 * @param user     Undurchsichtiger Zeiger, der unveraendert an den Rueckruf zurueckgeht
 * @param fmt      Der Formatierer, mit dem die Zeile erzeugt wird (make_tui_callback_formatter)
 * @return Die fertige Senke
 *
 * Dritter Einstieg derselben Widmung: WER EINE SENKE BRAUCHT, RUFT HIER. Sie stand bis
 * 2026-08-31 im anonymen Namensraum von log_standalone.cpp und zwang jene Datei, fuenf Typen der
 * Bibliothek zu nennen — obwohl ihre Widmung eine andere ist: sie sagt, WELCHE Senken ein
 * Programm ohne ECS-Welt bekommt, nicht wie eine Senke gebaut wird.
 *
 * WARUM SIE DEN FORMATIERER SELBST HAELT und nicht die Basisklasse ihn setzen laesst: die
 * Rueckruf-Senke der Bibliothek uebergibt nur den ROHDATENSATZ. Damit der Bereich in tools/ase-cli
 * byte-gleich zur Logdatei aussieht, muss die Zeile VOR dem Rueckruf formatiert werden — und der
 * geschuetzte formatter_ einer Basisklasse ist von aussen nicht erreichbar.
 */
[[nodiscard]] std::shared_ptr<spdlog::sinks::sink> make_callback_sink(
    void (*callback)(const char* line, uint32_t len, int level, void* user), void* user,
    std::shared_ptr<spdlog::formatter> fmt);

/**
 * @brief Baut den Datei-Formatierer eines Tiers oder eines Standalone-Werkzeugs
 * @param label Das Etikett, das in eckigen Klammern hinter [ASE] steht
 * @return Formatierer mit vollem Farbformat: grauer Zeitstempel, gefaerbte Stufe, %~-Meldung
 *
 * VOLLES FARB-FORMAT IN DER DATEI (Betreiber-Entscheid 2026-08-11): die Konsole IST ein tail auf
 * diese Datei, und die Farben sind Systemsprache. Das Zeitstempel-Grau kommt aus der
 * Anzeige-Politik (Generat-Getter), das Muster wird zur Laufzeit daraus zusammengesetzt — eine
 * Farbaenderung recompiliert das Generat, nie diese Datei. %~ statt %v: die Meldung geht gekuerzt
 * und semantisch gefaerbt in JEDEN Kanal.
 *
 * DAS TIER-DATEIMUSTER UND DAS STANDALONE-DATEIMUSTER SIND BYTEWEISE DASSELBE, und das ist keine
 * Beobachtung, sondern eine ZUSAGE — der Kopf von init_server_standalone sagt seit jeher "Mirrors
 * the LogSystem::on_start file formatter byte-for-byte so customer-side tools log identically to
 * engine/replica/world". Eine Zusage, die an zwei Stellen unabhaengig nachgebaut wird, ist eine
 * Zusage ohne Halt: nichts haette gemeldet, wenn eine der beiden Stellen ein Zeichen anders
 * gesetzt haette, und beide Ausgaben saehen fuer sich plausibel aus. Seit es EINE Fabrik gibt,
 * ist die Zusage eingeloest, statt beschrieben.
 *
 * WARUM DAS ETIKETT EIN `const char*` IST UND KEIN `std::string`: weil der Aufrufer eines hat.
 * LogConfig::label ist ein char[], und die erste Fassung dieser Schnittstelle nahm einen
 * std::string — das erzeugte im System eine Umwandlung und mit ihr einen
 * STD_STRING_FORBIDDEN-Befund an genau der Stelle, an der der Vorgaenger einen Stack-Puffer
 * benutzt hatte, um sie zu vermeiden. Die Umwandlung gehoert an die BIBLIOTHEKSGRENZE, wo
 * set_pattern ohnehin einen std::string verlangt, und die liegt in der zugehoerigen .cpp. Ein
 * Standalone-Aufrufer, der einen std::string haelt, reicht `.c_str()`.
 */
[[nodiscard]] std::unique_ptr<spdlog::formatter> make_file_formatter(const char* label);

/**
 * @brief Baut den Formatierer des HTTP-Ringpuffers (/api/logs)
 * @return Formatierer ohne Farbe und ohne Datum: Uhrzeit, Stufe, Meldung
 *
 * Der Ring wird ueber HTTP gelesen und nicht in einem Terminal betrachtet, also traegt er die
 * schlichte Stufe (%#) statt der gefaerbten und keinen Tagesteil.
 */
[[nodiscard]] std::unique_ptr<spdlog::formatter> make_ring_formatter();

/**
 * @brief Baut den Datei-Formatierer eines TUI-Werkzeugs
 * @param label Das Etikett, das in eckigen Klammern hinter [ASE] steht
 * @return Formatierer ohne Farbe: die Datei eines TUI-Werkzeugs wird gelesen, nicht getailt
 *
 * WARUM DIE ZWEI TUI-FORMATIERER GETRENNT BLEIBEN: ein TUI-Werkzeug schreibt in zwei Ziele mit
 * verschiedenen Anforderungen. Die Datei wird spaeter GELESEN (schlichte Stufe, kein ANSI), der
 * Rueckrufbereich wird ANGEZEIGT (gefaerbte Stufe, kein eigenes Zeilenende). Sie zu einer Fabrik
 * mit einem Schalter zusammenzuziehen waere eine Verzweigung ueber einen Zweck, den es nicht gibt.
 */
[[nodiscard]] std::unique_ptr<spdlog::formatter> make_tui_file_formatter(const char* label);

/**
 * @brief Baut den Formatierer des TUI-Rueckrufs (der Log-Bereich des Werkzeugs)
 * @param label Das Etikett, das in eckigen Klammern hinter [ASE] steht
 * @return Formatierer mit Farbe und OHNE eigenes Zeilenende
 *
 * OHNE eigenes Zeilenende (eol ""): der Rueckruf bekommt den Datensatz EXAKT — ein mehrzeiliger
 * Datensatz schliesst selbst mit seinem Leerzeilen-Umbruch, und die Senke rendert jede
 * eingebettete Zeile; ein zusaetzliches eol wuerde dort als zweite Leerzeile erscheinen. Die
 * Datei-Formatierer daneben behalten ihr eol.
 *
 * Als shared_ptr und nicht als unique_ptr, anders als die drei darueber: dieser Formatierer geht
 * NICHT an eine Senke, die ihn uebernimmt, sondern wird von make_callback_sink im Rueckruf
 * FESTGEHALTEN — er ueberlebt den Aufruf und wird bei jeder Zeile erneut gebraucht.
 */
[[nodiscard]] std::shared_ptr<spdlog::formatter> make_tui_callback_formatter(const char* label);

/**
 * @brief Baut einen Logger aus EINER Senke und legt ihn in den Slot dieses Managers
 * @param name  Name des Loggers (erscheint dort, wo das Muster ihn ausgibt)
 * @param sink  Die fertige Senke, gebaut von internal::make_console_sink oder
 *              internal::make_rotating_file_sink
 * @param level Stufenindex wie in log.hpp: 0 TRC, 1 DBG, 2 INF, 3 WRN, 4 ERR, 5 CRT.
 *              Werte darueber werden auf CRT geklemmt, damit ein Zahlendreher nicht still
 *              alles abschaltet — eine zu laute Ausgabe faellt auf, eine stumme nicht.
 *
 * WARUM DIESER EINSTIEG HIER STEHT UND NICHT BEI DEN AUFRUFERN: der Logger-Slot GEHOERT diesem
 * Manager. Ihn zu fuellen ist derselbe Vorgang wie store_logger, nur mit dem Bauschritt davor —
 * und der Kopf ueber logger_slot in der zugehoerigen .cpp sagt die Regel dazu bereits: ein
 * Durchreicher gehoert zu dem, wohin er reicht.
 *
 * WAS ES DEN AUFRUFERN ERSPART, und das ist der Zweck, nicht ein Nebeneffekt: init() in
 * log_standalone.cpp musste bis 2026-08-31 den Logger-Typ und die Stufenkonstante der
 * Bibliothek SELBST nennen, um ihn zu bauen. Es beschreibt jetzt nur noch, WELCHEN Logger sein
 * Programm braucht, und nennt keinen Fremdtyp mehr. Die Bibliothek bleibt, wo sie hingehoert:
 * bei dem, der ihre Werte haelt.
 *
 * Idempotenz ist NICHT Sache dieser Funktion: sie ueberschreibt den Slot. Der Aufrufer
 * entscheidet, ob er einen bestehenden Logger stehenlaesst — alle drei Einstiege in
 * log_standalone.cpp tun das mit ihrem eigenen Test an ihrer ersten Zeile, und dieser Test ist
 * dort richtig aufgehoben, weil er zu ihrer Zusage gehoert, nicht zum Ablegen.
 */
void install_logger(const std::string& name, std::shared_ptr<spdlog::sinks::sink> sink,
                    uint8_t level);

/**
 * @brief Sammelbehaelter fuer die Senken EINES Loggers
 *
 * Ein Logger mit mehreren Senken braucht eine Liste, und eine Liste braucht einen Elementtyp.
 * Genau daran nannten die Standalone-Einstiege den Bibliothekstyp — nicht weil sie mit ihm
 * rechneten, sondern weil sie einen Behaelter deklarieren mussten. Dieser Behaelter nimmt ihnen
 * das ab: `add` schluckt, was die Fabriken liefern, und der Aufrufer haelt sein Ergebnis in
 * `auto`.
 *
 * KEIN ERSATZ FUER ase::containers::Vector, sondern seine Verwendung an genau einer Stelle: die
 * Liste lebt hier drin, wo der Typ genannt werden darf, statt in jeder Datei, die einen Logger
 * aufsetzt.
 */
class SinkSet {
public:
    /** @brief Haengt eine fertige Senke an. Ein leerer Zeiger wird verworfen, nicht abgelegt. */
    void add(std::shared_ptr<spdlog::sinks::sink> sink);

    /** @brief true, solange keine Senke angehaengt wurde. */
    [[nodiscard]] bool empty() const;

    /** @brief Zahl der angehaengten Senken. */
    [[nodiscard]] uint32_t size() const;

private:
    // Die beiden Einstiege, die den Behaelter AUSLESEN duerfen. Sie stehen im selben Namensraum
    // und gehoeren zum Besitzer des Logger-Slots; jeder andere bekommt den Elementtyp nicht zu
    // sehen — das ist der ganze Zweck dieser Klasse.
    friend void install_logger(const std::string& name, const SinkSet& sinks, uint8_t level,
                               uint8_t flush_level);
    friend uint32_t attach_sinks(const SinkSet& sinks);
    friend uint32_t replay_capture_ring(const SinkSet& targets);
    ase::containers::Vector<std::shared_ptr<spdlog::sinks::sink>> sinks_;
};

/**
 * @brief Baut einen Logger aus MEHREREN Senken und legt ihn in den Slot dieses Managers
 * @param name        Name des Loggers
 * @param sinks       Die gesammelten Senken; ein leerer Satz legt KEINEN Logger ab
 * @param level       Stufenindex (0 TRC .. 5 CRT), oberhalb von CRT geklemmt
 * @param flush_level Ab welcher Stufe sofort auf den Datentraeger geschrieben wird
 *
 * Zweite Form desselben Vorgangs wie install_logger mit EINER Senke darueber. Sie existiert,
 * weil ein Programm ohne ECS-Welt regelmaessig zwei Ausgaenge braucht — eine Datei und ein
 * Terminal, oder eine Datei und einen Rueckruf in eine TUI.
 *
 * DIE ZWEITE STUFE IST NICHT DIESELBE WIE DIE ERSTE, und deshalb ist sie ein eigener Parameter:
 * `level` entscheidet, WAS geschrieben wird, `flush_level`, WANN es den Puffer verlaesst. Die
 * Standalone-Einstiege setzen beide auf TRC, weil ein Werkzeug, das abstuerzt, seine letzten
 * Zeilen sonst im Puffer mitnimmt — genau die, die den Absturz erklaeren.
 *
 * EIN LEERER SATZ LEGT NICHTS AB, statt einen Logger ohne Ausgang zu erzeugen. Ein solcher
 * Logger waere das Schlimmste von beidem: er besteht jeden Null-Test und schreibt trotzdem nie
 * eine Zeile.
 */
void install_logger(const std::string& name, const SinkSet& sinks, uint8_t level,
                    uint8_t flush_level);

/**
 * Fassungsvermoegen des Aufnahme-Rings der Startphase, in Zeilen.
 *
 * Er haelt alles, was zwischen dem ersten Griff in Kernel::build und dem Anhaengen der echten
 * Senken anfaellt: Kernel-Init, Umgebungslader, Kommandozeile, dlopen-Suche und jedes on_start
 * vor LogSystem. Der Ring VERWIRFT die aeltesten Zeilen, wenn er voll ist — eine zu kleine Zahl
 * frisst also stillschweigend genau den Anfang, den man beim Nachsehen sucht.
 */
inline constexpr uint32_t kCaptureRingLines = 2000u;

/**
 * @brief Richtet den Aufnahme-Ring der Startphase ein und gibt ihn als Senke zurueck
 * @param lines Fassungsvermoegen in Zeilen (siehe kCaptureRingLines)
 * @return Der Ring als Senke, fertig zum Anhaengen an einen Logger
 *
 * EIN Vorgang, nicht zwei: der Ring wird gebaut UND im Manager abgelegt, denn er hat zwei
 * Verwendungen, die nicht auseinanderlaufen duerfen — er ist die einzige Senke des
 * Start-Loggers, und er wird spaeter zurueckgespielt. Wer ihn baut, ohne ihn abzulegen, hat eine
 * Aufnahme, die niemand mehr findet; wer ihn ablegt, ohne ihn anzuhaengen, eine Aufnahme, in die
 * nie etwas hineinlaeuft. Beides sieht beim Start aus wie ein normaler Lauf.
 */
[[nodiscard]] std::shared_ptr<spdlog::sinks::sink> install_capture_ring(uint32_t lines);

/**
 * @brief Haengt weitere Senken an den BESTEHENDEN Logger an
 * @param sinks Die anzuhaengenden Senken; ein leerer Satz tut nichts
 * @return Zahl der angehaengten Senken, 0 wenn kein Logger da ist
 *
 * Anders als install_logger ERSETZT das nichts: der Logger behaelt, was er hat, und bekommt
 * dazu. Genau das braucht die Startklammer — der Aufnahme-Ring muss noch haengen, waehrend die
 * echten Senken dazukommen, sonst faellt zwischen beiden Schritten eine Zeile auf den Boden.
 */
uint32_t attach_sinks(const SinkSet& sinks);

/**
 * @brief Spielt den Inhalt des Aufnahme-Rings in die angegebenen Senken
 * @param targets Die Senken, die den Rueckstand erhalten
 * @return Zahl der zurueckgespielten ZEILEN, 0 wenn kein Ring da ist oder er leer war
 *
 * JEDE aufgenommene Zeile erreicht JEDE angegebene Senke — Datei, HTTP-Ring und Zaehler zeigen
 * fuer die Startphase danach denselben Inhalt. Genau darin liegt der Zweck des Rings: ohne ihn
 * fehlte jedem dieser Kanaele alles, was vor seinem Anhaengen passiert ist, und keiner koennte
 * sagen, dass etwas fehlt.
 *
 * Der Ring bleibt danach bestehen; ihn abzuhaengen ist ein eigener Griff (detach_capture_ring).
 * Die Trennung ist Absicht: zwischen Rueckspielen und Abhaengen darf der Logger nicht ohne Ziel
 * dastehen.
 */
uint32_t replay_capture_ring(const SinkSet& targets);

/**
 * @brief Nimmt den Aufnahme-Ring aus der Senkenliste des Loggers und gibt ihn frei
 * @return true, wenn ein Ring haengte und entfernt wurde
 *
 * Der Vergleich laeuft ueber die ROHE Adresse, weil der Manager den Besitz haelt und an der
 * Senkenliste nur ein Zeiger auf dieselbe Senke steht. Nach diesem Griff traegt der Logger
 * ausschliesslich die echten Senken, und der Ring ist Geschichte — beides gehoert zusammen,
 * deshalb ist es EIN Aufruf und nicht zwei.
 */
bool detach_capture_ring();

}  // namespace ase::log::internal
