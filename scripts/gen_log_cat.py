#!/usr/bin/env python3
"""
Generate Log Categories Header + Implementation
================================================
Reads taxonomy JSON files + Hub JSON files and generates C++ header AND implementation.

Usage:
    python core/core/ase-log/scripts/gen_log_cat.py

Output:
    core/core/ase-log/include/ase/log/log_cat_gen.hpp  (~3KB - declarations only)
    core/core/ase-log/src/log_cat_gen.cpp              (~200KB - libcuckoo init table)

The header is small and safe to include everywhere.
The init table + libcuckoo engine is compiled ONCE in the .cpp.
"""

import json
import sys
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple

# ---------------------------------------------------------------------------
# Console framework (SSOT terminal theme from sha-web-console)
# ---------------------------------------------------------------------------

SCRIPT_DIR = Path(__file__).parent.resolve()
ASE_LOG_DIR = SCRIPT_DIR.parent
PROJECT_ROOT = ASE_LOG_DIR.parent.parent.parent

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / "clients" / "sha-client-web" / "sha-web-console" / "python"))
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
                       project_root / "core" / "core",
                       project_root / "core" / "foundation",
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
        ' * AUTO-GENERATED FILE - DO NOT EDIT!',
        ' * ',
        ' * Generated by: core/core/ase-log/scripts/gen_log_cat.py',
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
        '',
        'extern std::atomic<uint8_t> g_level_mask;',
        'extern CategoryMask g_blocked_categories;',
        'extern CategoryMask g_whitelisted_categories;  ///< Whitelist (empty = all pass)',
        'extern std::atomic<bool> g_has_whitelist;       ///< True if any +CATEGORY was set',
        '',
        '// ============================================================================',
        '// Client Filter State (defined in log_cat_gen.cpp)',
        '// ============================================================================',
        '',
        'extern std::atomic<uint64_t> g_client_mask;      ///< Whitelist (0 = all pass)',
        'extern std::atomic<uint64_t> g_blocked_clients;   ///< Blacklist bitmask',
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
        'bool should_log_loc(uint8_t level, const char* file, const char* func);',
        '',
        '/// O(1) cached filter check for client file+function',
        'bool should_log_client_loc(uint8_t level, const char* file, const char* func, uint64_t client_bit);',
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
        ' * AUTO-GENERATED FILE - DO NOT EDIT!',
        ' * ',
        ' * Generated by: core/core/ase-log/scripts/gen_log_cat.py',
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
        'std::atomic<uint8_t> g_level_mask{LVL_ALL};',
        'CategoryMask g_blocked_categories{};',
        'CategoryMask g_whitelisted_categories{};',
        'std::atomic<bool> g_has_whitelist{false};',
        'std::atomic<uint64_t> g_client_mask{0};',
        'std::atomic<uint64_t> g_blocked_clients{0};',
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
        lines.append(f'    {{{hash_val}u, BIT_{cpp_id}}},  '
                     f'// "{name.lower()}" (module prefix -> {abbrev.lower()})')

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
        'bool should_log_loc(uint8_t level, const char* file, const char* func) {',
        '    if (!(level & g_level_mask.load(std::memory_order_relaxed))) return false;',
        '    uintptr_t key = reinterpret_cast<uintptr_t>(file)',
        '                  ^ (reinterpret_cast<uintptr_t>(func) << 1);',
        '    uint8_t cached = 0;',
        '    if (loc_cache().find(key, cached)) return cached != 0;  // O(1) hit',
        '    // Cache miss: compute from file + func categories',
        '    CategoryMask cats = file_to_categories(file);',
        '    CategoryMask func_cats = func_to_categories(func);',
        '    // Merge: file categories + function categories',
        '    for (int i = 0; i < CHUNK_COUNT; ++i)',
        '        cats.chunks[i] |= func_cats.chunks[i];',
        '    bool pass = true;',
        '    if (cats.intersects(g_blocked_categories)) pass = false;',
        '    if (pass && g_has_whitelist.load(std::memory_order_relaxed))',
        '        if (!cats.intersects(g_whitelisted_categories)) pass = false;',
        '    loc_cache().insert(key, pass ? 1u : 0u);',
        '    return pass;',
        '}',
        '',
        'bool should_log_client_loc(uint8_t level, const char* file, const char* func, uint64_t client_bit) {',
        '    if (!(level & g_level_mask.load(std::memory_order_relaxed))) return false;',
        '    uintptr_t key = reinterpret_cast<uintptr_t>(file)',
        '                  ^ (reinterpret_cast<uintptr_t>(func) << 1);',
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
        '        if (cats.intersects(g_blocked_categories)) cat_pass = false;',
        '        if (cat_pass && g_has_whitelist.load(std::memory_order_relaxed))',
        '            if (!cats.intersects(g_whitelisted_categories)) cat_pass = false;',
        '        loc_cache().insert(key, cat_pass ? 1u : 0u);',
        '    }',
        '    if (!cat_pass) return false;',
        '    if (client_bit) {',
        '        if (client_bit & g_blocked_clients.load(std::memory_order_relaxed)) return false;',
        '        uint64_t mask = g_client_mask.load(std::memory_order_relaxed);',
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
        '        // +CLT:01 or +CLT:01,03 -> client whitelist',
        '        if (std::strncmp(code, "CLT:", 4) == 0) {',
        '            parse_client_ids(code + 4, g_client_mask);',
        '            return;',
        '        }',
        '        // +CLT -> all clients in whitelist',
        '        if (std::strcmp(code, "CLT") == 0) {',
        '            g_client_mask.store(0xFFFFFFFFFFFFFFFFULL, std::memory_order_relaxed);',
        '            return;',
        '        }',
        '        // +LEVEL -> enable specific level',
        '        if (std::strcmp(code, "TRC") == 0) { g_level_mask.fetch_or(LVL_TRC); return; }',
        '        if (std::strcmp(code, "DBG") == 0) { g_level_mask.fetch_or(LVL_DBG); return; }',
        '        if (std::strcmp(code, "INF") == 0) { g_level_mask.fetch_or(LVL_INF); return; }',
        '        if (std::strcmp(code, "WRN") == 0) { g_level_mask.fetch_or(LVL_WRN); return; }',
        '        if (std::strcmp(code, "ERR") == 0) { g_level_mask.fetch_or(LVL_ERR); return; }',
        '        if (std::strcmp(code, "CRT") == 0) { g_level_mask.fetch_or(LVL_CRT); return; }',
        '        // +CATEGORY -> whitelist',
        '        {',
        '            uint32_t hash = 2166136261u;',
        '            for (const char* p = code; *p; ++p) {',
        '                char c = (*p >= \'A\' && *p <= \'Z\') ? static_cast<char>(*p + 32) : *p;',
        '                hash = (hash ^ static_cast<uint32_t>(c)) * 16777619u;',
        '            }',
        '            int bit = abbrev_to_bit(hash);',
        '            if (bit >= 0) {',
        '                g_whitelisted_categories.set(bit);',
        '                g_has_whitelist.store(true, std::memory_order_relaxed);',
        '            }',
        '        }',
        '    } else if (op == \'-\') {',
        '        // -CLT -> block all clients',
        '        if (std::strcmp(code, "CLT") == 0) {',
        '            g_blocked_clients.store(0xFFFFFFFFFFFFFFFFULL, std::memory_order_relaxed);',
        '            return;',
        '        }',
        '        // -CLT:05 or -CLT:05,07 -> block specific clients',
        '        if (std::strncmp(code, "CLT:", 4) == 0) {',
        '            parse_client_ids(code + 4, g_blocked_clients);',
        '            return;',
        '        }',
        '        // -CATEGORY -> blacklist',
        '        uint32_t hash = 2166136261u;',
        '        for (const char* p = code; *p; ++p) {',
        '            char c = (*p >= \'A\' && *p <= \'Z\') ? static_cast<char>(*p + 32) : *p;',
        '            hash = (hash ^ static_cast<uint32_t>(c)) * 16777619u;',
        '        }',
        '        int bit = abbrev_to_bit(hash);',
        '        if (bit >= 0) g_blocked_categories.set(bit);',
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
        '    if (has_level) g_level_mask.store(0);',
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
    project_root = ase_log_dir.parent.parent.parent  # ase/ (up from core/core/ase-log)

    taxonomy_dir = project_root / "core" / "core" / "ase-validator" / "ecs_validator" / "data" / "taxonomy"
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
