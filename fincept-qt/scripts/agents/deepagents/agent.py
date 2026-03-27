"""
agent.py — Full create_deep_agent wiring for Fincept Terminal.

Single responsibility:
  - create_agent() factory that wires ALL 16 create_deep_agent params
  - FinceptContext dataclass for user/session injection via context_schema
  - MCP tool wrappers as LangChain BaseTool instances
  - InMemoryStore + InMemoryCache always created
  - MemorySaver checkpointer always on (thread-level state)
"""

from __future__ import annotations

import logging
import os
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from langchain_core.tools import BaseTool, tool
from langgraph.checkpoint.memory import MemorySaver
from langgraph.store.memory import InMemoryStore
from langgraph.cache.memory import InMemoryCache

from deepagents.graph import resolve_model, GENERAL_PURPOSE_SUBAGENT, BASE_AGENT_PROMPT
from langchain.agents.middleware import InterruptOnConfig

from backends import get_backend
from models import create_model, extract_text
from subagents import get_subagents_for_type

logger = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# FinceptContext — injected into every agent invocation via context_schema
# ---------------------------------------------------------------------------

@dataclass
class FinceptContext:
    """
    Typed context passed to every deep agent via context_schema / invoke(context=...).

    Gives agents access to session and user state without polluting the message stream.
    """
    user_id:      str = ""
    session_id:   str = ""
    agent_type:   str = "general"
    portfolio_id: str = ""
    watchlist:    list[str] = field(default_factory=list)


# ---------------------------------------------------------------------------
# MCP tool wrappers — LangChain BaseTool instances wrapping Fincept capabilities
# ---------------------------------------------------------------------------

def _make_market_data_tool() -> BaseTool:
    @tool
    def market_data(query: str) -> str:
        """
        Fetch market data for a symbol or query.
        Input: symbol name, query like 'AAPL price', 'BTC/USD OHLCV', or 'top gainers'.
        Returns: JSON string with market data.
        """
        # Placeholder — C++ AgentService wires real data when calling via PythonRunner.
        # In standalone testing, returns a stub response.
        return f'{{"query": "{query}", "note": "market_data tool — connect via AgentService"}}'
    return market_data


def _make_portfolio_tool() -> BaseTool:
    @tool
    def portfolio_data(query: str) -> str:
        """
        Access portfolio positions, P&L, and allocation data.
        Input: query like 'current positions', 'portfolio P&L', 'sector allocation'.
        Returns: JSON string with portfolio data.
        """
        return f'{{"query": "{query}", "note": "portfolio_data tool — connect via AgentService"}}'
    return portfolio_data


def _make_news_tool() -> BaseTool:
    @tool
    def financial_news(query: str) -> str:
        """
        Fetch recent financial news articles for a topic or symbol.
        Input: topic, company name, or symbol like 'AAPL earnings', 'Fed rate decision'.
        Returns: JSON string with news headlines and summaries.
        """
        return f'{{"query": "{query}", "note": "financial_news tool — connect via AgentService"}}'
    return financial_news


def _make_economics_tool() -> BaseTool:
    @tool
    def economics_data(query: str) -> str:
        """
        Fetch macroeconomic indicators and central bank data.
        Input: indicator name like 'US GDP', 'CPI inflation', 'Fed funds rate', 'yield curve'.
        Returns: JSON string with economic data.
        """
        return f'{{"query": "{query}", "note": "economics_data tool — connect via AgentService"}}'
    return economics_data


def _build_tools() -> list[BaseTool]:
    """Build the list of MCP-wrapper tools for the agent."""
    return [
        _make_market_data_tool(),
        _make_portfolio_tool(),
        _make_news_tool(),
        _make_economics_tool(),
    ]


# ---------------------------------------------------------------------------
# System prompt
# ---------------------------------------------------------------------------

_FINCEPT_SYSTEM_PROMPT = """You are a Deep Agent for Fincept Terminal, an institutional-grade financial intelligence platform.

You have access to:
- Financial market data (prices, OHLCV, order books)
- Portfolio positions and P&L
- Financial news and research
- Macroeconomic indicators
- 1300+ analytics Python scripts in /scripts/
- Specialist subagents for research, analysis, trading, risk, and reporting

Standards:
- Apply CFA Level III analytical standards
- Always quantify uncertainty and data recency
- Distinguish between verified facts and estimates
- Consider risk in every recommendation
- Output structured, professional analysis

When delegating to subagents, be specific about what you need from each one.
"""

# ---------------------------------------------------------------------------
# Main factory
# ---------------------------------------------------------------------------

# Module-level shared store and cache (one per process)
_store = InMemoryStore()
_cache = InMemoryCache()


def create_agent(
    model: Any,
    config: dict[str, Any],
    scripts_dir: str | None = None,
) -> Any:
    """
    Build and return a compiled DeepAgent (CompiledStateGraph).

    Args:
        model       : BaseChatModel from models.create_model()
        config      : Full Fincept LLM config dict (used for agent_type, backend_mode, etc.)
        scripts_dir : Absolute path to scripts/ directory. Auto-detected if None.

    Returns:
        CompiledStateGraph — call .invoke() or .stream() on it.
    """
    from deepagents import create_deep_agent

    # Resolve scripts directory
    if scripts_dir is None:
        scripts_dir = _find_scripts_dir()

    agent_type   = config.get("agent_type",    "general")
    backend_mode = config.get("backend_mode",  "composite")
    interrupt_on = config.get("interrupt_on",  {"execute": True, "write_file": True})
    debug        = config.get("debug",         False)

    # Resolve model via library utility (handles str format like "openai:gpt-4o" too)
    resolved_model = resolve_model(model)

    # Backend
    backend = get_backend(
        mode=backend_mode,
        scripts_dir=scripts_dir,
        store=_store,
    )

    # Subagents
    subagents = get_subagents_for_type(agent_type)

    # Memory path — only include if AGENTS.md exists
    memory_dir  = Path(__file__).parent / "memory" / "AGENTS.md"
    memory_sources = ["/memory/AGENTS.md"] if memory_dir.exists() else None

    # Checkpointer
    checkpointer = MemorySaver()

    # Structured response format (opt-in via config)
    response_format = _build_response_format(config)

    # Build typed interrupt_on using InterruptOnConfig
    typed_interrupt: dict | None = None
    if interrupt_on:
        typed_interrupt = {
            tool: InterruptOnConfig(allowed_decisions=["approve", "edit", "decline"])
            if isinstance(v, bool) and v else v
            for tool, v in interrupt_on.items()
        }

    compiled = create_deep_agent(
        model=resolved_model,
        tools=_build_tools(),
        system_prompt=_FINCEPT_SYSTEM_PROMPT,
        middleware=(),                    # library builds full stack internally
        subagents=subagents,
        skills=None,                      # no skills defined yet — add when SKILL.md files exist
        memory=memory_sources,
        response_format=response_format,
        context_schema=FinceptContext,
        checkpointer=checkpointer,
        store=_store,
        backend=backend,
        interrupt_on=typed_interrupt,
        debug=debug,
        name=f"fincept-{agent_type}-agent",
        cache=_cache,
    )

    logger.info(
        "Created fincept-%s-agent with %d subagents, backend=%s",
        agent_type, len(subagents), backend_mode,
    )
    return compiled


def build_fincept_context(params: dict[str, Any]) -> FinceptContext:
    """Build a FinceptContext from CLI params dict."""
    ctx = params.get("context", {}) or {}
    return FinceptContext(
        user_id=      ctx.get("user_id",      ""),
        session_id=   ctx.get("session_id",   ""),
        agent_type=   params.get("agent_type", "general"),
        portfolio_id= ctx.get("portfolio_id", ""),
        watchlist=    ctx.get("watchlist",    []),
    )


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _find_scripts_dir() -> str | None:
    """Auto-detect the scripts/ directory relative to this file."""
    here = Path(__file__).resolve()
    # scripts/agents/deepagents/agent.py → go up 3 levels to scripts/
    candidate = here.parent.parent.parent
    if candidate.is_dir() and candidate.name == "scripts":
        return str(candidate)
    return None


def _build_response_format(config: dict[str, Any]) -> Any:
    """
    Build a response_format if the caller requests structured output.

    Config key: response_schema — name of a known Pydantic schema.
    Currently supports: "analysis_report"
    Returns None if not requested.
    """
    schema_name = config.get("response_schema")
    if not schema_name:
        return None

    try:
        from langchain.agents.structured_output import ToolStrategy

        if schema_name == "analysis_report":
            from pydantic import BaseModel

            class AnalysisReport(BaseModel):
                executive_summary: str
                key_findings:      list[str]
                risks:             list[str]
                recommendations:   list[str]
                confidence:        str  # "high" | "medium" | "low"

            return ToolStrategy(schema=AnalysisReport)

    except Exception as exc:
        logger.warning("Could not build response_format for '%s': %s", schema_name, exc)

    return None
