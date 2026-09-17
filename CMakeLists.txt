cmake_minimum_required(VERSION 3.20)

#[[
ASE CORE BUILD CONFIGURATION

@file        CMakeLists.txt
@brief       Build configuration for ase-log
@description Logging - ECS-based logging system with spdlog backend.
             Provides log::info, log::warn, log::error macros.

@module      ase-log
@layer       1 (Core)
@created     2025-12-15
@modified    2026-09-08
@version     00.01.33.00083 [feat]

CMAKE MODULE COMPLIANCE

[ ] cmake_minimum_required(VERSION 3.20) as FIRST line
[ ] project() with VERSION and LANGUAGES CXX
[ ] message(STATUS) for configuration output
[ ] add_library() with STATIC and EXPLICIT file list
[ ] add_library(ase::{module} ALIAS ...) for namespace
[ ] target_include_directories() with PUBLIC and generator expressions
[ ] target_compile_features() with cxx_std_20
[ ] target_link_libraries() with PUBLIC/PRIVATE separation
[ ] include(GNUInstallDirs) for installation
[ ] install(DIRECTORY include/ ...) for headers
]]

# Parse local VERSION (SSOT) if not building from root
if(NOT DEFINED ASE_LOG_VERSION)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
        message(FATAL_ERROR "VERSION file not found: ${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
    endif()
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" _VERSION_LINES)
    foreach(_LINE ${_VERSION_LINES})
        if(_LINE MATCHES "^ASE_LOG=(.*)$")
            set(ASE_LOG_VERSION "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    if(NOT DEFINED ASE_LOG_VERSION)
        message(FATAL_ERROR "ASE_LOG version not found in VERSION file")
    endif()
endif()

project(ase-log VERSION ${ASE_LOG_VERSION} LANGUAGES CXX)

# Generate version.hpp from template (SSOT)
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/ase/log/version.hpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/ase/log/version.hpp"
    @ONLY
)

# ==============================================================================
# CODE GENERATION: Log Categories from Taxonomy
# ==============================================================================
# Regenerates log_cat_gen.hpp/.cpp only when taxonomy JSON files change.
# Uses a stamp file so Ninja can track the output and skip when up-to-date.

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# ASE_ROOT for standalone server builds, CMAKE_SOURCE_DIR for root build
if(DEFINED ASE_ROOT)
    set(_ASE_BASE "${ASE_ROOT}")
else()
    set(_ASE_BASE "${CMAKE_SOURCE_DIR}")
endif()
set(_TAXONOMY_DIR "${_ASE_BASE}/core/ase-validator/ecs_validator/data/taxonomy")
set(_STAMP_FILE "${CMAKE_CURRENT_BINARY_DIR}/ase-log-codegen.stamp")

add_custom_command(
    OUTPUT ${_STAMP_FILE}
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_log_cat.py"
    COMMAND ${CMAKE_COMMAND} -E touch ${_STAMP_FILE}
    WORKING_DIRECTORY ${_ASE_BASE}
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_log_cat.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/data/log_categories.json"
            "${_TAXONOMY_DIR}/_index.json"
            "${_TAXONOMY_DIR}/01_prp_property.json"
            "${_TAXONOMY_DIR}/02_sta_state.json"
            "${_TAXONOMY_DIR}/03_act_action.json"
            "${_TAXONOMY_DIR}/04_ent_entity.json"
            "${_TAXONOMY_DIR}/05_rel_relation.json"
            "${_TAXONOMY_DIR}/06_str_structure.json"
            "${_TAXONOMY_DIR}/07_prc_process.json"
            "${_TAXONOMY_DIR}/08_per_perception.json"
            "${_TAXONOMY_DIR}/09_com_communication.json"
            "${_TAXONOMY_DIR}/10_pst_persistence.json"
            "${_TAXONOMY_DIR}/11_bio_biology.json"
            "${_TAXONOMY_DIR}/12_eco_ecology.json"
            "${_TAXONOMY_DIR}/13_skl_skill.json"
            "${_TAXONOMY_DIR}/14_cmb_combat.json"
            "${_TAXONOMY_DIR}/15_soc_social.json"
            "${_TAXONOMY_DIR}/16_bdi_bdi.json"
            "${_TAXONOMY_DIR}/17_nav_navigation.json"
            "${_TAXONOMY_DIR}/18_ter_terrain.json"
            "${_TAXONOMY_DIR}/19_cel_celestial.json"
            "${_TAXONOMY_DIR}/20_atm_atmosphere.json"
            "${_TAXONOMY_DIR}/21_tim_time.json"
            "${_TAXONOMY_DIR}/22_cal_calendar.json"
            "${_TAXONOMY_DIR}/23_eph_ephemeris.json"
            "${_TAXONOMY_DIR}/24_gis_gis.json"
            "${_TAXONOMY_DIR}/25_sig_signature.json"
            "${_TAXONOMY_DIR}/26_rec_recognition.json"
            "${_TAXONOMY_DIR}/27_hub_hub.json"
            "${_TAXONOMY_DIR}/28_net_network.json"
            "${_TAXONOMY_DIR}/29_ecs_ecs.json"
            "${_TAXONOMY_DIR}/30_ren_render.json"
            "${_TAXONOMY_DIR}/31_inp_input.json"
            "${_TAXONOMY_DIR}/32_aud_audio.json"
            "${_TAXONOMY_DIR}/33_mth_math.json"
            "${_TAXONOMY_DIR}/34_cfg_config.json"
            "${_TAXONOMY_DIR}/35_err_error.json"
            "${_TAXONOMY_DIR}/36_ant_antares.json"
            "${_ASE_BASE}/modules/ase-hub/data/hub_constants.json"
            "${_ASE_BASE}/modules/ase-hub/data/hub_metrics.json"
            "${_ASE_BASE}/modules/ase-hub/data/hub_tags.json"
    COMMENT "Checking log categories (regenerates only if taxonomy or hub JSON changed)..."
)

add_custom_target(ase-log-codegen DEPENDS ${_STAMP_FILE})

# ==============================================================================
# BUILD CONFIGURATION
# ==============================================================================

message(STATUS "Configuring ${PROJECT_NAME} v${PROJECT_VERSION}")

add_library(${PROJECT_NAME} STATIC
    #[[
    CORE
    Logging system implementation.
    ]]
    src/log_sys.cpp
    src/log_files.cpp
    src/log_capture.cpp
    src/log_resource_manager.cpp
    src/log_query.cpp
    #[[
    Zwei weitere Auszuege aus log_sys.cpp, 2026-08-30. Der Abnehmer trennt sie, nicht die Laenge:
    log_boot laeuft VOR jedem System und sieht nie eine Registry, log_standalone baut denselben
    Logger fuer Programme OHNE Kernel (ase-edge-daemon, ase-cli).

    HIER STANDEN BIS 2026-08-31 src/log_sinks.cpp UND src/log_format.cpp, mit derselben
    Begruendung: sie trugen die Bauformen der Senken und der Formatierer, die on_start und beide
    Standalone-Einstiege brauchen. Der Abnehmer-Grund stimmte, der ORT nicht. Beide Dateien gaben
    den spdlog-Typ in ihrer OEFFENTLICHEN Signatur zurueck und exportierten die Bibliothek damit
    aus dem Wrapper heraus, statt sie zu kapseln — und die namentliche Ausnahme von
    SPDLOG_DIRECT_FORBIDDEN nennt fuenf Dateien, von denen keine so hiess. Die sieben Fabriken
    stehen jetzt in src/log_resource_manager.cpp, bei dem, der die Senken ohnehin haelt und
    einhaengt. Mit ihnen entfiel include/ase/log/internal/log_pattern_flags.hpp: seine drei
    Musterflaggen hatten baumweit genau EINEN Verbraucher und liegen jetzt als
    Implementierungsdetail in dessen anonymem Namensraum.
    ]]
    src/log_boot.cpp
    src/log_standalone.cpp
    src/log_cat_gen.cpp
    src/log_err_cat_gen.cpp
    src/log_display.cpp
    src/log_display_gen.cpp

    #[[
    HEADERS
    Public header files for the logging module.
    ]]
    include/ase/log/colors.hpp
    include/ase/log/log.hpp

    #[[
    DIE ROTATIONS-SSOT, seit 2026-08-31 aus log.hpp herausgeschnitten.
    Der Schnitt folgt dem EINSATZZWECK und ist an seinen Verbraucherkreisen gemessen:
    log.hpp wird von 2224 Uebersetzungseinheiten eingebunden, die Quota-Symbole von SECHS
    Dateien (internal/log_files.hpp, src/log_files.cpp, src/log_sys.cpp, log.hpp selbst sowie
    tools/ase-edge-daemon/.../backend_paths.cpp und tools/ase-cli/src/logs/logs_cmd.cpp).
    Eine geaenderte Rotationsschranke uebersetzte damit 2224 Einheiten neu, obwohl sechs sie
    lesen — nicht die Zeilenzahl war der Anlass, sondern diese Kopplung.
    ]]
    include/ase/log/log_quota.hpp

    #[[
    DER LEBENSZYKLUS, am 2026-08-31 im zweiten Schnitt aus log.hpp herausgeloest — nach
    demselben Massstab wie die Quota darueber: EINSATZZWECK, gemessen am Verbraucherkreis.
    Hier standen die vier Einstiege fuer Programme OHNE ECS-Welt (init, init_server_standalone,
    init_tui_standalone, install_capture_logger), die Capture-Klammer um Boot- und
    Abschalt-Tabelle, der Filter-Einstieg aus argv, finalize_logger_after_boot und shutdown.
    Gerufen werden sie von 22 Dateien ausserhalb dieses Moduls — init aus 14, shutdown aus 10,
    die Capture-Klammer aus 3 —, waehrend log.hpp in 2224 Uebersetzungseinheiten steht.

    ZWEI DINGE MACHEN DEN SCHNITT WIRKSAM STATT KOSMETISCH, und beide sind Absicht:
    log.hpp bekommt KEINEN Durchreicher auf diesen Header (sonst haengt der ganze Baum weiter
    am Lebenszyklus), und dieser Header nennt keinen Typ der Logbibliothek mehr — `init` und
    `shutdown` waren `inline` und griffen dafuer direkt auf sie zu; ihre Rumpfe stehen jetzt
    bei ihren Geschwistern in src/log_standalone.cpp, dessen Kopf genau diesen Zweck traegt.

    KEINE NEUE LINKKANTE, gemessen: internal::logger_slot() ist in log.hpp nur deklariert und
    ausserhalb definiert. Wer log::info ruft, ruft sie — jeder Aufrufer linkt also ohnehin
    gegen diese Bibliothek, und eine nicht-inline init verlangt nichts, was vorher fehlte.
    ]]
    include/ase/log/log_lifecycle.hpp

    #[[
    DIE CLIENT-WEITERLEITUNG, am 2026-08-31 im dritten Schnitt aus log.hpp geloest. Sie schreibt
    in den ZWEITEN Logger des Moduls und bedient damit Zeilen, die ueber RTC aus einem Browser
    hereinkommen — ein anderer Zweck als der Serveraufruf, der log.hpp traegt.

    GEMESSEN: die client_*-Familie hat NULL Aufrufer im ganzen Baum, mit und ohne log::-Praefix,
    unabhaengig nachgemessen zu einem gleichlautenden Befund vom 2026-08-22. Positivkontrolle
    derselben Sondenform: `info` in 1022 Dateien, `warn` in 2365. SIE BLEIBT VOLLSTAENDIG — der
    zweite Logger existiert und wird gepflegt, es fehlt der Aufrufer, nicht die Sache. Aus einem
    Header, den 2686 Dateien fuer den gewoehnlichen Aufruf einbinden, gehoert sie dennoch heraus.

    Die Datei bindet log.hpp EIN, nie umgekehrt: ein Durchreicher dort haette den ganzen Baum
    wieder an diese Familie gekoppelt. Sie nennt keinen Typ der Logbibliothek.
    ]]
    include/ase/log/log_client.hpp

    include/ase/log/log_cat_gen.hpp
    include/ase/log/log_err_cat_gen.hpp
    include/ase/log/log_display.hpp
    include/ase/log/log_filter.hpp
    include/ase/log/internal/log_files.hpp
    include/ase/log/log_module.hpp
)
add_library(ase::log ALIAS ${PROJECT_NAME})

# ------------------------------------------------------------------
# ase::log::headers — headers-only facade for dlopen'd .module/.plugin
# ------------------------------------------------------------------
# L0/L1 ase-log stays STATIC (kernel dlopen migration: L1 compile-time).
# Main server binary links ase-log STATIC and exports its symbols via
# ENABLE_EXPORTS. dlopen'd .module / .plugin files must NOT embed their
# own copy of ase-log — otherwise each module gets its own g_logger_,
# g_level_mask, CategoryMask etc. Log calls from module code would hit
# a null logger copy and be silently dropped; CLI filter state would
# diverge per module; entt type ids would mismatch across boundaries.
#
# Every .module / .plugin CMakeLists uses this INTERFACE target in
# place of ase::log:
#     target_link_libraries(ase_hub PUBLIC ase::log::headers ...)
# The module gets the ase/log/*.hpp include paths + cxx20 requirement
# but NO static archive linkage. Unresolved ase::log::* symbols in the
# .module are satisfied at dlopen time (RTLD_GLOBAL) from the main
# server binary.
add_library(ase-log-headers INTERFACE)
add_library(ase::log::headers ALIAS ase-log-headers)
target_include_directories(ase-log-headers
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(ase-log-headers INTERFACE cxx_std_20)
# ase/log/log.hpp derives LogSystem from ecs::System — consumers need
# ecs headers too. Re-exported through this facade. ase/log/log.hpp also
# includes ase/containers/vector.hpp for the recent_logs() return type,
# so the header-only containers include path is propagated too.
target_link_libraries(ase-log-headers INTERFACE ase::ecs ase::containers)

# Ensure codegen runs before compiling (checks taxonomy, regenerates if needed)
add_dependencies(${PROJECT_NAME} ase-log-codegen)

# Track module definition header - forces rebuild when module registration changes
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ase/log/log_module.hpp
    PROPERTIES HEADER_FILE_ONLY TRUE
)
target_sources(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ase/log/log_module.hpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)

# ASE project root for SSOT log directory (logs/ always at ASE root, not subgit root)
target_compile_definitions(${PROJECT_NAME} PRIVATE ASE_PROJECT_ROOT="${_ASE_BASE}")

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        ase::ecs         # Layer 1 - System base class
        ase::containers  # Layer 0 - Vector (LogEntry buffer in log.hpp public API)
        spdlog::spdlog   # External - Logging backend
    PRIVATE
        libcuckoo        # O(1) concurrent hashmap for log filter engine
        ase::fileio      # Layer 0 - quota.conf read/write (implementation only, see below)
        ase::utils       # Layer 0 - strops (bounded char[] building), see the second block below
)
#[[ VERBOTEN: No Layer 3 dependencies! ]]
#[[
ase::fileio ADDED 2026-08-20 — a DOWNWARD binding (L0 from L1), and it became possible only
today. STD_FSTREAM_FORBIDDEN used to say "ase-persist is an empty stub module - do not switch
to it" and named the gap: ase-fileio could read and not write. The rule text now reads "Use
ase::fileio from foundation", and text_writer.hpp::write_text exists. log_files.cpp reads and
writes logs/quota.conf; those were the last two std::ifstream/std::ofstream in this module.
Precedent for the binding: tools/ase-codegen, core/ase-convert, clients/ase-client-explorer.
Der erste Pfad stand bis 2026-09-17 als core/ase-codegen da und loeste nicht auf; die anderen
zwei taten es, und genau deshalb las sich die Liste vertrauenswuerdig.
]]
#[[
ase::utils ADDED 2026-08-20, AND IT WAS MISSING WHILE ALREADY BEING USED.

log_sys.cpp includes <ase/utils/strops.hpp> and calls utils::str_copy / str_append; this
target listed neither ase::utils nor any path to it. It compiled anyway, and the reason is
worth writing down because it is not local to this module:

    MEASURED with `c++ -H` on log_sys.cpp's own compile_commands entry:
        . /usr/include/ase/utils/strops.hpp
    /usr/include/ase/utils/strops.hpp   2026-06-01 19:45   3619 bytes
    foundation/ase-utils/.../strops.hpp 2026-08-20 10:10   5717 bytes

An INSTALLED copy of ase-utils sits in /usr/include (27 ase headers in total) and satisfies
the include for any translation unit that does not carry an explicit -I to the source tree.
It is eleven weeks old. Code compiled that way builds against a header nobody in this repo
is reading — str_append_u64, added on 2026-08-20, does not exist in it, which is how the
shadowing surfaced at all.

Binding ase::utils here puts foundation/ase-utils/include ahead of the system path for this
target, so the tree's own header is the one that gets used.
]]

include(GNUInstallDirs)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

if(ASE_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()
