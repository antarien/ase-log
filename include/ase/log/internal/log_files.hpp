#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        log_files.hpp
 * @brief       Wo die Logdateien liegen und wie gross sie werden duerfen
 * @description Zwei Helfer, die log_sys.cpp braucht und die nicht zur oeffentlichen Log-API
 *              gehoeren: die Projektwurzel und die eine Bauform der Datei-Senke. Die
 *              oeffentliche Quota-API (log_dir_path, log_quota, set_log_quota) steht weiterhin
 *              in log.hpp — sie ist fuer Aufrufer da, diese beiden hier nicht.
 *
 *              WARUM ES DIESE DATEI GIBT (2026-08-20): log_sys.cpp trug 682 Zeilen und zwei
 *              Aufgaben — das Logging-System UND die Verwaltung seiner Dateien. Der
 *              God-System-Deckel hat das gemeldet, und er meldet keine Laenge, sondern eine
 *              fehlende Trennung. Die Naht wurde gemessen, nicht geschaetzt: der verschobene
 *              Block ruft NICHTS aus dem Rest der Datei (kein g_*, kein LogSystem::), und aus
 *              dem Rest kreuzen genau zwei Namen die Naht — die beiden hier.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    ecs/module
 * @created     2026-08-20
 * @modified    2026-08-20
 * @version     1.0.0
 *
 * CORE INFRASTRUCTURE COMPLIANCE
 *
 * [ ] NOT an ECS Component or System
 * [ ] Layer dependencies correct (L0: no ASE deps, L1: L0 only)
 * [ ] Namespace matches module: ase::{module}
 * [ ] Include guards via #pragma once
 * [ ] Header-only OR header+cpp pattern (not mixed)
 * [ ] All public functions documented with @brief, @param, @return
 * [ ] No global mutable state (constexpr/const only)
 * [ ] No singletons or static mutable variables
 * [ ] Thread-safe by design (pure functions or explicit mutex)
 * [ ] noexcept where possible (no-throw guarantee)
 * [ ] constexpr where possible (compile-time evaluation)
 * [ ] [[nodiscard]] on functions returning values
 * [ ] No implicit conversions (use explicit constructors)
 * [ ] No macros (except include guards) - use constexpr/templates
 * [ ] No magic numbers (use named constants)
 * [ ] No circular dependencies
 * [ ] API stable (changes require version bump)
 *
 * WARUM HIER NOCH spdlog IM HEADER STEHT (2026-08-20, nachgezogen 16:52)
 *
 *   Hier standen zwei filesystem-Befunde und ein spdlog-Befund, und der Text nannte beide
 *   gleichermassen unvermeidbar: "beide sind die Signatur selbst". Fuer spdlog stimmt das
 *   weiterhin — ein Header, der die Bauform der Datei-Senke bereitstellt, kann den Senkentyp
 *   nicht verschweigen.
 *
 *   Fuer filesystem stimmte es NICHT, und das war eine Behauptung, die sich als Begruendung
 *   getarnt hat. get_project_root() gab einen std::filesystem::path zurueck, aber alle drei
 *   Aufrufer haengten sofort ein Segment an und riefen .string() — der Pfadtyp trug nichts als
 *   einen '/', den ein std::string genauso traegt. Die Signatur liefert jetzt std::string, und
 *   damit ist <filesystem> aus diesem Header verschwunden statt erklaert worden zu sein.
 */

// LogQuota und die Rotationsschranken. Dieser Header nannte sie bisher, ohne sie einzubinden —
// er bekam sie ueber die Datei, die ihn einband. Ein Header, der einen Typ benutzt, holt ihn
// selbst; sonst haengt seine Uebersetzbarkeit an der Reihenfolge beim Einbinder.
#include <ase/log/log_quota.hpp>

#include <cstdint>
#include <string>

namespace ase::log::internal {

/**
 * Projektwurzel, unter der `logs/` liegt.
 *
 * SSOT ist das ASE_PROJECT_ROOT-Define aus CMake (_ASE_BASE); es loest sowohl fuer den zentralen
 * Bau als auch fuer einen eigenstaendigen Subgit-Bau richtig auf. Ohne das Define bleibt das
 * Arbeitsverzeichnis als Rueckfallebene.
 */
[[nodiscard]] std::string get_project_root();

/**
 * Bereitet das Verzeichnis einer Logdatei vor und liefert die dort geltenden Rotationsgrenzen.
 *
 * Raeumt alte Generationen weg (Sweep) und liest die Quota aus dem Verzeichnis der Datei SELBST,
 * nicht aus einem Pfad zur Uebersetzungszeit: der Edge-Daemon laeuft als vorgebautes Binary auf
 * Maschinen, auf denen es den Bau-Baum nicht gibt, und muss seine eigene Quota finden (oder
 * keine, und die Vorgaben nehmen).
 *
 * GETRENNT VON DER SENKE 2026-08-22 — hier stand `make_rotating_file_sink`, das Politik UND
 * Konstruktion in einer Funktion trug. Die Begruendung steht einmal in log_files.cpp; sichtbare
 * Folge ist, dass dieser Header keinen fremden Bibliothekstyp mehr nennt und mit <cstdint> und
 * <string> auskommt. Ausgabeparameter statt Rueckgabetyp, damit er auch den Quota-Typ nicht
 * braucht.
 *
 * @param path            Pfad der Logdatei, deren Verzeichnis vorbereitet wird
 * @param out_max_bytes   Groesse, bei der die aktive Datei rotiert
 * @param out_max_files   Zahl der behaltenen Generationen neben der aktiven Datei
 */
void prepare_log_sink(const std::string& path, uint64_t& out_max_bytes, uint32_t& out_max_files);

/**
 * Der absolute Pfad der Logdatei, mit angelegtem Elternverzeichnis.
 *
 * Ein RELATIVER Name wird gegen die Projektwurzel aufgeloest, nicht gegen das Arbeitsverzeichnis:
 * ein Tier, der aus einem anderen Verzeichnis gestartet wird, schreibt sonst sein Log woanders hin
 * als der Tier daneben, und die Konsole tailt eine Datei, die nie gefuellt wird. Ein ABSOLUTER
 * Name bleibt unveraendert — der Edge-Daemon bekommt seinen Pfad vom Aufrufer.
 *
 * WARUM DIESE FUNKTION HIER ENTSTANDEN IST (2026-08-20): derselbe Block stand DREIMAL woertlich
 * in log_sys.cpp (on_start, init_server_standalone, init_tui_standalone). Drei Kopien sind nicht
 * dreimal dieselbe Regel, sondern drei Stellen, an denen sie auseinanderlaufen kann — und sie
 * trugen neun der zehn filesystem-Befunde der Datei. Eine Kopie an dem Ort, der fuer Logdateien
 * zustaendig ist, ist die Aufloesung; drei Kopien im System selbst waren der Befund.
 */
[[nodiscard]] std::string resolve_log_path(const std::string& log_file);

}  // namespace ase::log::internal
