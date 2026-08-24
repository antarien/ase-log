/**
 * ASE CORE INFRASTRUCTURE IMPLEMENTATION
 *
 * @file        log_query.cpp
 * @brief       Der Lesezugriff auf den Logring — was /api/logs beantwortet
 * @description Nimmt die formatierten Zeilen aus dem HTTP-Ringpuffer und uebersetzt sie zurueck
 *              in LogEntry-Datensaetze, mit fortlaufender Nummer. Das ist die ABFRAGE-Seite des
 *              Logs; die Aufbau-Seite (Senken, Logger, Formatierer) steht in log_sys.cpp.
 *
 * WARUM ES DIESE DATEI GIBT (2026-08-22)
 *
 *   log_sys.cpp hatte den God-System-Deckel gerissen (805 Zeilen), und die Regel dazu sagt
 *   woertlich, was daraus folgt: "split FIRST, fix findings SECOND" — die vorgeschriebenen
 *   Reparaturen FUEGEN Zeilen hinzu, also blockiert eine uebervolle Datei beide Fronten
 *   gleichzeitig. Und: "NEVER SHORTEN DOCUMENTATION TO GET UNDER THE NUMBER". Ich hatte genau
 *   das zweimal getan, bevor das Tor mich darauf gestossen hat.
 *
 *   Die Naht wurde gemessen, nicht geschaetzt: parse_ring_line ruft NICHTS aus dem Rest der
 *   Datei — kein g_*, kein LogSystem::, kein spdlog. recent_logs kreuzt die Naht an genau drei
 *   Stellen: dem Ringpuffer (jetzt ueber internal::log_resources()), dem Zaehler g_log_counter_
 *   und dem eigenen Helfer. Der Zaehler ist ein privates statisches Member von LogSystem —
 *   erreichbar bleibt er, weil recent_logs eine METHODE dieser Klasse ist; eine
 *   Methodendefinition darf in jeder Uebersetzungseinheit stehen.
 *
 *   Die Trennung ist die, die der Deckel meldet, nicht die, die die Zeilenzahl verlangt:
 *   "wie baue ich das Logging auf" und "welche Zeilen hat es geschrieben" sind zwei Fragen.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    ecs/module
 * @created     2026-08-22
 * @modified    2026-08-22
 * @version     1.0.0
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

#include <ase/log/log.hpp>
#include <ase/log/internal/log_resource_manager.hpp>
#include <ase/containers/vector.hpp>
#include <ase/types/types.hpp>
#include <ase/utils/strops.hpp>

#include <spdlog/sinks/ringbuffer_sink.h>

#include <cstring>
#include <string>

namespace ase::log {

namespace {

// Parse ringbuffer formatted string "[HH:MM:SS.mmm] [LVL] [SystemName] message" → LogEntry
LogEntry parse_ring_line(const std::string& line, uint32_t seq) {
    static const char* level_names[] = {"TRC", "DBG", "INF", "WRN", "ERR", "CRT"};
    LogEntry entry{};
    entry.seq = seq;
    entry.level = kLogLevelInfo;

    // Zerlegt wird an ']' mit utils::str_split_next — es liefert Zeiger und LAENGE statt einer
    // Sicht auf fremden Speicher. Jedes der drei Klammerfelder kommt damit als (Zeiger, Laenge)
    // INKLUSIVE seiner oeffnenden '[' und eines etwaigen fuehrenden Leerzeichens; beides wird
    // hier abgeschnitten, genau wie es die vorige Fassung mit remove_prefix tat.
    const char* text = line.c_str();
    const auto text_len = static_cast<uint32_t>(line.size());
    uint32_t pos = 0;

    const char* field = nullptr;
    uint32_t field_len = 0;

    // Ein Klammerfeld liefert seinen INHALT: fuehrendes Leerzeichen weg, '[' weg.
    //
    // Der Rueckgabewert ist NICHT kosmetisch. Die vorige Fassung prüfte vor jedem Abschnitt
    // `sv[0] == '['` und liess eine Zeile OHNE Klammern unangetastet als Meldung stehen. Ohne
    // diese Pruefung landete das erste ']'-getrennte Stueck einer klammerlosen Zeile im
    // Zeitstempel — ein Fehler, den erst der Fallmengen-Vergleich gegen die alte Fassung gezeigt
    // hat, nicht der Validator und nicht der Compiler.
    auto bracket_body = [](const char*& f, uint32_t& len) -> bool {
        const char* start = f;
        uint32_t remaining = len;
        if (remaining > 0 && start[0] == ' ') { ++start; --remaining; }
        if (remaining == 0 || start[0] != '[') return false;
        f = start + 1;
        len = remaining - 1;
        return true;
    };

    // [HH:MM:SS.mmm]
    uint32_t after_stamp = pos;
    if (utils::str_split_next(text, text_len, ']', pos, &field, &field_len) &&
        bracket_body(field, field_len)) {
        if (types::is_in_rng_u32(field_len, 1u, static_cast<uint32_t>(sizeof(entry.timestamp)) - 1u)) {
            std::memcpy(entry.timestamp, field, field_len);
            entry.timestamp[field_len] = '\0';
        }
        after_stamp = pos;
    } else {
        // Keine Klammer: die Zeile traegt kein Kopffeld. Zurueck an den Anfang, alles ist Meldung.
        pos = after_stamp;
    }

    // [LVL]
    uint32_t after_level = pos;
    if (utils::str_split_next(text, text_len, ']', pos, &field, &field_len) &&
        bracket_body(field, field_len)) {
        // Die Zahl der Namen kommt aus dem Array selbst, nicht aus einer wiederholten 6.
        constexpr uint8_t level_count = static_cast<uint8_t>(sizeof(level_names) / sizeof(level_names[0]));
        for (uint8_t i = 0; i < level_count; ++i) {
            // str_equal vergleicht bis zur Laenge; die zweite Bedingung schliesst aus, dass ein
            // kuerzeres Feld ein laengeres Kuerzel als Praefix trifft.
            if (utils::str_equal(field, level_names[i], field_len) &&
                level_names[i][field_len] == '\0') {
                entry.level = i;
                break;
            }
        }
        after_level = pos;
    } else {
        pos = after_level;
    }

    // [SystemName] (optional)
    uint32_t after_system = pos;
    if (utils::str_split_next(text, text_len, ']', pos, &field, &field_len) &&
        bracket_body(field, field_len)) {
        // is_in_rng_u32 ist INKLUSIV, die ersetzte Bedingung war strikt. [1, sizeof-1] ist fuer
        // vorzeichenlose Werte dieselbe Menge wie (0, sizeof).
        if (types::is_in_rng_u32(field_len, 1u, static_cast<uint32_t>(sizeof(entry.system)) - 1u)) {
            std::memcpy(entry.system, field, field_len);
            entry.system[field_len] = '\0';
        }
        after_system = pos;
    } else {
        pos = after_system;
    }

    // Der Rest ist die Meldung — ab der aktuellen Position, NICHT weiter gesplittet: eine
    // Meldung darf ']' enthalten, und die vorige Fassung nahm den Rest ebenfalls am Stueck.
    if (pos < text_len) {
        uint32_t msg_len = text_len - pos;
        const uint32_t cap = static_cast<uint32_t>(sizeof(entry.message)) - 1u;
        if (msg_len > cap) msg_len = cap;
        if (text[pos] == ' ') { ++pos; --msg_len; }
        std::memcpy(entry.message, text + pos, msg_len);
        entry.message[msg_len] = '\0';
    }

    return entry;
}

}  // anonymous namespace

ase::containers::Vector<LogEntry> LogSystem::recent_logs(uint32_t since_seq) {
    auto* http_ring = internal::log_resources().get_http_ring();
    if (http_ring == nullptr) return {};
    auto all = http_ring->last_formatted();
    if (all.empty()) return {};

    uint32_t seq = g_log_counter_.load();
    uint32_t total = static_cast<uint32_t>(all.size());

    // Determine which lines to return
    uint32_t count;
    if (since_seq == kLogSeqAll) {
        count = (total > kRecentLogsPageSize) ? kRecentLogsPageSize : total;
    } else if (since_seq >= seq) {
        return {};
    } else {
        count = seq - since_seq;
        if (count > total) count = total;
    }

    // Parse formatted strings to LogEntry, assign sequence numbers
    ase::containers::Vector<LogEntry> result;
    result.reserve(count);
    uint32_t start_seq = seq - count + 1;
    for (uint32_t i = total - count; i < total; ++i) {
        result.push_back(parse_ring_line(all[i], start_seq + (i - (total - count))));
    }
    return result;
}

}  // namespace ase::log
