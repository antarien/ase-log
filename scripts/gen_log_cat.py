#!/usr/bin/env python3
"""
Generate Log Categories Header + Implementation
================================================
Reads taxonomy JSON files + Hub JSON files and generates C++ header AND implementation.

Usage:
    python core/ase-log/scripts/gen_log_cat.py

Output:
    core/ase-log/include/ase/log/log_cat_gen.hpp  (~3KB - declarations only)
    core/ase-log/src/log_cat_gen.cpp              (~200KB - libcuckoo init table)

The header is small and safe to include everywhere.
The init table + libcuckoo engine is compiled ONCE in the .cpp.
"""

import json
import sys
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

# ---------------------------------------------------------------------------
# Console framework (SSOT: sha-client-web/sha-web-console)
# ---------------------------------------------------------------------------

SCRIPT_DIR = Path(__file__).parent.resolve()
ASE_LOG_DIR = SCRIPT_DIR.parent
PROJECT_ROOT = ASE_LOG_DIR.parent.parent

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "clients" / "sha-client-web" / "public" / "console" / "python"))
from console import (C, section_header, section_line, section_pipe, section_detail,
                      register_labels, tprint, c256, CHECK, CROSS, SKIP, HASH, ARROW, WARN)


# ---------------------------------------------------------------------------
# Hub JSON Loading
# ---------------------------------------------------------------------------

def load_hub_value_parts(hub_data_dir: Path) -> Tuple[Set[str], Set[str], Set[str],
                                                       int, int, int]:
    """Load 3 Hub JSON files, split IDs at '_', return unique parts per file.

    Returns:
        (const_parts, metric_parts, tag_parts,
         const_count, metric_count, tag_count)
    """
    def _load_one(filename: str) -> Tuple[Set[str], int]:
        path = hub_data_dir / filename
        if not path.exists():
            return set(), 0
        with open(path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        ids = [v['id'] for v in data.get('global_values', [])]
        parts: Set[str] = set()
        for vid in ids:
            for part in vid.split('_'):
                if len(part) >= 2:
                    parts.add(part.upper())
        return parts, len(ids)

    const_parts, const_count = _load_one("hub_constants.json")
    metric_parts, metric_count = _load_one("hub_metrics.json")
    tag_parts, tag_count = _load_one("hub_tags.json")

    return const_parts, metric_parts, tag_parts, const_count, metric_count, tag_count


# ---------------------------------------------------------------------------
# Taxonomy + Module Loading
# ---------------------------------------------------------------------------

def load_all_abbreviations(taxonomy_dir: Path,
                           project_root: Path) -> Tuple[Dict[str, int], List[Tuple[str, str]]]:
    """Load ALL abbreviations from taxonomy + Hub JSONs + module prefixes.

    Merges three token sources (DRY: one function, three sources):
    1. Taxonomy abbreviations (existing)
    2. Hub value ID parts (NEW — split at '_')
    3. Module directory prefixes (existing)

    Returns:
        abbrev_bits: Dict mapping uppercase abbreviation -> bit position
        name_aliases: List of (name, target_abbrev) tuples for aliases
    """
    abbrevs: Set[str] = set()
    name_aliases: List[Tuple[str, str]] = []

    # --- Source 1: Taxonomy ---

    # Build name -> abbrev mapping from _index.json (semantic categories)
    index_name_to_abbrev: Dict[str, str] = {}
    index_file = taxonomy_dir / "_index.json"
    if index_file.exists():
        with open(index_file, 'r', encoding='utf-8') as f:
            data = json.load(f)
            for cat in data.get('categories', []):
                abbrev = cat['abbrev'].upper()
                name = cat.get('name', '').upper()
                abbrevs.add(abbrev)
                if name and name != abbrev:
                    index_name_to_abbrev[name] = abbrev

    # Load from individual taxonomy files (ALL nested abbreviations)
    # Also build noun word -> abbreviation mapping for module prefix resolution
    noun_word_to_abbrev: Dict[str, str] = {}
    taxonomy_keys: Set[str] = set()
    for f in taxonomy_dir.glob("*.json"):
        if f.name.startswith('_'):
            continue
        try:
            with open(f, 'r', encoding='utf-8') as fp:
                data = json.load(fp)
                extract_abbrevs_recursive(data, abbrevs, noun_word_to_abbrev, taxonomy_keys)
        except Exception as e:
            print(f"Warning: Failed to parse {f}: {e}", file=sys.stderr)

    # Remove _index.json names that collide with taxonomy entries
    for name in index_name_to_abbrev:
        abbrevs.discard(name)

    # --- Source 1b: Lifecycle method tokens (on_start, tick, on_stop) ---

    lifecycle_tokens = {'TICK', 'START', 'STOP', 'PRESTART', 'CLEANUP', 'INIT'}
    abbrevs |= lifecycle_tokens

    # --- Source 2: Hub value ID parts ---

    hub_data_dir = project_root / "modules" / "ase-hub" / "data"
    if hub_data_dir.exists():
        const_parts, metric_parts, tag_parts, _, _, _ = load_hub_value_parts(hub_data_dir)
        # Merge all hub parts into abbrevs (dedup via set union)
        abbrevs |= const_parts
        abbrevs |= metric_parts
        abbrevs |= tag_parts

    # --- Noun-alias cleanup: Hub parts can re-add noun words after discard ---
    # e.g. "TIME" discarded at line 161, re-added via "GAME_TIME" hub split.
    # Must alias to abbreviation (BIT_TIM), not get own bit (BIT_TIME).

    for name in index_name_to_abbrev:
        if name in abbrevs and index_name_to_abbrev[name] in abbrevs:
            abbrevs.discard(name)

    alias_redirect: Dict[str, str] = {}
    for noun_word, noun_abbrev in noun_word_to_abbrev.items():
        if noun_word in abbrevs and noun_abbrev in abbrevs and noun_word != noun_abbrev:
            if noun_word in taxonomy_keys:
                # Taxonomy key keeps own bit; alias abbreviation -> key instead
                # e.g. BODY is taxonomy key, BODS is abbreviation -> BODS→BODY
                abbrevs.discard(noun_abbrev)
                name_aliases.append((noun_abbrev, noun_word))
                alias_redirect[noun_abbrev] = noun_word
            else:
                # Normal case: noun word gets aliased to abbreviation
                # e.g. TIME→TIM, DOCK→DCKS
                abbrevs.discard(noun_word)
                name_aliases.append((noun_word, noun_abbrev))
                alias_redirect[noun_word] = noun_abbrev

    # Redirect entries that target removed abbreviations
    # e.g. CAPACITY→CAP, but CAP removed → redirect to CAP's target (CAPM)
    if alias_redirect:
        for key in noun_word_to_abbrev:
            target = noun_word_to_abbrev[key]
            if target in alias_redirect:
                noun_word_to_abbrev[key] = alias_redirect[target]
        for key in index_name_to_abbrev:
            target = index_name_to_abbrev[key]
            if target in alias_redirect:
                index_name_to_abbrev[key] = alias_redirect[target]
        for i, (name, target) in enumerate(name_aliases):
            if target in alias_redirect:
                name_aliases[i] = (name, alias_redirect[target])

    # --- Source 3: Module prefixes ---

    module_prefixes = _scan_module_prefixes(project_root)

    for prefix in module_prefixes:
        prefix_upper = prefix.upper()

        # Matches an _index.json category name? (e.g., "ephemeris" -> "eph")
        if prefix_upper in index_name_to_abbrev:
            name_aliases.append((prefix_upper, index_name_to_abbrev[prefix_upper]))
            continue

        # Already a taxonomy/hub entry? (e.g., "hub", "bdi", "gis", "sdk")
        if prefix_upper in abbrevs:
            continue

        # Matches a taxonomy noun word? (e.g., "erosion" -> "eros", "camera" -> "cam")
        if prefix_upper in noun_word_to_abbrev:
            name_aliases.append((prefix_upper, noun_word_to_abbrev[prefix_upper]))
            continue

        # No match - add as standalone module category (infra: mongodb, redis, etc.)
        abbrevs.add(prefix_upper)

    # Also add _index.json name aliases that are NOT module prefixes
    module_prefix_set = {p.upper() for p in module_prefixes}
    for name, abbrev in index_name_to_abbrev.items():
        if name not in module_prefix_set:
            name_aliases.append((name, abbrev))

    # Sort and assign bit positions
    sorted_abbrevs = sorted(abbrevs)
    return {abbrev: i for i, abbrev in enumerate(sorted_abbrevs)}, name_aliases


def _scan_module_prefixes(project_root: Path) -> List[str]:
    """Scan ALL ase-* and ase-pl-* directories in the entire project tree.

    Returns list of filename prefixes (directory name without ase- or ase-pl- prefix).
    For hyphenated names, also adds individual parts.
    """
    prefixes: Set[str] = set()

    for search_dir in [project_root / "modules",
                       project_root / "plugins",
                       project_root / "core",
                       project_root / "foundation",
                       project_root / "servers",
                       project_root / "clients",
                       project_root / "tests"]:
        if not search_dir.exists():
            continue
        for d in sorted(search_dir.iterdir()):
            if not d.is_dir():
                continue
            name = d.name
            if name.startswith("ase-pl-"):
                prefix = name[7:]  # Strip "ase-pl-"
            elif name.startswith("ase-"):
                prefix = name[4:]  # Strip "ase-"
            else:
                continue

            prefixes.add(prefix)
            # For hyphenated names like "render-data", also add individual parts
            if '-' in prefix:
                for part in prefix.split('-'):
                    if len(part) >= 2:
                        prefixes.add(part)

    return sorted(prefixes)


def extract_abbrevs_recursive(node: dict, abbrevs: Set[str],
                              noun_word_to_abbrev: Optional[Dict[str, str]] = None,
                              taxonomy_keys: Optional[Set[str]] = None) -> None:
    """Recursively extract ALL abbreviations from taxonomy node.

    Also builds noun_word_to_abbrev mapping (full word -> abbreviation)
    so module prefixes like "EROSION" can resolve to "EROS".
    taxonomy_keys collects structural abbreviation keys (2-4 char node keys).
    """
    if not isinstance(node, dict):
        return

    for key, value in node.items():
        # Skip metadata
        if key in ('name', 'description', '_schema_version') and isinstance(value, str):
            continue

        # Extract from 'nouns' dict - ALL of them
        if key == 'nouns' and isinstance(value, dict):
            for noun_word, noun_abbrev in value.items():
                if len(noun_abbrev) >= 2:
                    abbrevs.add(noun_abbrev.upper())
                # Map full word -> abbreviation (prefer non-self mappings)
                if noun_word_to_abbrev is not None and len(noun_word) >= 2:
                    word_upper = noun_word.upper()
                    abbrev_upper = noun_abbrev.upper()
                    existing = noun_word_to_abbrev.get(word_upper)
                    if existing is None or (existing == word_upper and abbrev_upper != word_upper):
                        noun_word_to_abbrev[word_upper] = abbrev_upper
            continue

        # The key itself might be an abbreviation (2-4 chars)
        if 2 <= len(key) <= 4 and key not in ('children', 'nouns', 'type', 'name'):
            abbrevs.add(key.upper())
            if taxonomy_keys is not None:
                taxonomy_keys.add(key.upper())

        # Recurse into everything
        if isinstance(value, dict):
            extract_abbrevs_recursive(value, abbrevs, noun_word_to_abbrev, taxonomy_keys)


def fnv1a_hash(s: str) -> int:
    """Calculate FNV-1a hash (same as C++ implementation)."""
    h = 2166136261
    for c in s.lower():
        h = ((h ^ ord(c)) * 16777619) & 0xFFFFFFFF
    return h


# ---------------------------------------------------------------------------
# Code Generation
# ---------------------------------------------------------------------------

def generate_header(abbrev_bits: Dict[str, int], output_path: Path) -> bool:
    """Generate the small C++ header file (declarations only).

    Returns True if file was changed.
    """
    fixed_chunk_count = 256   # 256 * 64 = 16384 bits max
    fixed_category_count = 16384

    num_categories = len(abbrev_bits)
    if num_categories > fixed_category_count:
        raise RuntimeError(
            f"Category count {num_categories} exceeds fixed max {fixed_category_count}! "
            f"Increase fixed_chunk_count in generate_header()."
        )

    lines = [
        '#pragma once',
        '',
        '/**',
        ' * ASE CORE INFRASTRUCTURE HEADER',
        ' *',
        ' * AUTO-GENERATED FILE - DO NOT EDIT!',
        ' * Edit core/ase-log/scripts/gen_log_cat.py and run it instead. A hand',
        ' * fix here survives exactly until the next generator run.',
        ' *',
        ' * @file        log_cat_gen.hpp',
        ' * @brief       Category bit positions and the runtime log filter state',
        ' * @description Declarations only: one bit per taxonomy category, the mask',
        ' *              type that holds them, and the filter state the log macros',
        ' *              read on every call. The init table and the libcuckoo engine',
        ' *              live in log_cat_gen.cpp so this header stays cheap to',
        ' *              include everywhere.',
        ' *',
        ' * @module      ase-log',
        ' * @layer       1 (Core)',
        ' * @category    ecs/module',
        ' * @created     2026-01-09',
        ' * @modified    2026-08-20',
        ' * @version     1.1.0',
        ' *',
        ' * CORE INFRASTRUCTURE COMPLIANCE',
        ' *',
        ' * [ ] NOT an ECS Component or System',
        ' * [ ] Layer dependencies correct (L0: no ASE deps, L1: L0 only)',
        ' * [ ] No global mutable state (constexpr/const only)',
        ' * [ ] No singletons or static mutable variables',
        ' * [ ] Thread-safe by design (pure functions or explicit mutex)',
        ' * [ ] All public functions documented with @brief, @param, @return',
        ' * [ ] constexpr where possible (compile-time evaluation)',
        ' * [ ] noexcept where possible (no-throw guarantee)',
        ' * [ ] [[nodiscard]] on functions returning values',
        ' * [ ] No magic numbers (use named constants)',
        ' * [ ] No implicit conversions (use explicit constructors)',
        ' * [ ] Header-only OR header+cpp pattern (not mixed)',
        ' * [ ] Include guards via #pragma once',
        ' * [ ] Namespace matches module: ase::{module}',
        ' * [ ] No circular dependencies',
        ' * [ ] No macros (except include guards) - use constexpr/templates',
        ' * [ ] API stable (changes require version bump)',
        ' * ',
        ' * Generated by: core/ase-log/scripts/gen_log_cat.py',
        ' * Source: taxonomy/ + hub_constants.json + hub_metrics.json + hub_tags.json',
        ' * ',
        ' * SMALL HEADER - safe to include everywhere.',
        ' * Init table + libcuckoo engine is in log_cat_gen.cpp.',
        ' * ',
        f' * Capacity: {fixed_category_count} categories, {fixed_chunk_count} chunks (64 bits each)',
        ' * FIXED size - adding categories only changes log_cat_gen.cpp, not this header.',
        ' */',
        '',
        '#include <atomic>',
        '#include <cstdint>',
        '',
        'namespace ase::log::filter {',
        '',
        '// ============================================================================',
        '// Constants (FIXED - do not change! Only .cpp changes when categories grow)',
        '// ============================================================================',
        '',
        f'constexpr int CATEGORY_COUNT = {fixed_category_count};',
        f'constexpr int CHUNK_COUNT = {fixed_chunk_count};',
        '',
        '// ============================================================================',
        '// Log Levels (bitmask)',
        '// ============================================================================',
        '',
        'constexpr uint8_t LVL_TRC = 0x01;  // Trace',
        'constexpr uint8_t LVL_DBG = 0x02;  // Debug',
        'constexpr uint8_t LVL_INF = 0x04;  // Info',
        'constexpr uint8_t LVL_WRN = 0x08;  // Warn',
        'constexpr uint8_t LVL_ERR = 0x10;  // Error',
        'constexpr uint8_t LVL_CRT = 0x20;  // Critical',
        'constexpr uint8_t LVL_ALL = 0x3F;  // All levels',
        '',
        '// ============================================================================',
        '// Category Bit-Array (scalable to unlimited categories)',
        '// ============================================================================',
        '',
        'struct CategoryMask {',
        '    uint64_t chunks[CHUNK_COUNT] = {};',
        '    ',
        '    void set(int bit_pos);',
        '    bool intersects(const CategoryMask& other) const;',
        '};',
        '',
        '// ============================================================================',
        '// Runtime Filter State (defined in log_cat_gen.cpp)',
        '// ============================================================================',
        '//',
        '// ACCESSORS, NOT FREE GLOBALS (changed 2026-08-20). These six pieces of state used',
        '// to be `extern` variables. Two reasons for the accessor form, and the rule finding',
        '// is only the second one:',
        '//',
        '//   1. STATIC INITIALISATION ORDER. should_log_loc() reads the level mask on EVERY',
        '//      log call, from every translation unit in the tree - including ones whose own',
        '//      static initialisation runs BEFORE this file\'s. A free global read at that',
        '//      moment is a read of an uninitialised object. A function-local static is',
        '//      initialised on first call instead, which is exactly when it is needed. This',
        '//      module has already paid for the neighbouring version of that problem once:',
        '//      log_sys.cpp::on_stop carries a long comment about merged statics being freed',
        '//      twice at exit.',
        '//   2. CONSISTENCY INSIDE THIS VERY FILE. loc_cache() and abbrev_map() are already',
        '//      built this way, by this same generator. Two shapes for the same job in one',
        '//      generated file is not a decision, it is a leftover.',
        '',
        'std::atomic<uint8_t>& level_mask();',
        'CategoryMask& blocked_categories();',
        'CategoryMask& whitelisted_categories();  ///< Whitelist (empty = all pass)',
        'std::atomic<bool>& has_whitelist();      ///< True if any +CATEGORY was set',
        '',
        '// ============================================================================',
        '// Client Filter State (defined in log_cat_gen.cpp)',
        '// ============================================================================',
        '',
        'std::atomic<uint64_t>& client_mask();     ///< Whitelist (0 = all pass)',
        'std::atomic<uint64_t>& blocked_clients();  ///< Blacklist bitmask',
        '',
        '// ============================================================================',
        '// Function Declarations (implemented in log_cat_gen.cpp)',
        '// ============================================================================',
        '',
        '/// Hash abbreviation to bit position (-1 if not found)',
        'int abbrev_to_bit(uint32_t hash);',
        '',
        '/// Extract categories from filename (tokenizes at _ . separators)',
        'CategoryMask file_to_categories(const char* file);',
        '',
        '/// Extract categories from function name (tokenizes at :: _ ( ) , space &)',
        'CategoryMask func_to_categories(const char* func);',
        '',
        '/// Parse CLI filter string (e.g., "+INF +WRN -BCT +EPH +TICK +CLT:01")',
        'void parse_log_filter(const char* filter_str);',
        '',
        '/// O(1) cached filter check for file+function (~12 cycles hot path)',
        '// The location key is computed at the CALL SITE (log::loc_key, constexpr-folded), not',
        '// here: hashing the two strings per call would multiply the fast path this cache exists',
        '// to avoid. Until 2026-08-22 the key came from reinterpret_cast on the two POINTERS —',
        '// forbidden, and unstable across translation units where identical __FILE__ literals',
        '// may sit at different addresses.',
        'bool should_log_loc(uint8_t level, const char* file, const char* func, uint64_t key);',
        '',
        '/// O(1) cached filter check for client file+function',
        'bool should_log_client_loc(uint8_t level, const char* file, const char* func, uint64_t key,',
        '                           uint64_t client_bit);',
        '',
        '/// Invalidate location cache (call after filter changes)',
        'void invalidate_loc_cache();',
        '',
        '}  // namespace ase::log::filter',
        '',
    ]

    new_content = '\n'.join(lines)
    return write_if_changed(output_path, new_content)


def generate_impl(abbrev_bits: Dict[str, int],
                  name_aliases: List[Tuple[str, str]],
                  output_path: Path) -> bool:
    """Generate the large C++ implementation file with libcuckoo engine.

    Returns True if file was changed.
    """
    num_categories = len(abbrev_bits)
    num_aliases = len(name_aliases)

    lines = [
        '/**',
        ' * ASE CORE INFRASTRUCTURE IMPLEMENTATION',
        ' *',
        ' * AUTO-GENERATED FILE - DO NOT EDIT!',
        ' * Edit core/ase-log/scripts/gen_log_cat.py and run it instead. A hand',
        ' * fix here survives exactly until the next generator run.',
        ' *',
        ' * @file        log_cat_gen.cpp',
        ' * @brief       Category lookup engine and runtime filter state',
        ' * @description The init table maps hashed category names to bit positions',
        ' *              and is handed to a libcuckoo map at first use, so a lookup',
        ' *              is O(1) instead of a switch over thousands of names. Kept in',
        ' *              one translation unit on purpose: the table is large and the',
        ' *              header is included nearly everywhere.',
        ' *',
        ' * @module      ase-log',
        ' * @layer       1 (Core)',
        ' * @category    ecs/module',
        ' * @created     2026-01-09',
        ' * @modified    2026-08-20',
        ' * @version     1.1.0',
        ' *',
        ' * CORE INFRASTRUCTURE IMPLEMENTATION COMPLIANCE',
        ' *',
        ' * [ ] NOT an ECS System implementation',
        ' * [ ] Layer dependencies correct (L0: no ASE deps, L1: L0 only)',
        ' * [ ] Own header included FIRST',
        ' * [ ] No global mutable state',
        ' * [ ] No static initialization order fiasco',
        ' * [ ] Thread-safe implementations (pure or mutex-protected)',
        ' * [ ] All error conditions handled',
        ' * [ ] No exceptions thrown (use Result<T> pattern)',
        ' * [ ] Implementation details in anonymous namespace',
        ' * [ ] No inline implementations of template specializations here',
        ' * [ ] Platform-specific code isolated and documented',
        ' * [ ] Performance-critical code profiled and optimized',
        ' * ',
        ' * Generated by: core/ase-log/scripts/gen_log_cat.py',
        ' * Source: taxonomy/ + hub_constants.json + hub_metrics.json + hub_tags.json',
        ' * ',
        f' * {num_categories} categories + {num_aliases} module aliases.',
        ' * Uses libcuckoo O(1) hashmap instead of switch statement.',
        ' * Compiled ONCE, not in every translation unit.',
        ' */',
        '',
        '#include <ase/log/log_cat_gen.hpp>',
        '#include <libcuckoo/cuckoohash_map.hh>',
        '#include <cstring>',
        '',
        'namespace ase::log::filter {',
        '',
        '// ============================================================================',
        '// Runtime Filter State (definitions)',
        '// ============================================================================',
        '',
        '// Function-local statics, same shape as loc_cache() and abbrev_map() below: each is',
        '// initialised on FIRST USE, not during static initialisation, so a log call from a',
        '// translation unit that initialises earlier than this one cannot read an',
        '// uninitialised object. See the header for the full reasoning.',
        'std::atomic<uint8_t>& level_mask() {',
        '    static std::atomic<uint8_t> mask{LVL_ALL};',
        '    return mask;',
        '}',
        '',
        'CategoryMask& blocked_categories() {',
        '    static CategoryMask mask{};',
        '    return mask;',
        '}',
        '',
        'CategoryMask& whitelisted_categories() {',
        '    static CategoryMask mask{};',
        '    return mask;',
        '}',
        '',
        'std::atomic<bool>& has_whitelist() {',
        '    static std::atomic<bool> flag{false};',
        '    return flag;',
        '}',
        '',
        'std::atomic<uint64_t>& client_mask() {',
        '    static std::atomic<uint64_t> mask{0};',
        '    return mask;',
        '}',
        '',
        'std::atomic<uint64_t>& blocked_clients() {',
        '    static std::atomic<uint64_t> mask{0};',
        '    return mask;',
        '}',
        '',
        '// ============================================================================',
        '// CategoryMask Implementation',
        '// ============================================================================',
        '',
        'void CategoryMask::set(int bit_pos) {',
        '    if (bit_pos >= 0 && bit_pos < CATEGORY_COUNT) {',
        '        chunks[bit_pos / 64] |= (1ULL << (bit_pos % 64));',
        '    }',
        '}',
        '',
        'bool CategoryMask::intersects(const CategoryMask& other) const {',
        '    for (int i = 0; i < CHUNK_COUNT; ++i) {',
        '        if (chunks[i] & other.chunks[i]) return true;',
        '    }',
        '    return false;',
        '}',
        '',
        '// ============================================================================',
        '// Category Bit Positions (auto-generated from taxonomy + hub JSONs)',
        '// ============================================================================',
        '',
    ]

    # Generate category bit position constants
    # Replace hyphens with underscores for valid C++ identifiers
    for abbrev, bit_pos in sorted(abbrev_bits.items(), key=lambda x: x[1]):
        cpp_id = abbrev.replace('-', '_')
        lines.append(f'constexpr int BIT_{cpp_id} = {bit_pos};')

    lines.extend([
        '',
        '// ============================================================================',
        '// Init Table + libcuckoo Engine (replaces big switch)',
        '// ============================================================================',
        '',
        'namespace {',
        '',
        'constexpr char to_lower(char c) {',
        '    return (c >= \'A\' && c <= \'Z\') ? static_cast<char>(c + 32) : c;',
        '}',
        '',
        'constexpr uint32_t hash_part(const char* start, const char* end) {',
        '    uint32_t hash = 2166136261u;',
        '    for (const char* p = start; p < end; ++p) {',
        '        char c = to_lower(*p);',
        '        hash = (hash ^ static_cast<uint32_t>(c)) * 16777619u;',
        '    }',
        '    return hash;',
        '}',
        '',
        'const char* find_filename(const char* path) {',
        '    const char* result = path;',
        '    for (const char* p = path; *p; ++p) {',
        '        if (*p == \'/\' || *p == \'\\\\\') result = p + 1;',
        '    }',
        '    return result;',
        '}',
        '',
        '// --- Init Table (constexpr, lives in .rodata) ---',
        'struct InitEntry { uint32_t hash; int bit; };',
        'constexpr InitEntry g_init_table[] = {',
    ])

    # Build hash -> bit mapping with collision detection
    hash_to_entry: Dict[int, str] = {}

    # Categories
    for abbrev, bit_pos in sorted(abbrev_bits.items()):
        hash_val = fnv1a_hash(abbrev)
        cpp_id = abbrev.replace('-', '_')
        if hash_val in hash_to_entry:
            print(f"Warning: Hash collision! {abbrev.lower()} collides with {hash_to_entry[hash_val]}",
                  file=sys.stderr)
        else:
            hash_to_entry[hash_val] = abbrev.lower()
            lines.append(f'    {{{hash_val}u, BIT_{cpp_id}}},  // "{abbrev.lower()}"')

    # Module prefix aliases
    for name, abbrev in sorted(name_aliases):
        hash_val = fnv1a_hash(name)
        cpp_id = abbrev.replace('-', '_')
        if hash_val in hash_to_entry:
            existing = hash_to_entry[hash_val]
            if existing != name.lower():
                print(f"Info: Module alias \"{name.lower()}\" hash collides with "
                      f"\"{existing}\" (both -> BIT_{cpp_id}), skipping",
                      file=sys.stderr)
            continue
        hash_to_entry[hash_val] = name.lower()
        # Unicode arrow, not ASCII: the tree-wide comment rule forbids "->" in
        # comments, and this single line emits 1154 of them.
        lines.append(f'    {{{hash_val}u, BIT_{cpp_id}}},  '
                     f'// "{name.lower()}" (module prefix → {abbrev.lower()})')

    init_table_size = len(hash_to_entry)

    lines.extend([
        '};',
        f'constexpr int INIT_TABLE_SIZE = {init_table_size};',
        '',
        '// --- libcuckoo Hashmaps (Meyers Singletons, SIOF-safe) ---',
        '',
        'libcuckoo::cuckoohash_map<uint32_t, int>& abbrev_map() {',
        f'    static libcuckoo::cuckoohash_map<uint32_t, int> map({init_table_size * 2});',
        '    return map;',
        '}',
        '',
        '// O(1) cache: file+func → pass/fail (key = file_ptr ^ (func_ptr << 1))',
        'libcuckoo::cuckoohash_map<uintptr_t, uint8_t>& loc_cache() {',
        '    static libcuckoo::cuckoohash_map<uintptr_t, uint8_t> map(8192);',
        '    return map;',
        '}',
        '',
        'struct StaticInit {',
        '    StaticInit() {',
        '        auto& m = abbrev_map();',
        '        for (int i = 0; i < INIT_TABLE_SIZE; ++i)',
        '            m.insert(g_init_table[i].hash, g_init_table[i].bit);',
        '    }',
        '};',
        'static StaticInit g_static_init;',
        '',
        '}  // anonymous namespace',
        '',
        '// ============================================================================',
        '// O(1) Hash Lookup (replaces big switch)',
        '// ============================================================================',
        '',
        'int abbrev_to_bit(uint32_t hash) {',
        '    int r = -1;',
        '    abbrev_map().find(hash, r);',
        '    return r;',
        '}',
        '',
        '// ============================================================================',
        '// Filename to Categories',
        '// ============================================================================',
        '',
        'CategoryMask file_to_categories(const char* file) {',
        '    CategoryMask mask{};',
        '    const char* name = find_filename(file);',
        '    const char* part_start = name;',
        '    ',
        '    for (const char* p = name; ; ++p) {',
        '        if (*p == \'_\' || *p == \'.\' || *p == \'\\0\') {',
        '            if (p > part_start) {',
        '                uint32_t h = hash_part(part_start, p);',
        '                int bit = abbrev_to_bit(h);',
        '                if (bit >= 0) mask.set(bit);',
        '            }',
        '            if (*p == \'\\0\' || *p == \'.\') break;',
        '            part_start = p + 1;',
        '        }',
        '    }',
        '    return mask;',
        '}',
        '',
        '// ============================================================================',
        '// O(1) Cached Filter — file + function categories (~12 cycles hot path)',
        '// ============================================================================',
        '',
        'CategoryMask func_to_categories(const char* func) {',
        '    CategoryMask mask{};',
        '    if (!func) return mask;',
        '    // Tokenize function name at :: _ ( ) , space',
        '    const char* part_start = func;',
        '    for (const char* p = func; ; ++p) {',
        '        bool is_sep = (*p == \':\' || *p == \'_\' || *p == \'(\' || *p == \')\' ||',
        '                       *p == \',\' || *p == \' \' || *p == \'&\' || *p == \'\\0\');',
        '        if (is_sep) {',
        '            if (p > part_start) {',
        '                uint32_t h = hash_part(part_start, p);',
        '                int bit = abbrev_to_bit(h);',
        '                if (bit >= 0) mask.set(bit);',
        '            }',
        '            if (*p == \'\\0\') break;',
        '            part_start = p + 1;',
        '        }',
        '    }',
        '    return mask;',
        '}',
        '',
        'bool should_log_loc(uint8_t level, const char* file, const char* func, uint64_t key) {',
        '    if (!(level & level_mask().load(std::memory_order_relaxed))) return false;',
        '    uint8_t cached = 0;',
        '    if (loc_cache().find(key, cached)) return cached != 0;  // O(1) hit',
        '    // Cache miss: compute from file + func categories',
        '    CategoryMask cats = file_to_categories(file);',
        '    CategoryMask func_cats = func_to_categories(func);',
        '    // Merge: file categories + function categories',
        '    for (int i = 0; i < CHUNK_COUNT; ++i)',
        '        cats.chunks[i] |= func_cats.chunks[i];',
        '    bool pass = true;',
        '    if (cats.intersects(blocked_categories())) pass = false;',
        '    if (pass && has_whitelist().load(std::memory_order_relaxed))',
        '        if (!cats.intersects(whitelisted_categories())) pass = false;',
        '    loc_cache().insert(key, pass ? 1u : 0u);',
        '    return pass;',
        '}',
        '',
        'bool should_log_client_loc(uint8_t level, const char* file, const char* func, uint64_t key,',
        '                           uint64_t client_bit) {',
        '    if (!(level & level_mask().load(std::memory_order_relaxed))) return false;',
        '    uint8_t cached = 0;',
        '    bool cat_pass;',
        '    if (loc_cache().find(key, cached)) {',
        '        cat_pass = cached != 0;',
        '    } else {',
        '        CategoryMask cats = file_to_categories(file);',
        '        CategoryMask func_cats = func_to_categories(func);',
        '        for (int i = 0; i < CHUNK_COUNT; ++i)',
        '            cats.chunks[i] |= func_cats.chunks[i];',
        '        cat_pass = true;',
        '        if (cats.intersects(blocked_categories())) cat_pass = false;',
        '        if (cat_pass && has_whitelist().load(std::memory_order_relaxed))',
        '            if (!cats.intersects(whitelisted_categories())) cat_pass = false;',
        '        loc_cache().insert(key, cat_pass ? 1u : 0u);',
        '    }',
        '    if (!cat_pass) return false;',
        '    if (client_bit) {',
        '        if (client_bit & blocked_clients().load(std::memory_order_relaxed)) return false;',
        '        uint64_t mask = client_mask().load(std::memory_order_relaxed);',
        '        if (mask && !(client_bit & mask)) return false;',
        '    }',
        '    return true;',
        '}',
        '',
        'void invalidate_loc_cache() { loc_cache().clear(); }',
        '',
        '// ============================================================================',
        '// CLI Argument Parser',
        '// ============================================================================',
        '',
        'namespace {',
        '',
        'void parse_client_ids(const char* ids, std::atomic<uint64_t>& target) {',
        '    while (*ids) {',
        '        int id = 0;',
        '        while (*ids >= \'0\' && *ids <= \'9\') { id = id * 10 + (*ids - \'0\'); ids++; }',
        '        if (id >= 0 && id < 64) {',
        '            uint64_t old_val = target.load(std::memory_order_relaxed);',
        '            target.store(old_val | (1ULL << id), std::memory_order_relaxed);',
        '        }',
        '        if (*ids == \',\') ids++;',
        '        else break;',
        '    }',
        '}',
        '',
        'void parse_filter_token(const char* token) {',
        '    if (!token || !*token) return;',
        '    ',
        '    char op = token[0];',
        '    const char* code = token + 1;',
        '    ',
        '    if (op == \'+\') {',
        '        // +CLT:01 or +CLT:01,03 → client whitelist',
        '        if (std::strncmp(code, "CLT:", 4) == 0) {',
        '            parse_client_ids(code + 4, client_mask());',
        '            return;',
        '        }',
        '        // +CLT → all clients in whitelist',
        '        if (std::strcmp(code, "CLT") == 0) {',
        '            client_mask().store(0xFFFFFFFFFFFFFFFFULL, std::memory_order_relaxed);',
        '            return;',
        '        }',
        '        // +LEVEL → enable specific level',
        '        if (std::strcmp(code, "TRC") == 0) { level_mask().fetch_or(LVL_TRC); return; }',
        '        if (std::strcmp(code, "DBG") == 0) { level_mask().fetch_or(LVL_DBG); return; }',
        '        if (std::strcmp(code, "INF") == 0) { level_mask().fetch_or(LVL_INF); return; }',
        '        if (std::strcmp(code, "WRN") == 0) { level_mask().fetch_or(LVL_WRN); return; }',
        '        if (std::strcmp(code, "ERR") == 0) { level_mask().fetch_or(LVL_ERR); return; }',
        '        if (std::strcmp(code, "CRT") == 0) { level_mask().fetch_or(LVL_CRT); return; }',
        '        // +CATEGORY → whitelist',
        '        {',
        '            uint32_t hash = 2166136261u;',
        '            for (const char* p = code; *p; ++p) {',
        '                char c = (*p >= \'A\' && *p <= \'Z\') ? static_cast<char>(*p + 32) : *p;',
        '                hash = (hash ^ static_cast<uint32_t>(c)) * 16777619u;',
        '            }',
        '            int bit = abbrev_to_bit(hash);',
        '            if (bit >= 0) {',
        '                whitelisted_categories().set(bit);',
        '                has_whitelist().store(true, std::memory_order_relaxed);',
        '            }',
        '        }',
        '    } else if (op == \'-\') {',
        '        // -CLT → block all clients',
        '        if (std::strcmp(code, "CLT") == 0) {',
        '            blocked_clients().store(0xFFFFFFFFFFFFFFFFULL, std::memory_order_relaxed);',
        '            return;',
        '        }',
        '        // -CLT:05 or -CLT:05,07 → block specific clients',
        '        if (std::strncmp(code, "CLT:", 4) == 0) {',
        '            parse_client_ids(code + 4, blocked_clients());',
        '            return;',
        '        }',
        '        // -CATEGORY → blacklist',
        '        uint32_t hash = 2166136261u;',
        '        for (const char* p = code; *p; ++p) {',
        '            char c = (*p >= \'A\' && *p <= \'Z\') ? static_cast<char>(*p + 32) : *p;',
        '            hash = (hash ^ static_cast<uint32_t>(c)) * 16777619u;',
        '        }',
        '        int bit = abbrev_to_bit(hash);',
        '        if (bit >= 0) blocked_categories().set(bit);',
        '    }',
        '}',
        '',
        '}  // anonymous namespace',
        '',
        'void parse_log_filter(const char* filter_str) {',
        '    if (!filter_str) return;',
        '    ',
        '    // Check if any +LEVEL token is present',
        '    bool has_level = false;',
        '    {',
        '        const char* p = filter_str;',
        '        while (*p) {',
        '            while (*p == \' \') p++;',
        '            if (*p == \'+\') {',
        '                const char* code = p + 1;',
        '                if (std::strncmp(code, "TRC", 3) == 0 ||',
        '                    std::strncmp(code, "DBG", 3) == 0 ||',
        '                    std::strncmp(code, "INF", 3) == 0 ||',
        '                    std::strncmp(code, "WRN", 3) == 0 ||',
        '                    std::strncmp(code, "ERR", 3) == 0 ||',
        '                    std::strncmp(code, "CRT", 3) == 0) {',
        '                    has_level = true;',
        '                    break;',
        '                }',
        '            }',
        '            while (*p && *p != \' \') p++;',
        '        }',
        '    }',
        '    if (has_level) level_mask().store(0);',
        '    ',
        '    char token[128];',
        '    int ti = 0;',
        '    for (const char* p = filter_str; ; ++p) {',
        '        if (*p == \' \' || *p == \'\\0\') {',
        '            if (ti > 0) {',
        '                token[ti] = \'\\0\';',
        '                parse_filter_token(token);',
        '                ti = 0;',
        '            }',
        '            if (*p == \'\\0\') break;',
        '        } else if (ti < 127) {',
        '            token[ti++] = *p;',
        '        }',
        '    }',
        '    // Invalidate file cache after filter changes',
        '    invalidate_loc_cache();',
        '}',
        '',
        '}  // namespace ase::log::filter',
        '',
    ])

    new_content = '\n'.join(lines)
    return write_if_changed(output_path, new_content)


# ---------------------------------------------------------------------------
# Error/Warning Categories (ERR::CAT / WRN::CAT) from log_categories.json
# ---------------------------------------------------------------------------
#
# WARUM DIESE KATEGORIEN HIER ENTSTEHEN UND NICHT MEHR IN log.hpp:
# Sie standen als Konstanten, drei Tabellen und ein Zaehler von Hand im L1-Header. Jede neue
# Kategorie war damit ein Schrieb an einer Datei mit rund 2000 Lesern - und die vier Stellen
# sind durch static_assert aneinander gebunden, also ist JEDER Zwischenstand zwischen zwei
# Edits ein Fenster, in dem der ganze Baum unbaubar ist. Gemessen 2026-08-23: ein fremder Bau
# starb bei 468 von 887 Zielen in genau so einem Fenster.
#
# Ab hier ist eine neue Kategorie EINE Zeile in log_categories.json plus ein Generatorlauf.


# @modified der erzeugten Dateien. BEWUSST EINE KONSTANTE, nie date.today():
# write_if_changed vergleicht den Inhalt, also wuerde ein tagesaktuelles Datum den HEADER an
# jedem neuen Tag neu schreiben — und der Header haengt an rund 1500 Uebersetzungseinheiten.
# Ein taeglicher Vollbau als Nebenwirkung einer Datumszeile ist genau die Kaskade, gegen die
# die Trennung Header/Impl gebaut ist. Wer den Generator inhaltlich aendert, zieht dieses
# Datum mit.
GEN_DATE = "2026-08-23"

# Die Pflichtblöcke aus INST_ASE_LINT.md Section 9. Uebernommen aus log_cat_gen.hpp/.cpp,
# die dieselbe Pruefung bestehen. Die Kaestchen bleiben LEER: das Tor ist der Pruefer, ein
# gesetztes [x] oder [-] gilt dem Validator als FEHLEND.
CORE_HEADER_CHECKLIST = [
    "[ ] NOT an ECS Component or System",
    "[ ] Layer dependencies correct (L0: no ASE deps, L1: L0 only)",
    "[ ] No global mutable state (constexpr/const only)",
    "[ ] No singletons or static mutable variables",
    "[ ] Thread-safe by design (pure functions or explicit mutex)",
    "[ ] All public functions documented with @brief, @param, @return",
    "[ ] constexpr where possible (compile-time evaluation)",
    "[ ] noexcept where possible (no-throw guarantee)",
    "[ ] [[nodiscard]] on functions returning values",
    "[ ] No magic numbers (use named constants)",
    "[ ] No implicit conversions (use explicit constructors)",
    "[ ] Header-only OR header+cpp pattern (not mixed)",
    "[ ] Include guards via #pragma once",
    "[ ] Namespace matches module: ase::{module}",
    "[ ] No circular dependencies",
    "[ ] No macros (except include guards) - use constexpr/templates",
    "[ ] API stable (changes require version bump)",
]

CORE_IMPL_CHECKLIST = [
    "[ ] NOT an ECS System implementation",
    "[ ] Layer dependencies correct (L0: no ASE deps, L1: L0 only)",
    "[ ] Own header included FIRST",
    "[ ] No global mutable state",
    "[ ] No static initialization order fiasco",
    "[ ] Thread-safe implementations (pure or mutex-protected)",
    "[ ] All error conditions handled",
    "[ ] No exceptions thrown (use Result<T> pattern)",
    "[ ] Implementation details in anonymous namespace",
    "[ ] No inline implementations of template specializations here",
    "[ ] Platform-specific code isolated and documented",
    "[ ] Performance-critical code profiled and optimized",
]


def syntax_check_generated(project_root: Path, header: Path, impl: Path) -> Tuple[bool, str]:
    """Uebersetzt die erzeugten Dateien mit -fsyntax-only. NICHTS wird gebaut.

    WARUM ES DAS GIBT, gemessen 2026-08-23: eine doppelte Kategorie in der SSOT erzeugte ein
    Generat mit einer Redefinition. `log.hpp` bindet es unbedingt ein, also war JEDE
    Uebersetzungseinheit im Baum betroffen — und `mod_cmpl_check` meldete die ganze Zeit
    `violations: 0`. Eine Redefinition ist kein Validator-Befund; kein Tor sieht sie.

    Solange kein Bau laufen darf, ist dieser Aufruf der EINZIGE Kanal, der einen
    Uebersetzungsfehler in core/ase-log sichtbar macht. Ohne ihn faellt er beim naechsten Bau
    auf, bei Ziel 1 von 1511.

    BEWUSST NUR UNTER --check und NICHT im CMake-Pfad: ein Compileraufruf, von dem der Bau
    abhaengt, waere ein neuer Bruchpunkt in der Kette, die er absichern soll.
    """
    import subprocess
    incs = [project_root / "core" / "ase-log" / "include"]
    cmd = ["g++", "-std=c++20", "-fsyntax-only"]
    for inc in incs:
        cmd += ["-I", str(inc)]
    cmd.append(str(impl))          # zieht den Header mit
    try:
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    except Exception as exc:       # noqa: BLE001
        return False, f"Compiler nicht aufrufbar: {exc}"
    if res.returncode == 0:
        return True, ""
    first = next((ln for ln in res.stderr.splitlines() if "error:" in ln), res.stderr[:200])
    return False, first


def load_log_categories(data_path: Path) -> dict:
    """Load the ERR/WRN category SSOT. Raises if the file is missing - a silent
    fallback would generate an EMPTY table and break every caller in the tree."""
    if not data_path.exists():
        raise FileNotFoundError(f"Category SSOT missing: {data_path}")
    with open(data_path, 'r', encoding='utf-8') as fh:
        return json.load(fh)


def load_display_policy(data_path: Path) -> dict:
    """Load the display policy SSOT (short_max_columns, literal_identifiers).
    Raises if the file is missing - without it the short-length contract cannot
    be enforced, and an unenforced cap is a wrap waiting to happen."""
    if not data_path.exists():
        raise FileNotFoundError(f"Display policy SSOT missing: {data_path}")
    with open(data_path, 'r', encoding='utf-8') as fh:
        return json.load(fh)


def policy_value(policy: dict, key: str):
    """Strict policy access - a missing key stops the run. Fallback-Defaults sind verboten:
    ein erfundener Vorgabewert nimmt der fehlenden Zeile ihre Meldung und kompiliert eine
    zweite, stille Wahrheit ins Generat."""
    if key not in policy:
        raise ValueError(
            f"log_display.json: required key '{key}' missing - the policy is the single "
            f"source and carries no fallback defaults")
    return policy[key]


def load_sha_colors(ase_log_dir: Path):
    """The SHA colour palette from the LOCAL down-top copy. colors.ts ist die SSOT der
    Werte; generate-colors.cjs verteilt die generierte python-Form als lokale Kopie nach
    core/ase-log/data/generated/ (Shader-Muster: der Producer-Lauf ueberschreibt die
    Konsumenten-Kopie). Dieser Generator liest AUSSCHLIESSLICH die lokale Kopie - ein
    Cross-Subgit-Import aus clients/ waere eine Schichtungs-Inversion. Fehlt die Kopie,
    bricht der Lauf laut."""
    import importlib.util
    path = ase_log_dir / "data" / "generated" / "colors.py"
    if not path.exists():
        raise FileNotFoundError(
            f"SHA colour copy missing: {path} - run "
            f"clients/sha-client-web/sha-web-console/generate-colors.cjs to distribute it")
    spec = importlib.util.spec_from_file_location("sha_generated_colors", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.C


def resolve_color_sgr(sha, name: str) -> str:
    """One palette name to its SGR parameter string. A name outside the palette stops the
    run loudly - a guessed colour in the record would look valid and be wrong."""
    value = getattr(sha, str(name), None)
    if not isinstance(value, str) or not value.startswith('\033[') or not value.endswith('m'):
        raise ValueError(
            f"colour name '{name}' is not in the SHA palette (colors.ts) - add it there and "
            f"rerun sha-web-console/generate-colors.cjs")
    return value[2:-1]


def validate_display_texts(cats: dict, policy: dict) -> None:
    """Enforce the display contracts on every category's short and hint.

    short is the essentials-only sentence rendered ON the head line next to the
    event values - its hard length cap (short_max_columns) exists so it can
    NEVER wrap on a supported console. hint is the standing advice the console
    renders as the separate muted block. Both are FLOWING TEXT WITHOUT SYMBOLS:
    no digits, no enumeration marks, no structural punctuation - only words,
    commas, periods and apostrophes. The literal identifiers listed in the
    policy are exempt, because a rewritten name would point at something else.

    The gate lives HERE, in the generator run, so no hand edit of the SSOT can
    break the contract silently: a violation raises and stops the run, naming
    the category and the offending characters.
    """
    import re
    cap = int(policy_value(policy, 'short_max_columns'))
    idents = policy_value(policy, 'literal_identifiers')
    allowed = re.compile(r"^[A-Za-z ,.']*$")
    for ns in ('err', 'wrn'):
        for cat in cats[ns]['categories']:
            key = f"{cats[ns]['namespace']}_{cat['name']}"
            for field in ('short', 'hint'):
                if field not in cat:
                    raise ValueError(
                        f"{key}: display field '{field}' missing - every category "
                        f"carries short AND hint (short = head line, hint = muted block)")
                stripped = cat[field]
                for ident in idents:
                    stripped = stripped.replace(ident, '')
                if not allowed.match(stripped):
                    bad = sorted({c for c in stripped if not allowed.match(c)})
                    raise ValueError(
                        f"{key}.{field} carries forbidden characters {bad} - display "
                        f"texts are flowing words, commas, periods and apostrophes only "
                        f"(literal identifiers exempt, see log_display.json)")
            if len(cat['short']) > cap:
                raise ValueError(
                    f"{key}.short is {len(cat['short'])} chars, the cap is {cap} "
                    f"(short_max_columns in log_display.json) - the short text stands "
                    f"on the head line and must never wrap")


def collect_noun_occurrences(taxonomy_dir: Path) -> Dict[str, List[str]]:
    """Every taxonomy noun word with ALL its documented abbreviations across the
    taxonomy files. The display resolver needs the full occurrence list, not a
    single winner: a word whose files disagree without a majority stays written
    out rather than guessed."""
    occurrences: Dict[str, List[str]] = {}

    def walk(node) -> None:
        if not isinstance(node, dict):
            return
        for key, value in node.items():
            if key == 'nouns' and isinstance(value, dict):
                for word, abbrev in value.items():
                    occurrences.setdefault(word.lower(), []).append(abbrev.lower())
            elif isinstance(value, dict):
                walk(value)

    for f in sorted(taxonomy_dir.glob("*.json")):
        if f.name.startswith('_'):
            continue
        try:
            with open(f, 'r', encoding='utf-8') as fp:
                walk(json.load(fp))
        except Exception as e:
            print(f"Warning: Failed to parse {f}: {e}", file=sys.stderr)
    return occurrences


def resolve_display_words(cats: dict, policy: dict, taxonomy_dir: Path) -> Dict[str, str]:
    """The display vocabulary: extra_words from the policy plus every word of
    every short and hint text, each resolved against the taxonomy SSOT. Kept are
    words with a clear majority abbreviation saving at least min_saving
    characters; ties and small savings stay written out - a wrong shortening
    misleads the operator, a missing one only costs columns."""
    import re
    occurrences = collect_noun_occurrences(taxonomy_dir)
    candidates: Set[str] = {w.lower() for w in policy_value(policy, 'extra_words')}
    for ns in ('err', 'wrn'):
        for cat in cats[ns]['categories']:
            for field in ('short', 'hint'):
                for w in re.findall(r"[A-Za-z]+", cat.get(field, '')):
                    candidates.add(w.lower())
    min_saving = int(policy_value(policy, 'min_saving'))
    min_length = 5
    fillers = {w.lower() for w in policy_value(policy, 'fillers')}
    resolved: Dict[str, str] = {}
    for word in sorted(candidates):
        if word in fillers or len(word) < min_length:
            continue
        hits = occurrences.get(word)
        if not hits:
            continue
        counts: Dict[str, int] = {}
        for ab in hits:
            counts[ab] = counts.get(ab, 0) + 1
        ranked = sorted(counts.items(), key=lambda kv: (-kv[1], kv[0]))
        if len(ranked) > 1 and ranked[0][1] == ranked[1][1]:
            continue
        abbrev = ranked[0][0]
        if len(word) - len(abbrev) < min_saving:
            continue
        resolved[word] = abbrev
    return resolved


def generate_display_impl(display_words: Dict[str, str], policy: dict, sha,
                          output_path: Path) -> bool:
    """The generated display word table: one open-addressing hash-slot array in
    .rodata plus the O(1) lookup. Fillers are entries with an EMPTY replacement,
    so a single probe answers both questions a console asks about a word. Slot
    placement happens HERE, at generation time; a hash-plus-length duplicate
    inside the set stops the run loudly - at runtime the pair IS the identity."""
    slot_count = 512
    slots: List[Optional[Tuple[int, int, str]]] = [None] * slot_count

    def insert(word: str, few: str) -> None:
        digest = fnv1a_hash(word)
        if digest == 0 or not (1 <= len(word) <= 255):
            raise ValueError(f"display word '{word}' cannot be hashed safely")
        idx = digest & (slot_count - 1)
        while slots[idx] is not None:
            if slots[idx][0] == digest and slots[idx][1] == len(word):
                raise ValueError(
                    f"display word '{word}' collides with an existing entry in hash AND "
                    f"length - hash identity would lie, rename or drop one side")
            idx = (idx + 1) & (slot_count - 1)
        slots[idx] = (digest, len(word), few)

    for word, few in sorted(display_words.items()):
        insert(word, few)
    for word in policy_value(policy, 'fillers'):
        insert(word.lower(), '')

    filled = sum(1 for s in slots if s is not None)
    if filled * 2 > slot_count:
        raise ValueError(f"display table holds {filled} of {slot_count} slots, over one "
                         f"half - raise slot_count in generate_display_impl")

    out = ['/**', ' * ASE CORE INFRASTRUCTURE IMPLEMENTATION', ' *',
           ' * AUTO-GENERATED FILE - DO NOT EDIT!',
           ' * Edit core/ase-log/data/log_display.json (policy, extra_words, fillers) or',
           ' * core/ase-log/data/log_categories.json (short and hint texts) and run',
           ' * core/ase-log/scripts/gen_log_cat.py instead. A hand fix here survives',
           ' * exactly until the next generator run.', ' *',
           ' * @file        log_display_gen.cpp',
           ' * @brief       Display word table, the O(1) shortening lookup',
           ' * @description One open-addressing slot array in .rodata plus the probe. The',
           ' *              words come from the display policy plus the vocabulary of every',
           ' *              short and hint text, each resolved against the taxonomy SSOT at',
           ' *              generation time. Hash plus length is the identity at runtime;',
           ' *              the one case that could break that, a duplicate inside the set,',
           ' *              stops the generator run. A filler word carries an EMPTY',
           ' *              replacement; an unknown word answers nullptr.',
           ' *', ' * @module      ase-log', ' * @layer       1 (Core)',
           ' * @category    ecs/module', ' * @created     2026-08-24',
           f' * @modified    {GEN_DATE}',
           ' * @version     1.0.0', ' *',
           ' * CORE INFRASTRUCTURE IMPLEMENTATION COMPLIANCE', ' *'] \
        + [f' * {line}' for line in CORE_IMPL_CHECKLIST] \
        + [' *',
           ' * Generated by: core/ase-log/scripts/gen_log_cat.py',
           ' * Source: log_display.json + log_categories.json + taxonomy/', ' */', '',
           '#include <ase/log/log_display.hpp>', '',
           'namespace ase::log::detail {', '', 'namespace {', '',
           'struct DisplaySlot {',
           '    uint32_t hash;',
           '    uint8_t len;',
           '    const char* few;',
           '};', '',
           f'constexpr uint32_t kDisplaySlotMask = {slot_count - 1}u;', '',
           f'constexpr DisplaySlot g_display_slots[{slot_count}] = {{']
    for s in slots:
        if s is None:
            out.append('    {0u, 0, nullptr},')
        else:
            out.append(f'    {{{s[0]}u, {s[1]}, "{_cpp_str(s[2])}"}},')
    # Die Politik traegt Farb-NAMEN, die WERTE kommen aus der SHA-Palette (colors.ts) ueber
    # die lokale Down-Top-Kopie in data/generated/ - der Betreiber-Lauf von generate-colors.cjs
    # verteilt sie dorthin. Keine Fallback-Defaults: ein fehlender Schluessel oder ein Name
    # ausserhalb der Palette bricht den Lauf laut, statt eine zweite stille Wahrheit zu
    # kompilieren.
    timestamp_sgr = resolve_color_sgr(sha, policy_value(policy, 'timestamp_color'))
    level_cfg = policy_value(policy, 'level_colors')
    level_sgrs = [resolve_color_sgr(sha, policy_value(level_cfg, name))
                  for name in ('trace', 'debug', 'info', 'warn', 'error', 'critical')]
    text_cfg = policy_value(policy, 'text_colors')
    text_sgrs = {name: resolve_color_sgr(sha, policy_value(text_cfg, name))
                 for name in ('string', 'number', 'url', 'path', 'tag', 'good', 'bad', 'punct')}

    # Outcome word set: good and bad words in ONE slot table, the stored value is the SGR of
    # their role - a console asks once and gets the colour, no second mapping.
    outcome_slot_count = 128
    outcome_slots: List[Optional[Tuple[int, int, str]]] = [None] * outcome_slot_count
    for role, words_list in (('good', policy_value(policy, 'outcome_good_words')),
                             ('bad', policy_value(policy, 'outcome_bad_words'))):
        for word in words_list:
            lowered = word.lower()
            digest = fnv1a_hash(lowered)
            if digest == 0 or not (1 <= len(lowered) <= 255):
                raise ValueError(f"outcome word '{word}' cannot be hashed safely")
            idx = digest & (outcome_slot_count - 1)
            while outcome_slots[idx] is not None:
                if outcome_slots[idx][0] == digest and outcome_slots[idx][1] == len(lowered):
                    raise ValueError(
                        f"outcome word '{word}' collides with an existing entry in hash AND "
                        f"length - hash identity would lie, rename or drop one side")
                idx = (idx + 1) & (outcome_slot_count - 1)
            outcome_slots[idx] = (digest, len(lowered), text_sgrs[role])

    out += ['};', '', '}  // namespace', '',
            'const char* display_word(uint32_t hash, uint32_t len) {',
            '    uint32_t idx = hash & kDisplaySlotMask;',
            '    while (g_display_slots[idx].hash != 0u) {',
            '        if (g_display_slots[idx].hash == hash && g_display_slots[idx].len == len) {',
            '            return g_display_slots[idx].few;',
            '        }',
            '        idx = (idx + 1u) & kDisplaySlotMask;',
            '    }',
            '    return nullptr;',
            '}', '',
            '// Der graue Zeitstempel jedes Formatstrings, als SGR-Parameter: die Politik nennt',
            '// den SHA-Palettennamen, der Wert kommt aus colors.ts. log_sys.cpp setzt seine',
            '// Muster zur Laufzeit daraus zusammen - eine Farbaenderung ist ein colors.ts-Edit,',
            '// der SHA-Generatorlauf und dieser eine Datei-Recompile, nie ein Baum-Rebuild.',
            'const char* display_timestamp_sgr() {',
            f'    return "{timestamp_sgr}";',
            '}', '',
            '// Die Farbe je Level-Tag (TRC..CRT), aus derselben Politik. Ein Index ausserhalb',
            '// des Bestands antwortet mit dem leeren String - der Aufrufer druckt dann ohne',
            '// Farbe, nie mit einer falschen.',
            'const char* display_level_sgr(uint32_t level) {']
    out.append('    static const char* const kLevelSgr[] = {')
    for name, sgr in zip(('trace', 'debug', 'info', 'warn', 'error', 'critical'), level_sgrs):
        out.append(f'        "{sgr}",  // {name}')
    out += ['    };',
            '    if (level < sizeof(kLevelSgr) / sizeof(kLevelSgr[0])) {',
            '        return kLevelSgr[level];',
            '    }',
            '    return "";',
            '}', '']
    out += ['// Die semantischen Rollenfarben der Konsolen-Textfaerbung, je Rolle ein Getter -',
            '// die Politik nennt je Rolle einen SHA-Palettennamen (text_colors), der Wert',
            '// kommt aus colors.ts ueber die lokale Down-Top-Kopie.']
    for name in ('string', 'number', 'url', 'path', 'tag', 'punct'):
        out += [f'const char* display_sgr_{name}() {{',
                f'    return "{text_sgrs[name]}";',
                '}', '']
    out += ['// Ergebnis-Woerter: EIN O(1)-Probe ueber gute und schlechte Woerter zugleich,',
            '// der Treffer liefert direkt die SGR-Parameter seiner Rolle; nullptr heisst',
            '// kein Ergebnis-Wort. Hash plus Laenge ist die Identitaet, In-Set-Kollisionen',
            '// bricht der Generatorlauf.',
            'namespace {', '',
            f'constexpr uint32_t kOutcomeSlotMask = {outcome_slot_count - 1}u;', '',
            f'constexpr DisplaySlot g_outcome_slots[{outcome_slot_count}] = {{']
    for s in outcome_slots:
        if s is None:
            out.append('    {0u, 0, nullptr},')
        else:
            out.append(f'    {{{s[0]}u, {s[1]}, "{_cpp_str(s[2])}"}},')
    out += ['};', '', '}  // namespace', '',
            'const char* display_outcome_sgr(uint32_t hash, uint32_t len) {',
            '    uint32_t idx = hash & kOutcomeSlotMask;',
            '    while (g_outcome_slots[idx].hash != 0u) {',
            '        if (g_outcome_slots[idx].hash == hash && g_outcome_slots[idx].len == len) {',
            '            return g_outcome_slots[idx].few;',
            '        }',
            '        idx = (idx + 1u) & kOutcomeSlotMask;',
            '    }',
            '    return nullptr;',
            '}', '',
            '}  // namespace ase::log::detail', '']
    return write_if_changed(output_path, '\n'.join(out))


def _flat_cats(block: dict, seen: Dict[int, str]) -> List[dict]:
    """Read one category list and attach each entry's FNV-1a hash.

    THE HASHED STRING IS "<NAMESPACE>_<NAME>", NOT THE BARE NAME - and that prefix is the
    whole reason the two levels can mirror each other. Every category exists on both levels
    wherever it makes sense, so the same NAME appears twice on purpose (HOST_OP_FAILED,
    CAPACITY_REACHED, ...). Hashing the bare name would give both the identical value, and
    then ERR::CAT::HOST_OP_FAILED and WRN::CAT::HOST_OP_FAILED would be the SAME constant:
    the namespaces would still separate them where they are written, and nothing would
    separate them where they are looked up. Passing the WRN constant to log::error would
    print the ERR help text - a plausible wrong answer, which is the exact failure this
    whole rebuild exists to remove.

    `seen` is shared across BOTH blocks, so a collision is caught wherever it happens. It
    cannot be worked around at the call site and it cannot be seen in a log line: two
    categories would answer to one value and the second one would silently print the
    first one's help text.
    """
    out: List[dict] = []
    for cat in block['categories']:
        name = cat['name']
        key = f"{block['namespace']}_{name}"
        digest = fnv1a_hash(key)
        if digest in seen:
            raise ValueError(
                f"\"{key}\" and \"{seen[digest]}\" both hash to {digest}. Rename one - a "
                f"collision makes two categories indistinguishable at runtime, and the "
                f"second one would print the first one's help text.")
        seen[digest] = key
        entry = dict(cat)
        entry['hash'] = digest
        entry['hash_key'] = key
        out.append(entry)
    return out


def _flat_pair(cats: dict) -> Tuple[List[dict], List[dict]]:
    """Both blocks against ONE collision set - see _flat_cats."""
    seen: Dict[int, str] = {}
    return _flat_cats(cats['err'], seen), _flat_cats(cats['wrn'], seen)


def _cpp_str(text: str) -> str:
    """Escape a JSON string for a C++ string literal."""
    return text.replace('\\', '\\\\').replace('"', '\\"')


def _wrap_literal(text: str, indent: str, width: int = 96, trailer: str = ',') -> str:
    """Split a long help text into adjacent string literals so no line runs off.

    `trailer` is what follows the last literal. The tables wanted a comma; the lookup
    functions want a semicolon because the literal is now the operand of a `return`.
    """
    escaped = _cpp_str(text)
    if len(escaped) <= width:
        return f'{indent}"{escaped}"{trailer}'
    words, lines, current = escaped.split(' '), [], ''
    for word in words:
        if current and len(current) + len(word) + 1 > width:
            lines.append(current)
            current = word
        else:
            current = f"{current} {word}" if current else word
    if current:
        lines.append(current)
    body = f'\n{indent}    '.join(f'"{ln} "' if i < len(lines) - 1 else f'"{ln}"'
                                  for i, ln in enumerate(lines))
    return f'{indent}{body}{trailer}'


def generate_err_cat_header(cats: dict, output_path: Path) -> bool:
    """Constants + getter declarations. NO tables - this header is included everywhere."""
    err, wrn = _flat_pair(cats)

    out = ['#pragma once', '', '/**', ' * ASE CORE INFRASTRUCTURE HEADER', ' *',
           ' * AUTO-GENERATED FILE - DO NOT EDIT!',
           ' * Edit core/ase-log/data/log_categories.json and run',
           ' * core/ase-log/scripts/gen_log_cat.py instead. A hand fix here survives',
           ' * exactly until the next generator run.', ' *',
           ' * @file        log_err_cat_gen.hpp',
           ' * @design      DSGN_021',
           ' * @brief       ERR::CAT and WRN::CAT name hashes plus the lookup declarations',
           ' * @description Constants only - each one is the FNV-1a hash of its own NAME.',
           ' *              There is no id, no table and no counter: a category cannot be',
           ' *              renumbered, so inserting or reordering one changes nothing that',
           ' *              already exists. The name/help/suffix lookups live in',
           ' *              log_err_cat_gen.cpp, so adding a category recompiles ONE file',
           ' *              instead of the tree.',
           ' *', ' * @module      ase-log', ' * @layer       1 (Core)',
           ' * @category    ecs/module', ' * @created     2026-08-23',
           f' * @modified    {GEN_DATE}',
           ' * @version     1.0.0', ' *',
           ' * CORE INFRASTRUCTURE COMPLIANCE', ' *'] \
        + [f' * {line}' for line in CORE_HEADER_CHECKLIST] \
        + [' *',
           ' * Generated by: core/ase-log/scripts/gen_log_cat.py',
           ' * Source: core/ase-log/data/log_categories.json', ' */', '',
           '#include <cstdint>', '', 'namespace ase::log {', '']

    for block, slots in (('ERR', err), ('WRN', wrn)):
        out.append(f'namespace {block} {{')
        out.append('namespace CAT {')
        for slot in slots:
            doc = slot.get('doc', '')
            if doc:
                for line in _doc_lines(doc):
                    out.append(f'    // {line}')
            out.append(f"    // FNV-1a of \"{slot['hash_key']}\" - the NAMESPACE is part of the")
            out.append('    // hashed string, so the mirrored halves are two distinct values.')
            out.append(f"    constexpr uint32_t {slot['name']} = {slot['hash']}u;")
        out.append('}  // namespace CAT')
        out.append(f'}}  // namespace {block}')
        out.append('')

    out.append('namespace detail {')
    out.append('    // NO counter and NO pinned-id asserts - both were artefacts of the table')
    out.append('    // scheme. A constant is now the hash of its own name, so it cannot shift')
    out.append('    // when a neighbour is inserted or removed: the protection is structural')
    out.append('    // instead of asserted. What CAN still go wrong is a hash collision inside')
    out.append('    // one namespace, and that is caught in the generator, where it can name')
    out.append('    // both categories - a static_assert here could only say that two numbers')
    out.append('    // differ.')
    out.append('    //')
    out.append('    // Defined in log_err_cat_gen.cpp. Header-only consumers link against')
    out.append('    // ase::log or resolve these at dlopen time (RTLD_GLOBAL), the same way')
    out.append('    // log_cat_gen.cpp is already consumed by 105 modules and plugins.')
    out.append('    const char* get_cat_name(uint32_t cat);')
    out.append('    const char* get_cat_help(uint32_t cat);')
    out.append('    const char* get_cat_suffix(uint32_t cat);')
    out.append('    const char* get_wrn_cat_name(uint32_t cat);')
    out.append('    const char* get_wrn_cat_help(uint32_t cat);')
    out.append('    // Die SHORT-Texte derselben SSOT: die Essenz, die log.hpp ans Zeilenende haengt.')
    out.append('    const char* get_cat_short(uint32_t cat);')
    out.append('    const char* get_wrn_cat_short(uint32_t cat);')
    out.append('    // Der TAIL ist der hint als EIGENE physische Zeile: Newline, Einzug aus der')
    out.append('    // Anzeige-Politik, dann der Rat - sonst nichts. Das Paar ist EIN Datensatz, ein')
    out.append('    // hint steht nie ohne seinen Kopf direkt darueber; eine eigene Marke waere die')
    out.append('    // Wiederholung dessen, was der Kopf schon sagt. Leer, wenn die Kategorie keinen')
    out.append('    // hint fuehrt - die Meldung bleibt dann einzeilig. log.hpp haengt ihn als letztes')
    out.append('    // Argument an; damit traegt JEDER Kanal denselben zweizeiligen Datensatz.')
    out.append('    const char* get_cat_tail(uint32_t cat);')
    out.append('    const char* get_wrn_cat_tail(uint32_t cat);')
    out.append('}  // namespace detail')
    out.append('')
    out.append('}  // namespace ase::log')
    out.append('')
    return write_if_changed(output_path, '\n'.join(out))


def _doc_lines(doc: str, width: int = 88) -> List[str]:
    """Wrap a doc string to comment width."""
    words, lines, current = doc.split(), [], ''
    for word in words:
        if current and len(current) + len(word) + 1 > width:
            lines.append(current)
            current = word
        else:
            current = f"{current} {word}" if current else word
    if current:
        lines.append(current)
    return lines


def shorten_display_text(text: str, resolved: Dict[str, str], fillers: Set[str]) -> str:
    """The display shortening applied AT GENERATION to the short and hint texts.

    Mirrors the runtime walk in log_display.cpp over prose: whole words of
    letters and digits, lowercased hash identity, abbreviation from the resolved
    taxonomy table, fillers dropped together with the space in front of them.
    In the record every short and hint stands behind a space or an indent, so
    the leading-filler exception of the runtime walk never applies here - the
    text is shortened as if space-preceded, and the result is a FIXED POINT of
    the runtime pass: the formatter that shortens the live values walks these
    texts too and must not move a single byte in them."""
    import re
    out: List[str] = []
    pos = 0
    eat_leading_space = False
    for m in re.finditer(r"[A-Za-z0-9]+", text):
        gap = text[pos:m.start()]
        if eat_leading_space and gap.startswith(' '):
            gap = gap[1:]
        eat_leading_space = False
        out.append(gap)
        word = m.group(0)
        lw = word.lower()
        if lw in fillers:
            joined = ''.join(out)
            if joined.endswith(' '):
                out[-1] = out[-1][:-1]
            elif joined.strip() == '':
                # Space-preceded semantics at the very start: in the record every short and
                # hint stands behind a space or an indent, so a LEADING filler drops here too,
                # together with the space that FOLLOWS it - and no emitted line ever begins
                # with a filler, which is what makes the runtime walk (it would eat one indent
                # column in that case) a no-op.
                eat_leading_space = True
            else:
                out.append(word)
        elif lw in resolved:
            out.append(resolved[lw])
        else:
            out.append(word)
        pos = m.end()
    tail_gap = text[pos:]
    if eat_leading_space and tail_gap.startswith(' '):
        tail_gap = tail_gap[1:]
    out.append(tail_gap)
    return ''.join(out)


def generate_err_cat_impl(cats: dict, policy: dict, sha, display_words: Dict[str, str],
                          output_path: Path) -> bool:
    """The nine lookup functions. No tables, no counters, no size asserts.

    The TAIL of a category is the hint rendered as its own physical line: a real
    newline, the indent from the display policy, then the hint sentence - nothing
    else. The pair is written as ONE record, so a hint line never exists without
    its head directly above it and needs no marker of its own; repeating the
    category name here would only duplicate what the head already says. It is
    COMPOSED HERE, at generation time, so the format layer appends one argument
    and every channel - file, tail, console - carries the identical two-line
    record. An empty hint (UNKNOWN) yields an empty tail and the record stays a
    single line.

    SHORT and TAIL are emitted ALREADY SHORTENED (shorten_display_text): the
    record carries the same abbreviated wording in every channel - a console
    that shortened for itself would show different bytes than a raw tail, and
    exactly that mismatch was measured live between cli and dist. The hint is
    ONE physical line, deliberately UNWRAPPED: one byte stream is read by
    consoles of every width, and a fixed generation-time wrap tears on any
    console narrower than it while ending early on any wider one (both measured
    live). Width-correct wrapping happens where the width is KNOWN - the render
    surfaces (CLI TUI sink, ase-logview on the tail path) share one geometry
    walk. A tail that exists closes with a trailing newline: together with the
    writer's own line end this puts ONE blank line under the record in every
    channel, so the next record does not glue onto the hint block. The
    full-wording help getter stays unshortened - it is not part of the
    record."""
    err, wrn = _flat_pair(cats)
    fillers = {w.lower() for w in policy_value(policy, 'fillers')}

    def display_text(text: str) -> str:
        once = shorten_display_text(text, display_words, fillers)
        twice = shorten_display_text(once, display_words, fillers)
        if once != twice:
            raise ValueError(
                f"display text is not a fixed point of the shortening walk - the runtime "
                f"formatter would rewrite it and the channels would diverge: '{once}' vs "
                f"'{twice}'")
        return once

    indent_cols = int(policy_value(policy, 'hint_line_indent'))
    hint_pad = ' ' * indent_cols
    # JEDE Farbe des Datensatzes wird HIER einkompiliert - die Tier-Logdateien tragen ANSI
    # als Systemsprache (log_sys.cpp), und nur eine Farbe im Datensatz ist in jedem Kanal
    # dieselbe. Die Politik nennt Farb-NAMEN, die WERTE kommen aus der SHA-Palette
    # (colors.ts) ueber die lokale Down-Top-Kopie in data/generated/ - keine Fallbacks.
    # hint_wrap_columns bricht den hint schon an der ERZEUGUNG in mehrere physische Zeilen,
    # jede mit Einzug und eigener Farbe: ein roher tail kann nicht umbrechen, und eine Zeile
    # ohne Einzug saehe dort zerrissen aus.
    hint_sgr = resolve_color_sgr(sha, policy_value(policy, 'hint_color'))
    cat_sgr = resolve_color_sgr(sha, policy_value(policy, 'category_color'))

    out = ['/**', ' * ASE CORE INFRASTRUCTURE IMPLEMENTATION', ' *',
           ' * AUTO-GENERATED FILE - DO NOT EDIT!',
           ' * Edit core/ase-log/data/log_categories.json and run',
           ' * core/ase-log/scripts/gen_log_cat.py instead. A hand fix here survives',
           ' * exactly until the next generator run.', ' *',
           ' * @file        log_err_cat_gen.cpp',
           ' * @brief       Name, help, short and suffix lookups for ERR::CAT and WRN::CAT',
           ' * @description Each function matches the name hash it was given and returns the',
           ' *              text that belongs to it. A hash that is NOT in the SSOT gets a',
           ' *              LOUD marker, never the first entry: falling back to a real',
           ' *              category would print a plausible wrong help text, and a reader who',
           ' *              sees a valid-looking category does not check it. Kept in one',
           ' *              translation unit so a changed help text recompiles this file,',
           ' *              not the tree.',
           ' *', ' * @module      ase-log', ' * @layer       1 (Core)',
           ' * @category    ecs/module', ' * @created     2026-08-23',
           f' * @modified    {GEN_DATE}',
           ' * @version     1.0.0', ' *',
           ' * CORE INFRASTRUCTURE IMPLEMENTATION COMPLIANCE', ' *'] \
        + [f' * {line}' for line in CORE_IMPL_CHECKLIST] \
        + [' *',
           ' * Generated by: core/ase-log/scripts/gen_log_cat.py',
           ' * Source: core/ase-log/data/log_categories.json', ' */', '',
           '#include <ase/log/log_err_cat_gen.hpp>', '',
           'namespace ase::log::detail {', '']

    # Was der Aufrufer sieht, wenn er einen Hash uebergibt, den die SSOT nicht kennt.
    # Der Name traegt die Marke, der Hilfetext nennt die Datei, das Suffix bleibt LEER:
    # ein Suffix haengt an JEDER Meldung seiner Kategorie und wuerde die Zeile verunstalten,
    # ohne etwas zu sagen, das der Name nicht schon sagt. Der Kurztext-Rueckfall steht in
    # der KOPFZEILE, also kurz, symbolfrei und laut.
    unknown_name = 'UNKNOWN_CATEGORY'
    unknown_help = ('The value passed is not a category in '
                    'core/ase-log/data/log_categories.json. Check the constant at the call '
                    'site: a category is ERR::CAT::<NAME> or WRN::CAT::<NAME>, and its value '
                    'is the hash of that NAME - a hand-written number cannot be one.')
    unknown_short = 'category value not in the SSOT'

    def emit_return(value: str, indent: str, kind: str) -> None:
        # 'tail': the hint as ONE physical line - a "\n", the hint colour and the indent in
        # the FIRST literal (because _wrap_literal splits on spaces and would silently eat
        # leading blanks), the reset and a trailing newline at the end: together with the
        # writer's own line end that newline puts the one blank line under the record - in
        # every channel, from the record itself, never from a console. Deliberately NO
        # generation-time wrap: the render surfaces wrap at their REAL width (see the
        # function docstring).
        # 'name': the category name wrapped in the category colour - the name BYTES stay
        # intact, so grep still matches. 'text': plain.
        if not value:
            out.append(f'{indent}return "";')
            return
        if kind == 'tail':
            out.append(f'{indent}return')
            out.append(f'{indent}    "\\n\\x1b[{hint_sgr}m{hint_pad}"')
            out.append(_wrap_literal(value, f'{indent}    ', trailer=''))
            out.append(f'{indent}    "\\x1b[0m\\n";')
            return
        if kind == 'name':
            out.append(f'{indent}return "\\x1b[{cat_sgr}m{_cpp_str(value)}\\x1b[0m";')
            return
        if len(_cpp_str(value)) <= 84:
            out.append(f'{indent}return "{_cpp_str(value)}";')
        else:
            out.append(f'{indent}return')
            out.append(_wrap_literal(value, f'{indent}    ', trailer=';'))

    getters = (
        ('get_cat_name', err, 'name', 'ERR', unknown_name, 'name'),
        ('get_cat_help', err, 'hint', 'ERR', unknown_help, 'text'),
        ('get_cat_short', err, 'short', 'ERR', unknown_short, 'text'),
        ('get_cat_tail', err, 'hint', 'ERR', unknown_help, 'tail'),
        ('get_cat_suffix', err, 'suffix', 'ERR', '', 'text'),
        ('get_wrn_cat_name', wrn, 'name', 'WRN', unknown_name, 'name'),
        ('get_wrn_cat_help', wrn, 'hint', 'WRN', unknown_help, 'text'),
        ('get_wrn_cat_short', wrn, 'short', 'WRN', unknown_short, 'text'),
        ('get_wrn_cat_tail', wrn, 'hint', 'WRN', unknown_help, 'tail'),
    )
    for fn, slots, field, block, fallback, kind in getters:
        # SHORT and TAIL leave the generator ALREADY shortened; the full-wording help getter
        # does not - it is documentation, not part of the record.
        shorten_it = kind == 'tail' or fn.endswith('_short')
        if shorten_it and fallback:
            fallback = display_text(fallback)
        out.append(f'const char* {fn}(uint32_t cat) {{')
        for slot in slots:
            value = slot.get(field, '')
            if shorten_it:
                value = display_text(value)
            out.append(f"    if (cat == {block}::CAT::{slot['name']}) {{")
            emit_return(value, '        ', kind)
            out.append('    }')
        out.append('')
        if not fallback:
            out.append('    // Kein Suffix - siehe Generatorkommentar: die Marke steht im NAMEN.')
            out.append('    return "";')
        else:
            emit_return(fallback, '    ', kind)
        out.append('}')
        out.append('')
    out.append('}  // namespace ase::log::detail')
    out.append('')
    return write_if_changed(output_path, '\n'.join(out))


def write_if_changed(path: Path, content: str) -> bool:
    """Write file only if content changed. Returns True if changed."""
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        old_content = path.read_text(encoding='utf-8')
        if old_content == content:
            return False

    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)
    return True


# ---------------------------------------------------------------------------
# Main with Professional Output
# ---------------------------------------------------------------------------

def main():
    script_dir = Path(__file__).parent  # ase-log/scripts/
    ase_log_dir = script_dir.parent     # ase-log/
    project_root = ase_log_dir.parent.parent  # ase/ (up from core/ase-log)

    taxonomy_dir = (project_root / "tools" / "ase-forge" / "ase-validator"
                    / "ecs_validator" / "data" / "taxonomy")
    hub_data_dir = project_root / "modules" / "ase-hub" / "data"
    header_path = ase_log_dir / "include" / "ase" / "log" / "log_cat_gen.hpp"
    impl_path = ase_log_dir / "src" / "log_cat_gen.cpp"

    if not taxonomy_dir.exists():
        print(f"Error: Taxonomy directory not found: {taxonomy_dir}", file=sys.stderr)
        sys.exit(1)

    # Count taxonomy files
    taxonomy_files = len([f for f in taxonomy_dir.glob("*.json") if not f.name.startswith('_')]) + 1

    # Load taxonomy tokens for stats
    taxonomy_abbrevs: Set[str] = set()
    index_file = taxonomy_dir / "_index.json"
    if index_file.exists():
        with open(index_file, 'r', encoding='utf-8') as f:
            data = json.load(f)
            for cat in data.get('categories', []):
                taxonomy_abbrevs.add(cat['abbrev'].upper())
    for f in taxonomy_dir.glob("*.json"):
        if f.name.startswith('_'):
            continue
        try:
            with open(f, 'r', encoding='utf-8') as fp:
                data = json.load(fp)
                extract_abbrevs_recursive(data, taxonomy_abbrevs)
        except Exception:
            pass

    # Load hub stats
    hub_stats = {}
    if hub_data_dir.exists():
        const_parts, metric_parts, tag_parts, const_count, metric_count, tag_count = \
            load_hub_value_parts(hub_data_dir)
        hub_stats = {
            'const_parts': const_parts, 'const_count': const_count,
            'metric_parts': metric_parts, 'metric_count': metric_count,
            'tag_parts': tag_parts, 'tag_count': tag_count,
        }

    # Load module prefixes
    module_prefixes = _scan_module_prefixes(project_root)

    # Generate
    abbrev_bits, name_aliases = load_all_abbreviations(taxonomy_dir, project_root)
    num_categories = len(abbrev_bits)
    num_aliases = len(name_aliases)

    header_changed = generate_header(abbrev_bits, header_path)
    impl_changed = generate_impl(abbrev_bits, name_aliases, impl_path)

    # ERR::CAT / WRN::CAT from their own SSOT. Separate output files: the filter bits above
    # answer "is this log line emitted", these answer "what kind of fault is it" - one shared
    # generator, two unrelated questions.
    cat_data_path = ase_log_dir / "data" / "log_categories.json"
    err_header_path = ase_log_dir / "include" / "ase" / "log" / "log_err_cat_gen.hpp"
    err_impl_path = ase_log_dir / "src" / "log_err_cat_gen.cpp"
    cat_data = load_log_categories(cat_data_path)
    display_policy = load_display_policy(ase_log_dir / "data" / "log_display.json")
    validate_display_texts(cat_data, display_policy)
    sha_colors = load_sha_colors(ase_log_dir)
    err_header_changed = generate_err_cat_header(cat_data, err_header_path)
    # Display word table BEFORE the category impl: short and tail leave the generator
    # already shortened against exactly this table, so the record carries the same bytes
    # in every channel and the hint wrap is computed on the final text.
    display_words = resolve_display_words(cat_data, display_policy, taxonomy_dir)
    err_impl_changed = generate_err_cat_impl(cat_data, display_policy, sha_colors,
                                             display_words, err_impl_path)
    _err_cats, _wrn_cats = _flat_pair(cat_data)
    err_count, wrn_count = len(_err_cats), len(_wrn_cats)

    display_impl_path = ase_log_dir / "src" / "log_display_gen.cpp"
    display_changed = generate_display_impl(display_words, display_policy, sha_colors,
                                            display_impl_path)

    # === Professional Output ===

    # Taxonomy
    section_header(f"Taxonomy ({taxonomy_files} files)", 39)
    register_labels(["category", "...", "total"])
    sorted_tax = sorted(taxonomy_abbrevs)
    for tok in sorted_tax[:5]:
        section_line(CHECK, "category", f"{C.CYAN}{tok}{C.RESET}")
    if len(sorted_tax) > 5:
        section_pipe()
        section_line(SKIP, "...", f"{C.MUTED}({len(sorted_tax) - 5} more){C.RESET}")
        section_pipe()
    section_line(CHECK, "total", f"{C.CYAN}{len(taxonomy_abbrevs):,}{C.RESET}")

    # Hub Constants
    if hub_stats:
        section_header(f"Hub Constants ({hub_stats['const_count']} IDs)", 34)
        register_labels(["unique-parts", "new-tokens"])
        const_new = hub_stats['const_parts'] - taxonomy_abbrevs
        section_line(CHECK, "unique-parts", f"{C.CYAN}{len(hub_stats['const_parts']):,}{C.RESET}")
        section_line(CHECK, "new-tokens", f"{C.GREEN}{len(const_new):,}{C.RESET}")

        # Hub Metrics
        section_header(f"Hub Metrics ({hub_stats['metric_count']} IDs)", 110)
        register_labels(["unique-parts", "new-tokens"])
        metric_new = hub_stats['metric_parts'] - taxonomy_abbrevs - hub_stats['const_parts']
        section_line(CHECK, "unique-parts", f"{C.CYAN}{len(hub_stats['metric_parts']):,}{C.RESET}")
        section_line(CHECK, "new-tokens", f"{C.GREEN}{len(metric_new):,}{C.RESET}")

        # Hub Tags
        section_header(f"Hub Tags ({hub_stats['tag_count']} IDs)", 141)
        register_labels(["unique-parts", "new-tokens"])
        tag_new = (hub_stats['tag_parts'] - taxonomy_abbrevs
                   - hub_stats['const_parts'] - hub_stats['metric_parts'])
        section_line(CHECK, "unique-parts", f"{C.CYAN}{len(hub_stats['tag_parts']):,}{C.RESET}")
        section_line(CHECK, "new-tokens", f"{C.GREEN}{len(tag_new):,}{C.RESET}")

    # Module Prefixes
    section_header(f"Module Prefixes ({len(module_prefixes)} dirs)", 214)
    register_labels(["alias", "...", "alias-total"])
    for name, abbrev in sorted(name_aliases)[:5]:
        section_line(CHECK, "alias", f"{C.MUTED}{name.lower()}{C.RESET} \u2192 {C.CYAN}{abbrev}{C.RESET}")
    if len(name_aliases) > 5:
        section_pipe()
        section_line(SKIP, "...", f"{C.MUTED}({len(name_aliases) - 5} more){C.RESET}")
        section_pipe()
    section_line(CHECK, "alias-total", f"{C.CYAN}{num_aliases}{C.RESET}")

    # Output
    section_header("Output", 71)
    register_labels([header_path.name, impl_path.name])
    if header_changed:
        section_line(CROSS, header_path.name, f"{C.YELLOW}CHANGED (triggers rebuild){C.RESET}")
    else:
        section_line(CHECK, header_path.name, f"{C.GREEN}UNCHANGED (no rebuild){C.RESET}")
    if impl_changed:
        section_line(CHECK, impl_path.name, f"{C.GREEN}CHANGED (1 file recompile){C.RESET}")
    else:
        section_line(CHECK, impl_path.name, f"{C.GREEN}UNCHANGED{C.RESET}")

    # Fault Categories
    section_header(f"Fault Categories (ERR {err_count} / WRN {wrn_count})", 203)
    register_labels([err_header_path.name, err_impl_path.name, display_impl_path.name,
                     "display-words", "source", "syntax"])
    if display_changed:
        section_line(CHECK, display_impl_path.name, f"{C.GREEN}CHANGED (1 file recompile){C.RESET}")
    else:
        section_line(CHECK, display_impl_path.name, f"{C.GREEN}UNCHANGED{C.RESET}")
    section_line(CHECK, "display-words", f"{C.CYAN}{len(display_words)}{C.RESET}")
    if err_header_changed:
        section_line(CROSS, err_header_path.name, f"{C.YELLOW}CHANGED (triggers rebuild){C.RESET}")
    else:
        section_line(CHECK, err_header_path.name, f"{C.GREEN}UNCHANGED (no rebuild){C.RESET}")
    if err_impl_changed:
        section_line(CHECK, err_impl_path.name, f"{C.GREEN}CHANGED (1 file recompile){C.RESET}")
    else:
        section_line(CHECK, err_impl_path.name, f"{C.GREEN}UNCHANGED{C.RESET}")
    section_line(CHECK, "source", f"{C.MUTED}{cat_data_path.name}{C.RESET}")
    if "--check" in sys.argv:
        ok, first_error = syntax_check_generated(project_root, err_header_path, err_impl_path)
        if ok:
            section_line(CHECK, "syntax", f"{C.GREEN}g++ -fsyntax-only OK{C.RESET}")
        else:
            section_line(CROSS, "syntax", f"{C.RED}{first_error[:90]}{C.RESET}")

    # Summary
    capacity = 16384
    pct = (num_categories / capacity) * 100
    section_header(f"Summary ({num_categories:,})", 179)
    register_labels(["taxonomy", "hub-constants", "hub-metrics", "hub-tags",
                     "module-prefix", "module-alias", "unique-bits", "capacity", "header"])
    section_line(CHECK, "taxonomy", f"{C.CYAN}{len(taxonomy_abbrevs):,}{C.RESET}")
    if hub_stats:
        section_line(CHECK, "hub-constants", f"{C.CYAN}{len(hub_stats['const_parts']):,}{C.RESET}")
        section_line(CHECK, "hub-metrics", f"{C.CYAN}{len(hub_stats['metric_parts']):,}{C.RESET}")
        section_line(CHECK, "hub-tags", f"{C.CYAN}{len(hub_stats['tag_parts']):,}{C.RESET}")
    section_line(CHECK, "module-prefix", f"{C.CYAN}{len(module_prefixes)}{C.RESET}")
    section_line(CHECK, "module-alias", f"{C.CYAN}{num_aliases}{C.RESET}")
    section_line(CHECK, "unique-bits", f"{C.YELLOW}{num_categories:,}{C.RESET}")

    cap_str = f"{C.YELLOW}{num_categories:,}{C.RESET} / {C.MUTED}{capacity:,}{C.RESET} ({C.CYAN}{pct:.0f}%{C.RESET})"
    if pct > 100:
        section_line(CROSS, "capacity", f"{cap_str} {C.RED}<- OVER!{C.RESET}")
    elif pct > 90:
        section_line(WARN, "capacity", f"{cap_str} {C.YELLOW}<- WARN >90%{C.RESET}")
    else:
        section_line(CHECK, "capacity", cap_str)

    if not header_changed:
        section_line(CHECK, "header", f"{C.GREEN}UNCHANGED{C.RESET} \u2192 {C.MUTED}no cascade{C.RESET}")
    section_pipe()


if __name__ == '__main__':
    main()
