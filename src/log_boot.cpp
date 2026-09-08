/**
 * ASE CORE INFRASTRUCTURE IMPLEMENTATION
 *
 * @file        log_boot.cpp
 * @brief       Die Startklammer des Loggers: auffangen, umhaengen, den CLI-Filter lesen
 * @description Drei Funktionen, die zusammen EINEN Uebergang beschreiben — vom Zustand "es gibt
 *              noch keinen Logger" in den Zustand "alle Kanaele tragen denselben Inhalt". Sie
 *              laufen ausserhalb jeder Registry und vor jedem System.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    process/computation
 * @created     2026-08-30
 * @modified    2026-08-30
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
 * WARUM ES DIESE DATEI GIBT (2026-08-30)
 *
 *   Sie ist ein AUSZUG aus log_sys.cpp, der vierte nach log_files.cpp, log_query.cpp und
 *   log_capture.cpp. Der Deckel meldet keine Laenge, sondern eine fehlende Trennung, und die
 *   Trennung ist hier am ABNEHMER ablesbar: LogSystem::on_start baut die Senken und laeuft im
 *   Schedule, diese drei Funktionen laufen VOR und NEBEN dem Schedule.
 *
 *   install_capture_logger steht als ERSTE Zeile von Kernel::build, also bevor irgendein System
 *   existiert. parse_cli_filter_from_argv liest argv, also bevor es eine Registry gibt.
 *   finalize_logger_after_boot laeuft, wenn der Startblock durch ist. Keine der drei bekommt je
 *   eine Registry zu sehen — ein ECS-System ist etwas anderes als der Weg dorthin.
 *
 * DIE ZWEI HAELFTEN DER PARK-LOGIK LIEGEN JETZT IN ZWEI DATEIEN, UND DAS IST ABSICHT
 *
 *   LogSystem::on_start BAUT die drei echten Senken und parkt sie am ResourceManager, ohne sie
 *   einzuhaengen; finalize_logger_after_boot HOLT sie dort ab und spielt den Aufnahme-Ring in
 *   sie hinein. Der Kanal zwischen beiden ist der Manager, nicht eine gemeinsame Datei — genau
 *   deshalb ist der Schnitt hier moeglich, ohne dass etwas Neues erfunden werden musste. Wer die
 *   andere Haelfte sucht, findet sie in log_sys.cpp unter demselben Namen (park_* dort, take_*
 *   hier).
 */

#include <ase/log/log.hpp>

#include <ase/log/log_filter.hpp>
#include <ase/log/internal/log_resource_manager.hpp>
#include <ase/types/types.hpp>

// DREI EINBINDUNGEN SIND MIT DEM UMBAU AM 2026-08-31 ENTFALLEN, weil ihre Verwendung entfiel und
// nicht, weil hier gekuerzt wurde: <spdlog/sinks/ringbuffer_sink.h> baute den Aufnahme-Ring,
// <algorithm> trug das std::remove_if beim Abhaengen, <memory> das make_shared, und
// ase/containers/vector.hpp den Senkenbehaelter. Alle vier Vorgaenge stehen jetzt beim Besitzer
// des Rings; diese Datei beschreibt den UEBERGANG und baut nichts mehr selbst.
#include <cstdint>
#include <cstring>

namespace ase::log {

// Install the capture-phase logger. Must be called as the FIRST line of
// Kernel::build so every later log call (KernelEnvLdrSystem, KernelCmdSystem,
// dlopen discovery, any system's on_start before LogSystem runs) goes into
// the capture ring instead of being dropped by the null-logger gate in log.hpp.
// No console sink — the App::startup() Schedule-Bootstrap block writes its
// own progress table to stdout and must not be interleaved with log lines.
void install_capture_logger() {
    auto& active_logger = internal::logger_slot();
    if (active_logger) return;  // idempotent (tests / CLI tools)
    // Der Ring wird in EINEM Griff gebaut und abgelegt: er ist die einzige Senke dieses Loggers
    // UND die Quelle des spaeteren Rueckspielens. Warum beides zusammengehoert, steht bei
    // internal::install_capture_ring; die Zeilenzahl ist dort benannt statt hier gesetzt.
    internal::SinkSet sinks;
    sinks.add(internal::install_capture_ring(internal::kCaptureRingLines));

    // Beide Stufen auf TRC: in der Startphase entscheidet noch kein Filter, was aufbewahrt wird —
    // wer hier siebt, siebt genau die Zeilen weg, die eine gescheiterte Startphase erklaeren.
    //
    // DER ABLEGEWEG IST DERSELBE WIE ZUVOR, nur benannt: install_logger ruft store_logger, der
    // ResourceManager haelt den Start-Logger also MIT, damit sein Fach zu JEDEM Zeitpunkt
    // dasselbe sagt wie der Slot. Diese Funktion laeuft als erste Zeile von Kernel::build, also
    // VOR LogSystem::on_start — ohne das Mithalten waere das Fach hier leer, und ein
    // Idempotenz-Check ueber den Manager wuerde aus "schon installiert, nichts tun" ein ZWEITES
    // Installieren machen. Doppelte Senken sind doppelte Zeilen, also Flut.
    // Aufraeumweg: finalize_logger_after_boot ersetzt den Start-Logger, on_stop ruft clear_all().
    internal::install_logger("ase-server", sinks, kLogLevelTrace, kLogLevelTrace);
}

void finalize_logger_after_boot() {
    auto& res = internal::log_resources();
    auto& logger = internal::logger_slot();
    if (!logger) {
        // Kein Logger heisst: der Start ist nie so weit gekommen. Dann sind die geparkten Senken
        // Ballast und der Aufnahme-Ring haelt Zeilen, die nirgends mehr hingespielt werden koennen.
        res.clear_all();
        return;
    }

    // Attach all three real sinks to the logger in one go. Order matters
    // only for the replay below (capture_ring → all four); after the
    // ring is detached the order is irrelevant because every log call
    // fans out to every sink.
    //
    // take_* raeumt den Parkplatz beim Herausgeben: jede Senke wird genau einmal eingehaengt.
    internal::SinkSet real_sinks;
    real_sinks.add(res.take_file_sink());
    real_sinks.add(res.take_http_sink());
    real_sinks.add(res.take_counting_sink());
    // ANHAENGEN, nicht ersetzen: der Aufnahme-Ring muss noch haengen, waehrend die echten Senken
    // dazukommen — zwischen beiden Schritten darf keine Zeile auf den Boden fallen. Der leere
    // Zeiger einer nicht geparkten Senke wird von add() verworfen, die drei Tests von frueher
    // stehen deshalb in SinkSet::add statt hier dreimal nebeneinander.
    internal::attach_sinks(real_sinks);

    // Replay the capture-ring into exactly these three real sinks. Every
    // captured log message reaches every real sink — no channel is missing
    // any entry — so file, HTTP-ring and counting end up showing
    // 100% identical content for the boot phase and beyond.
    //
    // ZWEI GRIFFE STATT EINEM BLOCK, und die Reihenfolge traegt die Zusage: erst
    // zurueckspielen, dann abhaengen. Dazwischen haengt der Ring noch, also hat der Logger
    // durchgehend ein Ziel. Beide Rumpfe stehen beim Besitzer des Rings — der Vergleich ueber
    // die rohe Adresse ebenfalls, denn nur er haelt den Besitzzeiger.
    if (internal::replay_capture_ring(real_sinks) > 0) {
        logger->flush();
    }
    // Rueckgabewert bewusst verworfen: "es haengte kein Ring" ist hier kein Fehler, sondern der
    // zweite gueltige Zustand — eine Startphase ohne Aufnahme (Test, CLI-Werkzeug) kommt genauso
    // hier an wie eine mit.
    internal::detach_capture_ring();
}

// Parse --log <+/-token...> from argv and feed the 3-axis filter engine.
// Collects every consecutive token starting with '+' or '-' that follows
// the "--log" flag, joined by spaces, exactly like KernelCmdSystem did
// before. Standalone (no Registry / no ECS) so it can run before
// install_capture_logger — i.e. before any log::* call could fire.
void parse_cli_filter_from_argv(int argc, char* argv[]) {
    if (!argv) return;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (!a || a[0] != '-' || a[1] != '-' || std::strcmp(a + 2, "log") != 0) continue;
        char filter_buf[512] = {};
        uint32_t pos = 0;
        for (int j = i + 1; j < argc && argv[j] && (argv[j][0] == '+' || argv[j][0] == '-'); ++j) {
            // Grenzen 1 und sizeof-2, weil is_in_rng_u32 INKLUSIV prueft (ase-types/types.hpp,
            // "v >= min && v <= max") und die ersetzte Bedingung STRIKT war: `pos > 0` schliesst
            // 0 aus, `pos < sizeof-1` schliesst sizeof-1 aus. Fuer vorzeichenlose Werte ist
            // [1, sizeof-2] dieselbe Menge wie (0, sizeof-1) — eine blinde Substitution mit
            // [0, sizeof-1] haette dagegen zwei Werte zusaetzlich durchgelassen.
            if (types::is_in_rng_u32(pos, 1u, static_cast<uint32_t>(sizeof(filter_buf)) - 2u))
                filter_buf[pos++] = ' ';
            for (const char* p = argv[j]; *p && pos < sizeof(filter_buf) - 1; ++p)
                filter_buf[pos++] = *p;
        }
        filter_buf[pos] = '\0';
        ase::log::filter::parse_log_filter(filter_buf);
        return;
    }
}

}  // namespace ase::log
