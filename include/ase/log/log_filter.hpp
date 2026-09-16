#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        log_filter.hpp
 * @design      DSGN_021
 * @brief       Runtime log filtering by category and level
 * @description Fast log filtering that extracts categories from filenames.
 *              Categories are matched against
 *              taxonomy abbreviations (BCT, PST, NET, etc.) found in the
 *              source filename. CLI: --log "+INF +WRN +ERR -BCT -PST"
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    error/logging
 * @created     2026-02-02
 * @modified    2026-02-02
 * @version     1.0.0
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

#include <ase/log/log_cat_gen.hpp>

#include <source_location>

namespace ase::log {

// ============================================================================
// Level Constants (aliases for cleaner API)
// ============================================================================

namespace level {
constexpr uint8_t trace    = filter::LVL_TRC;  ///< Trace level
constexpr uint8_t debug    = filter::LVL_DBG;  ///< Debug level
constexpr uint8_t info     = filter::LVL_INF;  ///< Info level
constexpr uint8_t warn     = filter::LVL_WRN;  ///< Warning level
constexpr uint8_t error    = filter::LVL_ERR;  ///< Error level
constexpr uint8_t critical = filter::LVL_CRT;  ///< Critical level
constexpr uint8_t all      = filter::LVL_ALL;  ///< All levels enabled
}  // namespace level

// ============================================================================
// Category Extraction
// ============================================================================

/**
 * @brief Extract categories from the calling file's path.
 * @param loc Source location (defaults to caller's location)
 * @return CategoryMask with bits set for matching taxonomy abbreviations
 */
[[nodiscard]] inline filter::CategoryMask get_file_categories(
    const std::source_location& loc = std::source_location::current()) noexcept {
    return filter::file_to_categories(loc.file_name());
}

// ============================================================================
// Runtime Filter Check (3-axis: level × category × lifecycle phase)
// ============================================================================

/**
 * @brief Check if a log message should be emitted (level + file + function categories).
 * @param lvl Log level (use level::trace, level::info, etc.)
 * @param loc Source location (defaults to caller's location)
 * @return true if message should be logged, false if filtered out
 *
 * Extracts category tokens from BOTH the source filename AND function name.
 * This enables lifecycle-phase filtering: +TICK, -START, +STOP, etc.
 */
/**
 * @brief Location key for the filter cache, computed at COMPILE time.
 *
 * WHY THIS EXISTS AND WHAT IT REPLACES: until 2026-08-22 the filter cache keyed on the
 * ADDRESSES of the two strings - `reinterpret_cast<uintptr_t>(file) ^ (cast(func) << 1)`
 * inside should_log_loc(). That carried two defects at once. The cast itself is forbidden
 * (REINTERPRET_CAST_FORBIDDEN, WRFL_ASE_FLYWEIGHT.md), and the key was not stable: identical
 * __FILE__ literals in different translation units may live at different addresses, so one
 * source line could occupy several cache slots while two different lines could collide.
 * Nobody measured that, because a filter cache that answers slightly wrong looks exactly like
 * a filter cache that answers.
 *
 * Hashing the CONTENT fixes both and costs nothing at run time. Every caller passes strings
 * that are call-site constants - the log_fmtN constructors take them from __builtin_FILE()/
 * __builtin_FUNCTION() defaults, and std::source_location::current() is fixed at the call site
 * too - so this folds to an immediate. THAT is why the key is built here and not inside
 * should_log_loc(): the same FNV computed per call would multiply the fast path the cache
 * exists to avoid (~12 cycles, see the note above the fmt-style overloads in log.hpp).
 *
 * THE SEPARATOR IS NOT COSMETIC: without it ("ab","c") and ("a","bc") hash identically. The
 * address form could not collide that way, so leaving it out would trade one silent defect
 * for another.
 *
 * IT LIVES IN THIS HEADER, not in log.hpp, because log.hpp carries an
 * `#include <ase/log/log_filter.hpp>` and
 * both sides need it: the log_fmtN wrappers there, should_log_loc/should_log_client below.
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

[[nodiscard]] inline bool should_log_loc(
    uint8_t lvl,
    const std::source_location& loc = std::source_location::current()) noexcept {
    return filter::should_log_loc(lvl, loc.file_name(), loc.function_name(),
                                  loc_key(loc.file_name(), loc.function_name()));
}

// ============================================================================
// Client Filter Check (4-axis: level × category × lifecycle × client)
// ============================================================================

/**
 * @brief Check if a client log message should be emitted.
 * @param lvl Log level (use level::trace, level::info, etc.)
 * @param client_bit Client bitmask (1ULL << client_id, 0 = server log, always passes)
 * @param loc Source location (defaults to caller's location)
 * @return true if message should be logged, false if filtered out
 */
[[nodiscard]] inline bool should_log_client(
    uint8_t lvl, uint64_t client_bit,
    const std::source_location& loc = std::source_location::current()) noexcept {
    return filter::should_log_client_loc(lvl, loc.file_name(), loc.function_name(),
                                         loc_key(loc.file_name(), loc.function_name()), client_bit);
}

// ============================================================================
// CLI Argument Parsing
// ============================================================================

/**
 * @brief Parse log filter string from CLI arguments.
 * @param filter_str Space-separated filter tokens
 *
 * Format: "+LEVEL +CATEGORY -CATEGORY +CLT:ID -CLT:ID"
 *   +TRC, +DBG, +INF, +WRN, +ERR, +CRT = Enable log level
 *   +XXX = Whitelist category (ONLY show files with this category)
 *   -XXX = Block category (hide files containing this category)
 *   +CLT:01 = Show only client 01 logs
 *   -CLT = Block all client logs
 *
 * Example: "+DBG +TIM"
 *   - Show only DEBUG level, only files containing TIM category
 *   - Result: only TimeLogOutpSystem etc. visible
 *
 * Example: "+DBG +WRN +ERR -BCT -PST"
 *   - Show DEBUG, WARN, ERROR; block files with BCT or PST
 */
inline void parse_cli_filter(const char* filter_str) noexcept {
    filter::parse_log_filter(filter_str);
}

/**
 * @brief Set log level mask directly.
 * @param mask Bitmask of enabled levels (e.g., level::info | level::warn)
 */
inline void set_level_mask(uint8_t mask) noexcept {
    filter::level_mask().store(mask, std::memory_order_relaxed);
}

/**
 * @brief Get current log level mask.
 * @return Bitmask of enabled levels
 */
[[nodiscard]] inline uint8_t get_level_mask() noexcept {
    return filter::level_mask().load(std::memory_order_relaxed);
}

/**
 * @brief Block a category by bit position.
 * @param bit_pos Category bit position (use filter::BIT_XXX constants)
 */
inline void block_category(int bit_pos) noexcept {
    filter::blocked_categories().set(bit_pos);
}

/**
 * @brief Block a specific client by ID (0-63).
 * @param client_id Client ID (0-63)
 */
inline void block_client(int client_id) noexcept {
    if (client_id >= 0 && client_id < 64) {
        uint64_t old_val = filter::blocked_clients().load(std::memory_order_relaxed);
        filter::blocked_clients().store(old_val | (1ULL << client_id), std::memory_order_relaxed);
    }
}

/**
 * @brief Add a client to the whitelist (0-63).
 * @param client_id Client ID (0-63)
 */
inline void whitelist_client(int client_id) noexcept {
    if (client_id >= 0 && client_id < 64) {
        uint64_t old_val = filter::client_mask().load(std::memory_order_relaxed);
        filter::client_mask().store(old_val | (1ULL << client_id), std::memory_order_relaxed);
    }
}

/**
 * @brief Reset all filters to defaults (all levels enabled, no categories/clients blocked).
 */
inline void reset_filters() noexcept {
    filter::level_mask().store(filter::LVL_ALL, std::memory_order_relaxed);
    for (int i = 0; i < filter::CHUNK_COUNT; ++i) {
        filter::blocked_categories().chunks[i] = 0;
        filter::whitelisted_categories().chunks[i] = 0;
    }
    filter::has_whitelist().store(false, std::memory_order_relaxed);
    filter::client_mask().store(0, std::memory_order_relaxed);
    filter::blocked_clients().store(0, std::memory_order_relaxed);
    filter::invalidate_loc_cache();
}

}  // namespace ase::log
