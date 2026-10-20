#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        log_ansi.hpp
 * @brief       Length of the terminal escape sequence at a position of a log line
 * @description A log record carries its colours as ECMA-48 escape sequences, and every walk
 *              over a record steps over them: the format layer passes them through byte-exact
 *              while it shortens and colours a line (log_display.cpp), the tier console counts
 *              display cells and breaks a long line without ever splitting one
 *              (ase-console, console_wrap.cpp). Both walks read the bytes this module writes,
 *              so both take the length of a sequence from here; two walks that disagree on
 *              where a sequence ends measure one record two ways.
 *
 *              WHAT COUNTS AS A SEQUENCE. ESC opens it. After ESC '[' (CSI) the parameter and
 *              intermediate bytes run up to the first final byte 0x40..0x7E. After ESC and any
 *              other byte the scan starts on that byte, so a two-byte escape (ESC M) and an
 *              escape with intermediates (ESC ( B) end on their own final byte. The scan for a
 *              CSI starts AFTER the '[': '[' itself lies inside the final range and would end
 *              the sequence at once, leaving its parameters behind as text.
 *
 *              AN UNTERMINATED SEQUENCE RUNS TO THE END OF THE LINE. A line cut inside a
 *              sequence ends in parameter bytes a terminal swallows: they take no cell, and
 *              passing them on as one zero-width run keeps the bytes exact and never steps past
 *              the end.
 *
 *              ITS OWN HEADER, cut by purpose like log_quota.hpp: two units step over
 *              sequences, while every unit that writes a line includes log.hpp. Header-only, so
 *              the console needs the ase::log::headers include path for it and no archive of
 *              the logger.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    ecs/module
 * @usecase     usecase:100362 - write a log line
 * @created     2026-10-18
 * @modified    2026-10-18
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

#include <cstddef>
#include <string>

namespace ase::log {

/**
 * @brief Bytes of the escape sequence that starts at line[pos].
 * @param line The line, escape sequences included.
 * @param pos  A position below line.size().
 * @return The length of the sequence up to and including its final byte, the rest of the line
 *         for an unterminated one, 0 when line[pos] is no ESC.
 */
[[nodiscard]] inline std::size_t ansi_seq_len(const std::string& line, std::size_t pos) noexcept {
    if (line[pos] != '\x1b') {
        return 0;
    }
    std::size_t i = (pos + 1 < line.size() && line[pos + 1] == '[') ? pos + 2 : pos + 1;
    while (i < line.size() && !(line[i] >= '@' && line[i] <= '~')) {
        ++i;
    }
    return (i < line.size()) ? (i - pos + 1) : (line.size() - pos);
}

}  // namespace ase::log
