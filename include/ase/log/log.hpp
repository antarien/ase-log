#pragma once

// GCC14 has a false positive -Wdangling-reference warning with spdlog/fmt
// See: https://github.com/fmtlib/fmt/issues/3415
#if defined(__GNUC__) && __GNUC__ >= 14
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdangling-reference"
#endif

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        log.hpp
 * @brief       The log call of the whole tree: plain, formatted and categorized lines
 * @description Provides the inline log functions every unit calls (info, warn, error, debug,
 *              trace, critical) in their plain, formatted and categorized forms. They write
 *              through the server logger the LogResourceManager holds; the system that opens its
 *              sinks in a running app is LogSystem (ase/log/systems/log_sys.hpp), the entries for
 *              a program without an ECS world stand in ase/log/log_lifecycle.hpp.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @design      DSGN_021
 * @category    error/logging
 * @usecase     usecase:100362 - write a log line
 * @created     2024-01-01
 * @modified    2026-10-19
 * @version     2.3.0
 *
 * THE CHECK OF A CALL STANDS HERE, THE FILTER CONTROL DOES NOT. Every line asks the filter
 * whether it may be written, so the level bits, the location key and the declarations of the
 * two checks belong to the header every caller holds. The filter state, the category mask and
 * the CLI control (parse_cli_filter, set_level_mask, block_category, ...) stand in
 * ase/log/log_filter.hpp and the generated log_cat_gen.hpp; a unit that sets a filter includes
 * log_filter.hpp itself, and a change there rebuilds those units, not every unit that logs.
 *
 * LAYER RULES:
 *   Layer 0 (Foundation): NO dependencies on other ASE modules (only std::)
 *   Layer 1 (Core):       May depend on Layer 0 only
 *
 * USAGE:
 *   // ECS-based apps (LogModule registers LogSystem, which opens the sinks):
 *   app.add_module<ase::log::LogModule>();
 *   ase::log::info("Server started on port {}", 8090);
 *
 *   // CLI tools (manual initialization, ase/log/log_lifecycle.hpp):
 *   ase::log::init("ase-codegen");
 *   ase::log::info("Processing module: {}", module_name);
 *   ase::log::shutdown();
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

#include <ase/log/log_err_cat_gen.hpp>
#include <spdlog/logger.h>
// Gebraucht werden hier VIER Symbole der Bibliothek, und alle vier stehen in `logger.h` bzw. dem
// `common.h`, das es mitbringt:
//     spdlog::logger            die beiden Slot-Referenzen
//     spdlog::sinks::sink       der Ringpuffer-Slot
//     spdlog::format_string_t   die neun log_fmt-Traeger
//     spdlog::level::level_enum set_level
//
// WAS DER DACHHEADER DARUEBER HINAUS MITBRAECHTE: die Logger-REGISTRY samt globaler API
// (register_logger, shutdown, die synchronous_factory, die freien Logfunktionen). Diese Datei
// ruft davon NICHTS — die Registry-Aufrufe stehen in log_sys.cpp, und die bindet ihren Header
// selbst ein. Ein Dachheader liefert, was die Bibliothek anzubieten HAT, nicht was der Aufrufer
// BRAUCHT; genau darin liegt sein Preis.
//
// Die Senken-Header stehen ebenso bei ihren Aufrufern (log_standalone.cpp, log_sys.cpp, die
// Fabriken in log_resource_manager.cpp): dieser Header steht in fast jeder Uebersetzungseinheit
// jedes Tiers, und der Preis faellt nicht bei Aenderungen an, sondern bei JEDEM einzelnen Bau.
#include <cstdint>
#include <memory>
#include <source_location>
#include <string>
#include <type_traits>

namespace ase::log {

// LEVEL BITS
//
// The one home of the level bitmask. The filter state (filter::level_mask() in log_cat_gen.cpp)
// holds an OR of these bits, the CLI filter sets them, and every check below tests one of them.
namespace level {
constexpr uint8_t trace    = 0x01u;  ///< Trace level
constexpr uint8_t debug    = 0x02u;  ///< Debug level
constexpr uint8_t info     = 0x04u;  ///< Info level
constexpr uint8_t warn     = 0x08u;  ///< Warning level
constexpr uint8_t error    = 0x10u;  ///< Error level
constexpr uint8_t critical = 0x20u;  ///< Critical level
constexpr uint8_t all      = 0x3Fu;  ///< All levels enabled
}  // namespace level

// THE TWO CHECKS OF THE FILTER (defined in log_cat_gen.cpp)
//
// O(1) cached check for file + function (~12 cycles on the hot path). The location key is
// computed at the CALL SITE (loc_key below, constexpr-folded), not inside the check: hashing the
// two strings per call would multiply the fast path the cache exists to avoid. A key taken from
// the two POINTERS by reinterpret_cast is forbidden and unstable across translation units, where
// identical __FILE__ literals may sit at different addresses.
namespace filter {
bool should_log_loc(uint8_t level, const char* file, const char* func, uint64_t key);

/// O(1) cached check for a client line: the location check plus the client masks
bool should_log_client_loc(uint8_t level, const char* file, const char* func, uint64_t key,
                           uint64_t client_bit);
}  // namespace filter

/**
 * @brief Location key for the filter cache, computed at COMPILE time.
 *
 * THE KEY HASHES THE CONTENT OF THE TWO STRINGS, NEVER THEIR ADDRESSES. An address key needs a
 * reinterpret_cast, which is forbidden (REINTERPRET_CAST_FORBIDDEN, WRFL_ASE_FLYWEIGHT.md), and
 * it is not stable: identical __FILE__ literals in different translation units may live at
 * different addresses, so one source line would occupy several cache slots while two different
 * lines could collide.
 *
 * Hashing the content costs nothing at run time. Every caller passes strings that are call-site
 * constants - the log_fmtN constructors take them from __builtin_FILE()/__builtin_FUNCTION()
 * defaults, and std::source_location::current() is fixed at the call site too - so this folds
 * to an immediate.
 *
 * THE SEPARATOR IS NOT COSMETIC: without it ("ab","c") and ("a","bc") hash identically.
 *
 * @param file The caller's source file name
 * @param func The caller's function name
 * @return The FNV-1a key of both strings with a separator between them
 */
[[nodiscard]] constexpr uint64_t loc_key(const char* file, const char* func) noexcept {
    constexpr uint64_t kOffsetBasis = 14695981039346656037ULL;
    constexpr uint64_t kPrime       = 1099511628211ULL;
    constexpr uint64_t kSeparator   = 0x1FULL;  // unit separator, occurs in neither string

    uint64_t hash = kOffsetBasis;
    for (const char* p = file; p != nullptr && *p != '\0'; ++p) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(*p));
        hash *= kPrime;
    }
    hash ^= kSeparator;
    hash *= kPrime;
    for (const char* p = func; p != nullptr && *p != '\0'; ++p) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(*p));
        hash *= kPrime;
    }
    return hash;
}

/**
 * @brief Whether a line of this level from this location passes the filter.
 * @param lvl Log level (level::trace, level::info, ...)
 * @param loc Source location (defaults to the caller's location)
 * @return true if the line is written, false if it is filtered out
 *
 * The categories come from BOTH the source file name AND the function name, which enables
 * lifecycle-phase filtering (+TICK, -START, +STOP).
 */
[[nodiscard]] inline bool should_log_loc(
    uint8_t lvl,
    const std::source_location& loc = std::source_location::current()) noexcept {
    return filter::should_log_loc(lvl, loc.file_name(), loc.function_name(),
                                  loc_key(loc.file_name(), loc.function_name()));
}

/**
 * @brief Whether a client line passes the filter.
 * @param lvl Log level (level::trace, level::info, ...)
 * @param client_bit Client bitmask (1ULL << client_id, 0 = server log, always passes)
 * @param loc Source location (defaults to the caller's location)
 * @return true if the line is written, false if it is filtered out
 */
[[nodiscard]] inline bool should_log_client(
    uint8_t lvl, uint64_t client_bit,
    const std::source_location& loc = std::source_location::current()) noexcept {
    return filter::should_log_client_loc(lvl, loc.file_name(), loc.function_name(),
                                         loc_key(loc.file_name(), loc.function_name()), client_bit);
}

// LEVEL INDEXES

// Level assumed when a ringbuffer line carries no parsable "[LVL]" field. INF is the only safe
// default: guessing lower would hide a warning behind a filter, guessing higher would raise every
// unparsable line to an alarm. The value is the index into the level table (0=trace..5=critical).
inline constexpr uint8_t kLogLevelInfo = 2u;

// Der unterste Index derselben Tabelle: alles wird durchgelassen. Die Standalone-Einstiege
// uebergeben ihre Stufe als ZAHL statt als Konstante der Bibliothek und setzen beide Stufen auf
// TRC, damit ein abstuerzendes Werkzeug nicht genau die Zeilen im Puffer mitnimmt, die den
// Absturz erklaeren.
inline constexpr uint8_t kLogLevelTrace = 0u;

// Highest level index the tables in this header carry: 0 TRC, 1 DBG, 2 INF, 3 WRN, 4 ERR, 5 CRT.
// The underlying library knows one more value above CRT that means "logging off"; a line can never
// arrive carrying it, but capture_entry() clamps to this bound rather than trusting that, because
// the alternative is an out-of-range index into a caller's colour table.
inline constexpr uint8_t kLogLevelMax = 5u;

// Stack buffer for a spdlog pattern assembled at startup. The longest pattern in the tree is the
// coloured server line (~55 chars of escapes and fields) plus LogConfig::label (declared
// `char label[16]` in log_module.hpp) plus the "] %v" tail — 128 leaves more than double the headroom, and the
// buffer lives for the two statements it takes to hand the pattern to spdlog.
inline constexpr uint32_t kLogPatternBufSize = 128u;

/**
 * WO DER LOGGER LIEGT
 *
 * Der Zustand liegt im LogResourceManager, dem von WRFL_ASE_FLYWEIGHT.md Section 1
 * vorgesehenen Halter. Er ist hier der einzig moegliche: `log::init` laeuft VOR `Kernel::build`,
 * also gibt es zum Zeitpunkt der Logger-Erzeugung keine Registry und damit kein `registry.ctx()`
 * (der Kopfkommentar von log_resource_manager.cpp fuehrt dieselbe Begruendung aus).
 *
 * ES IST EINE VORWAERTSDEKLARATION UND KEIN INCLUDE, mit Absicht: `internal/` bleibt intern.
 * Der Manager-Header wird von drei .cpp inkludiert und von keinem oeffentlichen Header — ihn
 * hier hereinzuziehen waere der bequeme Weg und wuerde die Kapselung fuer einen Zeilenersparnis
 * aufgeben.
 *
 * Every inline log function below reads these two slots directly. The system that opens the
 * sinks, LogSystem, has its own header (ase/log/systems/log_sys.hpp): a unit that writes a line
 * names neither it nor the ECS base class it derives from, and a change to its declaration
 * rebuilds only LogModule and its own implementation.
 */
namespace internal {
[[nodiscard]] std::shared_ptr<spdlog::logger>& logger_slot();
[[nodiscard]] std::shared_ptr<spdlog::logger>& client_logger_slot();
}  // namespace internal

// WAS DIESER HEADER NICHT TRAEGT
//
// Die Rotations- und Aufbewahrungsschranken samt LogQuota, log_quota(), log_quota_in(),
// set_log_quota(), log_dir_path() und log_quota_path() stehen in <ase/log/log_quota.hpp> — wer
// eine Quota liest oder schreibt, bindet diesen Header ein.
//
// Die vier Einstiege, mit denen ein Programm OHNE ECS-Welt einen Logger bekommt (init,
// init_server_standalone, init_tui_standalone, install_capture_logger), die Capture-Klammer, der
// Filter-Einstieg aus argv, finalize_logger_after_boot und shutdown stehen in
// <ase/log/log_lifecycle.hpp>. Die client_*- und die rtc_*-Familie, die in den ZWEITEN Logger des
// Moduls schreiben, stehen in <ase/log/log_client.hpp>.
//
// The read side of the ring (recent_logs, LogEntry, kLogSeqAll, kRecentLogsPageSize) stands in
// <ase/log/log_query.hpp>. It is the only part of the log that needs a container, and a unit that
// writes a line does not see that container.
//
// UND ES GIBT HIER ABSICHTLICH KEIN `#include` auf einen dieser Header. Ein Durchreicher waere
// bequem — er wuerde aber genau die Kopplung herstellen, die der Schnitt aufloest: ueber ihn
// haenge jede Einheit, die eine Zeile schreibt, am Lebenszyklus, an der Quota und an der
// Client-Familie, und eine Aenderung dort baute den Baum. Wer eine dieser Funktionen ruft,
// bindet den Header ein, in dem sie steht.
//
// NICHT DIE ZEILENZAHL WAR DER ANLASS, SONDERN DIE KOPPLUNG: ein Header, den der ganze Baum
// sieht, darf sich nur aendern, wenn sich der Zweck aendert, den der ganze Baum braucht. Das ist
// hier der AUFRUF.

// GLOBAL LOGGING FUNCTIONS (the server logger the LogResourceManager holds)

// Simple string logging with compile-time category filtering
// source_location default param captures CALL SITE, not this file
inline void info(const std::string& msg,
                 const std::source_location& loc = std::source_location::current()) {
    if (!should_log_loc(level::info, loc)) return;
    if (internal::logger_slot()) internal::logger_slot()->info("{}", msg);
}

inline void warn(const std::string& msg,
                 const std::source_location& loc = std::source_location::current()) {
    if (!should_log_loc(level::warn, loc)) return;
    if (internal::logger_slot()) internal::logger_slot()->warn("{}", msg);
}

inline void error(const std::string& msg,
                  const std::source_location& loc = std::source_location::current()) {
    if (!should_log_loc(level::error, loc)) return;
    if (internal::logger_slot()) internal::logger_slot()->error("{}", msg);
}

inline void debug(const std::string& msg,
                  const std::source_location& loc = std::source_location::current()) {
    if (!should_log_loc(level::debug, loc)) return;
    if (internal::logger_slot()) internal::logger_slot()->debug("{}", msg);
}

inline void trace(const std::string& msg,
                  const std::source_location& loc = std::source_location::current()) {
    if (!should_log_loc(level::trace, loc)) return;
    if (internal::logger_slot()) internal::logger_slot()->trace("{}", msg);
}

inline void critical(const std::string& msg,
                     const std::source_location& loc = std::source_location::current()) {
    if (!should_log_loc(level::critical, loc)) return;
    if (internal::logger_slot()) internal::logger_slot()->critical("{}", msg);
}

// FORMAT STRING WRAPPER THAT CAPTURES CALLER'S FILENAME VIA __builtin_FILE()

/**
 * WHY NINE WRAPPERS INSTEAD OF ONE PARAMETER PACK
 *
 * These wrap spdlog::format_string_t and capture the caller's source filename + function.
 * The constructor's __builtin_FILE()/__builtin_FUNCTION() defaults are evaluated at the CALL
 * SITE, which is what makes category-based filtering (file + lifecycle phase) work for
 * fmt-style calls. That capture is independent of consteval: a default argument is evaluated
 * where the call is written, so constexpr carries it just as well.
 *
 * One wrapper per arity, because a pack is forbidden and the format string has to be
 * parameterised over EXACTLY the argument types to keep fmt's compile-time check of the format
 * string against those types. Erasing the types into one value type would move that check to
 * run time, which is a capability loss and therefore not an option.
 *
 * THE WRAPPER IS NOT DECORATION — it is what makes the overload set safe. Taking
 * format_string_t directly and appending file/func as defaulted parameters compiles, and then
 * log::info("{} {}", "a", "b") silently binds to the ONE-argument overload with "b" as the
 * filename. Holding the defaults inside the wrapper's constructor makes that impossible.
 *
 * ARITY NINE: GEMESSEN 2026-08-22, the highest arity anywhere in the tree is 9
 * (replica_topo_swep_sys.cpp). Per function the measured maxima are info 8, warn 6, error 5,
 * debug 9; the other ten families have no caller at all. GESETZT is the uniform 9 for every
 * family — an uneven ceiling would be a trap, because adding one argument to a warn line would
 * break the build for a reason nobody can see at the call site. Needing a tenth means adding
 * one wrapper and one overload per level, and the compiler names the site.
 *
 * EVERY ARGUMENT NEEDS A FIELD IN THE FORMAT STRING. fmt checks only the other direction at
 * compile time: a field without an argument fails, an argument without a field is dropped in
 * silence. Because a log_fmtN is built implicitly from any string, log::debug("Sys", "op", detail)
 * makes "Sys" the format of a log_fmt2 and loses both real arguments without a word. Each
 * constructor therefore counts the replacement fields of its literal (a nested field such as the
 * dynamic width of "{:{}}" counts too) and refuses fewer fields than arguments.
 */
namespace detail {

// Declared and never defined, and not constexpr: the wrapper constructors are evaluated at
// compile time (fmt's format check is consteval), so a call that reaches this function ends
// the constant evaluation and the log line with too few fields does not compile.
void format_string_names_fewer_fields_than_arguments() noexcept;

/**
 * @brief The replacement fields of a format string, escaped braces ({{ and }}) excluded.
 * @param text The format string
 * @param length Its length in chars
 * @return The number of fields, nested fields included
 */
[[nodiscard]] constexpr uint32_t format_field_count(const char* text, uint64_t length) noexcept {
    uint32_t fields = 0u;
    uint64_t at = 0u;
    while (at < length) {
        if (text[at] != '{') {
            ++at;
            continue;
        }
        if (at + 1u < length && text[at + 1u] == '{') {
            at += 2u;
            continue;
        }
        ++fields;
        uint32_t depth = 1u;
        ++at;
        while (at < length && depth > 0u) {
            if (text[at] == '{') {
                ++fields;
                ++depth;
            }
            if (text[at] == '}') --depth;
            ++at;
        }
    }
    return fields;
}

/**
 * @brief Refuses a format string that names fewer fields than the call passes arguments.
 * @param format The literal the wrapper is built from, NUL-terminated
 * @param arguments The arity of the wrapper
 */
constexpr void require_field_per_argument(const char* format, uint32_t arguments) noexcept {
    uint64_t length = 0u;
    while (format[length] != '\0') ++length;
    if (format_field_count(format, length) < arguments) {
        format_string_names_fewer_fields_than_arguments();
    }
}

}  // namespace detail

template<typename T0>
struct log_fmt1 {
    spdlog::format_string_t<T0> fmt;
    const char* file;
    const char* func;
    uint64_t loc_hash;  // constexpr-folded call-site key, see loc_key() above

    template<typename S>
        requires (!std::is_arithmetic_v<std::remove_cvref_t<S>>)
    constexpr log_fmt1(const S& s, const char* f = __builtin_FILE(),
                       const char* fn = __builtin_FUNCTION())
        : fmt(s), file(f), func(fn), loc_hash(loc_key(f, fn)) {
        detail::require_field_per_argument(s, 1u);
    }
};

template<typename T0, typename T1>
struct log_fmt2 {
    spdlog::format_string_t<T0, T1> fmt;
    const char* file;
    const char* func;
    uint64_t loc_hash;  // constexpr-folded call-site key, see loc_key() above

    template<typename S>
        requires (!std::is_arithmetic_v<std::remove_cvref_t<S>>)
    constexpr log_fmt2(const S& s, const char* f = __builtin_FILE(),
                       const char* fn = __builtin_FUNCTION())
        : fmt(s), file(f), func(fn), loc_hash(loc_key(f, fn)) {
        detail::require_field_per_argument(s, 2u);
    }
};

template<typename T0, typename T1, typename T2>
struct log_fmt3 {
    spdlog::format_string_t<T0, T1, T2> fmt;
    const char* file;
    const char* func;
    uint64_t loc_hash;  // constexpr-folded call-site key, see loc_key() above

    template<typename S>
        requires (!std::is_arithmetic_v<std::remove_cvref_t<S>>)
    constexpr log_fmt3(const S& s, const char* f = __builtin_FILE(),
                       const char* fn = __builtin_FUNCTION())
        : fmt(s), file(f), func(fn), loc_hash(loc_key(f, fn)) {
        detail::require_field_per_argument(s, 3u);
    }
};

template<typename T0, typename T1, typename T2, typename T3>
struct log_fmt4 {
    spdlog::format_string_t<T0, T1, T2, T3> fmt;
    const char* file;
    const char* func;
    uint64_t loc_hash;  // constexpr-folded call-site key, see loc_key() above

    template<typename S>
        requires (!std::is_arithmetic_v<std::remove_cvref_t<S>>)
    constexpr log_fmt4(const S& s, const char* f = __builtin_FILE(),
                       const char* fn = __builtin_FUNCTION())
        : fmt(s), file(f), func(fn), loc_hash(loc_key(f, fn)) {
        detail::require_field_per_argument(s, 4u);
    }
};

template<typename T0, typename T1, typename T2, typename T3, typename T4>
struct log_fmt5 {
    spdlog::format_string_t<T0, T1, T2, T3, T4> fmt;
    const char* file;
    const char* func;
    uint64_t loc_hash;  // constexpr-folded call-site key, see loc_key() above

    template<typename S>
        requires (!std::is_arithmetic_v<std::remove_cvref_t<S>>)
    constexpr log_fmt5(const S& s, const char* f = __builtin_FILE(),
                       const char* fn = __builtin_FUNCTION())
        : fmt(s), file(f), func(fn), loc_hash(loc_key(f, fn)) {
        detail::require_field_per_argument(s, 5u);
    }
};

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
struct log_fmt6 {
    spdlog::format_string_t<T0, T1, T2, T3, T4, T5> fmt;
    const char* file;
    const char* func;
    uint64_t loc_hash;  // constexpr-folded call-site key, see loc_key() above

    template<typename S>
        requires (!std::is_arithmetic_v<std::remove_cvref_t<S>>)
    constexpr log_fmt6(const S& s, const char* f = __builtin_FILE(),
                       const char* fn = __builtin_FUNCTION())
        : fmt(s), file(f), func(fn), loc_hash(loc_key(f, fn)) {
        detail::require_field_per_argument(s, 6u);
    }
};

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6>
struct log_fmt7 {
    spdlog::format_string_t<T0, T1, T2, T3, T4, T5, T6> fmt;
    const char* file;
    const char* func;
    uint64_t loc_hash;  // constexpr-folded call-site key, see loc_key() above

    template<typename S>
        requires (!std::is_arithmetic_v<std::remove_cvref_t<S>>)
    constexpr log_fmt7(const S& s, const char* f = __builtin_FILE(),
                       const char* fn = __builtin_FUNCTION())
        : fmt(s), file(f), func(fn), loc_hash(loc_key(f, fn)) {
        detail::require_field_per_argument(s, 7u);
    }
};

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7>
struct log_fmt8 {
    spdlog::format_string_t<T0, T1, T2, T3, T4, T5, T6, T7> fmt;
    const char* file;
    const char* func;
    uint64_t loc_hash;  // constexpr-folded call-site key, see loc_key() above

    template<typename S>
        requires (!std::is_arithmetic_v<std::remove_cvref_t<S>>)
    constexpr log_fmt8(const S& s, const char* f = __builtin_FILE(),
                       const char* fn = __builtin_FUNCTION())
        : fmt(s), file(f), func(fn), loc_hash(loc_key(f, fn)) {
        detail::require_field_per_argument(s, 8u);
    }
};

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7, typename T8>
struct log_fmt9 {
    spdlog::format_string_t<T0, T1, T2, T3, T4, T5, T6, T7, T8> fmt;
    const char* file;
    const char* func;
    uint64_t loc_hash;  // constexpr-folded call-site key, see loc_key() above

    template<typename S>
        requires (!std::is_arithmetic_v<std::remove_cvref_t<S>>)
    constexpr log_fmt9(const S& s, const char* f = __builtin_FILE(),
                       const char* fn = __builtin_FUNCTION())
        : fmt(s), file(f), func(fn), loc_hash(loc_key(f, fn)) {
        detail::require_field_per_argument(s, 9u);
    }
};

// Formatted logging (fmt style) with full category filtering.
// The log_fmtN wrappers capture the caller's filename in their constexpr constructor,
// so category blacklist AND whitelist work for all fmt-style log calls.
// Fast path: should_log_loc() = level + O(1) cached file+func filter (~12 cycles).
//
// std::type_identity_t on the wrapper's parameters is what stops the compiler from deducing
// T0..Tn from the FORMAT STRING; they are deduced from the arguments alone, and the wrapper is
// then built from the literal with exactly those types. Without it the first parameter would
// fight the rest for the deduction and every call with a plain literal would fail.
//
// The zero-argument case is NOT here: log::info("plain text") binds to the non-template
// const std::string& overload above. That asymmetry — non-template for zero arguments,
// template for one and more — IS the resolution mechanism, and it is why 4819 measured
// call sites keep working untouched.

template<typename T0>
inline void info(log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_loc(level::info, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot()) internal::logger_slot()->info(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void info(log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                 T0&& a0, T1&& a1) {
    if (!filter::should_log_loc(level::info, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void info(log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>> lf, T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_loc(level::info, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                      std::forward<T2>(a2));
}

template<typename T0, typename T1, typename T2, typename T3>
inline void info(log_fmt4<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3) {
    if (!filter::should_log_loc(level::info, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                      std::forward<T2>(a2), std::forward<T3>(a3));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4>
inline void info(log_fmt5<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>,
                          std::type_identity_t<T4>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4) {
    if (!filter::should_log_loc(level::info, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                      std::forward<T2>(a2), std::forward<T3>(a3),
                                      std::forward<T4>(a4));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
inline void info(log_fmt6<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>,
                          std::type_identity_t<T4>, std::type_identity_t<T5>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5) {
    if (!filter::should_log_loc(level::info, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                      std::forward<T2>(a2), std::forward<T3>(a3),
                                      std::forward<T4>(a4), std::forward<T5>(a5));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6>
inline void info(log_fmt7<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>,
                          std::type_identity_t<T4>, std::type_identity_t<T5>,
                          std::type_identity_t<T6>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6) {
    if (!filter::should_log_loc(level::info, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                      std::forward<T2>(a2), std::forward<T3>(a3),
                                      std::forward<T4>(a4), std::forward<T5>(a5),
                                      std::forward<T6>(a6));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7>
inline void info(log_fmt8<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>,
                          std::type_identity_t<T4>, std::type_identity_t<T5>,
                          std::type_identity_t<T6>, std::type_identity_t<T7>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7) {
    if (!filter::should_log_loc(level::info, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                      std::forward<T2>(a2), std::forward<T3>(a3),
                                      std::forward<T4>(a4), std::forward<T5>(a5),
                                      std::forward<T6>(a6), std::forward<T7>(a7));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7, typename T8>
inline void info(log_fmt9<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>,
                          std::type_identity_t<T4>, std::type_identity_t<T5>,
                          std::type_identity_t<T6>, std::type_identity_t<T7>,
                          std::type_identity_t<T8>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7,
                 T8&& a8) {
    if (!filter::should_log_loc(level::info, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->info(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                      std::forward<T2>(a2), std::forward<T3>(a3),
                                      std::forward<T4>(a4), std::forward<T5>(a5),
                                      std::forward<T6>(a6), std::forward<T7>(a7),
                                      std::forward<T8>(a8));
}

template<typename T0>
inline void warn(log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_loc(level::warn, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot()) internal::logger_slot()->warn(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void warn(log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                 T0&& a0, T1&& a1) {
    if (!filter::should_log_loc(level::warn, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void warn(log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>> lf, T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_loc(level::warn, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                      std::forward<T2>(a2));
}

template<typename T0, typename T1, typename T2, typename T3>
inline void warn(log_fmt4<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3) {
    if (!filter::should_log_loc(level::warn, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                      std::forward<T2>(a2), std::forward<T3>(a3));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4>
inline void warn(log_fmt5<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>,
                          std::type_identity_t<T4>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4) {
    if (!filter::should_log_loc(level::warn, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                      std::forward<T2>(a2), std::forward<T3>(a3),
                                      std::forward<T4>(a4));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
inline void warn(log_fmt6<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>,
                          std::type_identity_t<T4>, std::type_identity_t<T5>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5) {
    if (!filter::should_log_loc(level::warn, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                      std::forward<T2>(a2), std::forward<T3>(a3),
                                      std::forward<T4>(a4), std::forward<T5>(a5));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6>
inline void warn(log_fmt7<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>,
                          std::type_identity_t<T4>, std::type_identity_t<T5>,
                          std::type_identity_t<T6>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6) {
    if (!filter::should_log_loc(level::warn, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                      std::forward<T2>(a2), std::forward<T3>(a3),
                                      std::forward<T4>(a4), std::forward<T5>(a5),
                                      std::forward<T6>(a6));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7>
inline void warn(log_fmt8<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>,
                          std::type_identity_t<T4>, std::type_identity_t<T5>,
                          std::type_identity_t<T6>, std::type_identity_t<T7>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7) {
    if (!filter::should_log_loc(level::warn, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                      std::forward<T2>(a2), std::forward<T3>(a3),
                                      std::forward<T4>(a4), std::forward<T5>(a5),
                                      std::forward<T6>(a6), std::forward<T7>(a7));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7, typename T8>
inline void warn(log_fmt9<std::type_identity_t<T0>, std::type_identity_t<T1>,
                          std::type_identity_t<T2>, std::type_identity_t<T3>,
                          std::type_identity_t<T4>, std::type_identity_t<T5>,
                          std::type_identity_t<T6>, std::type_identity_t<T7>,
                          std::type_identity_t<T8>> lf,
                 T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7,
                 T8&& a8) {
    if (!filter::should_log_loc(level::warn, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->warn(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                      std::forward<T2>(a2), std::forward<T3>(a3),
                                      std::forward<T4>(a4), std::forward<T5>(a5),
                                      std::forward<T6>(a6), std::forward<T7>(a7),
                                      std::forward<T8>(a8));
}

template<typename T0>
inline void error(log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_loc(level::error, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot()) internal::logger_slot()->error(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void error(log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                  T0&& a0, T1&& a1) {
    if (!filter::should_log_loc(level::error, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void error(log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>> lf, T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_loc(level::error, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                       std::forward<T2>(a2));
}

template<typename T0, typename T1, typename T2, typename T3>
inline void error(log_fmt4<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3) {
    if (!filter::should_log_loc(level::error, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                       std::forward<T2>(a2), std::forward<T3>(a3));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4>
inline void error(log_fmt5<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4) {
    if (!filter::should_log_loc(level::error, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                       std::forward<T2>(a2), std::forward<T3>(a3),
                                       std::forward<T4>(a4));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
inline void error(log_fmt6<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5) {
    if (!filter::should_log_loc(level::error, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                       std::forward<T2>(a2), std::forward<T3>(a3),
                                       std::forward<T4>(a4), std::forward<T5>(a5));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6>
inline void error(log_fmt7<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>,
                           std::type_identity_t<T6>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6) {
    if (!filter::should_log_loc(level::error, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                       std::forward<T2>(a2), std::forward<T3>(a3),
                                       std::forward<T4>(a4), std::forward<T5>(a5),
                                       std::forward<T6>(a6));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7>
inline void error(log_fmt8<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>,
                           std::type_identity_t<T6>, std::type_identity_t<T7>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7) {
    if (!filter::should_log_loc(level::error, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                       std::forward<T2>(a2), std::forward<T3>(a3),
                                       std::forward<T4>(a4), std::forward<T5>(a5),
                                       std::forward<T6>(a6), std::forward<T7>(a7));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7, typename T8>
inline void error(log_fmt9<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>,
                           std::type_identity_t<T6>, std::type_identity_t<T7>,
                           std::type_identity_t<T8>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7,
                  T8&& a8) {
    if (!filter::should_log_loc(level::error, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->error(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                       std::forward<T2>(a2), std::forward<T3>(a3),
                                       std::forward<T4>(a4), std::forward<T5>(a5),
                                       std::forward<T6>(a6), std::forward<T7>(a7),
                                       std::forward<T8>(a8));
}

template<typename T0>
inline void debug(log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_loc(level::debug, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot()) internal::logger_slot()->debug(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void debug(log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                  T0&& a0, T1&& a1) {
    if (!filter::should_log_loc(level::debug, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void debug(log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>> lf, T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_loc(level::debug, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                       std::forward<T2>(a2));
}

template<typename T0, typename T1, typename T2, typename T3>
inline void debug(log_fmt4<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3) {
    if (!filter::should_log_loc(level::debug, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                       std::forward<T2>(a2), std::forward<T3>(a3));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4>
inline void debug(log_fmt5<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4) {
    if (!filter::should_log_loc(level::debug, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                       std::forward<T2>(a2), std::forward<T3>(a3),
                                       std::forward<T4>(a4));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
inline void debug(log_fmt6<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5) {
    if (!filter::should_log_loc(level::debug, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                       std::forward<T2>(a2), std::forward<T3>(a3),
                                       std::forward<T4>(a4), std::forward<T5>(a5));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6>
inline void debug(log_fmt7<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>,
                           std::type_identity_t<T6>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6) {
    if (!filter::should_log_loc(level::debug, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                       std::forward<T2>(a2), std::forward<T3>(a3),
                                       std::forward<T4>(a4), std::forward<T5>(a5),
                                       std::forward<T6>(a6));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7>
inline void debug(log_fmt8<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>,
                           std::type_identity_t<T6>, std::type_identity_t<T7>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7) {
    if (!filter::should_log_loc(level::debug, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                       std::forward<T2>(a2), std::forward<T3>(a3),
                                       std::forward<T4>(a4), std::forward<T5>(a5),
                                       std::forward<T6>(a6), std::forward<T7>(a7));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7, typename T8>
inline void debug(log_fmt9<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>,
                           std::type_identity_t<T6>, std::type_identity_t<T7>,
                           std::type_identity_t<T8>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7,
                  T8&& a8) {
    if (!filter::should_log_loc(level::debug, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->debug(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                       std::forward<T2>(a2), std::forward<T3>(a3),
                                       std::forward<T4>(a4), std::forward<T5>(a5),
                                       std::forward<T6>(a6), std::forward<T7>(a7),
                                       std::forward<T8>(a8));
}

template<typename T0>
inline void trace(log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_loc(level::trace, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot()) internal::logger_slot()->trace(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void trace(log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                  T0&& a0, T1&& a1) {
    if (!filter::should_log_loc(level::trace, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->trace(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void trace(log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>> lf, T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_loc(level::trace, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->trace(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                       std::forward<T2>(a2));
}

template<typename T0, typename T1, typename T2, typename T3>
inline void trace(log_fmt4<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3) {
    if (!filter::should_log_loc(level::trace, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->trace(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                       std::forward<T2>(a2), std::forward<T3>(a3));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4>
inline void trace(log_fmt5<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4) {
    if (!filter::should_log_loc(level::trace, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->trace(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                       std::forward<T2>(a2), std::forward<T3>(a3),
                                       std::forward<T4>(a4));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
inline void trace(log_fmt6<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5) {
    if (!filter::should_log_loc(level::trace, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->trace(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                       std::forward<T2>(a2), std::forward<T3>(a3),
                                       std::forward<T4>(a4), std::forward<T5>(a5));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6>
inline void trace(log_fmt7<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>,
                           std::type_identity_t<T6>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6) {
    if (!filter::should_log_loc(level::trace, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->trace(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                       std::forward<T2>(a2), std::forward<T3>(a3),
                                       std::forward<T4>(a4), std::forward<T5>(a5),
                                       std::forward<T6>(a6));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7>
inline void trace(log_fmt8<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>,
                           std::type_identity_t<T6>, std::type_identity_t<T7>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7) {
    if (!filter::should_log_loc(level::trace, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->trace(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                       std::forward<T2>(a2), std::forward<T3>(a3),
                                       std::forward<T4>(a4), std::forward<T5>(a5),
                                       std::forward<T6>(a6), std::forward<T7>(a7));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7, typename T8>
inline void trace(log_fmt9<std::type_identity_t<T0>, std::type_identity_t<T1>,
                           std::type_identity_t<T2>, std::type_identity_t<T3>,
                           std::type_identity_t<T4>, std::type_identity_t<T5>,
                           std::type_identity_t<T6>, std::type_identity_t<T7>,
                           std::type_identity_t<T8>> lf,
                  T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7,
                  T8&& a8) {
    if (!filter::should_log_loc(level::trace, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->trace(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                       std::forward<T2>(a2), std::forward<T3>(a3),
                                       std::forward<T4>(a4), std::forward<T5>(a5),
                                       std::forward<T6>(a6), std::forward<T7>(a7),
                                       std::forward<T8>(a8));
}

template<typename T0>
inline void critical(log_fmt1<std::type_identity_t<T0>> lf, T0&& a0) {
    if (!filter::should_log_loc(level::critical, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot()) internal::logger_slot()->critical(lf.fmt, std::forward<T0>(a0));
}

template<typename T0, typename T1>
inline void critical(log_fmt2<std::type_identity_t<T0>, std::type_identity_t<T1>> lf,
                     T0&& a0, T1&& a1) {
    if (!filter::should_log_loc(level::critical, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->critical(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1));
}

template<typename T0, typename T1, typename T2>
inline void critical(log_fmt3<std::type_identity_t<T0>, std::type_identity_t<T1>,
                              std::type_identity_t<T2>> lf, T0&& a0, T1&& a1, T2&& a2) {
    if (!filter::should_log_loc(level::critical, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->critical(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                          std::forward<T2>(a2));
}

template<typename T0, typename T1, typename T2, typename T3>
inline void critical(log_fmt4<std::type_identity_t<T0>, std::type_identity_t<T1>,
                              std::type_identity_t<T2>, std::type_identity_t<T3>> lf,
                     T0&& a0, T1&& a1, T2&& a2, T3&& a3) {
    if (!filter::should_log_loc(level::critical, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->critical(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                          std::forward<T2>(a2), std::forward<T3>(a3));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4>
inline void critical(log_fmt5<std::type_identity_t<T0>, std::type_identity_t<T1>,
                              std::type_identity_t<T2>, std::type_identity_t<T3>,
                              std::type_identity_t<T4>> lf,
                     T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4) {
    if (!filter::should_log_loc(level::critical, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->critical(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                          std::forward<T2>(a2), std::forward<T3>(a3),
                                          std::forward<T4>(a4));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5>
inline void critical(log_fmt6<std::type_identity_t<T0>, std::type_identity_t<T1>,
                              std::type_identity_t<T2>, std::type_identity_t<T3>,
                              std::type_identity_t<T4>, std::type_identity_t<T5>> lf,
                     T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5) {
    if (!filter::should_log_loc(level::critical, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->critical(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                          std::forward<T2>(a2), std::forward<T3>(a3),
                                          std::forward<T4>(a4), std::forward<T5>(a5));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6>
inline void critical(log_fmt7<std::type_identity_t<T0>, std::type_identity_t<T1>,
                              std::type_identity_t<T2>, std::type_identity_t<T3>,
                              std::type_identity_t<T4>, std::type_identity_t<T5>,
                              std::type_identity_t<T6>> lf,
                     T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6) {
    if (!filter::should_log_loc(level::critical, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->critical(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                          std::forward<T2>(a2), std::forward<T3>(a3),
                                          std::forward<T4>(a4), std::forward<T5>(a5),
                                          std::forward<T6>(a6));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7>
inline void critical(log_fmt8<std::type_identity_t<T0>, std::type_identity_t<T1>,
                              std::type_identity_t<T2>, std::type_identity_t<T3>,
                              std::type_identity_t<T4>, std::type_identity_t<T5>,
                              std::type_identity_t<T6>, std::type_identity_t<T7>> lf,
                     T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7) {
    if (!filter::should_log_loc(level::critical, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->critical(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                          std::forward<T2>(a2), std::forward<T3>(a3),
                                          std::forward<T4>(a4), std::forward<T5>(a5),
                                          std::forward<T6>(a6), std::forward<T7>(a7));
}

template<typename T0, typename T1, typename T2, typename T3, typename T4, typename T5,
         typename T6, typename T7, typename T8>
inline void critical(log_fmt9<std::type_identity_t<T0>, std::type_identity_t<T1>,
                              std::type_identity_t<T2>, std::type_identity_t<T3>,
                              std::type_identity_t<T4>, std::type_identity_t<T5>,
                              std::type_identity_t<T6>, std::type_identity_t<T7>,
                              std::type_identity_t<T8>> lf,
                     T0&& a0, T1&& a1, T2&& a2, T3&& a3, T4&& a4, T5&& a5, T6&& a6, T7&& a7,
                     T8&& a8) {
    if (!filter::should_log_loc(level::critical, lf.file, lf.func, lf.loc_hash)) return;
    if (internal::logger_slot())
        internal::logger_slot()->critical(lf.fmt, std::forward<T0>(a0), std::forward<T1>(a1),
                                          std::forward<T2>(a2), std::forward<T3>(a3),
                                          std::forward<T4>(a4), std::forward<T5>(a5),
                                          std::forward<T6>(a6), std::forward<T7>(a7),
                                          std::forward<T8>(a8));
}

inline void set_level(spdlog::level::level_enum level) {
    if (internal::logger_slot()) internal::logger_slot()->set_level(level);
}

inline void flush() {
    if (internal::logger_slot()) internal::logger_slot()->flush();
}

// ERROR CATEGORIES (ERR::CAT) - DRY error messages with auto-generated help
//
// Usage: log::error(log::ERR::CAT::HUB_NOT_FOUND, "SystemName", owner, "VALUE_ID");
// Output: [ERR] [SystemName] HUB_NOT_FOUND: owner=123, value_id='VALUE_ID'
//         Check: 1) IniSystem created hub entity in on_start()
//                2) Spawn created hub values for entity
//                3) Correct owner ID, 4) No typo in value_id

// ERR::CAT UND WRN::CAT KOMMEN AUS DEM GENERATOR
//
// Konstanten, Tabellen und Zaehler sind durch static_assert aneinander gebunden. Standen sie
// von Hand hier, war JEDE neue Kategorie ein Schrieb an einer Datei mit rund 2000 Lesern, und
// jeder Zwischenstand zwischen zwei Edits ein Fenster, in dem der ganze Baum unbaubar ist.
//
// Aendern will man eine Kategorie in core/ase-log/data/log_categories.json; den Rest schreibt
// core/ase-log/scripts/gen_log_cat.py. Ein Hilfetext oder ein Suffix kostet danach EINE neu
// uebersetzte Datei (log_err_cat_gen.cpp) statt des Baums. Eine NEUE Kategorie aendert den
// generierten Header und damit doch den Baum — das ist unvermeidbar, solange Aufrufer sie beim
// Namen nennen, und es ist der seltene Fall.
//
// DER INCLUDE STEHT OBEN BEI DEN UEBRIGEN, NICHT HIER. An dieser Stelle steht die Datei bereits
// INNERHALB von `namespace ase::log`, ein Include dort verschachtelt den fremden Namensraum zu
// `ase::log::ase::log::ERR`, und clangd meldet es ueber jede einbindende Uebersetzungseinheit als
// „No member named 'ERR' in namespace 'ase::log'". Ein Include gehoert vor die erste
// Namensraum-Oeffnung, ohne Ausnahme.


// Die Kategorie-Texte (Namen, short, hint, tail, Suffixe) und ihre Getter stehen in
// core/ase-log/src/log_err_cat_gen.cpp; die Deklarationen kommen aus log_err_cat_gen.hpp, der
// oben eingebunden wird. Jede kategorisierte Meldung entsteht hier als ZWEIZEILIGER Datensatz:
// die Kopfzeile endet mit dem SHORT (Essenz, Laengenobergrenze aus log_display.json, bricht
// nie um), und der TAIL haengt den hint als eigene, nur eingerueckte physische Zeile an. Das
// Paar ist EIN Log-Datensatz (ein Sink-Write): ein hint steht nie ohne seinen Kopf direkt
// darueber, deshalb traegt die hint-Zeile keine eigene Marke — sie waere die Wiederholung
// dessen, was der Kopf schon sagt. Jeder Kanal — Datei, tail, Konsole — traegt denselben
// Inhalt; eine Kategorie ohne hint (UNKNOWN) bleibt einzeilig.
//
// Der Suffix bleibt an der KATEGORIE und nicht an der Stelligkeit: HUB_GLOBAL_MISSING druckt
// " (GLOBAL)", CONFIG_MISSING nichts.

// Categorized error logging for per-entity values
inline void error(uint32_t cat, const char* system, uint32_t owner, const char* value_id) {
    if (internal::logger_slot()) {
        internal::logger_slot()->error("[{}] {}: owner={}, value_id='{}'. {}{}",
            system, detail::get_cat_name(cat), owner, value_id, detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

// Categorized error logging for values WITHOUT an owner.
//
// Der Zusatz hinter der value_id kommt aus detail::get_cat_suffix() und damit aus der
// KATEGORIE, nicht aus dieser Ueberladung. HUB_GLOBAL_MISSING druckt „ (GLOBAL)";
// CONFIG_MISSING druckt keinen Zusatz, weil eine Umgebungsvariable keine GLOBAL-Zeile des Hubs
// ist.
inline void error(uint32_t cat, const char* system, const char* value_id) {
    if (internal::logger_slot()) {
        internal::logger_slot()->error("[{}] {}: value_id='{}'{}. {}{}",
            system, detail::get_cat_name(cat), value_id, detail::get_cat_suffix(cat),
            detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

// Categorized error logging for component/entity issues (no value_id)
inline void error(uint32_t cat, const char* system, uint32_t entity) {
    if (internal::logger_slot()) {
        internal::logger_slot()->error("[{}] {}: entity={}. {}{}",
            system, detail::get_cat_name(cat), entity, detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

// WARNING CATEGORIES (WRN::CAT) - DRY warning messages with auto-generated help
//
// Usage: log::warn(log::WRN::CAT::VALUE_OUT_OF_RANGE, "SystemName", owner, "VALUE_ID", value, min, max);
// Output: [WRN] [SystemName] VALUE_OUT_OF_RANGE: owner=123, value_id='VALUE_ID', value=1.5, range=[0.0,1.0]. value left its valid range
//             Check the value against the range it left. To clamp, reject or keep it is the caller's decision

// WRN::CAT steht ebenfalls in log_err_cat_gen.hpp, und beide Ebenen tragen denselben Bestand:
// jede Kategorie existiert als Fehler UND als Warnung, ausser wo die Abgrenzung sie
// ausschliesst (der Grund steht dann unter `_omitted` in log_categories.json). Ohne diese
// Entsprechung der Namen ist die Regel „die Ebene folgt der ZEILE, nie der Kategorie" unerfuellbar: eine
// Stelle, deren Kategorie es nur auf der anderen Ebene gibt, hat drei Auswege, und alle drei
// sind falsch — Ebene wechseln, Kategorie verbiegen, oder als freier String stehen bleiben.
//
// EINE KREUZUNG IST NICHT MOEGLICH, und zwar strukturell statt durch Nummernabstand. Der Wert
// einer Kategorie ist der FNV-1a-Hash von „<NAMENSRAUM>_<NAME>", also „WRN_HOST_OP_FAILED"
// gegen „ERR_HOST_OP_FAILED" — zwei verschiedene Zahlen.
//
// Und der Namensraum MUSS im gehashten String stehen: er trennt die Konstanten dort, wo sie
// GESCHRIEBEN werden, aber nichts trennt sie dort, wo der Wert NACHGESCHLAGEN wird. Waere nur
// der nackte Name gehasst, haetten ERR::CAT::HOST_OP_FAILED und WRN::CAT::HOST_OP_FAILED
// denselben Wert, und die WRN-Konstante an log::error druckte den ERR-Hilfetext.

// Die WRN-Nachschlagefunktionen stehen ebenfalls in log_err_cat_gen.cpp. Ein Hash, den die
// SSOT nicht kennt, faellt dort NICHT auf einen Eintrag zurueck, sondern bekommt eine LAUTE
// Marke: ein plausibler falscher Hilfetext ist teurer als ein sichtbar unbekannter, weil der
// Leser eine gueltige Kategorie sieht und nicht nachprueft.
//
// Und die Hilfetexte sagen dort NICHTS ueber den Kontrollfluss zu. Ein Satz wie „Fix: Value
// will be clamped to valid range" behauptet, was NACH der Meldung geschieht — das kann eine
// Kategorie nicht wissen: sie beschreibt den ZUSTAND eines Wertes, nicht die Reaktion des
// Aufrufers. Eine Bereichspruefung, die NICHT klemmt, fiele dadurch faelschlich durch.

// Categorized warning for out-of-range values
inline void warn(uint32_t cat, const char* system, uint32_t owner, const char* value_id, float value, float min, float max) {
    if (internal::logger_slot()) {
        internal::logger_slot()->warn("[{}] {}: owner={}, value_id='{}', value={}, range=[{},{}]. {}{}",
            system, detail::get_wrn_cat_name(cat), owner, value_id, value, min, max, detail::get_wrn_cat_short(cat), detail::get_wrn_cat_tail(cat));
    }
}

// Bereichsform OHNE Besitzer — fuer Stellen, an denen es keine Entity gibt: ein Rahmenfeld
// von der Leitung, eine Puffergrenze, ein Wert vor jeder Entity. Die owner-behaftete Form
// darueber verlangt einen Besitzer, und einen zu ERFINDEN ist teurer als der freie String, den
// diese Form ersetzt: `0` ist eine gueltige entt-Id, `types::InvalidEntityId` sagt „unbekannt"
// statt „gibt es nicht".
//
// DER SUFFIX IST ASYMMETRISCH, UND DAS IST ABSICHT — nicht zur Symmetrie „korrigieren": die
// ERR-Haelfte druckt `detail::get_cat_suffix(cat)`, die WRN-Haelfte nicht, und ein
// `get_wrn_cat_suffix` gibt es nicht. Der Zusatz ist das „ (GLOBAL)" von HUB_GLOBAL_MISSING,
// und GLOBAL heisst „kein Besitzer" — er gehoert deshalb genau dorthin, wo kein owner-Feld steht.
//
// EIN SOLLWERT wird hier als `min == max` gemeldet, nicht mit einer eigenen Form:
// `(value_id, ist, soll)` waere signaturgleich mit der Aggregatform und damit eine Redefinition
// — dieselbe Wand, die der Absatz am Ende dieses Namensraums fuer „zwei unabhaengige Zahlen ohne
// Besitzer" mit einer g++-Messung belegt. `range=[64,64]` ist wahr und lesbar: ein Bereich mit
// einem Element.
//
// EINE EINSEITIGE Schranke gehoert NICHT hierher, weder mit `max = 0` noch mit `FLT_MAX`; sie
// nimmt die Form ohne Grenzen. Drei Stellen haben das unabhaengig entschieden und begruendet:
//   replica_cell_rcv_sys.cpp  „eine erfundene Obergrenze waere eine Falschaussage in der
//                              strukturierten Form"
//   rsn_trg_reg_sys.cpp       „die Obergrenze gibt es nicht (jede Nummer >= 1 ist gueltig) —
//                              deshalb die Wertform und nicht die Bereichsform"
//   serial_jsn_sys.cpp        „die Bereichsgrenzen sind an dieser Stelle nicht als Zahl
//                              greifbar, deshalb die Wertform ohne min/max statt einer
//                              erfundenen Grenze"
// Diese drei Saetze stehen woertlich hier, damit der Leser die AUSSAGE pruefen kann, ohne sie
// glauben zu muessen. Ein Verweis, den man aufschlagen MUSS, wird nicht aufgeschlagen.
inline void error(uint32_t cat, const char* system, const char* value_id,
                  float value, float min, float max) {
    if (internal::logger_slot()) {
        internal::logger_slot()->error("[{}] {}: value_id='{}', value={}, range=[{},{}]{}. {}{}",
            system, detail::get_cat_name(cat), value_id, value, min, max,
            detail::get_cat_suffix(cat), detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

inline void warn(uint32_t cat, const char* system, const char* value_id,
                 float value, float min, float max) {
    if (internal::logger_slot()) {
        internal::logger_slot()->warn("[{}] {}: value_id='{}', value={}, range=[{},{}]. {}{}",
            system, detail::get_wrn_cat_name(cat), value_id, value, min, max,
            detail::get_wrn_cat_short(cat), detail::get_wrn_cat_tail(cat));
    }
}

// ERR-Haelfte der owner-behafteten Bereichsform. Eine Stelle, die einen Verbrauch gegen einen
// Deckel meldet, behaelt mit ihr die Untergrenze („X in [0,Y]" statt „X von Y") UND den Besitzer,
// den die Aggregatform nicht fuehrt. Der Unterschied zwischen ihr und ihrem Nachbarzweig ist die
// Semantik, nie die Verfuegbarkeit einer Form.
inline void error(uint32_t cat, const char* system, uint32_t owner, const char* value_id,
                  float value, float min, float max) {
    if (internal::logger_slot()) {
        internal::logger_slot()->error("[{}] {}: owner={}, value_id='{}', value={}, range=[{},{}]. {}{}",
            system, detail::get_cat_name(cat), owner, value_id, value, min, max,
            detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

// Categorized warning for negative values
inline void warn(uint32_t cat, const char* system, uint32_t owner, const char* value_id, float value) {
    if (internal::logger_slot()) {
        internal::logger_slot()->warn("[{}] {}: owner={}, value_id='{}', value={}. {}{}",
            system, detail::get_wrn_cat_name(cat), owner, value_id, value, detail::get_wrn_cat_short(cat), detail::get_wrn_cat_tail(cat));
    }
}

// Categorized warning WITHOUT a numeric value.
//
// Die beiden Ueberladungen darueber verlangen einen `float value` - fuer eine Warnung, deren
// Gegenstand KEINE Zahl ist, ist dies die kategorisierte Form; ohne sie bliebe eine solche Zeile
// ein freier String. Gemessen an ase-pl-webserver: ein fehlender HTTP-Header und ein
// fehlgeschlagener HMAC haben nichts, was man in `value` einsetzen koennte, ohne etwas zu
// erfinden.
//
// KEIN `owner`, und das ist Absicht: der Ausloeser ist ein fremder Aufrufer, kein Hub-Owner.
// Ein owner-Feld an dieser Stelle waere eine Einladung, eine Entity-ID zu erfinden.
inline void warn(uint32_t cat, const char* system, const char* value_id) {
    if (internal::logger_slot()) {
        internal::logger_slot()->warn("[{}] {}: value_id='{}'. {}{}",
            system, detail::get_wrn_cat_name(cat), value_id, detail::get_wrn_cat_short(cat), detail::get_wrn_cat_tail(cat));
    }
}

// Categorized warning for an AGGREGATE over a set - a count against its population.
//
// Die beiden wert-tragenden Ueberladungen oben verlangen `uint32_t owner`. Eine Meldung, die
// ueber eine MENGE zaehlt, hat keinen: sie gehoert keiner Entity, sondern dem Durchlauf. Jede
// kategorisierte Form mit Besitzer haette einen Owner erfunden - und eine erfundene Entity-ID im
// Log ist teurer als der freie String, den sie ersetzt.
//
// ZWEI Zahlen und nicht eine, weil die AUSSAGE das Verhaeltnis ist: „3 von 512" und „3 von 4"
// sind derselbe Zaehlstand und zwei verschiedene Lagen. Bewusst NICHT die 7-Argument-Form mit
// `min`/`max` missbraucht: dort ist der zweite Wert eine BEREICHSGRENZE, hier eine
// GRUNDGESAMTHEIT. Wer die Population ins max-Feld schreibt, behauptet eine Grenze, wo eine
// Anzahl steht - eine Falschaussage in der strukturierten Form, und die wird von Werkzeugen
// gelesen und geglaubt, waehrend ein freier String nur von Menschen gelesen wird.
inline void warn(uint32_t cat, const char* system, const char* value_id, float count, float population) {
    if (internal::logger_slot()) {
        internal::logger_slot()->warn("[{}] {}: value_id='{}', count={} of {}. {}{}",
            system, detail::get_wrn_cat_name(cat), value_id, count, population,
            detail::get_wrn_cat_short(cat), detail::get_wrn_cat_tail(cat));
    }
}

// Dieselbe Aggregatform auf FEHLEREBENE. Die Begruendung oben gilt unveraendert - hier steht
// nur, warum es sie zweimal geben MUSS.
//
// Die passende Kategorie fuer den haeufigsten Fall (eine Bahn ist voll, N von M verworfen) ist
// eine ERR-Kategorie. Stuende die Form nur unter WRN, laegen die zwei Haelften an
// entgegengesetzten Enden, und eine Stelle dazwischen haette drei Wege, alle drei verboten - die
// Ebene wechseln (Code an den Detektor anpassen), die Kategorie verbiegen (Falschaussage) oder als
// freier String stehen bleiben. Belegt an terrain_dlt_pub_sys.cpp und an
// capacity_orch_node_stat_sys.cpp (ase-pl-capacity-orch), wo der Dateikommentar die zwei Zahlen
// ausdruecklich als die Aussage benennt ("Names the REAL event ... Never silent").
//
// Der Unterschied zur WRN-Haelfte ist NICHT die Schwere, sondern was danach passiert: hier
// endet der Vorgang, dort laeuft er weiter. Wer das verwechselt, nimmt seine Zeile aus dem
// Strom, in dem sie gesucht wird.
inline void error(uint32_t cat, const char* system, const char* value_id, float count, float population) {
    if (internal::logger_slot()) {
        internal::logger_slot()->error("[{}] {}: value_id='{}', count={} of {}{}. {}{}",
            system, detail::get_cat_name(cat), value_id, count, population,
            detail::get_cat_suffix(cat), detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

// ERGAENZENDE FORMEN
//
// Jede traegt ihre ABGRENZUNG, nicht nur ihren Zweck: der Text hier ist das, was ein Aufrufer
// vor der Formwahl liest, und eine Form ohne Abgrenzung wird zum Sammelbecken.

// EIN Wert, KEIN Besitzer. Die haeufigste Luecke im Baum.
//
// Ohne diese Form haette jede kategorisierte Fehlerzeile, die eine Zahl nennt, nur zwei
// Auswege: die Zahl verlieren oder auf `warn` heruntergehen — und der zweite nimmt die Zeile
// aus dem Fehlerstrom, in dem sie gesucht wird.
//
// Gegen die 5-Argument-Form mit `owner` abgegrenzt: die hier ist fuer Stellen OHNE Entity —
// ein Dekodierhelfer, der vor jeder Entity laeuft, eine Puffergrenze, ein HTTP-Aufruf. Einen
// Owner zu erfinden waere teurer als der freie String, den die Form ersetzt: `0` ist eine
// gueltige entt-Id, die Zeile zeigte also auf eine fremde Entity.
inline void error(uint32_t cat, const char* system, const char* value_id, float value) {
    if (internal::logger_slot()) {
        internal::logger_slot()->error("[{}] {}: value_id='{}', value={}{}. {}{}",
            system, detail::get_cat_name(cat), value_id, value,
            detail::get_cat_suffix(cat), detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

inline void warn(uint32_t cat, const char* system, const char* value_id, float value) {
    if (internal::logger_slot()) {
        internal::logger_slot()->warn("[{}] {}: value_id='{}', value={}. {}{}",
            system, detail::get_wrn_cat_name(cat), value_id, value,
            detail::get_wrn_cat_short(cat), detail::get_wrn_cat_tail(cat));
    }
}

// EIN Wert NEBEN dem Besitzer, auf Fehlerebene — das Gegenstueck zur WRN-Form mit owner.
inline void error(uint32_t cat, const char* system, uint32_t owner, const char* value_id,
                  float value) {
    if (internal::logger_slot()) {
        internal::logger_slot()->error("[{}] {}: owner={}, value_id='{}', value={}. {}{}",
            system, detail::get_cat_name(cat), owner, value_id, value,
            detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

// Besitzer OHNE Wert auf WRN — die fehlende Entsprechung zur gleichnamigen ERR-Form.
// Fuer Warnungen, bei denen die Entity die Aussage traegt und es gar keinen Wert gibt:
// „Ausgangsschlange dieses Besitzers ist voll", „Zeile dieses Besitzers verworfen".
inline void warn(uint32_t cat, const char* system, uint32_t owner, const char* value_id) {
    if (internal::logger_slot()) {
        internal::logger_slot()->warn("[{}] {}: owner={}, value_id='{}'. {}{}",
            system, detail::get_wrn_cat_name(cat), owner, value_id,
            detail::get_wrn_cat_short(cat), detail::get_wrn_cat_tail(cat));
    }
}

// value_id + FREIER DETAILTEXT.
//
// `value_id` ist der FILTERSCHLUESSEL und bleibt je Aufrufstelle konstant; `detail` nimmt, was
// dort nicht hineinpasst — ein `dlerror()`, ein `strerror(errno)`, ein HTTP-Status, ein
// `e.what()`, auch drei Bezeichner in einem Satz. Die Zeile ist danach KATEGORISIERT, und das
// ist der ganze Unterschied zum freien String.
//
// Die Grenze: `detail` ist KEIN zweiter Filterschluessel. Was gefiltert werden soll, gehoert in
// `value_id`; ein wechselnder Wert dort macht jede Zeile zu einem eigenen Schluessel und
// zerstoert genau die Zaehlbarkeit, für die es die Kategorie gibt.
inline void error(uint32_t cat, const char* system, const char* value_id, const char* detail) {
    if (internal::logger_slot()) {
        internal::logger_slot()->error("[{}] {}: value_id='{}'{}, detail='{}'. {}{}",
            system, detail::get_cat_name(cat), value_id, detail::get_cat_suffix(cat),
            detail, detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

inline void warn(uint32_t cat, const char* system, const char* value_id, const char* detail) {
    if (internal::logger_slot()) {
        internal::logger_slot()->warn("[{}] {}: value_id='{}', detail='{}'. {}{}",
            system, detail::get_wrn_cat_name(cat), value_id, detail,
            detail::get_wrn_cat_short(cat), detail::get_wrn_cat_tail(cat));
    }
}

// Dieselbe Sache MIT Besitzer: wo die Zeile einer Entity gehoert UND einen freien Grund traegt.
// Der haeufige Fall ist eine abgewiesene Anfrage: die Entity sagt WELCHE, `detail` sagt WARUM.
inline void error(uint32_t cat, const char* system, uint32_t owner, const char* value_id,
                  const char* detail) {
    if (internal::logger_slot()) {
        internal::logger_slot()->error("[{}] {}: owner={}, value_id='{}', detail='{}'. {}{}",
            system, detail::get_cat_name(cat), owner, value_id, detail,
            detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

inline void warn(uint32_t cat, const char* system, uint32_t owner, const char* value_id,
                 const char* detail) {
    if (internal::logger_slot()) {
        internal::logger_slot()->warn("[{}] {}: owner={}, value_id='{}', detail='{}'. {}{}",
            system, detail::get_wrn_cat_name(cat), owner, value_id, detail,
            detail::get_wrn_cat_short(cat), detail::get_wrn_cat_tail(cat));
    }
}

// ZWEI identifizierende Zahlen.
//
// Beide benennen etwas, keine ist aus der anderen ableitbar: welche Anfrage verlorenging UND
// gegen welche Sammlung sie lief. Eine davon in `owner` zu stecken und die andere fallenzulassen
// waere kein Formatverlust, sondern ein INFORMATIONSverlust — danach ist die Zeile nicht mehr
// zuzuordnen.
//
// Die Grenze: eine Zahl, die je Aufrufstelle KONSTANT ist (ein Rahmentyp, eine feste Obergrenze),
// traegt in einer Logzeile keine Information und gehoert nicht hierher. Zwei IDENTIFIZIERENDE
// Zahlen sind etwas anderes als zwei Zahlen.
//
// Traegt zugleich eine 64-Bit-Kennung als hi/lo: beide Haelften in DERSELBEN Zeile, `value_id`
// mit dem Suffix `_hi`/`_lo` — sonst uebersetzt eine implizite Verengung auf uint32 still und
// das Log druckt einen abgeschnittenen Schluessel.
inline void error(uint32_t cat, const char* system, uint32_t owner, uint32_t subject,
                  const char* value_id) {
    if (internal::logger_slot()) {
        internal::logger_slot()->error("[{}] {}: owner={}, subject={}, value_id='{}'. {}{}",
            system, detail::get_cat_name(cat), owner, subject, value_id,
            detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

inline void warn(uint32_t cat, const char* system, uint32_t owner, uint32_t subject,
                 const char* value_id) {
    if (internal::logger_slot()) {
        internal::logger_slot()->warn("[{}] {}: owner={}, subject={}, value_id='{}'. {}{}",
            system, detail::get_wrn_cat_name(cat), owner, subject, value_id,
            detail::get_wrn_cat_short(cat), detail::get_wrn_cat_tail(cat));
    }
}

// KEINE Form `warn(cat, system, value_id, float a, float b)` — sie waere signaturgleich mit der
// Aggregatform oben und damit eine Redefinition, nicht eine Ueberladung (gemessen: g++ meldet
// „Redefinition"). Wer zwei unabhaengige Zahlen ohne Besitzer melden will, teilt die Zeile oder
// nimmt die Aggregatform NUR dann, wenn die zweite Zahl wirklich die Grundgesamtheit der ersten
// ist. Ein eigener Funktionsname waere der Ausweg; er ist bewusst nicht gebaut, solange die
// Entscheidung darueber nicht gefallen ist.

// Zwei identifizierende Zahlen UND ein Mengenpaar in EINER Zeile. Der Anlass ist eine Stelle,
// die den Fortschritt eines Vorgangs meldete und ihn nicht an seine Kennung binden konnte: die
// Aggregatform hat keinen Besitzer-Slot, „seen von total" stand ohne zu sagen, ZU WELCHEM
// Vorgang. Nur die zeitliche Nachbarschaft im Strom verband die zwei Zeilen, und die traegt nicht.
//
// EINE ZAHL, DIE IDENTIFIZIERT, IST ETWAS ANDERES ALS EINE, DIE MISST. Mengen duerfen `float`
// sein — sie sind gedeckelt, und ihre Rundung ist eine gerundete Menge. Kennungen nie: ein
// uint32-Hash ist ueber den ganzen Bereich gleichverteilt und verengt ueber 2^24 STILL, das Log
// druckt dann einen falschen, PLAUSIBLEN Wert. Deshalb stehen `owner` und `subject` als
// `uint32_t` und nicht als weitere Floats.
//
// EINE FORM FUER „zwei Kennungen UND einen dritten IDENTIFIZIERENDEN Wert" ist BEWUSST NICHT
// gebaut: ihre einzige Bedarfsstelle wurde verlustfrei anders geloest (die dritte Kennung in den
// owner-Slot, die 64-Bit-Kennung als Text im detail-Slot). Was bliebe, waere ein GEWINN —
// numerisch filterbar statt greppbar — und ein Gewinn ist keine Bedarfsstelle. Wer sie vermisst,
// nennt die Stelle, die sie braucht.
inline void error(uint32_t cat, const char* system, uint32_t owner, uint32_t subject,
                  const char* value_id, float count, float population) {
    if (internal::logger_slot()) {
        internal::logger_slot()->error("[{}] {}: owner={}, subject={}, value_id='{}', count={} of {}. {}{}",
            system, detail::get_cat_name(cat), owner, subject, value_id, count, population,
            detail::get_cat_short(cat), detail::get_cat_tail(cat));
    }
}

inline void warn(uint32_t cat, const char* system, uint32_t owner, uint32_t subject,
                 const char* value_id, float count, float population) {
    if (internal::logger_slot()) {
        internal::logger_slot()->warn("[{}] {}: owner={}, subject={}, value_id='{}', count={} of {}. {}{}",
            system, detail::get_wrn_cat_name(cat), owner, subject, value_id, count, population,
            detail::get_wrn_cat_short(cat), detail::get_wrn_cat_tail(cat));
    }
}

}  // namespace ase::log

#if defined(__GNUC__) && __GNUC__ >= 14
#pragma GCC diagnostic pop
#endif
