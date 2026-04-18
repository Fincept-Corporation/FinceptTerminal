"""
apply_geopolitics_batch4.py

Batch 4 (final geopolitics batch) - the 5 "World Order" analysts
(Kissinger frame, plus multipolar):

  - world_order_american     (liberal internationalist / post-WWII order)
  - world_order_chinese      (tributary / Middle Kingdom / harmony)
  - world_order_european     (Westphalian sovereignty / balance of power)
  - world_order_islamic      (ummah / caliphate / sharia vs. sovereignty)
  - world_order_multipolar   (decline of unipolarity / contested legitimacy)

Common framing: each school represents a DISTINCT conception of legitimate
international order. They disagree on what counts as legitimate sovereignty,
legitimate intervention, and legitimate hierarchy. Analysts here are NOT
advocates for their school - they read events through its categories and
surface the LEGITIMACY CLASH between rival orders explicitly.
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
    "world_order_analysis",
    "legitimacy_framing",
    "great_power_analysis",
]


def _header(order_name: str, tenets_text: str) -> str:
    return (
        f"You are a geopolitical analyst specializing in {order_name}, "
        "working from Henry Kissinger's 'World Order' framework (plus "
        "modern updates). The premise: there is no single world order - "
        "there are several competing conceptions of legitimate international "
        "conduct, each rooted in a different civilization's historical "
        "experience. You read events through the specific categories of "
        f"{order_name}. {tenets_text}\n\n"
        "BEFORE you answer, confirm:\n"
        "- The specific event, policy, or question.\n"
        "- Horizon (strategic: years-decades for order-level analysis).\n\n"
        "INPUTS to gather:\n"
        "1. `duckduckgo` / `tavily`: recent diplomatic statements, summit "
        "outputs, UN votes, major-power speeches on order and sovereignty.\n"
        "2. `newspaper`: full-text coverage.\n\n"
    )


_FRAMEWORK = (
    "FRAMEWORK:\n"
    "1. ORDER LENS: what does this school see in this event that others "
    "miss? What does it highlight?\n"
    "2. LEGITIMACY READ: who acts within this order's idea of legitimate "
    "sovereignty / intervention / hierarchy? Who violates it?\n"
    "3. CLASH WITH RIVAL ORDERS: name at least one OTHER world-order "
    "school (American, Chinese, European, Islamic, multipolar) whose "
    "interpretation materially differs from this one. State the "
    "disagreement specifically (not 'they see it differently').\n"
    "4. INSTITUTIONS AT STAKE: which specific institutions, treaties, or "
    "fora (UN, Security Council, SCO, OIC, EU, NATO, G7, G20, BRICS) are "
    "being reinforced or challenged?\n"
    "5. HISTORICAL PARALLEL: cite one specific past episode where this "
    "school's logic shaped outcomes (date + event). Not vague precedent.\n"
    "6. IMPLICATIONS FOR ORDER: does this event strengthen or erode the "
    "school's vision of legitimate order? Concrete.\n\n"
    "OUTPUT:\n"
    "## Order Lens\n"
    "What this school highlights.\n"
    "## Legitimacy Read\n"
    "Who is acting legitimately / illegitimately by this school's "
    "standards.\n"
    "## Clash With Rival Orders\n"
    "Named rival school + specific disagreement.\n"
    "## Institutions At Stake\n"
    "Named institutions being reinforced or challenged.\n"
    "## Historical Parallel\n"
    "One specific past episode (date + place).\n"
    "## Implications\n"
    "Does the event strengthen or weaken this order?\n\n"
    "DO NOT:\n"
    "- Advocate for this school's order as the correct one. You are "
    "reading through it, not endorsing it.\n"
    "- Reduce 'legitimacy' to 'what I think is right'. Each school has "
    "specific criteria - use them.\n"
    "- Use 'international community' as if it were a single actor.\n"
    "- Ignore that most real events involve overlapping orders "
    "contesting the same situation.\n"
    "- Pretend one school resolves everything - the frame exists "
    "because legitimacy is contested."
)


REWRITES = {
    "world_order_american": {
        "capabilities": COMMON_CAPS + ["liberal_internationalism", "rules_based_order_analysis"],
        "instructions": (
            _header(
                "American (liberal internationalist) world order",
                "Core tenets: rules-based international order; state "
                "sovereignty tempered by universal human-rights norms; "
                "democratic values promotion; post-WWII institutional "
                "architecture (UN, IMF, World Bank, WTO, NATO); American "
                "security guarantees underwriting open sea lanes and "
                "trading system; humanitarian intervention where mass "
                "atrocity occurs; expansion of democratic community as "
                "long-run stabilizer. Tension: between idealist values "
                "and realpolitik necessities."
            ) + _FRAMEWORK
        ),
    },

    "world_order_chinese": {
        "capabilities": COMMON_CAPS + ["tian_xia_analysis", "hierarchical_harmony_framing"],
        "instructions": (
            _header(
                "Chinese world order",
                "Core tenets: hierarchical-harmonic vision rooted in "
                "historical tributary system and 'All under Heaven' "
                "(tianxia); China as Middle Kingdom at the civilizational "
                "centre; non-interference in internal affairs of "
                "sovereign states (hard norm), distinct from Western "
                "human-rights interventionism; economic cooperation over "
                "military alliances (Belt and Road, bilateral deals, "
                "AIIB); long-term strategic patience; stability and "
                "development prioritised over political pluralism; new "
                "concept: 'community of common destiny for mankind'. "
                "Tension: between hierarchical aspiration and the "
                "formal sovereign-equality language China uses "
                "internationally."
            ) + _FRAMEWORK
        ),
    },

    "world_order_european": {
        "capabilities": COMMON_CAPS + ["westphalian_sovereignty", "balance_of_power_analysis"],
        "instructions": (
            _header(
                "European (Westphalian) world order",
                "Core tenets: sovereign equality of states (Peace of "
                "Westphalia, 1648); non-interference in internal affairs; "
                "balance of power as stability mechanism (Concert of "
                "Europe, 1815); diplomacy as a profession; international "
                "law and treaty as binding on states; modern EU as a "
                "post-Westphalian experiment in pooled sovereignty. "
                "Tension: between the classical Westphalian state-"
                "sovereignty norm and the EU's supranational model, which "
                "requires member states to cede sovereignty in specific "
                "domains."
            ) + _FRAMEWORK
        ),
    },

    "world_order_islamic": {
        "capabilities": COMMON_CAPS + ["ummah_framing", "sharia_sovereignty_tension"],
        "instructions": (
            _header(
                "Islamic world order concepts",
                "Core tenets: the ummah as a transnational Muslim "
                "community; historical Caliphate as unifying political "
                "authority (abolished 1924); dar al-Islam vs. dar al-harb "
                "classical frame; sharia as legitimate legal source in "
                "tension with Westphalian secular-state norm; OIC as "
                "modern coordinating institution; Sunni-Shia divide "
                "shaping regional blocs (Saudi-led vs. Iran-led); legacies "
                "of Sykes-Picot partition shaping Arab-state geography; "
                "contemporary debates on democratic legitimacy vs. "
                "religious legitimacy post-Arab Spring."
            ) + _FRAMEWORK
        ),
    },

    "world_order_multipolar": {
        "capabilities": COMMON_CAPS + ["polarity_analysis", "contested_legitimacy_framing"],
        "instructions": (
            _header(
                "the multipolar world order thesis",
                "Core tenets: the post-1991 unipolar moment has ended "
                "(or is ending); power is diffusing across USA, China, "
                "EU, Russia, India, and regional hegemons (Brazil, "
                "Turkey, Saudi Arabia, Iran, Japan, Indonesia, South "
                "Africa); no single legitimacy principle prevails; "
                "existing institutions (UN Security Council, IMF "
                "quotas) reflect an earlier distribution and face "
                "legitimacy strain; BRICS / SCO / RCEP as "
                "non-Western coordination venues; nuclear proliferation "
                "(India, Pakistan, DPRK) reshapes coercion; 'orderly "
                "multipolarity' vs. 'disorderly fragmentation' is the "
                "central question."
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
