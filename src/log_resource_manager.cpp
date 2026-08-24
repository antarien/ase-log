/**
 * ASE RESOURCE MANAGER IMPLEMENTATION
 *
 * @file        log_resource_manager.cpp
 * @brief       LogResourceManager - haelt die Besitzzeiger auf Logger und Senken
 * @description Die Objekte selbst leben hier, ausserhalb des Systems. log_sys.cpp baut die
 *              Senken und uebergibt sie; alle Leser bekommen rohe Zeiger, nie den Besitz.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    ecs/module
 * @created     2026-08-22
 * @modified    2026-08-22
 * @version     1.0.0
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
 */

#include <ase/log/internal/log_resource_manager.hpp>

#include <spdlog/logger.h>
#include <spdlog/sinks/ringbuffer_sink.h>
#include <spdlog/sinks/sink.h>

namespace ase::log::internal {

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

// Die drei Parkplaetze. park_* legt ab, take_* nimmt heraus UND raeumt den Platz: eine geparkte
// Senke wird genau einmal eingehaengt, und ein zweiter Griff soll leer ausgehen statt dieselbe
// Senke ein zweites Mal in die Liste zu legen.
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

}  // namespace ase::log::internal
