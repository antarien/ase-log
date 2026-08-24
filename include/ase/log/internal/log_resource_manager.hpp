#pragma once

/**
 * ASE RESOURCE MANAGER (NOT A COMPONENT!)
 *
 * @file        log_resource_manager.hpp
 * @brief       LogResourceManager - Flyweight Pattern fuer die spdlog-Senken und -Logger
 * @description Haelt die Besitzzeiger auf Logger und Senken AUSSERHALB des Systems. log_sys.cpp
 *              baut die Senken und uebergibt sie hierher; wer sie braucht, holt sie ueber die
 *              Zugriffsmethoden statt ueber einen Dateibereichs-Zeiger.
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
 * @module      ase-log
 * @layer       1 (Core)
 * @category    ecs/module
 * @created     2026-08-22
 * @modified    2026-08-22
 * @version     1.0.0
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

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

// Forward declaration - no need to include the heavy sink headers here. Die Senkentypen selbst
// stehen ausschliesslich in log_sys.cpp und in der zugehoerigen .cpp; dieser Header nennt sie nur.
namespace spdlog {
class logger;
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
     * take_* gibt die Senke heraus UND gibt den Platz frei: sie wird genau einmal eingehaengt.
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

}  // namespace ase::log::internal
