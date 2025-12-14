#!/usr/bin/env python
"""
Agno Trading Service - Main Entry Point
========================================

This service is called from Rust/Tauri commands and provides a unified interface
to the Agno trading agent system.

Usage from Rust:
    execute_python_command(&app, "agno_trading_service.py", &["command", "arg1", "arg2", ...])

Commands:
    - create_agent: Create a new trading agent
    - run_agent: Execute an agent with a prompt
    - create_team: Create a team of agents
    - run_team: Execute a team task
    - list_models: List available LLM models
    - get_agent_status: Get agent execution status
    - analyze_market: Run market analysis
    - generate_trade_signal: Generate trading signals
    - manage_risk: Run risk management analysis
"""

import sys
import json
import os
from typing import Dict, Any, Optional, List

# Add agno_trading to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'agno_trading'))

# Import after path is set
from config.settings import AgnoConfig, LLMConfig, LLMProvider, TradingConfig, TradingMode
from config.models import list_available_models, get_best_model_for_task, validate_model_config
from core.agent_manager import get_agent_manager, AgentConfig as CoreAgentConfig
from agents.market_analyst import MarketAnalystAgent
from agents.trading_strategy import TradingStrategyAgent
from agents.risk_manager import RiskManagerAgent
from agents.portfolio_manager import PortfolioManagerAgent


def create_response(success: bool, data: Any = None, error: str = None) -> str:
    """Create standardized JSON response"""
    return json.dumps({
        "success": success,
        "data": data,
        "error": error
    }, indent=2)


def list_models_command(provider: Optional[str] = None) -> str:
    """List available LLM models"""
    try:
        models = list_available_models(provider)
        return create_response(True, {"models": models})
    except Exception as e:
        return create_response(False, error=str(e))


def recommend_model_command(task: str, budget: str = "medium", reasoning: bool = False) -> str:
    """Recommend best model for a task"""
    try:
        provider, model = get_best_model_for_task(task, budget, reasoning)
        return create_response(True, {
            "provider": provider,
            "model": model,
            "model_string": f"{provider}:{model}"
        })
    except Exception as e:
        return create_response(False, error=str(e))


def validate_config_command(config_json: str) -> str:
    """Validate an Agno configuration"""
    try:
        config_dict = json.loads(config_json)

        # Create LLM config
        llm_data = config_dict.get("llm", {})
        llm_config = LLMConfig(
            provider=LLMProvider(llm_data.get("provider", "openai")),
            model=llm_data.get("model", "gpt-4"),
            temperature=llm_data.get("temperature", 0.7),
            max_tokens=llm_data.get("max_tokens"),
            stream=llm_data.get("stream", False),
            reasoning_effort=llm_data.get("reasoning_effort")
        )

        # Validate
        is_valid, error_msg = validate_model_config(llm_config)

        return create_response(is_valid, {"valid": is_valid}, error_msg)
    except Exception as e:
        return create_response(False, error=str(e))


def create_agent_command(agent_config_json: str) -> str:
    """Create a new trading agent"""
    try:
        config_dict = json.loads(agent_config_json)

        # Parse model string (format: "provider:model")
        agent_model = config_dict.get("agent_model", "openai:gpt-4o-mini")
        if ":" in agent_model:
            model_provider, model_name = agent_model.split(":", 1)
        else:
            model_provider = "openai"
            model_name = agent_model

        # Create agent configuration
        agent_config = CoreAgentConfig(
            name=config_dict.get("name", "Agent"),
            role=config_dict.get("role", "Trading Agent"),
            description=config_dict.get("description", "AI Trading Agent"),
            instructions=config_dict.get("instructions", []),
            model_provider=model_provider,
            model_name=model_name,
            temperature=config_dict.get("temperature", 0.7),
            tools=config_dict.get("tools", ["market_data", "technical_analysis"]),
            symbols=config_dict.get("symbols", ["BTC/USD"]),
            enable_memory=config_dict.get("enable_memory", True)
        )

        # Create agent via agent manager
        manager = get_agent_manager()
        agent_id = manager.create_agent(agent_config)

        return create_response(True, {
            "agent_id": agent_id,
            "status": "created",
            "config": {
                "name": agent_config.name,
                "role": agent_config.role,
                "model": f"{model_provider}:{model_name}"
            }
        })
    except Exception as e:
        return create_response(False, error=str(e))


def run_agent_command(agent_id: str, prompt: str, session_id: Optional[str] = None) -> str:
    """Run an agent with a prompt"""
    try:
        # Get agent manager and run the agent
        manager = get_agent_manager()
        result = manager.run_agent(agent_id, prompt, session_id)

        return create_response(True, result)
    except Exception as e:
        return create_response(False, error=str(e))


def analyze_market_command(
    symbol: str,
    agent_model: str = "openai:gpt-4o-mini",
    analysis_type: str = "comprehensive"
) -> str:
    """Analyze market for a given symbol"""
    try:
        # Parse model string
        if ":" in agent_model:
            model_provider, model_name = agent_model.split(":", 1)
        else:
            model_provider = "openai"
            model_name = agent_model

        # Create market analyst agent
        analyst = MarketAnalystAgent(
            model_provider=model_provider,
            model_name=model_name,
            symbols=[symbol]
        )

        # Run analysis
        result = analyst.analyze_market(symbol, analysis_type)

        return create_response(True, result)
    except Exception as e:
        return create_response(False, error=str(e))


def generate_trade_signal_command(
    symbol: str,
    strategy: str = "momentum",
    agent_model: str = "anthropic:claude-sonnet-4",
    market_data: Optional[str] = None
) -> str:
    """Generate trading signal"""
    try:
        # Parse model string
        if ":" in agent_model:
            model_provider, model_name = agent_model.split(":", 1)
        else:
            model_provider = "anthropic"
            model_name = agent_model

        # Create trading strategy agent
        strategist = TradingStrategyAgent(
            model_provider=model_provider,
            model_name=model_name,
            symbols=[symbol]
        )

        # Generate signal
        result = strategist.generate_trade_signal(symbol, strategy)

        return create_response(True, result)
    except Exception as e:
        return create_response(False, error=str(e))


def manage_risk_command(
    portfolio_data: str,
    agent_model: str = "openai:gpt-4",
    risk_tolerance: str = "moderate"
) -> str:
    """Run risk management analysis"""
    try:
        portfolio = json.loads(portfolio_data)

        # Parse model string
        if ":" in agent_model:
            model_provider, model_name = agent_model.split(":", 1)
        else:
            model_provider = "openai"
            model_name = agent_model

        # Create risk manager agent
        risk_manager = RiskManagerAgent(
            model_provider=model_provider,
            model_name=model_name,
            risk_tolerance=risk_tolerance
        )

        # Run risk analysis
        positions = portfolio.get("positions", [])
        portfolio_value = portfolio.get("total_value", 10000.0)

        result = risk_manager.analyze_portfolio_risk(positions, portfolio_value)

        return create_response(True, result)
    except Exception as e:
        return create_response(False, error=str(e))


def get_config_template_command(config_type: str = "default") -> str:
    """Get configuration template"""
    try:
        if config_type == "default":
            config = AgnoConfig()
        elif config_type == "conservative":
            config = AgnoConfig(
                trading=TradingConfig(
                    mode=TradingMode.PAPER,
                    max_position_size=0.05,
                    max_leverage=1.0,
                    stop_loss_pct=0.03,
                    take_profit_pct=0.10
                )
            )
        elif config_type == "aggressive":
            config = AgnoConfig(
                trading=TradingConfig(
                    mode=TradingMode.PAPER,
                    max_position_size=0.20,
                    max_leverage=2.0,
                    stop_loss_pct=0.08,
                    take_profit_pct=0.25
                )
            )
        else:
            return create_response(False, error=f"Unknown config type: {config_type}")

        return create_response(True, {"config": config.to_dict()})
    except Exception as e:
        return create_response(False, error=str(e))


def create_competition_command(
    name: str,
    models_json: str,
    task_type: str = "trading"
) -> str:
    """
    Create multi-model competition

    Args:
        name: Competition name
        models_json: JSON array of models ["openai:gpt-4", "anthropic:claude-sonnet-4"]
        task_type: Type of task
    """
    try:
        import json
        from agno_trading.core.team_orchestrator import TeamOrchestrator
        from agno_trading.core.agent_manager import AgentManager

        models = json.loads(models_json)
        manager = AgentManager()
        orchestrator = TeamOrchestrator()

        # Create agents for each model
        agents = []
        for model_str in models:
            agent_config = {
                "name": f"Agent-{model_str}",
                "role": "Market Analyst",
                "agent_model": model_str
            }
            agent_id = manager.create_agent(agent_config)
            agent = manager.get_agent(agent_id)

            agents.append({
                "model": model_str,
                "agent": agent
            })

        team_id = orchestrator.create_competition(name, agents, task_type)

        return create_response(True, {
            "team_id": team_id,
            "models": models,
            "agent_count": len(agents)
        })
    except Exception as e:
        return create_response(False, error=str(e))


def run_competition_command(team_id: str, symbol: str, task: str = "analyze") -> str:
    """
    Run multi-model competition

    Args:
        team_id: Competition ID
        symbol: Trading symbol
        task: Task type (analyze, signal)
    """
    try:
        import asyncio
        from agno_trading.core.team_orchestrator import TeamOrchestrator

        orchestrator = TeamOrchestrator()

        # Build prompt based on task
        if task == "analyze":
            prompt = f"Analyze {symbol} market conditions and provide trading insights."
        elif task == "signal":
            prompt = f"Generate a trade signal for {symbol} with entry/exit points."
        else:
            prompt = f"Evaluate {symbol} for trading opportunities."

        # Run parallel execution
        loop = asyncio.get_event_loop()
        result = loop.run_until_complete(
            orchestrator.run_parallel(team_id, prompt, streaming=True)
        )

        return create_response(True, result)
    except Exception as e:
        return create_response(False, error=str(e))


def get_leaderboard_command() -> str:
    """Get model performance leaderboard"""
    try:
        from agno_trading.core.team_orchestrator import TeamOrchestrator

        orchestrator = TeamOrchestrator()
        leaderboard = orchestrator.get_leaderboard()

        return create_response(True, {"leaderboard": leaderboard})
    except Exception as e:
        return create_response(False, error=str(e))


def get_recent_decisions_command(limit: int = 50) -> str:
    """Get recent model decisions"""
    try:
        from agno_trading.core.team_orchestrator import TeamOrchestrator

        orchestrator = TeamOrchestrator()
        decisions = orchestrator.get_recent_decisions(int(limit))

        return create_response(True, {"decisions": decisions})
    except Exception as e:
        return create_response(False, error=str(e))


# Command dispatcher
COMMANDS = {
    "list_models": list_models_command,
    "recommend_model": recommend_model_command,
    "validate_config": validate_config_command,
    "create_agent": create_agent_command,
    "run_agent": run_agent_command,
    "analyze_market": analyze_market_command,
    "generate_trade_signal": generate_trade_signal_command,
    "manage_risk": manage_risk_command,
    "get_config_template": get_config_template_command,
    "create_competition": create_competition_command,
    "run_competition": run_competition_command,
    "get_leaderboard": get_leaderboard_command,
    "get_recent_decisions": get_recent_decisions_command,
}


def main():
    """Main entry point for CLI usage"""
    if len(sys.argv) < 2:
        print(create_response(False, error="No command specified"))
        sys.exit(1)

    command = sys.argv[1]
    args = sys.argv[2:]

    if command not in COMMANDS:
        print(create_response(False, error=f"Unknown command: {command}"))
        sys.exit(1)

    try:
        # Call command function with args
        result = COMMANDS[command](*args)
        print(result)
    except TypeError as e:
        print(create_response(False, error=f"Invalid arguments for command '{command}': {str(e)}"))
        sys.exit(1)
    except Exception as e:
        print(create_response(False, error=f"Command execution failed: {str(e)}"))
        sys.exit(1)


if __name__ == "__main__":
    main()
