#!/usr/bin/env python3
"""
Renaissance Technologies Multi-Agent System CLI
================================================
Command-line interface for the RenTech multi-agent system.
Called by Tauri/Rust via worker pool or subprocess.

Usage:
    python rentech_cli.py <command> [args...]

Commands:
    # Agent operations
    create_agent <json_config>       - Create and return agent info
    run_agent <json_config>          - Run agent with task
    list_agent_presets               - List available agent presets

    # Team operations
    create_team <json_config>        - Create and return team info
    run_team <json_config>           - Run team with task
    list_team_presets                - List available team presets

    # IC Deliberation
    run_ic_deliberation <json_data>  - Run full IC deliberation on signal

    # System operations
    analyze_signal <json_data>       - Full signal analysis pipeline
    get_config_schema                - Get JSON schema for configurations
    health_check                     - Check system health

Example:
    python rentech_cli.py run_agent '{
        "config": {"name": "Analyst", "model": {"temperature": 0.3}},
        "task": "Analyze AAPL momentum signal"
    }'
"""

import sys
import json
import os
import asyncio
from typing import Dict, Any, Optional
from datetime import datetime

# Add script directory to path
script_dir = os.path.dirname(os.path.abspath(__file__)) if '__file__' in dir() else os.getcwd()
if script_dir not in sys.path:
    sys.path.insert(0, script_dir)

# Parent directory for imports
parent_dir = os.path.dirname(script_dir)
if parent_dir not in sys.path:
    sys.path.insert(0, parent_dir)


# =============================================================================
# RESPONSE HELPERS
# =============================================================================

def success_response(data: Any, message: str = "Success") -> str:
    """Create success response JSON"""
    return json.dumps({
        "success": True,
        "message": message,
        "data": data,
        "timestamp": datetime.utcnow().isoformat(),
    }, indent=2, default=str)


def error_response(error: str, traceback_str: Optional[str] = None) -> str:
    """Create error response JSON"""
    return json.dumps({
        "success": False,
        "error": error,
        "traceback": traceback_str,
        "timestamp": datetime.utcnow().isoformat(),
    }, indent=2)


# =============================================================================
# MODEL FACTORY
# =============================================================================

def create_model_from_settings(model_settings: Dict[str, Any]):
    """
    Create Agno model from ModelSettings dict.

    Uses the new dynamic model factory that reads from frontend SQLite database
    and supports all Agno providers (40+).

    Args:
        model_settings: Dictionary with model configuration

    Returns:
        Agno model instance
    """
    from .utils.model_factory import create_model_from_config

    # Convert model_settings to config dict format
    config_dict = {
        "provider": model_settings.get("provider", "openai"),
        "model_id": model_settings.get("model_id", "gpt-4o"),
        "api_key": model_settings.get("api_key"),
        "base_url": model_settings.get("base_url"),
        "temperature": model_settings.get("temperature", 0.7),
        "max_tokens": model_settings.get("max_tokens", 4096)
    }

    # Use factory to create model (will read from frontend DB if no override)
    return create_model_from_config(config_dict)


# =============================================================================
# AGENT OPERATIONS
# =============================================================================

def create_agent(json_str: str) -> str:
    """
    Create an agent from JSON configuration.

    Input JSON:
    {
        "config": {AgentConfig},
        "task": "optional task to run immediately"
    }
    """
    try:
        from agno.agent import Agent
        from .schemas import AgentConfig

        data = json.loads(json_str)
        config_data = data.get("config", data)

        # Parse config
        config = AgentConfig.from_dict(config_data)

        # Create model
        model = create_model_from_settings(config.model.to_dict())

        # Get Agno kwargs
        kwargs = config.to_agno_kwargs()
        kwargs["model"] = model

        # Create agent
        agent = Agent(**kwargs)

        # If task provided, run it
        task = data.get("task")
        if task:
            response = agent.run(task)
            content = response.content if hasattr(response, 'content') else str(response)

            return success_response({
                "agent_name": config.name,
                "task": task,
                "response": content,
            }, "Agent executed successfully")
        else:
            return success_response({
                "agent_name": config.name,
                "agent_id": config.id,
                "role": config.role,
                "status": "created",
            }, "Agent created successfully")

    except Exception as e:
        import traceback
        return error_response(str(e), traceback.format_exc())


def run_agent(json_str: str) -> str:
    """
    Run an agent with a task.

    Input JSON:
    {
        "config": {AgentConfig},
        "task": "The task to run",
        "context": {"optional": "context data"}
    }
    """
    try:
        from agno.agent import Agent
        from .schemas import AgentConfig

        data = json.loads(json_str)

        if "task" not in data:
            return error_response("Missing 'task' in input")

        config_data = data.get("config", {})
        task = data["task"]
        context = data.get("context", {})

        # Parse config
        config = AgentConfig.from_dict(config_data)

        # Create model
        model = create_model_from_settings(config.model.to_dict())

        # Get Agno kwargs
        kwargs = config.to_agno_kwargs()
        kwargs["model"] = model

        # Create and run agent
        agent = Agent(**kwargs)

        # Build task with context
        if context:
            context_str = "\n".join([f"- {k}: {v}" for k, v in context.items()])
            full_task = f"{task}\n\nContext:\n{context_str}"
        else:
            full_task = task

        response = agent.run(full_task)
        content = response.content if hasattr(response, 'content') else str(response)

        return success_response({
            "agent_name": config.name or "Agent",
            "task": task,
            "response": content,
        }, "Agent task completed")

    except Exception as e:
        import traceback
        return error_response(str(e), traceback.format_exc())


def list_agent_presets() -> str:
    """List available agent preset configurations"""
    try:
        from .schemas import (
            get_research_agent_config,
            get_trading_agent_config,
            get_risk_agent_config,
        )

        presets = {
            "research": get_research_agent_config().to_dict(),
            "trading": get_trading_agent_config().to_dict(),
            "risk": get_risk_agent_config().to_dict(),
        }

        return success_response({
            "presets": list(presets.keys()),
            "configs": presets,
        }, "Agent presets retrieved")

    except Exception as e:
        import traceback
        return error_response(str(e), traceback.format_exc())


# =============================================================================
# TEAM OPERATIONS
# =============================================================================

def create_team(json_str: str) -> str:
    """
    Create a team from JSON configuration.

    Input JSON:
    {
        "config": {TeamConfig},
        "task": "optional task to run immediately"
    }
    """
    try:
        from agno.agent import Agent
        from agno.team import Team
        from .schemas import TeamConfig, AgentConfig

        data = json.loads(json_str)
        config_data = data.get("config", data)

        # Parse config
        config = TeamConfig.from_dict(config_data)

        # Create model for team
        team_model = create_model_from_settings(config.model.to_dict())

        # Create member agents
        members = []
        for member_config in config.member_configs:
            member_model = create_model_from_settings(member_config.model.to_dict())
            member_kwargs = member_config.to_agno_kwargs()
            member_kwargs["model"] = member_model
            members.append(Agent(**member_kwargs))

        # Get Team kwargs
        team_kwargs = config.to_agno_kwargs()
        team_kwargs["model"] = team_model
        team_kwargs["members"] = members

        # Create team
        team = Team(**team_kwargs)

        # If task provided, run it
        task = data.get("task")
        if task:
            response = team.run(task)
            content = response.content if hasattr(response, 'content') else str(response)

            return success_response({
                "team_name": config.name,
                "member_count": len(members),
                "task": task,
                "response": content,
            }, "Team executed successfully")
        else:
            return success_response({
                "team_name": config.name,
                "team_id": config.id,
                "member_count": len(members),
                "members": [m.name for m in members],
                "status": "created",
            }, "Team created successfully")

    except Exception as e:
        import traceback
        return error_response(str(e), traceback.format_exc())


def run_team(json_str: str) -> str:
    """
    Run a team with a task.

    Input JSON:
    {
        "config": {TeamConfig},
        "task": "The task to run",
        "context": {"optional": "context data"}
    }
    """
    try:
        from agno.agent import Agent
        from agno.team import Team
        from .schemas import TeamConfig

        data = json.loads(json_str)

        if "task" not in data:
            return error_response("Missing 'task' in input")

        config_data = data.get("config", {})
        task = data["task"]
        context = data.get("context", {})

        # Parse config
        config = TeamConfig.from_dict(config_data)

        # Create model for team
        team_model = create_model_from_settings(config.model.to_dict())

        # Create member agents
        members = []
        for member_config in config.member_configs:
            member_model = create_model_from_settings(member_config.model.to_dict())
            member_kwargs = member_config.to_agno_kwargs()
            member_kwargs["model"] = member_model
            members.append(Agent(**member_kwargs))

        # Get Team kwargs
        team_kwargs = config.to_agno_kwargs()
        team_kwargs["model"] = team_model
        team_kwargs["members"] = members

        # Create team
        team = Team(**team_kwargs)

        # Build task with context
        if context:
            context_str = "\n".join([f"- {k}: {v}" for k, v in context.items()])
            full_task = f"{task}\n\nContext:\n{context_str}"
        else:
            full_task = task

        response = team.run(full_task)
        content = response.content if hasattr(response, 'content') else str(response)

        return success_response({
            "team_name": config.name or "Team",
            "member_count": len(members),
            "task": task,
            "response": content,
        }, "Team task completed")

    except Exception as e:
        import traceback
        return error_response(str(e), traceback.format_exc())


def list_team_presets() -> str:
    """List available team preset configurations"""
    try:
        from .schemas import (
            get_research_team_config,
            get_trading_team_config,
            get_risk_team_config,
            get_investment_committee_config,
        )

        presets = {
            "research": get_research_team_config().to_dict(),
            "trading": get_trading_team_config().to_dict(),
            "risk": get_risk_team_config().to_dict(),
            "investment_committee": get_investment_committee_config().to_dict(),
        }

        return success_response({
            "presets": list(presets.keys()),
            "configs": presets,
        }, "Team presets retrieved")

    except Exception as e:
        import traceback
        return error_response(str(e), traceback.format_exc())


# =============================================================================
# IC DELIBERATION
# =============================================================================

def run_ic_deliberation(json_str: str) -> str:
    """
    Run full Investment Committee deliberation on a signal.

    Input JSON:
    {
        "signal": {
            "ticker": "AAPL",
            "direction": "long",
            "signal_type": "momentum",
            "p_value": 0.008,
            "confidence": 0.58,
            "expected_return_bps": 85
        },
        "risk_assessment": {
            "risk_level": "medium",
            "var_utilization_pct": 45
        },
        "sizing_recommendation": {
            "final_size_pct": 1.5
        },
        "market_context": {
            "vix": 16.5,
            "regime": "bullish"
        }
    }
    """
    try:
        from .reasoning.ic_deliberation import get_ic_deliberator

        data = json.loads(json_str)

        signal = data.get("signal", {})
        signal_evaluation = data.get("signal_evaluation", {
            "statistical_quality": "good",
            "overall_score": 70,
        })
        risk_assessment = data.get("risk_assessment", {
            "risk_level": "medium",
        })
        sizing_recommendation = data.get("sizing_recommendation", {
            "final_size_pct": 1.0,
        })
        market_context = data.get("market_context", {
            "vix": 20,
            "regime": "neutral",
        })
        historical_decisions = data.get("historical_decisions", [])

        deliberator = get_ic_deliberator()

        # Run deliberation
        result = asyncio.run(deliberator.deliberate(
            signal=signal,
            signal_evaluation=signal_evaluation,
            risk_assessment=risk_assessment,
            sizing_recommendation=sizing_recommendation,
            market_context=market_context,
            historical_decisions=historical_decisions,
        ))

        # Get model config for response
        from .config import get_config
        cfg = get_config()

        return success_response({
            "deliberation_id": result.deliberation_id,
            "subject": result.subject,
            "decision": result.decision,
            "decision_rationale": result.decision_rationale,
            "conditions": result.conditions,
            "approved_size_pct": result.approved_size_pct,
            "confidence": result.confidence,
            "vote_summary": result.vote_summary,
            "member_opinions": [
                {
                    "member": op.member_role,
                    "vote": op.vote.value,
                    "rationale": op.rationale,
                    "confidence": op.confidence,
                }
                for op in result.member_opinions
            ],
            "pros": result.pros,
            "cons": result.cons,
            "reasoning_summary": result.reasoning_chain_summary,
            "llm_config": {
                "provider": cfg.models.provider.value,
                "model_id": cfg.models.model_id,
                "temperature": cfg.models.temperature,
            },
        }, f"IC Decision: {result.decision}")

    except Exception as e:
        import traceback
        return error_response(str(e), traceback.format_exc())


# =============================================================================
# SIGNAL ANALYSIS
# =============================================================================

def analyze_signal(json_str: str) -> str:
    """
    Run full signal analysis pipeline.

    Input JSON:
    {
        "ticker": "AAPL",
        "signal_type": "momentum",
        "direction": "long",
        "trade_value": 1000000,
        "include_deliberation": true
    }
    """
    try:
        from .main import RenaissanceTechnologiesSystem, AnalysisConfig

        data = json.loads(json_str)

        ticker = data.get("ticker", "AAPL")
        trade_value = data.get("trade_value", 1_000_000)
        period = data.get("period", "1y")
        include_deliberation = data.get("include_deliberation", False)

        # Create system and config
        system = RenaissanceTechnologiesSystem()
        config = AnalysisConfig(
            trade_value=trade_value,
            period=period,
        )

        # Run analysis
        decision = system.analyze(ticker, config)

        result = {
            "ticker": ticker,
            "signal": decision.signal.value,
            "confidence": decision.confidence,
            "statistical_edge": decision.statistical_edge,
            "signal_score": decision.signal_score,
            "risk_score": decision.risk_score,
            "execution_score": decision.execution_score,
            "arbitrage_score": decision.arbitrage_score,
            "position_sizing": {
                "recommended_size_pct": decision.position_sizing.recommended_size_pct,
                "kelly_fraction": decision.position_sizing.kelly_fraction,
                "leverage": decision.position_sizing.leverage_recommendation,
            },
            "signal_strength": decision.signal_strength,
            "key_factors": decision.key_factors,
            "risks": decision.risks,
            "reasoning": decision.reasoning,
        }

        # Include IC deliberation if requested
        if include_deliberation:
            # Calculate p-value from statistical significance
            p_value = 1 - (decision.signal_score / 10.0)  # Convert 0-10 scale to p-value
            p_value = max(0.001, min(0.5, p_value))  # Clamp between 0.001 and 0.5

            delib_result = run_ic_deliberation(json.dumps({
                "signal": {
                    "ticker": ticker,
                    "direction": decision.signal.value,
                    "signal_type": data.get("signal_type", "combined"),
                    "p_value": p_value,  # Use calculated p-value
                    "confidence": decision.confidence / 100,
                    "expected_return_bps": decision.statistical_edge * 10000,
                    "average_daily_volume": 10_000_000,  # TODO: Fetch from yfinance/market data API
                },
                "signal_evaluation": {
                    "statistical_quality": "good" if p_value < 0.02 else "moderate" if p_value < 0.05 else "weak",
                    "overall_score": decision.signal_score * 10,  # Convert to 0-100 scale
                },
                "risk_assessment": {
                    "risk_level": "high" if decision.risk_score < 4 else "medium" if decision.risk_score < 7 else "low",
                    "var_utilization_pct": 50.0,  # TODO: Calculate from portfolio VaR
                    "risk_flags": decision.risks,
                    "sector_concentrations": {},  # TODO: Calculate from portfolio holdings
                },
                "sizing_recommendation": {
                    "final_size_pct": decision.position_sizing.recommended_size_pct,
                    "final_notional": trade_value,
                },
                "market_context": {
                    "vix": 20.0,  # TODO: Fetch real-time VIX from market data
                    "regime": "bullish" if decision.signal.value == "long" else "bearish" if decision.signal.value == "short" else "neutral",
                },
            }))
            result["ic_deliberation"] = json.loads(delib_result).get("data", {})

        return success_response(result, f"Analysis complete: {decision.signal.value.upper()}")

    except Exception as e:
        import traceback
        return error_response(str(e), traceback.format_exc())


# =============================================================================
# SYSTEM OPERATIONS
# =============================================================================

def get_config_schema() -> str:
    """Get JSON schemas for all configurations"""
    try:
        from .schemas import get_agent_config_schema, get_team_config_schema

        return success_response({
            "agent_schema": get_agent_config_schema(),
            "team_schema": get_team_config_schema(),
        }, "Schemas retrieved")

    except Exception as e:
        import traceback
        return error_response(str(e), traceback.format_exc())


def health_check() -> str:
    """Check system health and available components"""
    try:
        status = {
            "system": "operational",
            "components": {},
        }

        # Check Agno
        try:
            from agno.agent import Agent
            from agno.team import Team
            status["components"]["agno"] = "ok"
        except ImportError as e:
            status["components"]["agno"] = f"error: {e}"

        # Check model
        try:
            from .config import get_config
            config = get_config()
            status["components"]["config"] = "ok"
            status["model"] = {
                "provider": config.models.provider.value,
                "model_id": config.models.model_id,
            }
        except Exception as e:
            status["components"]["config"] = f"error: {e}"

        # Check reasoning
        try:
            from .reasoning import get_reasoning_engine
            status["components"]["reasoning"] = "ok"
        except Exception as e:
            status["components"]["reasoning"] = f"error: {e}"

        # Check guardrails
        try:
            from .guardrails import get_guardrail_registry
            status["components"]["guardrails"] = "ok"
        except Exception as e:
            status["components"]["guardrails"] = f"error: {e}"

        return success_response(status, "Health check complete")

    except Exception as e:
        import traceback
        return error_response(str(e), traceback.format_exc())


# =============================================================================
# MAIN CLI
# =============================================================================

COMMANDS = {
    # Agent operations
    "create_agent": create_agent,
    "run_agent": run_agent,
    "list_agent_presets": list_agent_presets,

    # Team operations
    "create_team": create_team,
    "run_team": run_team,
    "list_team_presets": list_team_presets,

    # IC Deliberation
    "run_ic_deliberation": run_ic_deliberation,

    # Signal analysis
    "analyze_signal": analyze_signal,
    "analyze": analyze_signal,  # Alias for Tauri compatibility

    # System
    "get_config_schema": get_config_schema,
    "health_check": health_check,
}


def main(args: list) -> str:
    """Main entry point for script execution"""
    if len(args) < 1:
        return json.dumps({
            "success": False,
            "error": "Usage: <command> [json_data]",
            "commands": list(COMMANDS.keys()),
        }, indent=2)

    command = args[0]
    json_data = args[1] if len(args) > 1 else "{}"

    if command not in COMMANDS:
        return json.dumps({
            "success": False,
            "error": f"Unknown command: {command}",
            "available_commands": list(COMMANDS.keys()),
        }, indent=2)

    handler = COMMANDS[command]

    # Commands that don't need JSON input
    no_input_commands = ["list_agent_presets", "list_team_presets", "get_config_schema", "health_check"]

    if command in no_input_commands:
        return handler()
    else:
        return handler(json_data)


if __name__ == "__main__":
    result = main(sys.argv[1:])
    print(result)
