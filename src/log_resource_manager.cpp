/**
 * ASE RESOURCE MANAGER IMPLEMENTATION
 *
 * @file        log_resource_manager.cpp
 * @brief       LogResourceManager - baut und haelt die Besitzzeiger auf Logger und Senken
 * @description Die Objekte selbst leben hier, ausserhalb des Systems: die Fabriken bauen sie, der
 *              Manager haelt sie, die Einstiege haengen sie ein. Alle Leser bekommen rohe Zeiger,
 *              nie den Besitz.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    ecs/module
 * @created     2026-08-22
 * @modified    2026-08-31
 * @version     1.1.0
 *
 * ECS RESOURCE MANAGER IMPLEMENTATION COMPLIANCE
 *
 * [ ] NOT a Component - lives outside ECS registry
 * [ ] Accessed via registry.ctx().get<ResourceManager&>()
 * [ ] Components store ONLY uint32_t IDs
 * [ ] Thread-safe via std::mutex
 * [ ] Proper cleanup in clear_all() - closes/stops BEFORE clearing
 * [ ] No ECS anti-patterns (this is intentionally OOP bridge code)
 * [ ] Implementations in .cpp to avoid mass rebuilds
 * [ ] Header contains ONLY declarations
 * [ ] Layer dependencies checked (only depend on lower layers)
 * [ ] NO file-level static/constexpr (constants → types.hpp)
 * [ ] Filename matches convention
 * [ ] 1 File = 1 ResourceManager
 * [ ] Folder structure matches convention (src/{category}/)
 * [ ] Layer dependencies respected (no upward dependencies)
 * [ ] NO std::shared_ptr in Components - stored HERE via Flyweight Pattern
 * [ ] External resources (shared_ptr, handles) accessed via registry.ctx()
 * [ ] ResourceManager registered in on_start() via registry.ctx().emplace<>()
 * [ ] clear_all() closes resources in REVERSE dependency order
 * [ ] clear_all() called from IniSystem::on_stop() or ShutdownSystem
 *
 * ZWEI PUNKTE DIESER LISTE TREFFEN HIER ANDERS ZU — die Abweichung steht im Header begruendet:
 * der Zugriff laeuft ueber log_resources() statt ueber registry.ctx(), weil log::init vor
 * Kernel::build laeuft und ase-convert ohne jede Registry loggt.
 * clear_all() wird entsprechend aus LogSystem::on_stop gerufen, nicht aus einem IniSystem.
 *
 * WAS AM 2026-08-31 HIERHER GEZOGEN IST, UND WARUM ES KEIN RUECKBAU IST
 *
 *   Die drei Senken-Fabriken, die vier Formatierer-Fabriken und die drei Musterflaggen standen
 *   einen Tag lang in vier eigenen Dateien unter internal/. Sie sind hierher gezogen, weil die
 *   Ausnahme von SPDLOG_DIRECT_FORBIDDEN FUENF Dateien NAMENTLICH nennt und keine davon so hiess
 *   — die Begruendung "internal/ darf" gab es nur in den Koepfen jener Dateien, nie im
 *   Regelbestand.
 *
 *   DAS IST KEINE ZURUECKNAHME DES SCHNITTS, SONDERN SEINE KORREKTUR: die vier Dateien hatten
 *   die Bibliothek in ihren oeffentlichen Signaturen stehen und damit EXPORTIERT, statt sie zu
 *   kapseln. Ein Wrapper, der den gewrappten Typ herausreicht, hat an dieser Stelle aufgehoert,
 *   einer zu sein. Der Zweck des Schnitts — EINE Stelle je Bauform, keine Kopien, keine zweite
 *   Wahrheit ueber dieselbe Bytefolge — bleibt vollstaendig erhalten; nur die Grenze liegt
 *   jetzt dort, wo die Architektur sie zieht.
 *
 *   DIE MUSTERFLAGGEN SIND DABEI IN DEN ANONYMEN NAMENSRAUM GEWANDERT, und das ist eine
 *   Verschaerfung, keine Verschiebung: sie hatten baumweit genau EINEN Verbraucher, die vier
 *   Formatierer-Fabriken. Ein Header, den nur seine eigene Uebersetzungseinheit braucht, ist ein
 *   Vertrag ohne Gegenpartei.
 */

#include <ase/log/internal/log_resource_manager.hpp>

#include <spdlog/logger.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/sinks/sink.h>

// std::remove_if fuer detach_capture_ring, std::size_t fuer die Ringgroesse. Beide sind hier
// zulaessig: diese Datei ist die Umhuellung selbst, nicht ihr Verbraucher.
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

namespace ase::log::internal {

// DIE DREI MUSTERFLAGGEN UND DIE SIEBEN FABRIKEN STEHEN NICHT HIER, sondern in src/log_capture.cpp
// — ihr VERTRAG steht im Header dieser Datei, und wer sie sucht, findet sie darueber.
//
// Der Grund ist keine Widmungsfrage, sondern eine Erlaubnisfrage: eine Musterflagge LEITET von
// spdlog::custom_flag_formatter AB und ueberschreibt dessen rein virtuelles clone(). Damit
// braucht ihr Rumpf drei namentliche Ausnahmen gleichzeitig (SPDLOG_DIRECT_FORBIDDEN,
// INHERITANCE_FORBIDDEN_EXCEPT_ECS, PROTOTYPE_PATTERN_FORBIDDEN), und diese Datei traegt nur die
// erste. Die Schnittmenge der drei Listen enthaelt log_sys.cpp und log_capture.cpp; die
// Begruendung der Wahl steht am Rumpf selbst.
//
// EIN HEADER DARF DEN VERTRAG TRAGEN, weil dort keine Ableitung steht — nur die Definition
// braucht die Ausnahmen. Deshalb bleibt die Deklaration beim Besitzer der Senken, wo sie
// hingehoert, und nur der Rumpf zieht dorthin, wo er stehen darf.

spdlog::logger* LogResourceManager::get_logger() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return logger_.get();
}

void LogResourceManager::store_logger(std::shared_ptr<spdlog::logger> logger) {
    const std::lock_guard<std::mutex> guard(mutex_);
    logger_ = std::move(logger);
}

spdlog::logger* LogResourceManager::get_client_logger() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return client_logger_.get();
}

void LogResourceManager::store_client_logger(std::shared_ptr<spdlog::logger> logger) {
    const std::lock_guard<std::mutex> guard(mutex_);
    client_logger_ = std::move(logger);
}

std::shared_ptr<spdlog::logger>& LogResourceManager::logger_slot() {
    return logger_;
}

std::shared_ptr<spdlog::logger>& LogResourceManager::client_logger_slot() {
    return client_logger_;
}

LogResourceManager::RingSink* LogResourceManager::get_http_ring() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return http_ring_.get();
}

void LogResourceManager::store_http_ring(std::shared_ptr<RingSink> ring) {
    const std::lock_guard<std::mutex> guard(mutex_);
    http_ring_ = std::move(ring);
}

LogResourceManager::RingSink* LogResourceManager::get_capture_ring() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return capture_ring_.get();
}

void LogResourceManager::store_capture_ring(std::shared_ptr<RingSink> ring) {
    const std::lock_guard<std::mutex> guard(mutex_);
    capture_ring_ = std::move(ring);
}

void LogResourceManager::release_capture_ring() {
    const std::lock_guard<std::mutex> guard(mutex_);
    capture_ring_.reset();
}

// Die drei Parkplaetze. Ein park_ legt ab, ein take_ nimmt heraus UND raeumt den Platz: eine
// geparkte Senke wird genau einmal eingehaengt, und ein zweiter Griff soll leer ausgehen statt
// dieselbe Senke ein zweites Mal in die Liste zu legen.
void LogResourceManager::park_file_sink(std::shared_ptr<spdlog::sinks::sink> sink) {
    const std::lock_guard<std::mutex> guard(mutex_);
    pending_file_ = std::move(sink);
}

void LogResourceManager::park_http_sink(std::shared_ptr<spdlog::sinks::sink> sink) {
    const std::lock_guard<std::mutex> guard(mutex_);
    pending_http_ = std::move(sink);
}

void LogResourceManager::park_counting_sink(std::shared_ptr<spdlog::sinks::sink> sink) {
    const std::lock_guard<std::mutex> guard(mutex_);
    pending_counting_ = std::move(sink);
}

std::shared_ptr<spdlog::sinks::sink> LogResourceManager::take_file_sink() {
    const std::lock_guard<std::mutex> guard(mutex_);
    return std::move(pending_file_);
}

std::shared_ptr<spdlog::sinks::sink> LogResourceManager::take_http_sink() {
    const std::lock_guard<std::mutex> guard(mutex_);
    return std::move(pending_http_);
}

std::shared_ptr<spdlog::sinks::sink> LogResourceManager::take_counting_sink() {
    const std::lock_guard<std::mutex> guard(mutex_);
    return std::move(pending_counting_);
}

// Der Zaehler laeuft OHNE die Sperre: er ist selbst atomar, und die Zaehl-Senke ruft ihn je
// Logzeile aus einem beliebigen Thread. Die Sperre daneben schuetzt die Zeiger-Slots, nicht
// diesen Wert — sie hier zu nehmen wuerde jede Logzeile des Prozesses serialisieren, ohne
// irgendetwas sicherer zu machen.
void LogResourceManager::increment_counter() {
    counter_.fetch_add(1, std::memory_order_relaxed);
}

uint32_t LogResourceManager::log_counter() const {
    return counter_.load(std::memory_order_relaxed);
}

// Aus LogSystem::on_stop gerufen, damit kein spdlog-Objekt in die statische Zerstoerung
// hineinlebt. Der Zaehler bleibt stehen: er ist eine Zaehlung, kein Betriebsmittel, und eine
// spaete Abfrage soll die Zahl der Zeilen sehen, nicht eine Null.
void LogResourceManager::clear_all() {
    const std::lock_guard<std::mutex> guard(mutex_);
    logger_.reset();
    client_logger_.reset();
    http_ring_.reset();
    capture_ring_.reset();
    pending_file_.reset();
    pending_http_.reset();
    pending_counting_.reset();
}

LogResourceManager& log_resources() {
    static LogResourceManager instance;
    return instance;
}

/**
 * @brief The one storage slot for the server logger — see the note above LogSystem in log.hpp.
 *
 * GELOESCHT 2026-08-22: LogSystem::g_logger_ und LogSystem::g_client_logger_. Ein ECS-System
 * haelt keinen Zustand; solange der Logger dort lag, musste jede freie Funktion ihn ueber
 * `LogSystem::logger()` holen, und ein statischer System-Aufruf ist
 * STATIC_SYSTEM_CALLS_FORBIDDEN (WRFL_ASE_MODULE_DEPENDENCIES.md Section 3).
 *
 * Die Funktion gibt den Slot des LogResourceManagers zurueck, nicht eine Kopie: die Accessoren
 * in log.hpp reichen eine Referenz durch, ueber die auch ZUGEWIESEN wird (186 Lesestellen, drei
 * Schreibstellen). Ein Rueckgabewert waere hier kein Detail, sondern ein stiller Verlust — die
 * Zuweisung liefe ins Leere.
 *
 * HIERHER GEWANDERT AM 2026-08-30, aus log_sys.cpp. Die beiden Funktionen tun nichts, als den
 * Slot dieses Managers durchzureichen; sie standen in der Systemdatei, weil der Zustand dort
 * einmal gelegen HAT. Nach dem Umzug in den Manager war die Weiterleitung das letzte, was von
 * jener Zeit uebrig war — und ein Durchreicher gehoert zu dem, wohin er reicht.
 */
std::shared_ptr<spdlog::logger>& logger_slot() {
    return log_resources().logger_slot();
}

std::shared_ptr<spdlog::logger>& client_logger_slot() {
    return log_resources().client_logger_slot();
}

/*
 * DER BAUSCHRITT VOR DEM ABLEGEN — hierher gezogen am 2026-08-31.
 *
 * Er stand in init() in log_standalone.cpp und zwang jene Datei, den Logger-Typ und die
 * Stufenkonstante der Bibliothek zu nennen, obwohl ihre Widmung eine andere ist: sie sagt,
 * WELCHEN Logger ein Programm ohne ECS-Welt bekommt, nicht WIE einer entsteht. Der Slot gehoert
 * diesem Manager, also gehoert das Fuellen des Slots hierher — dieselbe Begruendung, die
 * logger_slot() darueber hergefuehrt hat.
 *
 * DIE STUFE KOMMT ALS ZAHL HEREIN, NICHT ALS BIBLIOTHEKSTYP, und das ist der Punkt der ganzen
 * Uebung: der Aufrufer haelt log.hpp's kLogLevelInfo in der Hand, einen uint8_t, und muss
 * dafuer nichts von der Bibliothek wissen. Die Umrechnung passiert genau hier, wo sie erlaubt
 * ist und wo sie nur EINMAL steht.
 */
void install_logger(const std::string& name, std::shared_ptr<spdlog::sinks::sink> sink,
                    uint8_t level) {
    auto logger = std::make_shared<spdlog::logger>(name, std::move(sink));

    // Geklemmt statt durchgereicht: spdlog kennt ueber `critical` hinaus noch `off`, und ein
    // Zahlendreher landete sonst dort — der Logger existierte, schriebe aber nie eine Zeile.
    // Eine zu laute Ausgabe faellt beim ersten Blick ins Terminal auf, eine stumme nie.
    const auto lvl = level >= static_cast<uint8_t>(spdlog::level::critical)
                         ? spdlog::level::critical
                         : static_cast<spdlog::level::level_enum>(level);
    logger->set_level(lvl);

    log_resources().store_logger(std::move(logger));
}

void SinkSet::add(std::shared_ptr<spdlog::sinks::sink> sink) {
    // Ein leerer Zeiger wird verworfen statt abgelegt: die Fabriken geben einen zurueck, wenn
    // ihre Quelle fehlt (kein Logpfad, kein Rueckruf), und ein Nullzeiger in der Senkenliste
    // laesst die Bibliothek beim ERSTEN Schreibversuch fallen, nicht beim Anhaengen. Der
    // Aufrufer sieht den Unterschied dann an size(), bevor irgendetwas passiert ist.
    if (!sink) return;
    sinks_.push_back(std::move(sink));
}

bool SinkSet::empty() const {
    return sinks_.empty();
}

uint32_t SinkSet::size() const {
    return static_cast<uint32_t>(sinks_.size());
}

void install_logger(const std::string& name, const SinkSet& sinks, uint8_t level,
                    uint8_t flush_level) {
    // Ohne Senke KEIN Logger. Ein Logger mit leerer Senkenliste besteht jeden Null-Test und
    // schreibt trotzdem nie eine Zeile — der Aufrufer haelt sich fuer eingerichtet und ist blind.
    // Bleibt der Slot leer, faellt es am naechsten `if (LogSystem::logger())` sofort auf.
    if (sinks.empty()) return;

    auto logger = std::make_shared<spdlog::logger>(name, sinks.sinks_.begin(), sinks.sinks_.end());

    const auto to_level = [](uint8_t v) {
        return v >= static_cast<uint8_t>(spdlog::level::critical)
                   ? spdlog::level::critical
                   : static_cast<spdlog::level::level_enum>(v);
    };
    logger->set_level(to_level(level));
    logger->flush_on(to_level(flush_level));

    log_resources().store_logger(std::move(logger));
}

std::shared_ptr<spdlog::sinks::sink> install_capture_ring(uint32_t lines) {
    auto ring = std::make_shared<LogResourceManager::RingSink>(static_cast<std::size_t>(lines));
    log_resources().store_capture_ring(ring);
    return ring;
}

uint32_t attach_sinks(const SinkSet& sinks) {
    auto& slot = logger_slot();
    if (!slot) return 0;
    for (const auto& s : sinks.sinks_) {
        slot->sinks().push_back(s);
    }
    return sinks.size();
}

uint32_t replay_capture_ring(const SinkSet& targets) {
    auto* ring = log_resources().get_capture_ring();
    if (ring == nullptr || targets.empty()) return 0;

    // last_raw() gibt die aufgenommenen Rohdatensaetze; jeder geht durch JEDE Zielsenke, damit
    // kein Kanal einen Teil der Startphase auslaesst. Die Reihenfolge bleibt die der Aufnahme.
    auto captured = ring->last_raw();
    for (auto& msg : captured) {
        for (const auto& s : targets.sinks_) {
            s->log(msg);
        }
    }
    return static_cast<uint32_t>(captured.size());
}

bool detach_capture_ring() {
    auto& res = log_resources();
    auto* ring = res.get_capture_ring();
    if (ring == nullptr) return false;

    auto& slot = logger_slot();
    if (slot) {
        // Ueber die ROHE Adresse vergleichen: der Manager haelt den Besitz, in der Senkenliste des
        // Loggers steht nur ein zweiter Zeiger auf dieselbe Senke. Ein Vergleich der shared_ptr
        // waere hier nicht falsch, aber er verlangte den Besitzzeiger — und den gibt der Manager
        // absichtlich nicht heraus.
        auto& sinks_vec = slot->sinks();
        sinks_vec.erase(std::remove_if(sinks_vec.begin(), sinks_vec.end(),
                                       [ring](const std::shared_ptr<spdlog::sinks::sink>& s) {
                                           return s.get() == ring;
                                       }),
                        sinks_vec.end());
    }
    res.release_capture_ring();
    return true;
}

}  // namespace ase::log::internal
