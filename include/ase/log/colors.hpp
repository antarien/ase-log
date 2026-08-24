#pragma once

/**
 * ASE CORE INFRASTRUCTURE HEADER
 *
 * @file        colors.hpp
 * @brief       SSOT for the terminal colour of every module and plugin
 * @description One 256-colour ANSI code per unit, grouped by layer, plus the
 *              lookup that turns a module name into its code. Everything that
 *              prints a module name reads it from here, so a unit has the same
 *              colour in the boot sequence, in a log line and in any tool that
 *              renders the tree.
 *
 *              Colours use the 256-colour palette for consistent terminal
 *              rendering: \x1b[38;5;XXXm where XXX is 0-255.
 *
 *              Used by: the boot sequence (app.cpp), the log systems, and any
 *              visualisation that needs to identify a module.
 *
 * @module      ase-log
 * @layer       1 (Core)
 * @category    ecs/module
 * @created     2026-01-09
 * @modified    2026-08-20
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

#include <string>

namespace ase::log {

// Module color codes (256-color palette)
namespace module_colors {

// =============================================================================
// Layer 0: Foundation (Dark Blues/Slate)
// =============================================================================
constexpr int ASE_ALLOC      = 59;   // Dark slate
constexpr int ASE_CONTAINERS = 60;   // Slate
constexpr int ASE_JSON       = 61;   // Slate blue
constexpr int ASE_MATH       = 62;   // Slate blue
constexpr int ASE_PLATFORM   = 63;   // Light slate blue
constexpr int ASE_TEST       = 64;   // Olive
constexpr int ASE_TYPES      = 65;   // Dark olive
constexpr int ASE_UTILS      = 66;   // Olive green

// =============================================================================
// Layer 1: Core (Blues/Cyans/Purples)
// =============================================================================
constexpr int ASE_ASYNC      = 31;   // Teal
constexpr int ASE_CODEGEN    = 32;   // Dark cyan
constexpr int ASE_CONFIG     = 33;   // Deep sky blue
constexpr int ASE_CONVERT    = 68;   // Steel blue
constexpr int ASE_ECS        = 39;   // Deep sky blue bright
constexpr int ASE_EVENTS     = 38;   // Sky cyan
constexpr int ASE_LOG        = 37;   // Cyan
constexpr int ASE_MONGODB    = 97;   // Light purple (MongoDB)
constexpr int ASE_NEO4J      = 98;   // Light purple (Neo4j)
constexpr int ASE_REFLECT    = 99;   // Light slate blue
constexpr int ASE_SERIAL     = 67;   // Steel blue

// =============================================================================
// Layer 2: Kernel (Bright Cyan/Turquoise)
// =============================================================================
constexpr int ASE_KERNEL     = 45;   // Turquoise

// =============================================================================
// Layer 3: Modules
// =============================================================================

// --- World/Environment (Greens) ---
constexpr int ASE_TERRAIN    = 34;   // Sea green
constexpr int ASE_WORLD      = 35;   // Aquamarine
constexpr int ASE_GIS        = 36;   // Dark cyan
constexpr int ASE_SPATIAL    = 42;   // Spring green

// --- Time/Celestial (Light Blues) ---
constexpr int ASE_TIME       = 75;   // Light steel blue
constexpr int ASE_CALENDAR   = 111;  // Sky blue
constexpr int ASE_CELESTIAL  = 117;  // Light cyan
constexpr int ASE_EPHEMERIS  = 123;  // Pale cyan
constexpr int ASE_ATMOSPHERE = 159;  // Very light blue

// --- Network/Replication (Oranges) ---
constexpr int ASE_NETWORK    = 208;  // Dark orange
constexpr int ASE_REPLICATION= 214;  // Orange
constexpr int ASE_PERSIST    = 215;  // Sandy orange

// --- Player/Input/Camera (Yellows/Golds) ---
constexpr int ASE_PLAYER     = 220;  // Gold
constexpr int ASE_INPUT      = 221;  // Light gold
constexpr int ASE_CAMERA     = 222;  // Light yellow

// --- Combat/Damage (Reds) ---
constexpr int ASE_COMBAT     = 160;  // Red
constexpr int ASE_WOUNDING   = 167;  // Dark red/brown

// --- AI/Behavior (Magentas/Pinks) ---
constexpr int ASE_BDI        = 163;  // Medium violet
constexpr int ASE_PERCEPTION = 169;  // Hot pink
constexpr int ASE_OBSERVATION= 170;  // Light pink
constexpr int ASE_RECOGNITION= 171;  // Pale pink
constexpr int ASE_PATHFINDING= 133;  // Medium orchid

// --- Biology/Life (Yellow-Greens/Chartreuse) ---
constexpr int ASE_LIFECYCLE  = 106;  // Yellow-green
constexpr int ASE_METABOLISM = 107;  // Chartreuse
constexpr int ASE_GENETICS   = 108;  // Dark sea green
constexpr int ASE_EVOLUTION  = 109;  // Medium aquamarine
constexpr int ASE_HERITAGE   = 114;  // Pale green
constexpr int ASE_FOODCHAIN  = 142;  // Dark khaki
constexpr int ASE_ABIOGENESIS= 149;  // Light green

// --- Social/Skills (Orchids/Plums) ---
constexpr int ASE_SOCIAL     = 176;  // Plum
constexpr int ASE_SKILL      = 177;  // Light plum

// --- Physics/Entity (Blue-Grays) ---
constexpr int ASE_PHYSICS    = 103;  // Light slate gray
constexpr int ASE_ENTITY     = 104;  // Slate gray

// --- Signatures/Entropy (Purples) ---
constexpr int ASE_SIGNATURE  = 135;  // Medium purple
constexpr int ASE_ENTROPY    = 141;  // Light purple

// --- Hub (Central - Gold/Bronze) ---
constexpr int ASE_HUB        = 178;  // Gold/Bronze

// --- Other Modules ---
constexpr int ASE_TRAVEL     = 180;  // Tan
constexpr int ASE_BBOL       = 181;  // Light tan
constexpr int ASE_SCRIPTING  = 182;  // Thistle
constexpr int ASE_SDK        = 183;  // Light thistle
constexpr int ASE_RENDER_DATA= 146;  // Light yellow-green

// =============================================================================
// Layer 4: Plugins (Distinctive colors)
// =============================================================================
constexpr int ASE_PL_SKY        = 51;   // Cyan (sky!)
constexpr int ASE_PL_WEATHER    = 195;  // Light cyan
constexpr int ASE_PL_WATER      = 39;   // Deep sky blue
constexpr int ASE_PL_EROSION    = 137;  // Dark khaki
constexpr int ASE_PL_FIRE       = 202;  // Orange red
constexpr int ASE_PL_FLORA      = 70;   // Medium sea green
constexpr int ASE_PL_FAUNA      = 143;  // Dark khaki
constexpr int ASE_PL_PREDATOR   = 196;  // Red (danger!)
constexpr int ASE_PL_VEGETATION = 71;   // Dark sea green
constexpr int ASE_PL_PROCEDURAL = 139;  // Rosy brown
constexpr int ASE_PL_ML_TERRAIN = 140;  // Light rosy brown
constexpr int ASE_PL_SURVIVAL   = 173;  // Light salmon
constexpr int ASE_PL_BUILDING   = 179;  // Wheat
constexpr int ASE_PL_CRAFTING   = 186;  // Khaki
constexpr int ASE_PL_DEBUG      = 243;  // Gray
constexpr int ASE_PL_AUTH       = 99;   // Light slate blue
constexpr int ASE_PL_CHAT       = 219;  // Pink
constexpr int ASE_PL_VOICE      = 218;  // Light pink
constexpr int ASE_PL_REDIS      = 124;  // Red (Redis!)
constexpr int ASE_PL_REPLAY     = 147;  // Light steel blue
constexpr int ASE_PL_WEBSERVER  = 69;   // Cornflower blue
constexpr int ASE_PL_PROMETHEUS = 166;  // Orange (Prometheus)

// =============================================================================
// Layer 5: Servers (Whites/Brights)
// =============================================================================
constexpr int ASE_SERVER_GAME   = 255;  // White
constexpr int ASE_SERVER_MASTER = 254;  // Almost white
constexpr int ASE_SERVER_TOOLS  = 253;  // Light gray

// =============================================================================
// Shared Libraries (Teals)
// =============================================================================
constexpr int ASE_API_TYPES  = 30;   // Dark teal
constexpr int ASE_PROTOCOL   = 31;   // Teal

// =============================================================================
// Clients (Bright colors)
// =============================================================================
constexpr int ASE_CLIENT_WEB     = 201;  // Magenta/Pink
constexpr int ASE_CLIENT_DESKTOP = 200;  // Magenta
constexpr int ASE_CLIENT_MOBILE  = 199;  // Pink
constexpr int ASE_CLIENT_SHARED  = 198;  // Light magenta

// =============================================================================
// Docs
// =============================================================================
constexpr int ASE_DOCS       = 248;  // Light gray

// Default for unknown modules
constexpr int UNKNOWN        = 250;  // Light gray

}  // namespace module_colors

/**
 * Get ANSI color code for a module name
 * @param module_name_cstr The module name (e.g., "ase-terrain", "ase-pl-sky")
 * @return 256-color palette code
 *
 * Takes a C string, not a std::string_view: the type is on the forbidden list
 * for core headers. The name is copied into a std::string once, so the ninety
 * comparisons below keep comparing VALUES - with a bare const char* every one
 * of them would silently become a pointer comparison and always be false.
 * One allocation per call, on a path that runs at boot and shutdown.
 */
inline int get_module_color_code(const char* module_name_cstr) {
    using namespace module_colors;

    std::string module_name(module_name_cstr != nullptr ? module_name_cstr : "");

    // Strip "ase-" prefix for comparison if present
    if (module_name.starts_with("ase-")) {
        module_name = module_name.substr(4);
    }

    // ==========================================================================
    // Layer 0: Foundation
    // ==========================================================================
    if (module_name == "alloc")       return ASE_ALLOC;
    if (module_name == "containers")  return ASE_CONTAINERS;
    if (module_name == "json")        return ASE_JSON;
    if (module_name == "math")        return ASE_MATH;
    if (module_name == "platform")    return ASE_PLATFORM;
    if (module_name == "test")        return ASE_TEST;
    if (module_name == "types")       return ASE_TYPES;
    if (module_name == "utils")       return ASE_UTILS;

    // ==========================================================================
    // Layer 1: Core
    // ==========================================================================
    if (module_name == "async")       return ASE_ASYNC;
    if (module_name == "codegen")     return ASE_CODEGEN;
    if (module_name == "config")      return ASE_CONFIG;
    if (module_name == "convert")     return ASE_CONVERT;
    if (module_name == "ecs")         return ASE_ECS;
    if (module_name == "events")      return ASE_EVENTS;
    if (module_name == "log")         return ASE_LOG;
    if (module_name == "mongodb")     return ASE_MONGODB;
    if (module_name == "neo4j")       return ASE_NEO4J;
    if (module_name == "reflect")     return ASE_REFLECT;
    if (module_name == "serial")      return ASE_SERIAL;

    // ==========================================================================
    // Layer 2: Kernel
    // ==========================================================================
    if (module_name == "kernel")      return ASE_KERNEL;

    // ==========================================================================
    // Layer 3: Modules - World/Environment
    // ==========================================================================
    if (module_name == "terrain")     return ASE_TERRAIN;
    if (module_name == "world")       return ASE_WORLD;
    if (module_name == "gis")         return ASE_GIS;
    if (module_name == "spatial")     return ASE_SPATIAL;

    // Layer 3: Modules - Time/Celestial
    if (module_name == "time")        return ASE_TIME;
    if (module_name == "calendar")    return ASE_CALENDAR;
    if (module_name == "celestial")   return ASE_CELESTIAL;
    if (module_name == "ephemeris")   return ASE_EPHEMERIS;
    if (module_name == "atmosphere")  return ASE_ATMOSPHERE;

    // Layer 3: Modules - Network/Replication
    if (module_name == "network")     return ASE_NETWORK;
    if (module_name == "replication") return ASE_REPLICATION;
    if (module_name == "persist")     return ASE_PERSIST;

    // Layer 3: Modules - Player/Input/Camera
    if (module_name == "player")      return ASE_PLAYER;
    if (module_name == "input")       return ASE_INPUT;
    if (module_name == "camera")      return ASE_CAMERA;

    // Layer 3: Modules - Combat
    if (module_name == "combat")      return ASE_COMBAT;
    if (module_name == "wounding")    return ASE_WOUNDING;

    // Layer 3: Modules - AI/Behavior
    if (module_name == "bdi")         return ASE_BDI;
    if (module_name == "perception")  return ASE_PERCEPTION;
    if (module_name == "observation") return ASE_OBSERVATION;
    if (module_name == "recognition") return ASE_RECOGNITION;
    if (module_name == "pathfinding") return ASE_PATHFINDING;

    // Layer 3: Modules - Biology/Life
    if (module_name == "lifecycle")   return ASE_LIFECYCLE;
    if (module_name == "metabolism")  return ASE_METABOLISM;
    if (module_name == "genetics")    return ASE_GENETICS;
    if (module_name == "evolution")   return ASE_EVOLUTION;
    if (module_name == "heritage")    return ASE_HERITAGE;
    if (module_name == "foodchain")   return ASE_FOODCHAIN;
    if (module_name == "abiogenesis") return ASE_ABIOGENESIS;

    // Layer 3: Modules - Social/Skills
    if (module_name == "social")      return ASE_SOCIAL;
    if (module_name == "skill")       return ASE_SKILL;

    // Layer 3: Modules - Physics/Entity
    if (module_name == "physics")     return ASE_PHYSICS;
    if (module_name == "entity")      return ASE_ENTITY;

    // Layer 3: Modules - Signatures/Entropy
    if (module_name == "signature")   return ASE_SIGNATURE;
    if (module_name == "entropy")     return ASE_ENTROPY;

    // Layer 3: Modules - Hub
    if (module_name == "hub")         return ASE_HUB;

    // Layer 3: Modules - Other
    if (module_name == "travel")      return ASE_TRAVEL;
    if (module_name == "bbol")        return ASE_BBOL;
    if (module_name == "scripting")   return ASE_SCRIPTING;
    if (module_name == "sdk")         return ASE_SDK;
    if (module_name == "render-data") return ASE_RENDER_DATA;

    // ==========================================================================
    // Layer 4: Plugins
    // ==========================================================================
    if (module_name == "pl-sky")        return ASE_PL_SKY;
    if (module_name == "pl-weather")    return ASE_PL_WEATHER;
    if (module_name == "pl-water")      return ASE_PL_WATER;
    if (module_name == "pl-erosion")    return ASE_PL_EROSION;
    if (module_name == "pl-fire")       return ASE_PL_FIRE;
    if (module_name == "pl-flora")      return ASE_PL_FLORA;
    if (module_name == "pl-fauna")      return ASE_PL_FAUNA;
    if (module_name == "pl-predator")   return ASE_PL_PREDATOR;
    if (module_name == "pl-vegetation") return ASE_PL_VEGETATION;
    if (module_name == "pl-procedural") return ASE_PL_PROCEDURAL;
    if (module_name == "pl-ml-terrain") return ASE_PL_ML_TERRAIN;
    if (module_name == "pl-survival")   return ASE_PL_SURVIVAL;
    if (module_name == "pl-building")   return ASE_PL_BUILDING;
    if (module_name == "pl-crafting")   return ASE_PL_CRAFTING;
    if (module_name == "pl-debug")      return ASE_PL_DEBUG;
    if (module_name == "pl-auth")       return ASE_PL_AUTH;
    if (module_name == "pl-chat")       return ASE_PL_CHAT;
    if (module_name == "pl-voice")      return ASE_PL_VOICE;
    if (module_name == "pl-redis")      return ASE_PL_REDIS;
    if (module_name == "pl-replay")     return ASE_PL_REPLAY;
    if (module_name == "pl-webserver")  return ASE_PL_WEBSERVER;
    if (module_name == "pl-prometheus") return ASE_PL_PROMETHEUS;

    // ==========================================================================
    // Layer 5: Servers
    // ==========================================================================
    if (module_name == "server-game")   return ASE_SERVER_GAME;
    if (module_name == "server-master") return ASE_SERVER_MASTER;
    if (module_name == "server-tools")  return ASE_SERVER_TOOLS;

    // ==========================================================================
    // Shared Libraries
    // ==========================================================================
    if (module_name == "api-types")     return ASE_API_TYPES;
    if (module_name == "protocol")      return ASE_PROTOCOL;

    // ==========================================================================
    // Clients
    // ==========================================================================
    if (module_name == "client-web")     return ASE_CLIENT_WEB;
    if (module_name == "client-desktop") return ASE_CLIENT_DESKTOP;
    if (module_name == "client-mobile")  return ASE_CLIENT_MOBILE;
    if (module_name == "client-shared")  return ASE_CLIENT_SHARED;

    // ==========================================================================
    // Docs
    // ==========================================================================
    if (module_name == "docs")          return ASE_DOCS;

    return UNKNOWN;
}

/**
 * Format module name with ANSI color
 * @param module_name The module name (e.g., "ase-terrain")
 * @return Formatted string with color codes
 */
inline std::string format_module_colored(const char* module_name_cstr) {
    const char* module_name = (module_name_cstr != nullptr) ? module_name_cstr : "";
    int color = get_module_color_code(module_name);
    return "\x1b[38;5;" + std::to_string(color) + "m[" + std::string(module_name) + "]\x1b[0m";
}

}  // namespace ase::log
