#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        log_display.hpp
 * @brief       Log record display walks: word abbreviation and semantic colouring
 * @description The two record transformations of the log PRODUCTION path, applied
 *              by the format layer (log_sys.cpp, the semantic message flag) so
 *              every channel - tier file, tail, HTTP ring, TUI callback - carries
 *              the identical bytes. A console renders what the record says and
 *              adds NOTHING: no shortening, no colouring, only geometry (width
 *              wrapping). Exactly that was measured live as a SSOT break: cli
 *              showed abbreviated words where dist showed full ones, values were
 *              coloured in one channel and plain in the other.
 *
 *              shorten_display_line swaps known words for their taxonomy
 *              abbreviations and drops filler words. The word table is GENERATED
 *              (log_display_gen.cpp) from the display policy
 *              (core/ase-log/data/log_display.json) plus the vocabulary of every
 *              short and hint text in log_categories.json, resolved against the
 *              taxonomy SSOT; lookups are O(1) hash probes, hash plus length is
 *              the identity, and in-set collisions stop the generator run. The
 *              static short and hint texts leave the GENERATOR already shortened
 *              (fixed point of this walk), so at runtime this walk only touches
 *              the live values of the message.
 *
 *              colorize_log_line colours the message body by SHAPE, not by word
 *              list: quoted values, numbers, URLs, paths, bracket tags, outcome
 *              words (O(1) probe of the generated set), structural punctuation.
 *              Every colour value comes from the SHA palette via the generated
 *              getters below - this unit holds no colour of its own.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    ecs/module
 * @created     2026-08-24
 * @modified    2026-08-24
 * @version     1.3.0
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

#include <cstdint>
#include <string>

namespace ase::log {

namespace detail {

/**
 * @brief O(1) probe of the generated display word table (log_display_gen.cpp).
 * @param hash FNV-1a over the lowercased word bytes.
 * @param len  Length of the word in bytes; hash plus length is the identity.
 * @return The abbreviation, an EMPTY string for a filler word, nullptr for an
 *         unknown word.
 */
[[nodiscard]] const char* display_word(uint32_t hash, uint32_t len);

/**
 * @brief The grey of the timestamp column, as SGR parameters.
 * @return The parameter string (for example "38;2;108;108;108") to place between "\x1b["
 *         and "m". The display policy names a SHA palette colour, the value comes from
 *         colors.ts; log_sys.cpp composes its format patterns from it at runtime, so a
 *         colour change is a colors.ts edit, the SHA generator run and one generated-file
 *         recompile - never a tree rebuild.
 */
[[nodiscard]] const char* display_timestamp_sgr();

/**
 * @brief The colour of one level tag (TRC..CRT), as SGR parameters from the display policy.
 * @param level The spdlog level index, trace being zero.
 * @return The parameter string, or an EMPTY string for an index outside the set - the
 *         caller then prints uncoloured, never wrongly coloured.
 */
[[nodiscard]] const char* display_level_sgr(uint32_t level);

/**
 * @brief The semantic role colours of the record text colouring, one getter per role. The
 *        display policy names a SHA palette colour per role (text_colors), the values come
 *        from colors.ts.
 * @return The parameter string of the role.
 */
[[nodiscard]] const char* display_sgr_string();
[[nodiscard]] const char* display_sgr_number();
[[nodiscard]] const char* display_sgr_url();
[[nodiscard]] const char* display_sgr_path();
[[nodiscard]] const char* display_sgr_tag();
[[nodiscard]] const char* display_sgr_punct();

/**
 * @brief O(1) probe of the generated outcome word set (good and bad words in one table).
 * @param hash FNV-1a over the lowercased word bytes.
 * @param len  Length of the word in bytes; hash plus length is the identity.
 * @return The SGR parameters of the word's role (good or bad), nullptr for a word that
 *         carries no outcome.
 */
[[nodiscard]] const char* display_outcome_sgr(uint32_t hash, uint32_t len);

}  // namespace detail

/**
 * @brief Shorten one formatted log line: the word abbreviation walk of the record.
 * @param line The line as the format layer composed it, ANSI sequences included.
 * @return The line with known words swapped for their taxonomy abbreviations and
 *         filler words dropped. Bracket groups and the category name before a ':'
 *         are identifiers and pass verbatim. Runs in PRODUCTION (semantic message
 *         flag in log_sys.cpp), so every channel carries the shortened bytes.
 */
[[nodiscard]] std::string shorten_display_line(const std::string& line);

/**
 * @brief Colour one message body semantically: the shape-based colouring of the record.
 * @param line The (already shortened) line; leading bracket groups are treated as the
 *             prefix and stay untinted, pre-existing ANSI sequences pass untouched.
 * @return The line with quoted values, numbers, URLs, paths, bracket tags, outcome words
 *         and structural punctuation wrapped in their role colours from the generated
 *         getters. Runs in PRODUCTION directly after the shortening walk - the outcome
 *         word set therefore carries full AND abbreviated forms.
 */
[[nodiscard]] std::string colorize_log_line(const std::string& line);

}  // namespace ase::log
