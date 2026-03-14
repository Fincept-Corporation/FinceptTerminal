"""
Agent Factory - High-level factory functions for creating configured DeepAgents
Simplified agent creation for common Fincept Terminal use cases
"""

import sys
import os
from typing import Optional, List, Any, Dict

# Add current directory to path for imports
if __name__ == "__main__" or "." not in __name__:
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from deep_agent_wrapper import DeepAgentWrapper
from fincept_llm_adapter import create_fincept_llm
from subagent_templates import (
    create_research_subagent,
    create_data_analyst_subagent,
    create_trading_subagent,
    create_risk_analyzer_subagent,
    create_reporter_subagent,
    create_portfolio_optimizer_subagent,
    create_backtester_subagent
)


def create_fincept_deep_agent(
    model: Optional[Any] = None,
    api_key: Optional[str] = None,
    use_fincept_llm: bool = True,
    system_prompt: Optional[str] = None,
    enable_checkpointing: bool = True,
    enable_longterm_memory: bool = False,
    enable_summarization: bool = True,
    recursion_limit: int = 100,
    subagents: Optional[List[str]] = None
) -> DeepAgentWrapper:
    """
    Create a fully configured Fincept DeepAgent.

    Args:
        model: LLM model (if None and use_fincept_llm=True, uses FinceptLLM)
        api_key: API key for external models
        use_fincept_llm: Use Fincept's LLM infrastructure
        system_prompt: Custom system prompt
        enable_checkpointing: Enable state persistence
        enable_longterm_memory: Enable memory across sessions
        enable_summarization: Auto-summarize long contexts
        recursion_limit: Max iterations
        subagents: List of subagent template names to include

    Returns:
        Configured DeepAgentWrapper ready for use
    """

    # Setup model
    if model is None and use_fincept_llm:
        model = create_fincept_llm(api_key=api_key)

    # Create wrapper
    agent = DeepAgentWrapper(
        model=model,
        api_key=api_key,
        system_prompt=system_prompt,
        enable_checkpointing=enable_checkpointing,
        enable_longterm_memory=enable_longterm_memory,
        enable_summarization=enable_summarization,
        recursion_limit=recursion_limit
    )

    # Add subagents if specified
    if subagents:
        template_map = {
            "research": create_research_subagent,
            "data_analyst": create_data_analyst_subagent,
            "trading": create_trading_subagent,
            "risk_analyzer": create_risk_analyzer_subagent,
            "reporter": create_reporter_subagent,
            "portfolio_optimizer": create_portfolio_optimizer_subagent,
            "backtester": create_backtester_subagent
        }

        for subagent_name in subagents:
            if subagent_name in template_map:
                subagent_config = template_map[subagent_name]()
                # Add subagent directly to list (includes system_prompt key)
                agent.subagents.append(subagent_config)

    return agent


def create_research_agent(
    model: Optional[Any] = None,
    api_key: Optional[str] = None
) -> DeepAgentWrapper:
    """
    Create agent optimized for research tasks.

    Includes: research, data_analyst, reporter subagents
    """
    return create_fincept_deep_agent(
        model=model,
        api_key=api_key,
        subagents=["data_analyst", "reporter"],
        system_prompt="""You are a financial research agent specializing in comprehensive market analysis.

Your workflow:
1. Break research questions into sub-questions
2. Delegate data analysis to data_analyst subagent
3. Synthesize findings
4. Delegate report writing to reporter subagent

Focus on thoroughness and accuracy."""
    )


def create_trading_strategy_agent(
    model: Optional[Any] = None,
    api_key: Optional[str] = None
) -> DeepAgentWrapper:
    """
    Create agent optimized for trading strategy development.

    Includes: data_analyst, trading, backtester, risk_analyzer, reporter subagents
    """
    return create_fincept_deep_agent(
        model=model,
        api_key=api_key,
        subagents=["data_analyst", "trading", "backtester", "risk_analyzer", "reporter"],
        system_prompt="""You are a quantitative trading strategy development agent.

Your workflow:
1. Define strategy objectives
2. Delegate data analysis to data_analyst
3. Delegate strategy design to trading subagent
4. Delegate backtesting to backtester
5. Delegate risk analysis to risk_analyzer
6. Delegate report generation to reporter

Ensure strategies are robust and well-tested."""
    )


def create_portfolio_management_agent(
    model: Optional[Any] = None,
    api_key: Optional[str] = None
) -> DeepAgentWrapper:
    """
    Create agent optimized for portfolio management.

    Includes: data_analyst, portfolio_optimizer, risk_analyzer, reporter subagents
    """
    return create_fincept_deep_agent(
        model=model,
        api_key=api_key,
        subagents=["data_analyst", "portfolio_optimizer", "risk_analyzer", "reporter"],
        system_prompt="""You are a portfolio management agent.

Your workflow:
1. Analyze current portfolio state
2. Delegate data analysis to data_analyst
3. Delegate optimization to portfolio_optimizer
4. Delegate risk assessment to risk_analyzer
5. Delegate reporting to reporter

Balance returns, risk, and constraints."""
    )


def create_risk_assessment_agent(
    model: Optional[Any] = None,
    api_key: Optional[str] = None
) -> DeepAgentWrapper:
    """
    Create agent optimized for risk analysis.

    Includes: data_analyst, risk_analyzer, reporter subagents
    """
    return create_fincept_deep_agent(
        model=model,
        api_key=api_key,
        subagents=["data_analyst", "risk_analyzer", "reporter"],
        system_prompt="""You are a comprehensive risk assessment agent.

Your workflow:
1. Identify risk factors and exposures
2. Delegate data analysis to data_analyst
3. Delegate risk calculations to risk_analyzer
4. Synthesize risk assessment
5. Delegate reporting to reporter

Prioritize downside protection and tail risk."""
    )


def create_general_financial_agent(
    model: Optional[Any] = None,
    api_key: Optional[str] = None
) -> DeepAgentWrapper:
    """
    Create general-purpose financial agent with all subagents.

    Includes: All available subagents
    """
    return create_fincept_deep_agent(
        model=model,
        api_key=api_key,
        subagents=[
            "research",
            "data_analyst",
            "trading",
            "risk_analyzer",
            "portfolio_optimizer",
            "backtester",
            "reporter"
        ],
        system_prompt="""You are a comprehensive financial intelligence agent.

You have access to specialized subagents for:
- Research and analysis
- Data processing
- Trading strategy development
- Risk assessment
- Portfolio optimization
- Backtesting
- Report generation

Delegate tasks to appropriate subagents based on their expertise.
Coordinate their outputs to accomplish complex financial tasks."""
    )


# Factory function registry
AGENT_FACTORIES = {
    "research": create_research_agent,
    "trading_strategy": create_trading_strategy_agent,
    "portfolio_management": create_portfolio_management_agent,
    "risk_assessment": create_risk_assessment_agent,
    "general": create_general_financial_agent
}


def create_agent_by_type(
    agent_type: str,
    model: Optional[Any] = None,
    api_key: Optional[str] = None
) -> DeepAgentWrapper:
    """
    Create agent by type name.

    Args:
        agent_type: Type of agent (research, trading_strategy, portfolio_management, etc.)
        model: Optional LLM model
        api_key: Optional API key

    Returns:
        Configured agent
    """
    if agent_type not in AGENT_FACTORIES:
        raise ValueError(f"Unknown agent type: {agent_type}. Available: {list(AGENT_FACTORIES.keys())}")

    return AGENT_FACTORIES[agent_type](model=model, api_key=api_key)
