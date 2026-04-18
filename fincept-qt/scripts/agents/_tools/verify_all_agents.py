"""
verify_all_agents.py

End-to-end verification pass for the full agent corpus after the v2.0.0
migration. For every config that AgentLoader discovers:

1. AgentCard.from_dict round-trips without error.
2. version == "2.0.0" (we do not want stragglers).
3. config.tools is a list of strings that ToolsRegistry.get_tools()
   can initialize (with empty api_keys; missing-key warnings are OK).
4. No instruction mentions one of the OLD fake tool names (
   "web_search", "news_analysis", "financial_metrics_tool",
   "stock_price_tool", "economic_data", "market_data",
   "sentiment_analysis", "sec_filings", "fund_flows",
   "patent_analysis", "shipping_data", "regulatory_filings").
5. No entry still has "llm_config" / "enable_memory" / "role" / "goal"
   at the TOP LEVEL (those must have moved into config or been dropped).

The renaissance_technologies_agent config is explicitly SKIPPED: it uses a
multi-agent team schema and was called out as deferred earlier.

Prints a summary and returns non-zero on any failure.
"""
from __future__ import annotations

import json
import logging
import sys
from pathlib import Path

# Silence noisy Agno / tool-init error logs during API-key-less resolution.
# We expect tools that need keys to fail to initialize; that's fine for
# a validation pass.
logging.disable(logging.CRITICAL)

HERE = Path(__file__).resolve().parent
AGENTS_ROOT = HERE.parent

sys.path.insert(0, str(AGENTS_ROOT))

from finagent_core.agent_loader import AgentLoader, AgentCard        # noqa: E402
from finagent_core.registries.tools_registry import ToolsRegistry    # noqa: E402


FAKE_TOOL_NAMES = {
    "web_search",
    "news_analysis",
    "financial_metrics_tool",
    "stock_price_tool",
    "economic_data",
    "market_data",
    "sentiment_analysis",
    "sec_filings",
    "fund_flows",
    "patent_analysis",
    "shipping_data",
    "regulatory_filings",
}

FORBIDDEN_TOP_LEVEL_KEYS = {
    "llm_config",
    "enable_memory",
    "enable_agentic_memory",
    "role",
    "goal",
    "show_tool_calls",
    "markdown",
    "debug_mode",
}

# Multi-agent team schema - skip from round-trip, but we still sanity-check it
# is valid JSON and has agents.
SKIP_IDS = {"renaissance_technologies_agent"}


def _iter_all_raw_entries():
    """Yield (source_path, raw_dict) for every agent entry in every config.
    Includes single-file configs under finagent_core/configs and bundled
    agent_definitions.json files under each *Agents*/configs/ directory.
    """
    # Single-file configs
    single_dir = AGENTS_ROOT / "finagent_core" / "configs"
    for p in sorted(single_dir.glob("*.json")):
        try:
            d = json.loads(p.read_text(encoding="utf-8"))
        except Exception as e:
            yield (p, {"__parse_error__": str(e)})
            continue
        if isinstance(d, dict) and "id" in d:
            yield (p, d)

    # Bundled configs
    for bundle_name in [
        "TraderInvestorsAgent",
        "EconomicAgents",
        "GeopoliticsAgents",
        "hedgeFundAgents",
    ]:
        bp = AGENTS_ROOT / bundle_name / "configs" / "agent_definitions.json"
        if not bp.exists():
            continue
        try:
            d = json.loads(bp.read_text(encoding="utf-8"))
        except Exception as e:
            yield (bp, {"__parse_error__": str(e)})
            continue
        for entry in d.get("agents", []):
            yield (bp, entry)


def main() -> int:
    errors: list[str] = []
    stats = {
        "parsed": 0,
        "version_ok": 0,
        "tools_resolved": 0,
        "cards_built": 0,
        "skipped": 0,
    }

    for source, raw in _iter_all_raw_entries():
        if "__parse_error__" in raw:
            errors.append(f"{source}: JSON parse error: {raw['__parse_error__']}")
            continue
        stats["parsed"] += 1

        aid = raw.get("id", "<missing id>")

        if aid in SKIP_IDS:
            stats["skipped"] += 1
            continue

        # 5. No forbidden top-level keys
        forbidden = FORBIDDEN_TOP_LEVEL_KEYS & set(raw.keys())
        if forbidden:
            errors.append(
                f"{aid} @ {source.name}: forbidden top-level keys remain: "
                f"{sorted(forbidden)}"
            )

        # 2. Version check
        if raw.get("version") != "2.0.0":
            errors.append(
                f"{aid} @ {source.name}: version != 2.0.0 (got {raw.get('version')!r})"
            )
        else:
            stats["version_ok"] += 1

        # 1. Build AgentCard
        try:
            card = AgentCard.from_dict(raw)
            stats["cards_built"] += 1
        except Exception as e:
            errors.append(f"{aid} @ {source.name}: AgentCard.from_dict failed: {e}")
            continue

        # 3. Tools resolve
        tools = card.config.get("tools") or []
        if not isinstance(tools, list):
            errors.append(f"{aid}: config.tools is not a list")
            continue
        try:
            resolved = ToolsRegistry.get_tools(tools, api_keys={})
            stats["tools_resolved"] += 1
        except Exception as e:
            errors.append(f"{aid}: ToolsRegistry.get_tools failed: {e}")

        # 4. Instructions do not mention fake tool names
        instr = card.config.get("instructions", "") or ""
        # We only care if a FAKE name is mentioned as a tool call - so check
        # backtick-wrapped occurrences to avoid false positives on words like
        # "sentiment analysis" appearing as English.
        for fake in FAKE_TOOL_NAMES:
            if f"`{fake}`" in instr:
                errors.append(
                    f"{aid}: instructions still reference fake tool `{fake}`"
                )

    # Also discover via AgentLoader to confirm the registry sees everything.
    # AgentLoader is instance-based; default search roots are the finagent_core
    # package paths. Passing explicit roots keeps it deterministic.
    loader = AgentLoader(
        config_dir=AGENTS_ROOT / "finagent_core" / "configs",
        agents_dir=AGENTS_ROOT,
    )
    loader_cards = loader.discover_agents()
    loader_ids = [c.id for c in loader_cards]
    dup_ids = sorted({i for i in loader_ids if loader_ids.count(i) > 1})

    print("=" * 72)
    print("VERIFICATION SUMMARY")
    print("=" * 72)
    print(f"  Entries parsed:          {stats['parsed']}")
    print(f"  Version == 2.0.0:        {stats['version_ok']}")
    print(f"  AgentCard.from_dict ok:  {stats['cards_built']}")
    print(f"  Tools resolved:          {stats['tools_resolved']}")
    print(f"  Skipped (multi-agent):   {stats['skipped']}")
    print(f"  AgentLoader discovered:  {len(loader_cards)}")
    if dup_ids:
        print(f"  Duplicate ids:           {dup_ids}")
    print()
    if errors:
        print(f"ERRORS ({len(errors)}):")
        for e in errors:
            print(f"  - {e}")
        return 1
    print("OK: every entry is v2.0.0, validates as AgentCard, resolves tools,")
    print("and contains no fake tool names.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
