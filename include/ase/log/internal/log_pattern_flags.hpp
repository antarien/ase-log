#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER (internal)
 *
 * @file        log_pattern_flags.hpp
 * @brief       The custom spdlog pattern flags of the record format layer
 * @description The three custom flags every record-writing formatter is built
 *              from. They are the ONE place where a channel's bytes are decided;
 *              a console renders what the record says and adds nothing.
 *
 *              ColoredLevelFlag (%*): the coloured 3-char level tag, colour per
 *              level from the generated getters (SHA palette via the display
 *              policy). PlainLevelFlag (%#): the same tag uncoloured, for the
 *              reduced HTTP-ring pattern. SemanticMessageFlag (%~): the message
 *              body, shortened against the generated word table and coloured by
 *              shape - it stands where %v stood, in EVERY channel, so tier file,
 *              tail, HTTP ring and TUI callback carry identical bytes (a console
 *              that shortened or coloured for itself showed different bytes than
 *              a raw tail, measured live between cli and dist 2026-08-24).
 *
 *              This header is INTERNAL and deliberately spdlog-bound: formatter
 *              flags ARE spdlog types. It is included by the ase-log translation
 *              units only, never by a consumer of log.hpp - the tree-wide log
 *              header stays free of formatter details, so a change here is a
 *              module-local recompile, never a cascade.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    ecs/module
 * @created     2026-08-24
 * @modified    2026-08-24
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

#include <ase/log/log_display.hpp>
#include <spdlog/pattern_formatter.h>

#include <cstddef>
#include <memory>
#include <string>

namespace ase::log::internal {

// The coloured 3-character level tag (%*), matching the ecs.cpp boot log. The colour per
// level comes from the generated getters (display policy names a SHA palette colour, the
// value lives in colors.ts) - a colour change is a generat recompile, never an edit here.
class ColoredLevelFlag : public spdlog::custom_flag_formatter {
public:
    void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override {
        static const char* levels[] = {"TRC", "DBG", "INF", "WRN", "ERR", "CRT", "OFF"};
        auto idx = static_cast<size_t>(msg.level);
        if (idx < sizeof(levels) / sizeof(levels[0])) {
            const char* sgr = detail::display_level_sgr(static_cast<uint32_t>(idx));
            if (sgr[0] != '\0') {
                dest.append(std::string_view("\x1b["));
                dest.append(std::string_view(sgr));
                dest.append(std::string_view("m"));
            }
            dest.append(std::string_view(levels[idx]));
            dest.append(std::string_view("\x1b[0m"));
        }
    }

    [[nodiscard]] std::unique_ptr<custom_flag_formatter> clone() const override {
        return std::make_unique<ColoredLevelFlag>();
    }
};

// The plain 3-character level tag (%#) for the reduced HTTP-ring pattern.
class PlainLevelFlag : public spdlog::custom_flag_formatter {
public:
    void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override {
        static const char* levels[] = {"TRC", "DBG", "INF", "WRN", "ERR", "CRT", "OFF"};
        auto idx = static_cast<size_t>(msg.level);
        if (idx < sizeof(levels) / sizeof(levels[0])) {
            dest.append(std::string_view(levels[idx]));
        }
    }

    [[nodiscard]] std::unique_ptr<custom_flag_formatter> clone() const override {
        return std::make_unique<PlainLevelFlag>();
    }
};

// The semantic message (%~): it stands where %v stood, in EVERY channel that writes the
// record. ONE production walk per line - shorten the live values against the generated
// word table (one FNV-1a pass, one O(1) hash-plus-length probe per word, never a table
// scan and never a string-list comparison; the static short and hint texts leave the
// GENERATOR already shortened), then colour the HEAD by shape. The tail lines (everything
// from the first embedded newline on) pass verbatim: they are finished generat bytes in
// the hint grey, and a live value never stands there. Running this HERE, in the format
// layer, is what keeps every channel byte-identical.
class SemanticMessageFlag : public spdlog::custom_flag_formatter {
public:
    void format(const spdlog::details::log_msg& msg, const std::tm&, spdlog::memory_buf_t& dest) override {
        const std::string raw(msg.payload.data(), msg.payload.size());
        const std::size_t nl = raw.find('\n');
        const std::string head =
            colorize_log_line(shorten_display_line(nl == std::string::npos ? raw : raw.substr(0, nl)));
        dest.append(head.data(), head.data() + head.size());
        if (nl != std::string::npos) {
            dest.append(raw.data() + nl, raw.data() + raw.size());
        }
    }

    [[nodiscard]] std::unique_ptr<custom_flag_formatter> clone() const override {
        return std::make_unique<SemanticMessageFlag>();
    }
};

}  // namespace ase::log::internal
