/**
 * ASE CORE INFRASTRUCTURE IMPLEMENTATION
 *
 * @file        log_capture.cpp
 * @brief       Capture-Klammer: Logausgabe umleiten, puffern, zeilenweise zurueckgeben
 * @description Wer eine Fortschrittstabelle auf stdout schreibt, darf sich von einer Logzeile
 *              nicht die Zeile zerreissen lassen. Diese Datei haengt fuer die Dauer einer
 *              solchen Ausgabe die Senken des Loggers um, sammelt {Stufe, Text} und gibt sie
 *              danach einzeln heraus — durch Puffer des Aufrufers, ohne dass ein Typ der
 *              Logbibliothek die Schnittstelle kreuzt.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    ecs/module
 * @created     2026-08-20
 * @modified    2026-08-20
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
 *
 * WARUM ES DIESE DATEI GIBT (2026-08-20)
 *
 *   Sie ist ein AUSZUG aus log_sys.cpp, und der Auszug war keine Stilfrage. log_sys.cpp stand
 *   bei 781 Zeilen; das Schreibtor laesst eine Datei nahe dem 800er-Deckel nur bearbeiten,
 *   solange sie nicht WAECHST, und die Klammer haette sie auf 869 gebracht. Die Regel sagt
 *   dazu ausdruecklich: erst SPLITTEN, dann Befunde beheben — in dieser Reihenfolge, weil
 *   jede vorgeschriebene Reparatur Zeilen HINZUFUEGT und wer zuerst repariert an beiden
 *   Fronten gleichzeitig steht.
 *
 *   Der Deckel misst auch keine Laenge, sondern eine fehlende Trennung, und hier gab es eine:
 *   das Logging-System baut Senken auf und schreibt durch sie; diese Datei haengt sie fuer
 *   eine begrenzte Zeit ab. Das ist eine eigene Aufgabe mit eigenem Zustand.
 *
 * WARUM SIE IN ase-log LIEGT UND NICHT DORT, WO SIE GEBRAUCHT WIRD
 *
 *   Zwei Aufrufer in ase-ecs (boot_logger.cpp, shutdown_sequence.cpp) hatten je eine EIGENE
 *   Kopie dieses Mechanismus: eine private Senkenklasse, einen gemerkten Senkenvektor, eine
 *   Abspielschleife. Zweimal dasselbe, in einem Modul, dem weder der Logger noch seine Senken
 *   gehoeren — beide griffen am Logger vorbei und tauschten dessen Senkenvektor von Hand.
 *
 *   Die Senken gehoeren dem Logger, also gehoert die Klammer hierher. Und sie gibt KEINEN Typ
 *   der Logbibliothek nach aussen: die Stufe ist eine kleine Ganzzahl, der Text geht durch
 *   einen Puffer des Aufrufers. Einen Senkenzeiger herauszureichen haette die Kopplung nur in
 *   eine andere oeffentliche Signatur verschoben — der Fehler, den ein Wrapper um einen
 *   verbotenen Typ macht, waehrend er wie eine Loesung aussieht.
 */

#include <ase/log/log.hpp>

// Die Deklarationen der Senken- und Formatierer-Fabriken, deren Rumpf am Ende dieser Datei steht.
#include <ase/log/internal/log_resource_manager.hpp>
// Die Rotationspolitik (welche Datei, wie gross, wie viele Generationen).
#include <ase/log/internal/log_files.hpp>
// Die Anzeige-Politik: Zeitstempel-Grau, Stufenfarbe, Kuerzung und Einfaerbung der Meldung.
#include <ase/log/log_display.hpp>

#include <ase/containers/vector.hpp>

#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/callback_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

namespace ase::log {

namespace {

/**
 * Queue-Senke: schreibt nirgendwohin, sondern sammelt {Stufe, Text}.
 *
 * Dieselbe base_sink<std::mutex>-Form wie CountingSink und TuiCallbackSink in log_sys.cpp.
 * Der Mutex ist hier keine Zierde: ein Logaufruf aus einem beliebigen Thread kann im Puffer
 * landen, waehrend der Aufrufer auf dem Hauptthread seine Tabelle zeichnet.
 */
class CaptureQueueSink : public spdlog::sinks::base_sink<std::mutex> {
public:
    struct Line {
        uint8_t     level = 0;
        std::string text;
    };

    const ase::containers::Vector<Line>& lines() const { return lines_; }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        const uint32_t raw = static_cast<uint32_t>(msg.level);
        Line line;
        line.level = static_cast<uint8_t>(raw > kLogLevelMax ? kLogLevelMax : raw);
        line.text.assign(msg.payload.data(), msg.payload.size());
        lines_.push_back(std::move(line));
    }

    void flush_() override {}

private:
    ase::containers::Vector<Line> lines_;
};

/**
 * Zustand der Klammer, an EINER Stelle statt in drei Variablen.
 *
 * Ein funktionslokales static, keine Dateiglobale — dieselbe Form, auf die gen_log_cat.py
 * seine sechs Zaehler am 2026-08-20 umgestellt hat. Drei getrennte Variablen (Senke, gemerkte
 * Senken, offen-Flag) koennten auseinanderlaufen; als ein Wert koennen sie es nicht.
 */
struct CaptureState {
    std::shared_ptr<CaptureQueueSink>         queue;
    ase::containers::Vector<spdlog::sink_ptr> saved;
    bool                                      open = false;
};

CaptureState& capture_state() {
    static CaptureState state;
    return state;
}

}  // namespace

bool capture_begin() {
    CaptureState& state = capture_state();
    if (state.open) {
        // Keine Verschachtelung: der innere Abschluss wuerde die falschen Senken
        // zuruecklegen, und zwar lautlos. Ein zweiter Ruf aendert deshalb gar nichts.
        return false;
    }

    auto& logger = LogSystem::logger();
    if (!logger) {
        // Noch kein Logger: es gibt nichts umzuleiten. Der Aufrufer darf weitermachen,
        // seine Logzeilen haben schlicht kein Ziel — genau wie ohne diese Klammer.
        return false;
    }

    state.queue = std::make_shared<CaptureQueueSink>();
    state.saved = logger->sinks();
    logger->sinks().clear();
    logger->sinks().push_back(state.queue);
    state.open = true;
    return true;
}

uint32_t capture_count() {
    const CaptureState& state = capture_state();
    if (!state.open || !state.queue) {
        return 0u;
    }
    return static_cast<uint32_t>(state.queue->lines().size());
}

uint32_t capture_entry(uint32_t index, uint8_t& out_level, char* out_text, uint32_t out_cap) {
    const CaptureState& state = capture_state();
    if (!state.open || !state.queue) {
        return 0u;
    }

    const auto& lines = state.queue->lines();
    if (index >= static_cast<uint32_t>(lines.size())) {
        return 0u;
    }

    const CaptureQueueSink::Line& line = lines[index];
    out_level = line.level;

    const uint32_t full = static_cast<uint32_t>(line.text.size());
    if (out_text != nullptr && out_cap > 0u) {
        const uint32_t fits = (full < out_cap - 1u) ? full : out_cap - 1u;
        for (uint32_t i = 0u; i < fits; ++i) {
            out_text[i] = line.text[i];
        }
        out_text[fits] = '\0';
    }

    // Die VOLLE Laenge, nicht die kopierte: nur so kann ein Aufrufer eine Kappung sehen.
    // Ein bool haette "passt" und "wurde geschnitten" gleich aussehen lassen.
    return full;
}

uint32_t capture_replay() {
    CaptureState& state = capture_state();
    if (!state.open || !state.queue) {
        return 0u;
    }

    auto& logger = LogSystem::logger();
    if (!logger) {
        return 0u;
    }
    if (state.saved.empty()) {
        return 0u;  // beim Oeffnen der Klammer hing nichts am Logger
    }

    // Ziel sind die GEMERKTEN Senken, nicht die aktiven: aktiv ist waehrend der Klammer nur der
    // Puffer selbst, und ein Logaufruf liefe dorthin zurueck. Deshalb kann diese Umschaltung
    // nur hier stehen — ein Aufrufer haelt die gemerkten Senken nicht.
    ase::containers::Vector<spdlog::sink_ptr> queue_only = logger->sinks();
    logger->sinks().clear();
    for (auto& sink : state.saved) {
        logger->sinks().push_back(sink);
    }

    uint32_t replayed = 0u;
    for (const auto& line : state.queue->lines()) {
        logger->log(static_cast<spdlog::level::level_enum>(line.level), "{}", line.text);
        ++replayed;
    }

    // Zurueck auf den Puffer: die Klammer ist noch offen, und was nach dem Abspielen kommt,
    // gehoert weiterhin gesammelt und nicht ausgegeben.
    logger->sinks().clear();
    for (auto& sink : queue_only) {
        logger->sinks().push_back(sink);
    }
    return replayed;
}

void capture_end(bool restore_sinks) {
    CaptureState& state = capture_state();
    if (!state.open) {
        return;
    }

    auto& logger = LogSystem::logger();
    if (logger && restore_sinks) {
        logger->sinks().clear();
        for (auto& sink : state.saved) {
            logger->sinks().push_back(sink);
        }
    }

    state.saved.clear();
    state.queue.reset();
    state.open = false;
}

namespace internal {

/*
 * DIE SENKEN- UND FORMATSCHICHT — Rumpf hier, Vertrag in internal/log_resource_manager.hpp.
 *
 * WARUM DIE RUEMPFE IN DIESER DATEI STEHEN UND NICHT BEI IHREN DEKLARATIONEN
 *
 *   Drei Regeln treffen diesen Code gleichzeitig, und er braucht von jeder eine Ausnahme:
 *   SPDLOG_DIRECT_FORBIDDEN (die Fabriken nennen die Bibliothek), INHERITANCE_FORBIDDEN_EXCEPT_ECS
 *   (eine Musterflagge IST eine Ableitung von spdlog::custom_flag_formatter) und
 *   PROTOTYPE_PATTERN_FORBIDDEN (deren clone() ist rein virtuell, ohne sie uebersetzt nichts).
 *
 *   Die SCHNITTMENGE der drei namentlichen Ausnahmelisten enthaelt genau zwei Dateien: log_sys.cpp
 *   und diese. log_sys.cpp kaeme mit Flaggen und Fabriken ueber den Trenner des
 *   GOD_SYSTEM_UNSPLIT_BAND — und aus einem Band fuehrt nur eine Trennung, nie eine Kuerzung.
 *   Damit bleibt diese Datei, und die Widmung traegt es: sie haengt SENKEN um, die Fabriken BAUEN
 *   Senken. Beides ist Senkenarbeit an derselben Bibliotheksgrenze.
 *
 *   DIE DEKLARATIONEN BLEIBEN, WO SIE HINGEHOEREN — beim Ressourcen-Verwalter, der die Senken
 *   haelt und einhaengt. Ein Header darf sie tragen, weil dort keine Ableitung steht; nur der
 *   RUMPF braucht alle drei Ausnahmen. Wer die Fabriken sucht, findet sie ueber ihren Vertrag,
 *   nicht ueber diese Datei.
 */

namespace {

// Die gefaerbte 3-Zeichen-Stufenmarke (%*). Die Farbe je Stufe kommt aus den erzeugten Gettern
// (die Anzeige-Politik nennt eine SHA-Palettenfarbe) — ein Farbwechsel ist ein Generat-Neubau,
// nie eine Aenderung hier.
class ColoredLevelFlag : public spdlog::custom_flag_formatter {
public:
    void format(const spdlog::details::log_msg& msg, const std::tm&,
                spdlog::memory_buf_t& dest) override {
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

// Die schlichte 3-Zeichen-Stufenmarke (%#) fuer das reduzierte Muster des HTTP-Rings.
class PlainLevelFlag : public spdlog::custom_flag_formatter {
public:
    void format(const spdlog::details::log_msg& msg, const std::tm&,
                spdlog::memory_buf_t& dest) override {
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

// Die semantische Meldung (%~): sie steht, wo %v stand, in JEDEM Kanal, der den Datensatz
// schreibt. EIN Durchgang je Zeile — die Live-Werte gegen die erzeugte Worttabelle kuerzen (ein
// FNV-1a-Lauf, je Wort eine O(1)-Sonde aus Hash und Laenge, nie ein Tabellendurchlauf), dann den
// KOPF nach Form einfaerben. Die Folgezeilen ab dem ersten eingebetteten Umbruch gehen woertlich
// durch: sie sind fertige Generat-Bytes im Hinweisgrau, und ein Live-Wert steht dort nie. Dass
// das HIER laeuft, in der Formatschicht, ist der Grund, warum jeder Kanal byte-gleich bleibt.
class SemanticMessageFlag : public spdlog::custom_flag_formatter {
public:
    void format(const spdlog::details::log_msg& msg, const std::tm&,
                spdlog::memory_buf_t& dest) override {
        const std::string raw(msg.payload.data(), msg.payload.size());
        const std::size_t nl = raw.find('\n');
        const std::string head = colorize_log_line(
            shorten_display_line(nl == std::string::npos ? raw : raw.substr(0, nl)));
        dest.append(head.data(), head.data() + head.size());
        if (nl != std::string::npos) {
            dest.append(raw.data() + nl, raw.data() + raw.size());
        }
    }

    [[nodiscard]] std::unique_ptr<custom_flag_formatter> clone() const override {
        return std::make_unique<SemanticMessageFlag>();
    }
};

}  // namespace

// DIE DREI SENKEN-FABRIKEN. Wer eine Senke braucht, ruft hier, statt sie selbst zu konstruieren —
// die Bauform steht an EINER Stelle, und die Aufrufer nennen keinen Typ der Bibliothek mehr.

std::shared_ptr<spdlog::sinks::sink> make_rotating_file_sink(const std::string& path) {
    uint64_t max_bytes = 0;
    uint32_t max_files = 0;
    prepare_log_sink(path, max_bytes, max_files);
    return std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        path, static_cast<std::size_t>(max_bytes), static_cast<std::size_t>(max_files), true);
}

std::shared_ptr<spdlog::sinks::sink> make_console_sink(const std::string& pattern) {
    auto sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    sink->set_pattern(pattern);
    return sink;
}

std::shared_ptr<spdlog::sinks::sink> make_callback_sink(
    void (*callback)(const char* line, uint32_t len, int level, void* user), void* user,
    std::shared_ptr<spdlog::formatter> fmt) {
    return std::make_shared<spdlog::sinks::callback_sink_mt>(
        [callback, user, fmt](const spdlog::details::log_msg& msg) {
            if (callback == nullptr) return;
            spdlog::memory_buf_t buf;
            fmt->format(msg, buf);
            callback(buf.data(), static_cast<uint32_t>(buf.size()), static_cast<int>(msg.level),
                     user);
        });
}

// DIE VIER FORMATIERER-FABRIKEN. Sie setzen je ein Muster zusammen und haengen die drei Flaggen
// von oben an — die EINE Stelle, an der steht, wie eine Logzeile aussieht. Das Tier-Dateimuster
// und das Standalone-Dateimuster sind byteweise dasselbe, weil sie DIESELBE Funktion sind und
// nicht, weil zwei Stellen dasselbe behaupten.

std::unique_ptr<spdlog::formatter> make_file_formatter(const char* label) {
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<ColoredLevelFlag>('*');
    formatter->add_flag<SemanticMessageFlag>('~');
    formatter->set_pattern(std::string("\x1b[") + detail::display_timestamp_sgr() +
                           "m[%Y-%m-%d %H:%M:%S.%e]\x1b[0m [%*] [ASE] [" + std::string(label) +
                           "] %~");
    return formatter;
}

std::unique_ptr<spdlog::formatter> make_ring_formatter() {
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<PlainLevelFlag>('#');
    formatter->add_flag<SemanticMessageFlag>('~');
    formatter->set_pattern("[%H:%M:%S.%e] [%#] %~");
    return formatter;
}

std::unique_ptr<spdlog::formatter> make_tui_file_formatter(const char* label) {
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<PlainLevelFlag>('#');
    formatter->add_flag<SemanticMessageFlag>('~');
    formatter->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%#] [ASE] [" + std::string(label) + "] %~");
    return formatter;
}

std::shared_ptr<spdlog::formatter> make_tui_callback_formatter(const char* label) {
    auto formatter = std::make_shared<spdlog::pattern_formatter>(spdlog::pattern_time_type::local,
                                                                std::string());
    formatter->add_flag<ColoredLevelFlag>('*');
    formatter->add_flag<SemanticMessageFlag>('~');
    formatter->set_pattern(std::string("\x1b[") + detail::display_timestamp_sgr() +
                           "m[%Y-%m-%d %H:%M:%S.%e]\x1b[0m [%*] [ASE] [" + std::string(label) +
                           "] %~");
    return formatter;
}

}  // namespace internal

}  // namespace ase::log
