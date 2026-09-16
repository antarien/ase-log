#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        log_client.hpp
 * @design      DSGN_021
 * @brief       Weiterleitung von Browser-Konsolenzeilen in das Serverlog
 * @description Die client_*-Familie schreibt in den ZWEITEN Logger des Moduls
 *              (LogSystem::client_logger) — ohne [SERVER]-Praefix, weil die Zeile nicht vom
 *              Server stammt, sondern ueber RTC aus einem Browser hereinkommt. Drei Formen je
 *              Stufe: ungefiltert, gefiltert ueber ein Client-Bit, und formatiert mit
 *              Aufrufort-Erfassung.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    process/computation
 * @created     2026-08-31
 * @modified    2026-08-31
 * @version     1.0.0
 *
 * WARUM DIESER ZWECK EINE EIGENE DATEI IST — GEMESSEN, NICHT GESCHAETZT
 *
 * Diese Familie stand bis 2026-08-31 in `log.hpp`, den 2224 Uebersetzungseinheiten einbinden und
 * 2686 Dateien fuer den gewoehnlichen Aufruf brauchen. Sie selbst hat einen anderen Zweck als
 * jede dieser Dateien: sie bedient einen zweiten Logger fuer fremde Zeilen.
 *
 * ZUR AUFRUFERZAHL, und sie ist der Grund fuer die Trennung, NICHT fuer eine Loeschung:
 * GEMESSEN 2026-08-31, unabhaengig nachgemessen zu einem gleichlautenden Befund vom 2026-08-22:
 * die client_*-Familie hat NULL Aufrufer im ganzen Baum — mit und ohne `log::`-Praefix. Die
 * Positivkontrolle derselben Sondenform findet `info` in 1022 und `warn` in 2365 Dateien, die
 * Sonde misst also wirklich.
 *
 * SIE BLEIBT VOLLSTAENDIG ERHALTEN. Eine vorbereitete Schnittstelle ist kein toter Code: der
 * zweite Logger existiert, wird von log_sys.cpp aufgesetzt und vom ResourceManager gehalten.
 * Was fehlt, ist der Aufrufer, nicht die Sache. Aus einem Header, den der ganze Baum sieht,
 * gehoert sie trotzdem heraus — der Baum zahlt sonst bei JEDER Uebersetzung fuer eine Familie,
 * die niemand ruft.
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
 *   ase::log::client_info(1ULL << client_id, "browser sagt hallo");
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

// Traegt LogSystem::client_logger, die log_fmtN-Traeger und die Filter-Einstiege. Die Richtung
// ist Absicht: DIESE Datei bindet log.hpp ein, nie umgekehrt — ein Durchreicher dort haette den
// ganzen Baum wieder an diese Familie gekoppelt und den Schnitt aufgehoben.
#include <ase/log/log.hpp>

#include <cstdint>
#include <source_location>
#include <string>
#include <type_traits>
#include <utility>

namespace ase::log {

// ============================================================================
// Client Logging Functions (uses client_logger without [SERVER] prefix)
// For forwarding browser console logs via RTC
// ============================================================================

// Unfiltered client logging (backward compatible, no client filter applied)
inline void client_info(const std::string& msg) {
    if (LogSystem::client_logger()) LogSystem::client_logger()->info("{}", msg);
}

inline void client_warn(const std::string& msg) {
    if (LogSystem::client_logger()) LogSystem::client_logger()->warn("{}", msg);
}

inline void client_error(const std::string& msg) {
    if (LogSystem::client_logger()) LogSystem::client_logger()->error("{}", msg);
}

inline void client_debug(const std::string& msg) {
    if (LogSystem::client_logger()) LogSystem::client_logger()->debug("{}", msg);
}

// Filtered client logging with client_bit (use 1ULL << client_id)
// Respects level filter, category filter, AND client filter (+CLT:01, -CLT)
inline void client_info(uint64_t client_bit, const std::string& msg,
                        const std::source_location& loc = std::source_location::current()) {
    if (!should_log_client(level::info, client_bit, loc)) return;
    if (LogSystem::client_logger()) LogSystem::client_logger()->info("{}", msg);
}

inline void client_warn(uint64_t client_bit, const std::string& msg,
                        const std::source_location& loc = std::source_location::current()) {
    if (!should_log_client(level::warn, client_bit, loc)) return;
    if (LogSystem::client_logger()) LogSystem::client_logger()->warn("{}", msg);
}

inline void client_error(uint64_t client_bit, const std::string& msg,
                         const std::source_location& loc = std::source_location::current()) {
    if (!should_log_client(level::error, client_bit, loc)) return;
    if (LogSystem::client_logger()) LogSystem::client_logger()->error("{}", msg);
}

inline void client_debug(uint64_t client_bit, const std::string& msg,
                         const std::source_location& loc = std::source_location::current()) {
    if (!should_log_client(level::debug, client_bit, loc)) return;
    if (LogSystem::client_logger()) LogSystem::client_logger()->debug("{}", msg);
}

// Filtered client logging with fmt-style formatting
// Full filtering: level + O(1) cached category + client filter
//
// ARITY THREE HERE, NOT NINE — and the difference is measured, not guessed:
// GEMESSEN 2026-08-22, the client_* family has ZERO callers in the whole tree (both with and
// without the log:: prefix, cross-checked against log::info which finds 464 files). There is no
// measured arity to cover. GESETZT is three as a reserve, the same shape ring_buffer.hpp chose
// for the same reason. Needing a fourth costs one overload written after this pattern, and the
// COMPILER names the call site — a missing overload cannot fail silently at run time.
// The six main levels in log.hpp carry nine because there the demand is measured (debug reaches 9).

template<typename T0>
inline void client_info(uint64_t client_bit, log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_client_loc(filter::LVL_INF, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->info(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void client_info(uint64_t client_bit,
                        log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                        T0&& a0, T1&& a1) {
    if (!filter::should_log_client_loc(filter::LVL_INF, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void client_info(uint64_t client_bit,
                        log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                                 std::type_identity_t<T2>> lf,
                        T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_client_loc(filter::LVL_INF, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                         std::forward<T2>(a2));
}

template<typename T0>
inline void client_warn(uint64_t client_bit, log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_client_loc(filter::LVL_WRN, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->warn(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void client_warn(uint64_t client_bit,
                        log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                        T0&& a0, T1&& a1) {
    if (!filter::should_log_client_loc(filter::LVL_WRN, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void client_warn(uint64_t client_bit,
                        log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                                 std::type_identity_t<T2>> lf,
                        T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_client_loc(filter::LVL_WRN, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                         std::forward<T2>(a2));
}

template<typename T0>
inline void client_error(uint64_t client_bit, log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_client_loc(filter::LVL_ERR, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->error(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void client_error(uint64_t client_bit,
                         log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                         T0&& a0, T1&& a1) {
    if (!filter::should_log_client_loc(filter::LVL_ERR, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void client_error(uint64_t client_bit,
                         log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                                  std::type_identity_t<T2>> lf,
                         T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_client_loc(filter::LVL_ERR, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                          std::forward<T2>(a2));
}

template<typename T0>
inline void client_debug(uint64_t client_bit, log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_client_loc(filter::LVL_DBG, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->debug(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void client_debug(uint64_t client_bit,
                         log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                         T0&& a0, T1&& a1) {
    if (!filter::should_log_client_loc(filter::LVL_DBG, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void client_debug(uint64_t client_bit,
                         log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                                  std::type_identity_t<T2>> lf,
                         T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_client_loc(filter::LVL_DBG, lf.file, lf.func, client_bit)) return;
    if (LogSystem::client_logger())
        LogSystem::client_logger()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                          std::forward<T2>(a2));
}

// ============================================================================
// RTC Server Logging Functions (uses client_logger with [SERVER] [RTC] prefix)
// For RTC-related server logs: [ASE] [SERVER] [RTC] message
// ============================================================================
//
// HIERHER GEZOGEN AM 2026-08-31, aus log.hpp — und sie stand dort zwei Zeilen unter der
// client_*-Familie, die am selben Tag denselben Weg genommen hat. Beim ersten Schnitt
// uebersehen, weil er nach dem NAMEN ging und nicht nach dem ZIEL.
//
// DAS ZIEL IST DER SCHNITT: beide Familien schreiben in den ZWEITEN Logger
// (LogSystem::client_logger). Sie unterscheiden sich in der RICHTUNG, nicht im Kanal —
// client_* traegt Zeilen, die aus einem Browser HEREINKOMMEN, rtc_* traegt Zeilen, die der
// Server ueber die RTC-Strecke SELBST erzeugt. Deshalb das [SERVER] [RTC]-Praefix hier und
// gar keines dort.
//
// GEMESSEN, mit greifender Positivkontrolle: NULL Aufrufer im ganzen Baum (dieselbe Sondenform
// findet `log::debug` und `client_info` je an mehreren Stellen). Eine vorbereitete
// Schnittstelle ist kein toter Code — der zweite Logger existiert und wird aufgesetzt. Aus
// einem Header, den 2224 Uebersetzungseinheiten fuer den GEWOEHNLICHEN Aufruf einbinden,
// gehoert sie dennoch heraus: sonst zahlt der ganze Baum bei JEDER Uebersetzung fuer eine
// Familie, die niemand ruft — und schleppt dabei `spdlog::format_string_t` in seinem Vertrag
// mit, zwoelfmal.

inline void rtc_info(const std::string& msg) {
    if (LogSystem::client_logger()) LogSystem::client_logger()->info("[SERVER] [RTC] {}", msg);
}

inline void rtc_warn(const std::string& msg) {
    if (LogSystem::client_logger()) LogSystem::client_logger()->warn("[SERVER] [RTC] {}", msg);
}

inline void rtc_error(const std::string& msg) {
    if (LogSystem::client_logger()) LogSystem::client_logger()->error("[SERVER] [RTC] {}", msg);
}

inline void rtc_debug(const std::string& msg) {
    if (LogSystem::client_logger()) LogSystem::client_logger()->debug("[SERVER] [RTC] {}", msg);
}

// DIE FORMATIERENDEN FASSUNGEN NEHMEN DEN log_fmtN-TRAEGER, wie die client_*-Familie darueber.
//
// In log.hpp nannten sie `spdlog::format_string_t` DIREKT, und der Kommentar dort begruendete es
// damit, dass ein Traeger hier nichts zu erfassen habe: rtc_* filtert nicht nach Aufrufort. Das
// stimmt — und traegt die Entscheidung trotzdem nicht. In log.hpp fiel die direkte Nennung nicht
// auf, weil jene Datei in der namentlichen Ausnahme von SPDLOG_DIRECT_FORBIDDEN steht; DIESE
// nicht. Der Bibliothekstyp im Vertrag ist hier zwoelfmal ein Befund.
//
// `log_fmtN` KAPSELT genau diesen Typ (`spdlog::format_string_t<T0..Tn>` als Feld `fmt`) und
// wird in log.hpp definiert, wo er stehen darf. Die Uebersetzungspruefung der Formatzeichenkette
// gegen T0..Tn bleibt damit unveraendert — sie sitzt im Traeger. Was HINZUKOMMT, ist der
// Aufrufort, den diese Familie nicht auswertet; das kostet sie nichts und nimmt ihr nichts.
//
// Stelligkeit drei, aus demselben Grund wie bei client_* oben: GEMESSEN null Aufrufer baumweit,
// GESETZT drei als Reserve. Eine vierte ist eine Ueberladung, und der Uebersetzer nennt die
// Stelle, die sie braucht.

template<typename T0>
inline void rtc_info(log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0));
        LogSystem::client_logger()->info("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1>
inline void rtc_info(log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf, T0&& a0,
                     T1&& a1) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
        LogSystem::client_logger()->info("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1, typename T2>
inline void rtc_info(log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                              std::type_identity_t<T2>> lf,
                     T0&& a0, T1&& a1, T2&& a2) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                               std::forward<T2>(a2));
        LogSystem::client_logger()->info("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0>
inline void rtc_warn(log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0));
        LogSystem::client_logger()->warn("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1>
inline void rtc_warn(log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf, T0&& a0,
                     T1&& a1) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
        LogSystem::client_logger()->warn("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1, typename T2>
inline void rtc_warn(log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                              std::type_identity_t<T2>> lf,
                     T0&& a0, T1&& a1, T2&& a2) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                               std::forward<T2>(a2));
        LogSystem::client_logger()->warn("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0>
inline void rtc_error(log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0));
        LogSystem::client_logger()->error("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1>
inline void rtc_error(log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf, T0&& a0,
                      T1&& a1) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
        LogSystem::client_logger()->error("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1, typename T2>
inline void rtc_error(log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                               std::type_identity_t<T2>> lf,
                      T0&& a0, T1&& a1, T2&& a2) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                               std::forward<T2>(a2));
        LogSystem::client_logger()->error("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0>
inline void rtc_debug(log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0));
        LogSystem::client_logger()->debug("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1>
inline void rtc_debug(log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf, T0&& a0,
                      T1&& a1) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
        LogSystem::client_logger()->debug("[SERVER] [RTC] {}", msg);
    }
}

template<typename T0, typename T1, typename T2>
inline void rtc_debug(log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                               std::type_identity_t<T2>> lf,
                      T0&& a0, T1&& a1, T2&& a2) {
    if (LogSystem::client_logger()) {
        auto msg = fmt::format(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                               std::forward<T2>(a2));
        LogSystem::client_logger()->debug("[SERVER] [RTC] {}", msg);
    }
}

}  // namespace ase::log
