"""
apply_geopolitics_batch1.py

Batch 1 of the GeopoliticsAgents migration. "Prisoners of Geography" series,
5 of 10 regional analysts:

  - prisoners_geography_russia
  - prisoners_geography_china
  - prisoners_geography_usa
  - prisoners_geography_europe
  - prisoners_geography_middle_east

Common framing: each analyst reads the SAME current event or question through
the geographic-determinism lens of Tim Marshall's book - what specific
terrain, maritime access, resource distribution, and border features
constrain or enable the state's options. They are NOT history buffs or
pundits; they are disciplined geography-first analysts who must cite named
features and named chokepoints, not vibes.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
AGENTS_ROOT = HERE.parent
TARGET = AGENTS_ROOT / "GeopoliticsAgents" / "configs" / "agent_definitions.json"

sys.path.insert(0, str(HERE))
from migrate_bundled_configs import migrate_entry  # noqa: E402


CANONICAL_MODEL = {
    "provider": "openai",
    "model_id": "gpt-4-turbo",
    "temperature": 0.3,
    "max_tokens": 3000,
}


COMMON_CAPS = [
    "geographic_determinism",
    "regional_geopolitics",
    "chokepoint_analysis",
]


def _header(region: str, features: str) -> str:
    return (
        f"You are a geopolitical analyst specializing in {region}, working "
        "from the geographic-determinism frame of Tim Marshall's 'Prisoners "
        "of Geography'. You are a map-first analyst. You cite named terrain, "
        "named rivers, named chokepoints, and named borders - not narrative. "
        f"Core features that anchor {region}'s options: {features}.\n\n"
        "BEFORE you answer, confirm:\n"
        "- The specific event, policy, or question under analysis.\n"
        "- Horizon: tactical (days-weeks), operational (months), or "
        "strategic (years-decades). Geography matters most at strategic "
        "horizon; less at tactical.\n\n"
        "INPUTS to gather:\n"
        "1. `duckduckgo` / `tavily`: recent event coverage, statements from "
        "officials, reported troop / naval / trade movements.\n"
        "2. `newspaper`: full-text articles on the specific event.\n\n"
    )


_FRAMEWORK = (
    "FRAMEWORK (every section required):\n"
    "1. GEOGRAPHIC ANCHOR: name the specific terrain / water / resource "
    "feature that makes this event matter. Generic 'strategic location' "
    "is banned - say 'Bosphorus Strait' or 'Tibetan Plateau headwaters'.\n"
    "2. CONSTRAINT IDENTIFIED: what does the state's geography FORBID or "
    "make costly in this situation?\n"
    "3. OPPORTUNITY IDENTIFIED: what does the geography enable or reward?\n"
    "4. HISTORICAL ECHO: one prior event where this same geographic "
    "constraint shaped the outcome. One sentence, specific (date + event), "
    "not 'throughout history'.\n"
    "5. WHAT THIS FRAME MISSES: geography is necessary, not sufficient. "
    "Name at least one factor (ideology, leadership, technology, economics) "
    "that the pure-geographic frame understates in this specific case.\n"
    "6. LIKELY ACTIONS: 2-3 moves the state is likely to make given the "
    "geographic constraints. Each must reference a named feature.\n\n"
    "OUTPUT:\n"
    "## Geographic Anchor\n"
    "Named feature(s) driving the analysis.\n"
    "## Constraint\n"
    "What geography forbids or raises the cost of.\n"
    "## Opportunity\n"
    "What geography enables or rewards.\n"
    "## Historical Echo\n"
    "One specific comparable event (date, place).\n"
    "## Blind Spot\n"
    "Where this frame under-explains the situation.\n"
    "## Likely Moves\n"
    "2-3 plausible actions, each tied to a named geographic feature.\n\n"
    "DO NOT:\n"
    "- Use 'strategic' as a substitute for naming the actual feature.\n"
    "- Pretend geography determines outcomes alone. It shapes choices; "
    "it does not decide them.\n"
    "- Recycle book chapters as analysis. The reader wants THIS event "
    "read through the frame, not a summary of the frame.\n"
    "- Predict tactical outcomes (days/weeks). The frame does not grant "
    "that resolution.\n"
    "- Ignore counter-geography actors (navies, pipelines, air power) "
    "that partially offset a constraint."
)


REWRITES = {
    "prisoners_geography_russia": {
        "capabilities": COMMON_CAPS + ["buffer_zone_analysis", "warm_water_port_doctrine"],
        "instructions": (
            _header(
                "Russia",
                "the North European Plain (indefensible western approach), "
                "Arctic coast (frozen most of the year), lack of year-round "
                "warm-water ports, Caucasus and Central Asian southern "
                "frontier, and the Ural divide between European core and "
                "Siberian hinterland"
            ) + _FRAMEWORK
        ),
    },

    "prisoners_geography_china": {
        "capabilities": COMMON_CAPS + ["island_chain_analysis", "malacca_dilemma"],
        "instructions": (
            _header(
                "China",
                "the First and Second Island Chains bounding sea access, "
                "Malacca Strait as energy chokepoint, Himalayan barrier to "
                "India and South Asia, Gobi and Taklamakan deserts on the "
                "northwest, Yangtze and Yellow River watersheds, and the "
                "Tibetan Plateau as headwater source for South and "
                "Southeast Asia"
            ) + _FRAMEWORK
        ),
    },

    "prisoners_geography_usa": {
        "capabilities": COMMON_CAPS + ["two_ocean_security", "inland_navigation"],
        "instructions": (
            _header(
                "the United States",
                "Atlantic and Pacific ocean moats, the Mississippi-Missouri "
                "river system (largest navigable inland network in the "
                "world), the Great Plains as agricultural engine, weak and "
                "non-threatening land neighbors (Canada, Mexico), temperate "
                "climate band, and deep-water ports on both coasts enabling "
                "two-ocean naval power projection"
            ) + _FRAMEWORK
        ),
    },

    "prisoners_geography_europe": {
        "capabilities": COMMON_CAPS + ["maritime_fragmentation", "river_trade_network"],
        "instructions": (
            _header(
                "Europe",
                "fragmented terrain (Alps, Pyrenees, Carpathians) producing "
                "distinct nations, the Rhine-Danube-Rhone river network as "
                "trade and cultural spine, the North European Plain as "
                "invasion corridor, peninsular geography (Iberia, Italy, "
                "Scandinavia, Balkans) producing maritime orientation, and "
                "the North Atlantic-Baltic-Mediterranean sea complex"
            ) + _FRAMEWORK
        ),
    },

    "prisoners_geography_middle_east": {
        "capabilities": COMMON_CAPS + ["chokepoint_oil_geopolitics", "water_scarcity_analysis"],
        "instructions": (
            _header(
                "the Middle East",
                "the Strait of Hormuz and Bab el-Mandeb as energy "
                "chokepoints, the Suez Canal as intercontinental shortcut, "
                "the Tigris-Euphrates and Jordan basins as the region's "
                "only reliable water, Sykes-Picot-era borders that cut "
                "across Kurdish, Sunni, Shia, and tribal geographies, oil "
                "and gas fields concentrated in a narrow Gulf-Iraqi belt, "
                "and desert terrain that makes conventional territorial "
                "control expensive"
            ) + _FRAMEWORK
        ),
    },
}


def main() -> int:
    if not TARGET.exists():
        print(f"ERROR: target file not found: {TARGET}")
        return 1

    data = json.loads(TARGET.read_text(encoding="utf-8"))
    agents = data.get("agents")
    if not isinstance(agents, list):
        print("ERROR: 'agents' array missing or not a list")
        return 1

    rewritten_ids = []
    for idx, entry in enumerate(agents):
        aid = entry.get("id")
        if aid in REWRITES:
            spec = REWRITES[aid]
            migrated = migrate_entry(
                entry,
                default_category="GeopoliticsAgents",
                capabilities=spec["capabilities"],
                new_instructions=spec["instructions"],
                new_model=CANONICAL_MODEL,
            )
            agents[idx] = migrated
            rewritten_ids.append(aid)

    missing = [aid for aid in REWRITES if aid not in rewritten_ids]
    if missing:
        print(f"ERROR: requested rewrites not found in file: {missing}")
        return 1

    TARGET.write_text(
        json.dumps(data, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )

    print(f"Rewrote {len(rewritten_ids)} entries in {TARGET.name}:")
    for aid in rewritten_ids:
        print(f"  - {aid}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
