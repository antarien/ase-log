#!/usr/bin/env python3
"""
Checks the token sources of the log filter against one bit space.

@file        validate_log_tokens.py
@brief       validate_log_tokens.py - checks the token sources of the log filter against one bit space
@description Reads the taxonomy, the Hub JSONs and the module prefixes, builds the
             bit table the generator builds, and reports every alias whose target
             falls on another bit and a token space over its capacity. The meaning
             of every finding and the warning threshold stand in the rule stock of
             the validator; this gate measures the place and asks the stock for
             the statement.
@module      ase-log
@layer       1 (Core)
@category    scripts
@modified    2026-10-28
@version     1.1.0

@design      DSGN_016
@usecase     usecase:100001 - check source files
@actor       Rule Compliance Checker (RULECHK)

USAGE
    python validate_log_tokens.py -?            # Help
    python validate_log_tokens.py -A            # All JSON sources
    python validate_log_tokens.py -A -q         # Summary only
    python validate_log_tokens.py -J taxonomy   # Only taxonomy JSONs
    python validate_log_tokens.py -J hub        # Only Hub JSONs
    python validate_log_tokens.py -J modules    # Only module prefix scan
    python validate_log_tokens.py --no-color    # No colors (pipe-compatible)
    python validate_log_tokens.py -A -O         # Show token overlaps
"""

import argparse
import json
import sys
from pathlib import Path
from typing import Dict, Set, Tuple

# Path setup: the token logic of gen_log_cat.py is reused, never written a second time.

SCRIPT_DIR = Path(__file__).parent.resolve()
ASE_LOG_DIR = SCRIPT_DIR.parent
PROJECT_ROOT = ASE_LOG_DIR.parent.parent

# Console framework (SSOT: sha-client-web/sha-web-console)
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "clients" / "sha-client-web" / "public" / "console" / "python"))
from console import (C, section_header, section_line, section_pipe, section_detail,
                      register_labels, tprint, c256, set_no_color, CHECK, CROSS, SKIP, HASH, ARROW, WARN)

# Token loading from gen_log_cat.py, the one place it is written.
sys.path.insert(0, str(SCRIPT_DIR))
from gen_log_cat import (
    load_all_abbreviations,
    extract_abbrevs_recursive,
    _scan_module_prefixes,
    fnv1a_hash,
)

# THIS GATE CARRIES NEITHER ITS MESSAGE TEXTS NOR ITS THRESHOLD. It measures, places and calls
# out; what a finding MEANS stands in the rule stock, and from which fill level it warns stands
# in `data/config.json`.
#
# WHY THE GATE IS A `.py`, with the evidence: it holds ENTRIES OF SEVERAL JSON SOURCES AGAINST
# EACH OTHER and computes bits. An alias and its target must fall on THE SAME bit; that only
# shows once both tables are built. A regex sees one entry and no bit. Point 3 of the order in
# `Skill(ase-validator)`.
#
# It lies in `core/ase-log/scripts/`, not in `scripts/prebuild/`; the list of the battery is
# `gates.conf`, and that is where it is registered.
sys.path.insert(0, str(PROJECT_ROOT / 'tools' / 'ase-forge' / 'ase-validator'))
from ecs_validator.validators.rule_engine import apply_structural_rule   # noqa: E402
from ecs_validator.validators.rule_loader import get_config_value        # noqa: E402

_RULE_ALIAS_BRUCH = 'LOG_TOKEN_ALIAS_BIT_MISMATCH'
_RULE_RAUM_VOLL = 'LOG_TOKEN_SPACE_EXHAUSTED'
_RULE_RAUM_KNAPP = 'LOG_TOKEN_SPACE_NEARLY_FULL'


def _regeltext(rid, variablen):
    """Meldetext AUS DEM BESTAND — kein Rueckfall auf einen eigenen Text.

    Fehlt die Regel, sagt das Tor, dass es nicht melden kann (rc=2), statt sich eine
    Formulierung auszudenken, die niemand pflegt. Genau dort saesse sonst wieder ein
    Zweitsystem — und es waere unsichtbar, weil es nur im Ausnahmefall spraeche.
    """
    regel = apply_structural_rule(rid, variablen)
    if regel is None:
        print("ERROR: rule %s missing from data/rules/ — nothing to report with" % rid,
              file=sys.stderr)
        return None
    return regel if isinstance(regel, dict) else getattr(regel, '__dict__', {})


# Hub JSON parsing

def load_hub_value_parts(hub_data_dir: Path, filename: str) -> Tuple[Set[str], int]:
    """Load a hub JSON file, split IDs at '_', return (unique_parts, total_ids)."""
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


def load_taxonomy_tokens(taxonomy_dir: Path) -> Tuple[Set[str], int, Dict[str, str]]:
    """Load all taxonomy abbreviations, return (tokens, file_count, noun_word_to_abbrev).

    A taxonomy file that cannot be read is named on stderr and left out of the count; its
    tokens are missing from the analysis, and the line says which file that is.
    """
    abbrevs: Set[str] = set()
    noun_word_to_abbrev: Dict[str, str] = {}
    file_count = 0

    index_file = taxonomy_dir / "_index.json"
    if index_file.exists():
        with open(index_file, 'r', encoding='utf-8') as f:
            data = json.load(f)
            for cat in data.get('categories', []):
                abbrevs.add(cat['abbrev'].upper())
        file_count += 1

    for f in sorted(taxonomy_dir.glob("*.json")):
        if f.name.startswith('_'):
            continue
        try:
            with open(f, 'r', encoding='utf-8') as fp:
                data = json.load(fp)
                extract_abbrevs_recursive(data, abbrevs, noun_word_to_abbrev)
            file_count += 1
        except (OSError, ValueError) as exc:
            print("WARNING: taxonomy file %s not read: %s" % (f.name, exc), file=sys.stderr)

    return abbrevs, file_count, noun_word_to_abbrev


# Main analysis

def run_analysis(args: argparse.Namespace) -> int:
    if args.no_color:
        set_no_color(True)

    taxonomy_dir = (PROJECT_ROOT / "tools" / "ase-forge" / "ase-validator"
                    / "ecs_validator" / "data" / "taxonomy")
    hub_data_dir = PROJECT_ROOT / "modules" / "ase-hub" / "data"

    if not taxonomy_dir.exists():
        print(f"Error: Taxonomy directory not found: {taxonomy_dir}", file=sys.stderr)
        return 2
    if not hub_data_dir.exists():
        print(f"Error: Hub data directory not found: {hub_data_dir}", file=sys.stderr)
        return 2

    show_all = args.all or not args.json_source
    show_taxonomy = show_all or args.json_source == "taxonomy"
    show_hub = show_all or args.json_source == "hub"
    show_modules = show_all or args.json_source == "modules"
    show_overlap = args.overlap
    quiet = args.quiet

    # Collect all token sets
    taxonomy_tokens: Set[str] = set()
    taxonomy_files = 0
    hub_const_parts: Set[str] = set()
    hub_const_ids = 0
    hub_metric_parts: Set[str] = set()
    hub_metric_ids = 0
    hub_tag_parts: Set[str] = set()
    hub_tag_ids = 0
    module_prefixes: Set[str] = set()
    noun_word_to_abbrev: Dict[str, str] = {}

    # Load taxonomy (also needed for module prefix noun-alias resolution)
    if show_taxonomy or show_modules or show_all:
        taxonomy_tokens, taxonomy_files, noun_word_to_abbrev = load_taxonomy_tokens(taxonomy_dir)

    # Load hub JSONs
    if show_hub or show_all:
        hub_const_parts, hub_const_ids = load_hub_value_parts(hub_data_dir, "hub_constants.json")
        hub_metric_parts, hub_metric_ids = load_hub_value_parts(hub_data_dir, "hub_metrics.json")
        hub_tag_parts, hub_tag_ids = load_hub_value_parts(hub_data_dir, "hub_tags.json")

    # Load module prefixes
    if show_modules or show_all:
        module_prefix_list = _scan_module_prefixes(PROJECT_ROOT)
        module_prefixes = {p.upper() for p in module_prefix_list}

    # Compute unique tokens from hub (excluding those already in taxonomy)
    hub_const_new = hub_const_parts - taxonomy_tokens
    hub_metric_new = hub_metric_parts - taxonomy_tokens - hub_const_parts
    hub_tag_new = hub_tag_parts - taxonomy_tokens - hub_const_parts - hub_metric_parts

    # All unique tokens
    all_tokens = taxonomy_tokens | hub_const_parts | hub_metric_parts | hub_tag_parts | module_prefixes
    total_unique = len(all_tokens)

    # Get existing abbrev_bits for actual bit count
    abbrev_bits, name_aliases = load_all_abbreviations(taxonomy_dir, PROJECT_ROOT)
    actual_bits = len(abbrev_bits)

    capacity = 16384  # Fixed capacity from header (256 chunks × 64 bits)

    # One label column for the whole script: every label of every section, those shown only
    # without --quiet and the two that always print, is registered before the first line.
    register_labels(["category", "...", "total", "part", "unique-parts", "new-tokens",
                     "alias", "index-alias", "noun-alias", "standalone", "taxonomy-match",
                     "shared", "shared-total", "BROKEN", "broken-total", "status",
                     "taxonomy", "hub-constants", "hub-metrics", "hub-tags",
                     "module-prefix", "unique-bits", "broken-aliases", "capacity"])

    if not quiet:
        # Taxonomy section
        if show_taxonomy:
            section_header(f"Taxonomy ({taxonomy_files} files)", 39)
            sorted_tax = sorted(taxonomy_tokens)
            display_count = min(5, len(sorted_tax))
            for tok in sorted_tax[:display_count]:
                section_line(CHECK, "category", f"{C.CYAN}{tok}{C.RESET}")
            if len(sorted_tax) > display_count:
                section_pipe()
                section_line(SKIP, "...", f"{C.MUTED}({len(sorted_tax) - display_count} more){C.RESET}")
                section_pipe()
            section_line(CHECK, "total", f"{C.CYAN}{len(taxonomy_tokens):,}{C.RESET}")

        # Hub constants section
        if show_hub:
            section_header(f"Hub Constants ({hub_const_ids} IDs)", 34)
            part_counts: Dict[str, int] = {}
            with open(hub_data_dir / "hub_constants.json", 'r') as f:
                data = json.load(f)
            for v in data.get('global_values', []):
                for part in v['id'].split('_'):
                    if len(part) >= 2:
                        part_counts[part.upper()] = part_counts.get(part.upper(), 0) + 1
            top_parts = sorted(part_counts.items(), key=lambda x: -x[1])[:5]
            for part, count in top_parts:
                section_line(CHECK, "part", f"{C.CYAN}{part:<24}{C.RESET} {C.YELLOW}{count:>6}{C.RESET}")
            if len(part_counts) > 5:
                section_pipe()
                section_line(SKIP, "...", f"{C.MUTED}({len(part_counts) - 5} more){C.RESET}")
                section_pipe()
            section_line(CHECK, "unique-parts", f"{C.CYAN}{len(hub_const_parts):,}{C.RESET}")
            section_line(CHECK, "new-tokens", f"{C.GREEN}{len(hub_const_new):,}{C.RESET}")

        if show_hub:
            section_header(f"Hub Metrics ({hub_metric_ids} IDs)", 110)
            part_counts = {}
            with open(hub_data_dir / "hub_metrics.json", 'r') as f:
                data = json.load(f)
            for v in data.get('global_values', []):
                for part in v['id'].split('_'):
                    if len(part) >= 2:
                        part_counts[part.upper()] = part_counts.get(part.upper(), 0) + 1
            top_parts = sorted(part_counts.items(), key=lambda x: -x[1])[:5]
            for part, count in top_parts:
                section_line(CHECK, "part", f"{C.CYAN}{part:<24}{C.RESET} {C.YELLOW}{count:>6}{C.RESET}")
            if len(part_counts) > 5:
                section_pipe()
                section_line(SKIP, "...", f"{C.MUTED}({len(part_counts) - 5} more){C.RESET}")
                section_pipe()
            section_line(CHECK, "unique-parts", f"{C.CYAN}{len(hub_metric_parts):,}{C.RESET}")
            section_line(CHECK, "new-tokens", f"{C.GREEN}{len(hub_metric_new):,}{C.RESET}")

        if show_hub:
            section_header(f"Hub Tags ({hub_tag_ids} IDs)", 141)
            part_counts = {}
            with open(hub_data_dir / "hub_tags.json", 'r') as f:
                data = json.load(f)
            for v in data.get('global_values', []):
                for part in v['id'].split('_'):
                    if len(part) >= 2:
                        part_counts[part.upper()] = part_counts.get(part.upper(), 0) + 1
            top_parts = sorted(part_counts.items(), key=lambda x: -x[1])[:5]
            for part, count in top_parts:
                section_line(CHECK, "part", f"{C.CYAN}{part:<24}{C.RESET} {C.YELLOW}{count:>6}{C.RESET}")
            if len(part_counts) > 5:
                section_pipe()
                section_line(SKIP, "...", f"{C.MUTED}({len(part_counts) - 5} more){C.RESET}")
                section_pipe()
            section_line(CHECK, "unique-parts", f"{C.CYAN}{len(hub_tag_parts):,}{C.RESET}")
            section_line(CHECK, "new-tokens", f"{C.GREEN}{len(hub_tag_new):,}{C.RESET}")

        # Module prefixes section
        if show_modules:
            index_name_to_abbrev: Dict[str, str] = {}
            index_file = taxonomy_dir / "_index.json"
            if index_file.exists():
                with open(index_file, 'r') as f:
                    idx_data = json.load(f)
                    for cat in idx_data.get('categories', []):
                        name = cat.get('name', '').upper()
                        abbrev = cat['abbrev'].upper()
                        if name and name != abbrev:
                            index_name_to_abbrev[name] = abbrev

            alias_count = 0
            noun_alias_count = 0
            standalone_count = 0
            taxonomy_match_count = 0
            section_header(f"Module Prefixes ({len(module_prefixes)} dirs)", 214)
            aliases = []
            for prefix in sorted(module_prefixes):
                if prefix in index_name_to_abbrev:
                    aliases.append((prefix, index_name_to_abbrev[prefix]))
                    alias_count += 1
                elif prefix in taxonomy_tokens:
                    taxonomy_match_count += 1
                elif prefix in noun_word_to_abbrev:
                    aliases.append((prefix, noun_word_to_abbrev[prefix]))
                    noun_alias_count += 1
                else:
                    standalone_count += 1
            for name, abbrev in aliases[:5]:
                section_line(CHECK, "alias", f"{C.MUTED}{name.lower()}{C.RESET} \u2192 {C.CYAN}{abbrev}{C.RESET}")
            if len(aliases) > 5:
                section_pipe()
                section_line(SKIP, "...", f"{C.MUTED}({len(aliases) - 5} more){C.RESET}")
                section_pipe()
            section_line(CHECK, "index-alias", f"{C.CYAN}{alias_count}{C.RESET}")
            section_line(CHECK, "noun-alias", f"{C.GREEN}{noun_alias_count}{C.RESET}")
            section_line(CHECK, "standalone", f"{C.YELLOW}{standalone_count}{C.RESET}")
            section_line(CHECK, "taxonomy-match", f"{C.CYAN}{taxonomy_match_count}{C.RESET}")

        # Overlaps section
        if show_overlap:
            section_header("Overlaps", 97)
            sources: Dict[str, list] = {}
            for tok in all_tokens:
                srcs = []
                if tok in taxonomy_tokens:
                    srcs.append("taxonomy")
                if tok in hub_const_parts:
                    srcs.append("hub_constants")
                if tok in hub_metric_parts:
                    srcs.append("hub_metrics")
                if tok in hub_tag_parts:
                    srcs.append("hub_tags")
                if tok in module_prefixes:
                    srcs.append("modules")
                if len(srcs) > 1:
                    sources[tok] = srcs

            for tok in sorted(sources.keys())[:20]:
                section_line(SKIP, "shared", f"{C.CYAN}{tok}{C.RESET} \u2014 {C.MUTED}{' + '.join(sources[tok])}{C.RESET}")
            if len(sources) > 20:
                section_line(SKIP, "...", f"{C.MUTED}({len(sources) - 20} more){C.RESET}")
            section_line(CHECK, "shared-total", f"{C.CYAN}{len(sources):,}{C.RESET}")

    # Broken aliases section, always checked.
    # Verify every ACTUAL alias in the generator maps to the correct bit.
    # Uses name_aliases from load_all_abbreviations (the generator's own list).
    broken_aliases: list = []

    # Build alias→target lookup from generator's actual aliases
    all_alias_pairs: Dict[str, str] = {}
    for alias_name, target_abbrev in name_aliases:
        all_alias_pairs[alias_name] = target_abbrev

    for alias_name, target_abbrev in sorted(all_alias_pairs.items()):
        # The alias_name should NOT have its own bit (it was removed from abbrevs)
        # The target_abbrev MUST have a bit (it's the canonical abbreviation)
        alias_bit = abbrev_bits.get(alias_name)
        target_bit = abbrev_bits.get(target_abbrev)
        if alias_bit is not None and target_bit is not None and alias_bit != target_bit:
            broken_aliases.append((alias_name, target_abbrev, alias_bit, target_bit))

    has_broken = len(broken_aliases) > 0
    if not quiet or has_broken:
        section_header(f"Alias Integrity ({len(all_alias_pairs)} pairs)", 242)
        if has_broken:
            # DIE FUNDSTELLEN LIEFERT DAS TOR \u2014 Alias, Ziel und BEIDE Bits, damit die Adresse
            # vollstaendig ist. Die AUSSAGE steht darunter und kommt aus dem Bestand.
            for noun, abbr, n_bit, a_bit in broken_aliases[:20]:
                section_line(CROSS, "BROKEN",
                    f"{C.RED}{noun}(bit={n_bit}){C.RESET} != "
                    f"{C.CYAN}{abbr}(bit={a_bit}){C.RESET}")
            if len(broken_aliases) > 20:
                # KEIN STILLER DECKEL: was nicht gedruckt wurde, nennt sich selbst.
                section_line(SKIP, "...",
                    f"{C.MUTED}({len(broken_aliases) - 20} more){C.RESET}")
            _n, _a, _nb, _ab = broken_aliases[0]
            _v = _regeltext(_RULE_ALIAS_BRUCH,
                            {'alias': _n, 'target': _a, 'alias_bit': _nb, 'target_bit': _ab})
            if _v is None:
                print("GATE-COUNT: 0")
                return 2
            section_detail(_v.get('message', ''))
            section_detail(_v.get('suggestion', ''))
            section_line(CROSS, "broken-total",
                f"{C.RED}{len(broken_aliases)}{C.RESET}")
        else:
            section_line(CHECK, "status",
                f"{C.GREEN}All noun aliases resolve to correct bits{C.RESET}")

    # Summary section, always shown.
    section_header(f"Summary ({total_unique:,})", 179)
    section_line(CHECK, "taxonomy", f"{C.CYAN}{len(taxonomy_tokens):,}{C.RESET}")
    section_line(CHECK, "hub-constants", f"{C.CYAN}{len(hub_const_parts):,}{C.RESET}")
    section_line(CHECK, "hub-metrics", f"{C.CYAN}{len(hub_metric_parts):,}{C.RESET}")
    section_line(CHECK, "hub-tags", f"{C.CYAN}{len(hub_tag_parts):,}{C.RESET}")
    section_line(CHECK, "module-prefix", f"{C.CYAN}{len(module_prefixes):,}{C.RESET}")
    section_line(CHECK, "unique-bits", f"{C.YELLOW}{actual_bits:,}{C.RESET}")
    if has_broken:
        section_line(CROSS, "broken-aliases",
            f"{C.RED}{len(broken_aliases)} BROKEN{C.RESET}")

    pct = (actual_bits / capacity) * 100
    cap_str = f"{C.YELLOW}{actual_bits:,}{C.RESET} / {C.MUTED}{capacity:,}{C.RESET} ({C.CYAN}{pct:.0f}%{C.RESET})"
    # THE WARNING THRESHOLD COMES FROM THE DATA. Point 2 of the order in `Skill(ase-validator)`:
    # a threshold belongs in data/config.json, never in a script.
    #
    # THE CAPACITY STAYS IN THE CODE, and that is deliberate: 16384 follows from the bit field
    # header (256 blocks of 64 bits) and is a PROPERTY of the format. In the configuration it
    # would be a settable number, and an overflow could be configured away by raising it
    # instead of being fixed.
    _warn_pct = get_config_value('log_token_capacity.warn_percent')
    if pct > 100:
        section_line(CROSS, "capacity", f"{cap_str} {C.RED}<- OVER!{C.RESET}")
        _v = _regeltext(_RULE_RAUM_VOLL, {'used': actual_bits, 'capacity': capacity})
        if _v is None:
            print("GATE-COUNT: 0")
            return 2
        section_detail(_v.get('message', ''))
        section_detail(_v.get('suggestion', ''))
    elif _warn_pct is not None and pct > _warn_pct:
        section_line(CHECK, "capacity", f"{cap_str} {C.YELLOW}<- WARN{C.RESET}")
        # `round`, NOT `int`: the line above shows `{pct:.0f}`, rounded. With `int` the same
        # output would name two percentages for one thing, a difference that makes the reader
        # doubt the measurement rather than the rounding mode.
        _v = _regeltext(_RULE_RAUM_KNAPP,
                        {'used': actual_bits, 'capacity': capacity, 'percent': round(pct)})
        if _v is None:
            print("GATE-COUNT: 0")
            return 2
        section_detail(_v.get('message', ''))
        section_detail(_v.get('suggestion', ''))
    else:
        section_line(CHECK, "capacity", cap_str)
    tprint()

    # THE FINDING COUNT FOR THE COLUMN OF THE VALIDATION BOX, machine-readable, without effect
    # on the exit code. Without it the gate reports `rc=1` and an EMPTY cell, and no session can
    # work against an empty cell: it sees THAT the gate fails, not HOW FAR it still has to go.
    #
    # PLACES ARE COUNTED, NOT SYMPTOMS: every broken alias is ONE finding, and the capacity
    # overrun is ONE, not the number of bits above it. Counted by bits, fixing a single alias
    # would drop the number by several and the progress could not be read.
    befunde = len(broken_aliases) + (1 if actual_bits > capacity else 0)
    # THE EXTENT AS A COLUMN OF ITS OWN (`GATE-SCOPE`). This gate inspects JSON sources of the
    # log filter, not the C++ tree, so the column names bits and capacity, not modules. Without
    # it a 0 does not say whether the token space is half full or full to the brim.
    print(f"GATE-SCOPE: {actual_bits}/{capacity} Bits belegt")
    print(f"GATE-COUNT: {befunde}")

    # Exit code
    if has_broken:
        return 1
    if actual_bits > capacity:
        return 1
    return 0


def main():
    parser = argparse.ArgumentParser(
        description="Validate log filter token sources (taxonomy + Hub JSONs + modules)")
    parser.add_argument("-A", "--all", action="store_true",
                        help="Analyze all JSON sources")
    parser.add_argument("-J", "--json-source", choices=["taxonomy", "hub", "modules"],
                        help="Analyze only specific JSON source")
    parser.add_argument("-q", "--quiet", action="store_true",
                        help="Only show Summary section")
    parser.add_argument("--no-color", action="store_true",
                        help="Disable colors (pipe-compatible)")
    parser.add_argument("-O", "--overlap", action="store_true",
                        help="Show token overlaps between sources")
    args = parser.parse_args()
    sys.exit(run_analysis(args))


if __name__ == '__main__':
    main()
