/**
 * ASE CORE INFRASTRUCTURE IMPLEMENTATION
 *
 * @file        log_display.cpp
 * @brief       Record shortening and semantic colouring walks
 * @description The two record transformations of the log PRODUCTION path.
 *              shorten_display_line walks a formatted line once, steps over ANSI
 *              sequences, copies bracket groups and the category name verbatim
 *              (identifiers, never prose), hashes every other word on the way
 *              and swaps or drops it via the GENERATED table in
 *              log_display_gen.cpp - one FNV-1a pass and an O(1) probe per word,
 *              no linear scan. colorize_log_line walks the message body once and
 *              colours tokens by SHAPE: quoted values, numbers, URLs, paths,
 *              bracket tags, outcome words (O(1) probe of the generated set),
 *              structural punctuation; every colour comes from the generated
 *              getters, this file holds none of its own. Both run in the format
 *              layer (log_sys.cpp), so every channel carries identical bytes.
 *              Nothing here splits or augments a record: a categorized message
 *              carries its short text and its hint lines from the format layer.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    ecs/module
 * @created     2026-08-24
 * @modified    2026-08-24
 * @version     1.2.0
 *
 * CORE INFRASTRUCTURE IMPLEMENTATION COMPLIANCE
 *
 * [ ] NOT an ECS System implementation
 * [ ] Layer dependencies correct (L0: no ASE deps, L1: L0 only)
 * [ ] Own header included FIRST
 * [ ] No global mutable state
 * [ ] No static initialization order fiasco
 * [ ] Thread-safe implementations (pure or mutex-protected)
 * [ ] All error conditions handled
 * [ ] No exceptions thrown (use Result<T> pattern)
 * [ ] Implementation details in anonymous namespace
 * [ ] No inline implementations of template specializations here
 * [ ] Platform-specific code isolated and documented
 * [ ] Performance-critical code profiled and optimized
 */

#include <ase/log/log_display.hpp>

#include <cstddef>

namespace ase::log {

namespace {

// FNV-1a over lowercased bytes - the one hashing convention of the whole log system:
// gen_log_cat.py fnv1a_hash lowercases before hashing, and both the category constants and
// the display word table are built with exactly this function.
constexpr uint32_t kFnvOffsetBasis = 2166136261u;
constexpr uint32_t kFnvPrime = 16777619u;

constexpr const char* kReset = "\x1b[0m";

char lower(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// A word character for matching purposes. Digits count so "ws9003" is one token and never
// matched against the word table.
bool is_word_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// A token character for the colouring scanner: letters, digits and the separators that
// appear INSIDE identifiers, so "cli-116345.log" and "ws://host:9003" stay one token
// instead of five.
bool is_token_char(char c) {
    return is_alpha(c) || is_digit(c) || c == '_' || c == '-' || c == '.' || c == '/' ||
           c == ':' || c == '~';
}

// Length of the ANSI escape sequence starting at pos, or 0 when there is none. A CSI
// sequence ends at the first byte in 0x40..0x7E; anything unterminated is treated as a
// plain byte so a truncated line can never make the walker run past the end.
std::size_t ansi_len(const std::string& s, std::size_t pos) {
    if (s[pos] != '\x1b' || pos + 1 >= s.size() || s[pos + 1] != '[') {
        return 0;
    }
    std::size_t i = pos + 2;
    while (i < s.size() && !(s[i] >= '@' && s[i] <= '~')) {
        ++i;
    }
    return (i < s.size()) ? (i - pos + 1) : 0;
}

// The composed escape per semantic role, built ONCE from the generated getters on first use.
const std::string& col_string() {
    static const std::string v = std::string("\x1b[") + detail::display_sgr_string() + "m";
    return v;
}
const std::string& col_number() {
    static const std::string v = std::string("\x1b[") + detail::display_sgr_number() + "m";
    return v;
}
const std::string& col_url() {
    static const std::string v = std::string("\x1b[") + detail::display_sgr_url() + "m";
    return v;
}
const std::string& col_path() {
    static const std::string v = std::string("\x1b[") + detail::display_sgr_path() + "m";
    return v;
}
const std::string& col_tag() {
    static const std::string v = std::string("\x1b[") + detail::display_sgr_tag() + "m";
    return v;
}
const std::string& col_punct() {
    static const std::string v = std::string("\x1b[") + detail::display_sgr_punct() + "m";
    return v;
}

// Where the message body starts: after the LAST "] " of the leading run of bracketed
// fields (source marker, or timestamp/level/label when a full line is passed). The message
// itself may contain brackets too, so the scan stops at the first character that is not a
// bracket group.
std::size_t message_start(const std::string& s) {
    std::size_t i = 0;
    std::size_t last = 0;
    while (i < s.size()) {
        // Skip a colour sequence that wraps a prefix field. The scan MUST start after the
        // "\x1b[", because '[' itself falls inside the CSI terminator range 0x40..0x7E -
        // starting on it ends the sequence immediately and leaves the parameters behind as
        // if they were text.
        const std::size_t esc = ansi_len(s, i);
        if (esc > 0) {
            i += esc;
            continue;
        }
        if (s[i] == ' ') {
            ++i;
            continue;
        }
        if (s[i] != '[') {
            break;
        }
        const std::size_t close = s.find(']', i);
        if (close == std::string::npos) {
            break;
        }
        i = close + 1;
        last = i;
    }
    return last;
}

// Classify one token by shape and return its colour escape, empty when it stays uncoloured.
std::string classify(const std::string& tok) {
    if (tok.empty()) {
        return std::string();
    }

    // URL or host:port - has a scheme separator.
    if (tok.find("://") != std::string::npos) {
        return col_url();
    }

    // Absolute path or home path.
    if (tok[0] == '/' || (tok.size() > 1 && tok[0] == '~' && tok[1] == '/')) {
        return col_path();
    }

    // Pure number, possibly grouped or fractional (108, 2.027.784, 50.0).
    bool all_num = true;
    bool has_digit = false;
    for (char c : tok) {
        if (is_digit(c)) {
            has_digit = true;
        } else if (c != '.' && c != '-') {
            all_num = false;
            break;
        }
    }
    if (all_num && has_digit) {
        return col_number();
    }

    // A file name keeps the path colour: it names a thing on disk, like a path does.
    if (tok.size() > 4 && tok.compare(tok.size() - 4, 4, ".log") == 0) {
        return col_path();
    }

    // Outcome words: one O(1) probe of the generated set, the hit IS the role's colour.
    uint32_t h = kFnvOffsetBasis;
    for (const char c : tok) {
        h = (h ^ static_cast<uint32_t>(static_cast<unsigned char>(lower(c)))) * kFnvPrime;
    }
    const char* sgr = detail::display_outcome_sgr(h, static_cast<uint32_t>(tok.size()));
    if (sgr != nullptr) {
        return std::string("\x1b[") + sgr + "m";
    }
    return std::string();
}

}  // namespace

std::string shorten_display_line(const std::string& line) {
    std::string out;
    out.reserve(line.size());

    std::size_t i = 0;
    while (i < line.size()) {
        // Step over colour sequences untouched: they carry no words and must survive
        // byte-exact.
        const std::size_t esc = ansi_len(line, i);
        if (esc > 0) {
            out.append(line, i, esc);
            i += esc;
            continue;
        }

        // A bracket group is an IDENTIFIER, not prose: [Vault], [WebSocket] name the source
        // of the line. Abbreviating them would rename the very thing the operator greps for.
        if (line[i] == '[') {
            const std::size_t close = line.find(']', i);
            if (close != std::string::npos) {
                out.append(line, i, close - i + 1);
                i = close + 1;
                continue;
            }
        }

        // A category name is an IDENTIFIER too: a run of capitals and underscores before a
        // ':' (HOST_OP_FAILED:) is what the operator greps for. The generic word walk below
        // would split it at the underscores and rewrite single words - copy the run verbatim.
        // The record wraps the name in its category colour, so any ANSI sequences between the
        // run and the ':' are stepped over for the check and copied with the run.
        if (line[i] >= 'A' && line[i] <= 'Z') {
            std::size_t j = i;
            while (j < line.size() && ((line[j] >= 'A' && line[j] <= 'Z') || line[j] == '_')) {
                ++j;
            }
            std::size_t k = j;
            std::size_t esc_after = ansi_len(line, k);
            while (k < line.size() && esc_after > 0) {
                k += esc_after;
                esc_after = (k < line.size()) ? ansi_len(line, k) : 0;
            }
            if (j - i >= 2 && k < line.size() && line[k] == ':') {
                out.append(line, i, k - i);
                i = k;
                continue;
            }
        }

        if (!is_word_char(line[i])) {
            out.push_back(line[i]);
            ++i;
            continue;
        }

        // Collect one whole word and hash it on the way - the hash pass IS the read, no
        // second walk. Whole words only, so identifiers and paths stay alone.
        std::size_t end = i;
        uint32_t h = kFnvOffsetBasis;
        while (end < line.size() && is_word_char(line[end])) {
            h = (h ^ static_cast<uint32_t>(static_cast<unsigned char>(lower(line[end])))) * kFnvPrime;
            ++end;
        }
        const std::size_t len = end - i;

        // O(1) probe of the generated table. nullptr: unknown word, copied. Empty: filler,
        // dropped together with the single space in front of it - never at the very start
        // of the line, where the word survives verbatim instead.
        const char* few = detail::display_word(h, static_cast<uint32_t>(len));
        if (few != nullptr && few[0] == '\0') {
            if (!out.empty() && out.back() == ' ') {
                out.pop_back();
            } else {
                out.append(line, i, len);
            }
            i = end;
            continue;
        }
        if (few == nullptr) {
            out.append(line, i, len);
        } else {
            // Always lowercase, whatever the source wrote: an abbreviation is a symbol, not
            // a word, and the taxonomy spells every one of them in lowercase.
            out += few;
        }
        i = end;
    }

    return out;
}

std::string colorize_log_line(const std::string& line) {
    const std::size_t begin = message_start(line);
    if (begin >= line.size()) {
        return line;
    }

    std::string out;
    out.reserve(line.size() + 64);
    out.append(line, 0, begin);

    std::size_t i = begin;
    while (i < line.size()) {
        const char c = line[i];

        // A pre-existing colour sequence is copied untouched. Same rule as in message_start:
        // the scan starts after "\x1b[", never on the '[' itself.
        const std::size_t esc = ansi_len(line, i);
        if (esc > 0) {
            out.append(line, i, esc);
            i += esc;
            continue;
        }

        // Quoted value: colour the quotes and everything between them as one string token.
        if (c == '\'' || c == '"') {
            const std::size_t close = line.find(c, i + 1);
            if (close != std::string::npos) {
                out += col_string();
                out.append(line, i, close - i + 1);
                out += kReset;
                i = close + 1;
                continue;
            }
        }

        // Bracket tag such as [Wskt] or [CLIENT]: a source marker inside the message.
        if (c == '[') {
            const std::size_t close = line.find(']', i);
            if (close != std::string::npos && close - i <= 16) {
                out += col_tag();
                out.append(line, i, close - i + 1);
                out += kReset;
                i = close + 1;
                continue;
            }
        }

        if (is_token_char(c)) {
            std::size_t end = i;
            while (end < line.size() && is_token_char(line[end])) {
                ++end;
            }
            // Give back trailing punctuation. ':' and '.' belong INSIDE a token
            // ("ws://h:9003", "cli-1.log") but not at its end, where they close a clause -
            // without this "unrc:" would never match the word "unrc" and would stay
            // uncoloured.
            while (end > i + 1) {
                const char last = line[end - 1];
                if (last == ':' || last == '.' || last == ',' || last == ';') {
                    --end;
                    continue;
                }
                break;
            }
            const std::string tok = line.substr(i, end - i);
            const std::string colour = classify(tok);
            if (!colour.empty()) {
                out += colour;
                out += tok;
                out += kReset;
            } else {
                out += tok;
            }
            i = end;
            continue;
        }

        // Structural punctuation between fields reads better dimmed than in body colour.
        if (c == '(' || c == ')' || c == ',' || c == ';' || c == '|') {
            out += col_punct();
            out.push_back(c);
            out += kReset;
            ++i;
            continue;
        }

        out.push_back(c);
        ++i;
    }

    return out;
}

}  // namespace ase::log
