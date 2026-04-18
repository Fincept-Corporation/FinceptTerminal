"""
migrate_bundled_configs.py

One-off helper used during the agent-config v2.0.0 migration.

Takes a bundled-shape agent entry (as stored in
TraderInvestorsAgent / EconomicAgents / GeopoliticsAgents / hedgeFundAgents
`agent_definitions.json` or `team_config.json`) and rewrites it to the canonical
v2.0.0 shape used by finagent_core/configs/*_agent.json.

Canonical v2.0.0 top-level shape:
    {
      "id": ..., "name": ..., "description": ...,
      "category": ..., "version": "2.0.0", "provider": "local",
      "capabilities": [...],
      "config": {
        "model": {provider, model_id, temperature, max_tokens},
        "instructions": ...,
        "tools": [real Agno tool names only],
        "output_format": "markdown",
        "memory": true,
        "agentic_memory": true,
        ...extras...
      }
    }

This module only transforms shape + tool names. Instruction rewrites are done by
hand per-persona (see batch tasks).
"""
from __future__ import annotations

from typing import Any, Dict, Iterable, List, Optional

# -----------------------------------------------------------------------------
# Fake -> real tool name map (agreed with user).
#
# Keys are the invented names that appear across the bundled files.
# Values are Agno tool names that resolve through ToolsRegistry.get_tools().
# A fake name may expand to >1 real tool.
# -----------------------------------------------------------------------------
TOOL_MAP: Dict[str, List[str]] = {
    "web_search":            ["duckduckgo", "tavily"],
    "news_analysis":         ["newspaper", "tavily"],
    "financial_metrics_tool":["yfinance", "financial_datasets"],
    "stock_price_tool":      ["yfinance"],
    "economic_data":         ["openbb"],
    "market_data":           ["yfinance"],
    "sentiment_analysis":    ["tavily", "newspaper"],
    "sec_filings":           ["edgar"],
    "fund_flows":            ["edgar"],
    "patent_analysis":       ["tavily", "firecrawl"],
    "shipping_data":         ["tavily", "newspaper"],
    "regulatory_filings":    ["edgar", "tavily"],
}


def translate_tools(tools: Iterable[str]) -> List[str]:
    """Map fake tool names to real Agno tool names, preserve order, dedupe."""
    seen: List[str] = []
    for t in tools or []:
        mapped = TOOL_MAP.get(t, [t])  # if already real, keep as-is
        for m in mapped:
            if m not in seen:
                seen.append(m)
    return seen


# Keys that stay at the entry top level (never go under config)
TOP_LEVEL = {"id", "name", "description", "category", "version",
             "provider", "capabilities"}

# Bundled-shape keys we drop — not consumed anywhere downstream
DROP_KEYS = {"role", "goal", "enable_memory", "enable_agentic_memory",
             "debug_mode", "show_tool_calls", "markdown"}


def migrate_entry(
    entry: Dict[str, Any],
    *,
    default_category: str,
    capabilities: Optional[List[str]] = None,
    new_instructions: Optional[str] = None,
    new_model: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    """Convert one bundled-shape entry to canonical v2.0.0.

    Args:
        entry:              the raw dict from agent_definitions.json's `agents[i]`.
        default_category:   e.g., "TraderInvestorsAgent", "EconomicAgents".
        capabilities:       list to set on the card (optional).
        new_instructions:   if provided, replaces the original instructions verbatim.
        new_model:          if provided, replaces the llm_config-derived model block.
    """
    if "id" not in entry:
        raise ValueError("entry has no 'id'")

    out: Dict[str, Any] = {
        "id": entry["id"],
        "name": entry.get("name", entry["id"]),
        "description": entry.get("description", ""),
        "category": entry.get("category", default_category),
        "version": "2.0.0",
        "provider": entry.get("provider", "local"),
        "capabilities": capabilities or entry.get("capabilities") or [],
    }

    # ---- config block ------------------------------------------------------
    cfg: Dict[str, Any] = {}

    # model
    if new_model is not None:
        cfg["model"] = dict(new_model)
    elif "model" in entry and isinstance(entry["model"], dict):
        cfg["model"] = dict(entry["model"])
    elif "llm_config" in entry and isinstance(entry["llm_config"], dict):
        cfg["model"] = dict(entry["llm_config"])
    else:
        # fallback — analyst temperature
        cfg["model"] = {
            "provider": "openai",
            "model_id": "gpt-4-turbo",
            "temperature": 0.3,
            "max_tokens": 3000,
        }

    # instructions
    cfg["instructions"] = new_instructions if new_instructions is not None \
                          else entry.get("instructions", "")

    # tools — translate fake names
    cfg["tools"] = translate_tools(entry.get("tools", []))

    # memory — normalize from enable_memory / enable_agentic_memory
    cfg["memory"] = bool(entry.get("enable_memory", entry.get("memory", True)))
    cfg["agentic_memory"] = bool(
        entry.get("enable_agentic_memory", entry.get("agentic_memory", True))
    )

    # output_format
    cfg["output_format"] = entry.get("output_format", "markdown")

    # preserve extras (knowledge_base, output_schema, scoring_weights,
    # thresholds, analysis_rules, data_sources, book_source, ui_parameters, etc.)
    preserved_extras = set(entry.keys()) - TOP_LEVEL - DROP_KEYS - {
        "llm_config", "model", "instructions", "tools",
        "output_format", "memory", "agentic_memory",
    }
    for k in preserved_extras:
        cfg[k] = entry[k]

    out["config"] = cfg
    return out


# -----------------------------------------------------------------------------
# CLI self-test
# -----------------------------------------------------------------------------
if __name__ == "__main__":
    import json, sys

    sample = {
        "id": "warren_buffett_agent",
        "name": "Warren Buffett",
        "role": "Value investor",
        "goal": "Moat-first equity analysis",
        "description": "Moat / management / valuation lens.",
        "llm_config": {"provider": "openai", "model_id": "gpt-4-turbo",
                       "temperature": 0.5, "max_tokens": 3000},
        "tools": ["financial_metrics_tool", "stock_price_tool", "web_search"],
        "enable_memory": True,
        "enable_agentic_memory": True,
        "instructions": "ORIGINAL TEXT",
        "scoring_weights": {"moat": 0.4, "management": 0.3, "valuation": 0.3},
        "thresholds": {"roic_min": 0.12},
    }

    migrated = migrate_entry(sample, default_category="TraderInvestorsAgent")
    print(json.dumps(migrated, indent=2))

    # Validate via AgentCard
    sys.path.insert(0, ".")
    from finagent_core.agent_loader import AgentCard
    from finagent_core.registries.tools_registry import ToolsRegistry

    card = AgentCard.from_dict(migrated)
    print()
    print(f"id:      {card.id}")
    print(f"version: {card.version}")
    print(f"tools:   {card.config['tools']}")

    # Resolve tools through the registry (no API keys → some will warn; fine)
    tools = ToolsRegistry.get_tools(card.config["tools"], api_keys={})
    print(f"resolved tool count: {len(tools)}")
    print(f"tool classes: {[type(t).__name__ for t in tools]}")
