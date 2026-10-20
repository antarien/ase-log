#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        log_filter.hpp
 * @design      DSGN_021
 * @brief       Runtime log filter control: the CLI filter, the level mask, blocked categories and clients
 * @description The control side of the log filter. Categories are matched against taxonomy
 *              abbreviations (BCT, PST, NET, etc.) found in the source filename and function
 *              name. CLI: --log "+INF +WRN +ERR -BCT -PST". The check every log call makes and
 *              the level bits stand in log.hpp; a unit that sets or reads the filter includes
 *              this header itself.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    error/logging
 * @usecase     usecase:100362 - write a log line
 * @created     2026-02-02
 * @modified    2026-10-19
 * @version     1.1.0
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

#include <ase/log/log.hpp>
#include <ase/log/log_cat_gen.hpp>

#include <cstdint>
#include <source_location>

namespace ase::log {

// CATEGORY EXTRACTION

/**
 * @brief Extract categories from the calling file's path.
 * @param loc Source location (defaults to caller's location)
 * @return CategoryMask with bits set for matching taxonomy abbreviations
 */
[[nodiscard]] inline filter::CategoryMask get_file_categories(
    const std::source_location& loc = std::source_location::current()) noexcept {
    return filter::file_to_categories(loc.file_name());
}

// CLI ARGUMENT PARSING

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
    filter::level_mask().store(level::all, std::memory_order_relaxed);
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
