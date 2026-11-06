#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        log_client.hpp
 * @design      DSGN_021
 * @brief       Weiterleitung von Browser-Konsolenzeilen in das Serverlog
 * @description Die client_*-Familie schreibt in den ZWEITEN Logger des Moduls
 *              (internal::client_logger_slot) — ohne [SERVER]-Praefix, weil die Zeile nicht vom
 *              Server stammt, sondern ueber RTC aus einem Browser hereinkommt. Drei Formen je
 *              Stufe: ungefiltert, gefiltert ueber ein Client-Bit, und formatiert mit
 *              Aufrufort-Erfassung. Der Aufrufer ist das Browser-Log-Relay von ase-network:
 *              NetworkDchnLogInfoSystem und NetworkDchnLogDbgSystem schreiben jede farbige
 *              Zeile mit dem Bit ihres Clients; damit greift der Client-Filter +CLT/-CLT aus
 *              ARCH_ASE_LOG Abschnitt 10. Warn- und Fehlerzeilen des Relays bleiben in der
 *              kategorisierten Form von log.hpp (WRN::CAT/ERR::CAT, Quelle, Besitzer, Detail),
 *              weil dieser Strom ueber Kategorie und Quellnamen gefiltert wird und client_warn
 *              und client_error keine Kategorie tragen.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    process/computation
 * @usecase     usecase:100362 - write a log line
 * @created     2026-08-31
 * @modified    2026-10-30
 * @version     1.3.0
 *
 * WARUM DIESER ZWECK EINE EIGENE DATEI IST
 *
 * `log.hpp` binden fast alle Uebersetzungseinheiten jedes Tiers fuer den gewoehnlichen Aufruf
 * ein. Diese Familie hat einen anderen Zweck als jede dieser Dateien: sie bedient einen zweiten
 * Logger fuer fremde Zeilen, und nur das Relay braucht sie. Stuende sie in log.hpp, zahlte der
 * ganze Baum bei jeder Uebersetzung fuer eine Familie, die eine Handvoll Systeme ruft.
 *
 * DAS CLIENT-BIT HAT 64 PLAETZE, DIE CLIENT-KENNUNG NICHT
 *
 * Die Filtermasken sind uint64_t (ARCH_ASE_LOG 10.2), die Client-Kennung vergibt ase-network
 * aufsteigend als uint32_t je WebSocket-Verbindung (PLAN_ASE_AUTH_ECS Abschnitt 1), bei hoechstens
 * 64 gleichzeitigen Clients (ARCH_ASE_NETWORK, DefaultMaxClients). Eine Verschiebung `1ULL << id`
 * ist ab der Kennung 64 undefiniert. client_bit() bildet die Kennung deshalb modulo der
 * Maskenbreite ab: die Kennungen 1 bis 63 behalten ihr eigenes Bit, wie +CLT:01 es erwartet, und
 * eine hoehere Kennung teilt sich das Bit mit ihrem Rest. Das Ergebnis ist nie der Wert 0, mit
 * dem Serverzeilen jeden Client-Filter passieren — -CLT sperrt damit weiterhin jede Client-Zeile.
 *
 * KEIN FREMDER BIBLIOTHEKSTYP IN DIESER DATEI: die Formatierung laeuft ueber die log_fmtN-Traeger
 * aus log.hpp, die den Aufrufort mit __builtin_FILE() erfassen. Die Schwesterdatei log_rtc.hpp
 * kann das nicht — sie filtert nicht nach Aufrufort und nimmt die Formatzeichenkette direkt.
 *
 * LAYER RULES:
 *   Layer 0 (Foundation): NO dependencies on other ASE modules (only std::)
 *   Layer 1 (Core):       May depend on Layer 0 only
 *
 * USAGE:
 *   #include <ase/log/log_client.hpp>
 *   ase::log::client_info(ase::log::client_bit(client_id), "browser sagt hallo");
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

// Traegt internal::client_logger_slot, die log_fmtN-Traeger und die Filter-Einstiege. Die
// Richtung ist Absicht: DIESE Datei bindet log.hpp ein, nie umgekehrt — ein Durchreicher dort
// haette den ganzen Baum wieder an diese Familie gekoppelt und den Schnitt aufgehoben.
#include <ase/log/log.hpp>

#include <cstdint>
#include <limits>
#include <source_location>
#include <string>
#include <type_traits>
#include <utility>

namespace ase::log {

/**
 * @brief Das Filter-Bit eines Clients (ARCH_ASE_LOG 10.2).
 * @param client_id Kennung des Clients, wie ase-network sie vergibt
 * @return Genau ein gesetztes Bit an der Stelle client_id modulo der Maskenbreite; nie 0
 */
[[nodiscard]] constexpr uint64_t client_bit(uint32_t client_id) noexcept {
    return uint64_t{1} << (client_id % std::numeric_limits<uint64_t>::digits);
}

// CLIENT LOGGING FUNCTIONS (uses the client logger without [SERVER] prefix)
// For forwarding browser console logs via RTC

// Unfiltered client logging (no client filter applied)
inline void client_info(const std::string& msg) {
    if (internal::client_logger_slot()) internal::client_logger_slot()->info("{}", msg);
}

inline void client_warn(const std::string& msg) {
    if (internal::client_logger_slot()) internal::client_logger_slot()->warn("{}", msg);
}

inline void client_error(const std::string& msg) {
    if (internal::client_logger_slot()) internal::client_logger_slot()->error("{}", msg);
}

inline void client_debug(const std::string& msg) {
    if (internal::client_logger_slot()) internal::client_logger_slot()->debug("{}", msg);
}

// Filtered client logging with client_bit (use client_bit(client_id))
// Respects level filter, category filter, AND client filter (+CLT:01, -CLT)
inline void client_info(uint64_t client_bit, const std::string& msg,
                        const std::source_location& loc = std::source_location::current()) {
    if (!should_log_client(level::info, client_bit, loc)) return;
    if (internal::client_logger_slot()) internal::client_logger_slot()->info("{}", msg);
}

inline void client_warn(uint64_t client_bit, const std::string& msg,
                        const std::source_location& loc = std::source_location::current()) {
    if (!should_log_client(level::warn, client_bit, loc)) return;
    if (internal::client_logger_slot()) internal::client_logger_slot()->warn("{}", msg);
}

inline void client_error(uint64_t client_bit, const std::string& msg,
                         const std::source_location& loc = std::source_location::current()) {
    if (!should_log_client(level::error, client_bit, loc)) return;
    if (internal::client_logger_slot()) internal::client_logger_slot()->error("{}", msg);
}

inline void client_debug(uint64_t client_bit, const std::string& msg,
                         const std::source_location& loc = std::source_location::current()) {
    if (!should_log_client(level::debug, client_bit, loc)) return;
    if (internal::client_logger_slot()) internal::client_logger_slot()->debug("{}", msg);
}

// Filtered client logging with fmt-style formatting
// Full filtering: level + O(1) cached category + client filter
//
// STELLIGKEIT EINS BIS VIER. Die vierte traegt die Zeile des Relays: Farbe, Client-Kennung,
// Bereich und Text. Eine fuenfte ist eine Ueberladung nach demselben Muster, und der Uebersetzer
// nennt die Stelle, die sie braucht — eine fehlende Ueberladung scheitert nie still zur Laufzeit.
// Die sechs Hauptstufen in log.hpp fuehren neun, weil dort der Bedarf so weit reicht.

template<typename T0>
inline void client_info(uint64_t client_bit, log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_client_loc(level::info, lf.file, lf.func, lf.loc_hash, client_bit)) return;
    if (internal::client_logger_slot())
        internal::client_logger_slot()->info(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void client_info(uint64_t client_bit,
                        log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                        T0&& a0, T1&& a1) {
    if (!filter::should_log_client_loc(level::info, lf.file, lf.func, lf.loc_hash, client_bit)) return;
    if (internal::client_logger_slot())
        internal::client_logger_slot()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void client_info(uint64_t client_bit,
                        log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                                 std::type_identity_t<T2>> lf,
                        T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_client_loc(level::info, lf.file, lf.func, lf.loc_hash, client_bit)) return;
    if (internal::client_logger_slot())
        internal::client_logger_slot()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                             std::forward<T2>(a2));
}

template<typename T0, typename T1, typename T2, typename T3>
inline void client_info(uint64_t client_bit,
                        log_fmt4<std::type_identity_t<T0>, std::type_identity_t<T1>,
                                 std::type_identity_t<T2>, std::type_identity_t<T3>> lf,
                        T0&& a0, T1&& a1, T2&& a2, T3&& a3) {
    if (!filter::should_log_client_loc(level::info, lf.file, lf.func, lf.loc_hash, client_bit)) return;
    if (internal::client_logger_slot())
        internal::client_logger_slot()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                             std::forward<T2>(a2), std::forward<T3>(a3));
}

template<typename T0>
inline void client_warn(uint64_t client_bit, log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_client_loc(level::warn, lf.file, lf.func, lf.loc_hash, client_bit)) return;
    if (internal::client_logger_slot())
        internal::client_logger_slot()->warn(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void client_warn(uint64_t client_bit,
                        log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                        T0&& a0, T1&& a1) {
    if (!filter::should_log_client_loc(level::warn, lf.file, lf.func, lf.loc_hash, client_bit)) return;
    if (internal::client_logger_slot())
        internal::client_logger_slot()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void client_warn(uint64_t client_bit,
                        log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                                 std::type_identity_t<T2>> lf,
                        T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_client_loc(level::warn, lf.file, lf.func, lf.loc_hash, client_bit)) return;
    if (internal::client_logger_slot())
        internal::client_logger_slot()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                             std::forward<T2>(a2));
}

template<typename T0, typename T1, typename T2, typename T3>
inline void client_warn(uint64_t client_bit,
                        log_fmt4<std::type_identity_t<T0>, std::type_identity_t<T1>,
                                 std::type_identity_t<T2>, std::type_identity_t<T3>> lf,
                        T0&& a0, T1&& a1, T2&& a2, T3&& a3) {
    if (!filter::should_log_client_loc(level::warn, lf.file, lf.func, lf.loc_hash, client_bit)) return;
    if (internal::client_logger_slot())
        internal::client_logger_slot()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                             std::forward<T2>(a2), std::forward<T3>(a3));
}

template<typename T0>
inline void client_error(uint64_t client_bit, log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_client_loc(level::error, lf.file, lf.func, lf.loc_hash, client_bit)) return;
    if (internal::client_logger_slot())
        internal::client_logger_slot()->error(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void client_error(uint64_t client_bit,
                         log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                         T0&& a0, T1&& a1) {
    if (!filter::should_log_client_loc(level::error, lf.file, lf.func, lf.loc_hash, client_bit)) return;
    if (internal::client_logger_slot())
        internal::client_logger_slot()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void client_error(uint64_t client_bit,
                         log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                                  std::type_identity_t<T2>> lf,
                         T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_client_loc(level::error, lf.file, lf.func, lf.loc_hash, client_bit)) return;
    if (internal::client_logger_slot())
        internal::client_logger_slot()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                              std::forward<T2>(a2));
}

template<typename T0, typename T1, typename T2, typename T3>
inline void client_error(uint64_t client_bit,
                         log_fmt4<std::type_identity_t<T0>, std::type_identity_t<T1>,
                                  std::type_identity_t<T2>, std::type_identity_t<T3>> lf,
                         T0&& a0, T1&& a1, T2&& a2, T3&& a3) {
    if (!filter::should_log_client_loc(level::error, lf.file, lf.func, lf.loc_hash, client_bit)) return;
    if (internal::client_logger_slot())
        internal::client_logger_slot()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                              std::forward<T2>(a2), std::forward<T3>(a3));
}

template<typename T0>
inline void client_debug(uint64_t client_bit, log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_client_loc(level::debug, lf.file, lf.func, lf.loc_hash, client_bit)) return;
    if (internal::client_logger_slot())
        internal::client_logger_slot()->debug(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void client_debug(uint64_t client_bit,
                         log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                         T0&& a0, T1&& a1) {
    if (!filter::should_log_client_loc(level::debug, lf.file, lf.func, lf.loc_hash, client_bit)) return;
    if (internal::client_logger_slot())
        internal::client_logger_slot()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void client_debug(uint64_t client_bit,
                         log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                                  std::type_identity_t<T2>> lf,
                         T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_client_loc(level::debug, lf.file, lf.func, lf.loc_hash, client_bit)) return;
    if (internal::client_logger_slot())
        internal::client_logger_slot()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                              std::forward<T2>(a2));
}

template<typename T0, typename T1, typename T2, typename T3>
inline void client_debug(uint64_t client_bit,
                         log_fmt4<std::type_identity_t<T0>, std::type_identity_t<T1>,
                                  std::type_identity_t<T2>, std::type_identity_t<T3>> lf,
                         T0&& a0, T1&& a1, T2&& a2, T3&& a3) {
    if (!filter::should_log_client_loc(level::debug, lf.file, lf.func, lf.loc_hash, client_bit)) return;
    if (internal::client_logger_slot())
        internal::client_logger_slot()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                              std::forward<T2>(a2), std::forward<T3>(a3));
}

// RTC SERVER LOGGING FUNCTIONS (uses the client logger with [SERVER] [RTC] prefix)
// For RTC-related server logs: [ASE] [SERVER] [RTC] message
//
// DAS ZIEL IST DER SCHNITT: beide Familien schreiben in den ZWEITEN Logger
// (internal::client_logger_slot). Sie unterscheiden sich in der RICHTUNG, nicht im Kanal —
// client_* traegt Zeilen, die aus einem Browser HEREINKOMMEN, rtc_* traegt Zeilen, die der
// Server ueber die RTC-Strecke SELBST erzeugt. Deshalb das [SERVER] [RTC]-Praefix hier und
// gar keines dort. Eine vorbereitete Schnittstelle ist kein toter Code — der zweite Logger
// existiert und wird von log_sys.cpp aufgesetzt.

inline void rtc_info(const std::string& msg) {
    if (internal::client_logger_slot()) internal::client_logger_slot()->info("[SERVER] [RTC] {}", msg);
}

inline void rtc_warn(const std::string& msg) {
    if (internal::client_logger_slot()) internal::client_logger_slot()->warn("[SERVER] [RTC] {}", msg);
}

inline void rtc_error(const std::string& msg) {
    if (internal::client_logger_slot()) internal::client_logger_slot()->error("[SERVER] [RTC] {}", msg);
}

inline void rtc_debug(const std::string& msg) {
    if (internal::client_logger_slot()) internal::client_logger_slot()->debug("[SERVER] [RTC] {}", msg);
}

// DIE FORMATIERENDEN FASSUNGEN NEHMEN DEN log_fmtN-TRAEGER, wie die client_*-Familie darueber.
//
// rtc_* filtert nicht nach Aufrufort, ein Traeger haette hier also nichts zu erfassen — und
// traegt die Entscheidung trotzdem: diese Datei steht NICHT in der namentlichen Ausnahme von
// SPDLOG_DIRECT_FORBIDDEN, und `spdlog::format_string_t` im Vertrag waere hier zwoelfmal ein
// Befund.
//
// `log_fmtN` KAPSELT genau diesen Typ (`spdlog::format_string_t<T0..Tn>` als Feld `fmt`) und
// wird in log.hpp definiert, wo er stehen darf. Die Uebersetzungspruefung der Formatzeichenkette
// gegen T0..Tn bleibt damit unveraendert — sie sitzt im Traeger. Was HINZUKOMMT, ist der
// Aufrufort, den diese Familie nicht auswertet; das kostet sie nichts und nimmt ihr nichts.
//
// Stelligkeit drei als Reserve. Eine vierte ist eine Ueberladung, und der Uebersetzer nennt die
// Stelle, die sie braucht.

template<typename T0>
inline void rtc_info(log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (internal::client_logger_slot()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0));
        internal::client_logger_slot()->info("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1>
inline void rtc_info(log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf, T0&& a0,
                     T1&& a1) {
    if (internal::client_logger_slot()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
        internal::client_logger_slot()->info("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1, typename T2>
inline void rtc_info(log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                              std::type_identity_t<T2>> lf,
                     T0&& a0, T1&& a1, T2&& a2) {
    if (internal::client_logger_slot()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                               std::forward<T2>(a2));
        internal::client_logger_slot()->info("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0>
inline void rtc_warn(log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (internal::client_logger_slot()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0));
        internal::client_logger_slot()->warn("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1>
inline void rtc_warn(log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf, T0&& a0,
                     T1&& a1) {
    if (internal::client_logger_slot()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
        internal::client_logger_slot()->warn("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1, typename T2>
inline void rtc_warn(log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                              std::type_identity_t<T2>> lf,
                     T0&& a0, T1&& a1, T2&& a2) {
    if (internal::client_logger_slot()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                               std::forward<T2>(a2));
        internal::client_logger_slot()->warn("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0>
inline void rtc_error(log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (internal::client_logger_slot()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0));
        internal::client_logger_slot()->error("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1>
inline void rtc_error(log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf, T0&& a0,
                      T1&& a1) {
    if (internal::client_logger_slot()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
        internal::client_logger_slot()->error("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1, typename T2>
inline void rtc_error(log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                               std::type_identity_t<T2>> lf,
                      T0&& a0, T1&& a1, T2&& a2) {
    if (internal::client_logger_slot()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                               std::forward<T2>(a2));
        internal::client_logger_slot()->error("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0>
inline void rtc_debug(log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (internal::client_logger_slot()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0));
        internal::client_logger_slot()->debug("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1>
inline void rtc_debug(log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf, T0&& a0,
                      T1&& a1) {
    if (internal::client_logger_slot()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
        internal::client_logger_slot()->debug("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1, typename T2>
inline void rtc_debug(log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                               std::type_identity_t<T2>> lf,
                      T0&& a0, T1&& a1, T2&& a2) {
    if (internal::client_logger_slot()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                               std::forward<T2>(a2));
        internal::client_logger_slot()->debug("[SERVER] [RTC] {}", msg);
    }
}

}  // namespace ase::log
