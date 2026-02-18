"""
Alpha Arena Main Entry Point

Single JSON payload interface for Rust/Tauri integration.
"""

import asyncio
import json
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional

# Add parent directory to path for imports
parent_dir = str(Path(__file__).parent.parent)
if parent_dir not in sys.path:
    sys.path.insert(0, parent_dir)

# Wrap all imports in try-except to provide meaningful error messages
try:
    from alpha_arena.types.models import (
        CompetitionConfig,
        CompetitionModel,
        CompetitionMode,
        CompetitionStatus,
    )
    from alpha_arena.core.competition import (
        AlphaArenaCompetition,
        create_competition,
        get_competition,
        list_competitions,
        delete_competition,
    )
    from alpha_arena.core.database import get_database
    from alpha_arena.config.agent_cards import list_agent_cards, get_agent_card
    from alpha_arena.utils.logging import setup_logging, get_logger
    from alpha_arena.utils.uuid import generate_competition_id

    IMPORT_ERROR = None
except ImportError as e:
    IMPORT_ERROR = str(e)
    # Create minimal stubs so the script can still run and report error
    def setup_logging(**kwargs): pass
    class DummyLogger:
        def info(self, *a, **k): pass
        def error(self, *a, **k): pass
        def warning(self, *a, **k): pass
        def exception(self, *a, **k): pass
    def get_logger(name): return DummyLogger()

# Setup logging
setup_logging(level="INFO")
logger = get_logger("main")


async def handle_action(action: str, params: Dict, api_keys: Dict) -> Dict[str, Any]:
    """
    Handle an action request.

    Args:
        action: Action to perform
        params: Action parameters
        api_keys: API keys for providers

    Returns:
        Result dictionary
    """
    # Check for import errors first
    if IMPORT_ERROR:
        return {
            "success": False,
            "error": f"Import error: {IMPORT_ERROR}. Please ensure all dependencies are installed (pydantic, httpx, agno).",
            "details": {"missing_dependency": IMPORT_ERROR}
        }

    try:
        if action == "create_competition":
            return await create_competition_handler(params, api_keys)

        elif action == "start_competition":
            return await start_competition_handler(params, api_keys)

        elif action == "run_cycle":
            return await run_cycle_handler(params, api_keys)

        elif action == "stop_competition":
            return await stop_competition_handler(params)

        elif action == "get_leaderboard":
            return await get_leaderboard_handler(params)

        elif action == "get_decisions":
            return await get_decisions_handler(params)

        elif action == "get_snapshots":
            return await get_snapshots_handler(params)

        elif action == "list_competitions":
            return await list_competitions_handler(params)

        elif action == "get_competition":
            return await get_competition_handler(params)

        elif action == "delete_competition":
            return await delete_competition_handler(params)

        elif action == "list_agents":
            return await list_agents_handler(params)

        elif action == "get_agent":
            return await get_agent_handler(params)

        elif action == "system_info":
            return get_system_info()

        elif action == "get_evaluation":
            return await get_evaluation_handler(params)

        # HITL (Human-in-the-Loop) actions
        elif action == "check_approval":
            return await check_approval_handler(params)

        elif action == "approve_decision":
            return await approve_decision_handler(params)

        elif action == "reject_decision":
            return await reject_decision_handler(params)

        elif action == "get_pending_approvals":
            return await get_pending_approvals_handler(params)

        elif action == "get_hitl_status":
            return await get_hitl_status_handler(params)

        elif action == "update_hitl_rule":
            return await update_hitl_rule_handler(params)

        # Portfolio metrics actions
        elif action == "get_portfolio_metrics":
            return await get_portfolio_metrics_handler(params)

        elif action == "get_equity_curve":
            return await get_equity_curve_handler(params)

        # Grid strategy actions
        elif action == "create_grid_agent":
            return await create_grid_agent_handler(params)

        elif action == "get_grid_status":
            return await get_grid_status_handler(params)

        # Research actions
        elif action == "get_research":
            return await get_research_handler(params)

        elif action == "get_sec_filings":
            return await get_sec_filings_handler(params)

        # Features actions
        elif action == "get_features":
            return await get_features_handler(params)

        # Trading styles actions
        elif action == "list_trading_styles":
            return await list_trading_styles_handler(params)

        elif action == "create_styled_agents":
            return await create_styled_agents_handler(params)

        # Broker actions
        elif action == "list_brokers":
            return await list_brokers_handler(params)

        elif action == "get_broker":
            return await get_broker_handler(params)

        elif action == "get_broker_ticker":
            return await get_broker_ticker_handler(params)

        # Sentiment actions
        elif action == "get_sentiment":
            return await get_sentiment_handler(params)

        elif action == "get_market_mood":
            return await get_market_mood_handler(params)

        # Memory actions
        elif action == "get_agent_context":
            return await get_agent_context_handler(params)

        elif action == "get_agent_memory_summary":
            return await get_agent_memory_summary_handler(params)

        elif action == "search_agent_memory":
            return await search_agent_memory_handler(params)

        else:
            return {"success": False, "error": f"Unknown action: {action}"}

    except Exception as e:
        logger.exception(f"Error handling action {action}: {e}")
        return {"success": False, "error": str(e)}


async def create_competition_handler(params: Dict, api_keys: Dict) -> Dict[str, Any]:
    """Create a new competition."""
    try:
        logger.info(f"Creating competition with params: {list(params.keys())}")
        logger.info(f"API keys provided for: {list(api_keys.keys())}")

        # Generate competition ID if not provided
        competition_id = params.get("competition_id") or generate_competition_id()

        # Build models from params
        models_data = params.get("models", [])
        if not models_data:
            return {"success": False, "error": "No models provided in configuration"}

        if len(models_data) < 2:
            return {"success": False, "error": "At least 2 models are required for a competition"}

        # Validate API keys before creating agents
        missing_keys = []
        for model_data in models_data:
            provider = (model_data.get("provider") or "").lower()
            has_key = bool(model_data.get("api_key"))
            has_provider_key = bool(api_keys.get(provider) or api_keys.get(model_data.get("provider", "")))
            no_key_required = provider in ("ollama", "fincept")

            if not has_key and not has_provider_key and not no_key_required:
                missing_keys.append(f"{model_data.get('name', 'Unknown')} ({provider})")

        if missing_keys:
            return {
                "success": False,
                "error": f"Missing API keys for: {', '.join(missing_keys)}. Configure API keys in Settings > LLM Config.",
            }

        models = []

        for model_data in models_data:
            # Get agent card for additional info
            card = get_agent_card(model_data.get("name", ""))

            model = CompetitionModel(
                name=model_data.get("name"),
                provider=model_data.get("provider") or (card.provider if card else "openai"),
                model_id=model_data.get("model_id") or (card.model_id if card else "gpt-4o-mini"),
                api_key=model_data.get("api_key"),
                initial_capital=model_data.get("initial_capital", params.get("initial_capital", 10000)),
                trading_style=model_data.get("trading_style"),  # Pass trading style if specified
                metadata=model_data.get("metadata", {}),  # Pass any additional metadata
            )
            models.append(model)
            style_info = f" [style: {model.trading_style}]" if model.trading_style else ""
            logger.info(f"Configured model: {model.name} ({model.provider}/{model.model_id}){style_info}")

        # Extract global LLM settings from Settings tab (if provided)
        llm_settings = params.get("llm_settings", {})
        llm_temperature = llm_settings.get("temperature", 0.7) if llm_settings else 0.7
        llm_max_tokens = llm_settings.get("max_tokens", 2000) if llm_settings else 2000
        llm_system_prompt = llm_settings.get("system_prompt", "") if llm_settings else ""

        logger.info(f"LLM global settings: temperature={llm_temperature}, max_tokens={llm_max_tokens}, system_prompt={'(set)' if llm_system_prompt else '(default)'}")

        # Create config
        config = CompetitionConfig(
            competition_id=competition_id,
            competition_name=params.get("competition_name", f"Competition {competition_id[:8]}"),
            models=models,
            symbols=params.get("symbols", ["BTC/USD"]),
            initial_capital=params.get("initial_capital", 10000),
            mode=CompetitionMode(params.get("mode", "baseline")),
            cycle_interval_seconds=params.get("cycle_interval_seconds", 150),
            exchange_id=params.get("exchange_id", "kraken"),
            max_cycles=params.get("max_cycles"),
            custom_prompt=llm_system_prompt if llm_system_prompt else None,
            temperature=llm_temperature,
            max_tokens=llm_max_tokens,
        )

        logger.info(f"Competition config created: {competition_id}")

        # Create competition
        competition = create_competition(config)

        # Initialize with API keys - use asyncio.wait_for for overall timeout
        try:
            init_success = await asyncio.wait_for(
                competition.initialize(api_keys),
                timeout=60.0  # 60 second overall timeout
            )
        except asyncio.TimeoutError:
            logger.error("Competition initialization timed out after 60 seconds")
            return {
                "success": False,
                "error": "Competition initialization timed out. This may be due to network issues or unavailable LLM providers.",
            }

        if init_success:
            # Save to database
            try:
                db = get_database()
                db.save_competition(config, CompetitionStatus.CREATED)
            except Exception as db_error:
                logger.warning(f"Failed to save competition to database: {db_error}")
                # Continue anyway - competition is functional

            # Report which agents initialized successfully
            initialized_agents = list(competition._agents.keys())
            failed_agents = [m.name for m in models if m.name not in initialized_agents]

            result = {
                "success": True,
                "competition_id": competition_id,
                "config": config.model_dump(),
                "status": "created",
                "models_count": len(initialized_agents),
                "initialized_agents": initialized_agents,
            }

            if failed_agents:
                result["warnings"] = [f"{name}: failed to initialize LLM" for name in failed_agents]
                result["failed_agents"] = failed_agents
                logger.warning(f"{len(failed_agents)} agent(s) failed to initialize: {failed_agents}")

            return result
        else:
            # Competition init failed - not enough agents with real LLMs
            initialized_count = len(competition._agents)
            total_count = len(models)
            return {
                "success": False,
                "error": (
                    f"Competition initialization failed. Only {initialized_count}/{total_count} agents "
                    f"initialized successfully. At least 2 agents with valid LLM connections are required. "
                    f"Check that API keys are valid and LLM providers are accessible."
                ),
            }

    except Exception as e:
        logger.exception(f"Error creating competition: {e}")
        return {
            "success": False,
            "error": str(e),
            "error_type": type(e).__name__,
        }


async def start_competition_handler(params: Dict, api_keys: Dict) -> Dict[str, Any]:
    """Start a competition."""
    competition_id = params.get("competition_id")
    if not competition_id:
        return {"success": False, "error": "Missing competition_id"}

    competition = get_competition(competition_id)

    # If not in memory, try to restore from database
    if not competition:
        logger.info(f"Competition {competition_id} not in memory for start, trying to restore...")
        db = get_database()
        comp_data = db.get_competition(competition_id)

        if comp_data:
            try:
                config = CompetitionConfig(**comp_data["config"])
                competition = create_competition(config)
            except Exception as e:
                logger.exception(f"Failed to restore competition: {e}")
                return {"success": False, "error": f"Failed to restore competition: {str(e)}"}
        else:
            return {"success": False, "error": f"Competition not found: {competition_id}"}

    if await competition.start(api_keys):
        db = get_database()
        db.update_competition_status(
            competition_id,
            CompetitionStatus.RUNNING,
            start_time=competition.start_time,
        )

        return {
            "success": True,
            "competition_id": competition_id,
            "status": "running",
        }
    else:
        return {"success": False, "error": "Failed to start competition"}


async def run_cycle_handler(params: Dict, api_keys: Dict) -> Dict[str, Any]:
    """Run a single competition cycle."""
    competition_id = params.get("competition_id")
    if not competition_id:
        return {"success": False, "error": "Missing competition_id"}

    # Try to get competition from memory first
    competition = get_competition(competition_id)

    # If not in memory, try to restore from database
    if not competition:
        logger.info(f"Competition {competition_id} not in memory, trying to restore from database...")
        db = get_database()
        comp_data = db.get_competition(competition_id)

        if comp_data:
            logger.info(f"Found competition in database, restoring...")
            try:
                # Reconstruct the competition from database
                config = CompetitionConfig(**comp_data["config"])
                competition = create_competition(config)

                # Initialize the competition with API keys
                init_success = await asyncio.wait_for(
                    competition.initialize(api_keys),
                    timeout=60.0
                )

                if not init_success:
                    return {"success": False, "error": "Failed to reinitialize competition from database"}

                # Restore cycle count
                competition.cycle_count = comp_data.get("cycle_count", 0)
                logger.info(f"Competition restored with cycle_count={competition.cycle_count}")
            except Exception as e:
                logger.exception(f"Failed to restore competition: {e}")
                return {"success": False, "error": f"Failed to restore competition: {str(e)}"}
        else:
            return {"success": False, "error": f"Competition not found: {competition_id}"}

    # Collect streaming results
    events = []
    async for response in competition.run_cycle():
        event_data = {
            "event": response.event.value if hasattr(response.event, 'value') else str(response.event),
            "content": response.content,
            "metadata": response.metadata,
        }
        events.append(event_data)

    # Update database
    db = get_database()
    db.update_competition_status(
        competition_id,
        competition.status,
        cycle_count=competition.cycle_count,
    )

    # Save decisions
    for decision in competition.get_decisions(limit=len(competition.config.models)):
        db.save_decision(decision)

    # Save snapshots
    for snapshot in competition.get_snapshots()[-len(competition.config.models):]:
        db.save_snapshot(snapshot)

    # Save leaderboard
    leaderboard = competition.get_leaderboard()
    db.save_leaderboard(competition_id, competition.cycle_count, leaderboard)

    return {
        "success": True,
        "competition_id": competition_id,
        "cycle_number": competition.cycle_count,
        "events": events,
        "leaderboard": [e.model_dump() for e in leaderboard],
    }


async def stop_competition_handler(params: Dict) -> Dict[str, Any]:
    """Stop a competition."""
    competition_id = params.get("competition_id")
    if not competition_id:
        return {"success": False, "error": "Missing competition_id"}

    competition = get_competition(competition_id)

    # If not in memory, we can still update the database status
    if not competition:
        logger.info(f"Competition {competition_id} not in memory, updating database status...")
        db = get_database()
        comp_data = db.get_competition(competition_id)
        if not comp_data:
            return {"success": False, "error": f"Competition not found: {competition_id}"}
        # Just update status in database
        db.update_competition_status(competition_id, CompetitionStatus.PAUSED)
        return {
            "success": True,
            "competition_id": competition_id,
            "status": "paused",
        }

    await competition.stop()

    db = get_database()
    db.update_competition_status(competition_id, CompetitionStatus.PAUSED)

    return {
        "success": True,
        "competition_id": competition_id,
        "status": "paused",
    }


async def get_leaderboard_handler(params: Dict) -> Dict[str, Any]:
    """Get current leaderboard."""
    competition_id = params.get("competition_id")
    if not competition_id:
        return {"success": False, "error": "Missing competition_id"}

    competition = get_competition(competition_id)
    if competition:
        leaderboard = competition.get_leaderboard()
        return {
            "success": True,
            "leaderboard": [e.model_dump() for e in leaderboard],
            "cycle_number": competition.cycle_count,
        }

    # Try database
    db = get_database()
    leaderboard = db.get_latest_leaderboard(competition_id)
    if leaderboard:
        return {
            "success": True,
            "leaderboard": leaderboard,
        }

    # No leaderboard history yet - check if competition exists and build initial leaderboard from config
    comp_data = db.get_competition(competition_id)
    if comp_data:
        config = comp_data.get("config", {})
        models = config.get("models", [])
        initial_capital = config.get("initial_capital", 10000)
        initial_leaderboard = []
        for i, model in enumerate(models):
            initial_leaderboard.append({
                "rank": i + 1,
                "model_name": model.get("name", f"Model {i+1}"),
                "portfolio_value": model.get("initial_capital", initial_capital),
                "total_pnl": 0.0,
                "return_pct": 0.0,
                "trades_count": 0,
                "cash": model.get("initial_capital", initial_capital),
                "positions_count": 0,
                "win_rate": None,
                "sharpe_ratio": None,
            })
        return {
            "success": True,
            "leaderboard": initial_leaderboard,
        }

    return {"success": False, "error": f"Competition not found: {competition_id}"}


async def get_decisions_handler(params: Dict) -> Dict[str, Any]:
    """Get decisions for a competition."""
    competition_id = params.get("competition_id")
    model_name = params.get("model_name")
    limit = params.get("limit", 50)

    if not competition_id:
        return {"success": False, "error": "Missing competition_id"}

    # Try live competition first
    competition = get_competition(competition_id)
    if competition:
        decisions = competition.get_decisions(model_name=model_name, limit=limit)
        return {
            "success": True,
            "decisions": [d.model_dump() for d in decisions],
        }

    # Fall back to database
    db = get_database()
    decisions = db.get_decisions(competition_id, model_name, limit)
    return {
        "success": True,
        "decisions": decisions,
    }


async def get_snapshots_handler(params: Dict) -> Dict[str, Any]:
    """Get performance snapshots for charting."""
    competition_id = params.get("competition_id")
    model_name = params.get("model_name")

    if not competition_id:
        return {"success": False, "error": "Missing competition_id"}

    # Try live competition first
    competition = get_competition(competition_id)
    if competition:
        snapshots = competition.get_snapshots(model_name=model_name)
        return {
            "success": True,
            "snapshots": [s.model_dump() for s in snapshots],
        }

    # Fall back to database
    db = get_database()
    snapshots = db.get_snapshots(competition_id, model_name)
    return {
        "success": True,
        "snapshots": snapshots,
    }


async def list_competitions_handler(params: Dict) -> Dict[str, Any]:
    """List all competitions."""
    limit = params.get("limit", 50)

    # Get from database
    db = get_database()
    competitions = db.list_competitions(limit)

    return {
        "success": True,
        "competitions": competitions,
        "count": len(competitions),
    }


async def get_competition_handler(params: Dict) -> Dict[str, Any]:
    """Get a specific competition by ID."""
    competition_id = params.get("competition_id")
    if not competition_id:
        return {"success": False, "error": "Missing competition_id"}

    db = get_database()
    comp_data = db.get_competition(competition_id)

    if comp_data:
        return {
            "success": True,
            "competition": comp_data,
        }

    return {"success": False, "error": f"Competition not found: {competition_id}"}


async def delete_competition_handler(params: Dict) -> Dict[str, Any]:
    """Delete a competition."""
    competition_id = params.get("competition_id")
    if not competition_id:
        return {"success": False, "error": "Missing competition_id"}

    # Remove from memory
    delete_competition(competition_id)

    # Remove from database
    db = get_database()
    db.delete_competition(competition_id)

    return {
        "success": True,
        "competition_id": competition_id,
        "deleted": True,
    }


async def list_agents_handler(params: Dict) -> Dict[str, Any]:
    """List available agents."""
    cards = list_agent_cards()

    return {
        "success": True,
        "agents": [
            {
                "name": card.name,
                "provider": card.provider,
                "model_id": card.model_id,
                "description": card.description,
                "enabled": card.enabled,
            }
            for card in cards
        ],
        "count": len(cards),
    }


async def get_agent_handler(params: Dict) -> Dict[str, Any]:
    """Get agent details."""
    name = params.get("name")
    if not name:
        return {"success": False, "error": "Missing agent name"}

    card = get_agent_card(name)
    if card:
        return {
            "success": True,
            "agent": card.model_dump(),
        }

    return {"success": False, "error": f"Agent not found: {name}"}


async def get_evaluation_handler(params: Dict) -> Dict[str, Any]:
    """Get evaluation metrics for a specific model in a competition."""
    competition_id = params.get("competition_id")
    model_name = params.get("model_name")

    if not competition_id:
        return {"success": False, "error": "Missing competition_id"}
    if not model_name:
        return {"success": False, "error": "Missing model_name"}

    # Try live competition first
    competition = get_competition(competition_id)
    if competition:
        leaderboard = competition.get_leaderboard()
        for entry in leaderboard:
            if entry.model_name == model_name:
                metrics = entry.model_dump()
                return {
                    "success": True,
                    "metrics": {
                        "total_return": metrics.get("return_pct", 0),
                        "portfolio_value": metrics.get("portfolio_value", 0),
                        "total_trades": metrics.get("total_trades", 0),
                        "win_rate": metrics.get("win_rate", 0),
                        "sharpe_ratio": metrics.get("sharpe_ratio", 0),
                        "max_drawdown": metrics.get("max_drawdown", 0),
                    },
                }
        return {"success": False, "error": f"Model not found in competition: {model_name}"}

    # Fall back to database
    db = get_database()
    leaderboard = db.get_latest_leaderboard(competition_id)
    if leaderboard:
        for entry in leaderboard:
            entry_name = entry.get("model_name", "") if isinstance(entry, dict) else getattr(entry, "model_name", "")
            if entry_name == model_name:
                if isinstance(entry, dict):
                    return {
                        "success": True,
                        "metrics": {
                            "total_return": entry.get("return_pct", 0),
                            "portfolio_value": entry.get("portfolio_value", 0),
                            "total_trades": entry.get("total_trades", 0),
                            "win_rate": entry.get("win_rate", 0),
                            "sharpe_ratio": entry.get("sharpe_ratio", 0),
                            "max_drawdown": entry.get("max_drawdown", 0),
                        },
                    }
        return {"success": False, "error": f"Model not found in leaderboard: {model_name}"}

    return {"success": False, "error": f"Competition not found: {competition_id}"}


def get_system_info() -> Dict[str, Any]:
    """Get system information."""
    # Get available trading styles
    try:
        from alpha_arena.config.trading_styles import list_trading_styles
        styles = [s.id for s in list_trading_styles()]
    except Exception:
        styles = []

    return {
        "success": True,
        "version": "2.1.0",
        "framework": "alpha_arena",
        "features": [
            "multi_model_competition",
            "streaming_responses",
            "paper_trading",
            "real_time_market_data",
            "performance_tracking",
            "evaluation_metrics",
            "guardrails",
            "database_persistence",
            "hitl_approval",
            "portfolio_metrics",
            "grid_trading",
            "research_agent",
            "features_pipeline",
            "trading_styles",  # NEW: Single provider with multiple agent styles
            "real_time_portfolio_updates",  # NEW: Portfolio updates after trades
        ],
        "supported_providers": [
            "openai",
            "anthropic",
            "google",
            "deepseek",
            "groq",
            "openrouter",
            "ollama",
        ],
        "trading_styles": styles,
        "default_exchange": "kraken",
    }


# =============================================================================
# HITL (Human-in-the-Loop) Handlers
# =============================================================================

async def check_approval_handler(params: Dict) -> Dict[str, Any]:
    """Check if a trading decision requires approval."""
    try:
        from alpha_arena.core.hitl import check_decision_approval

        decision_dict = params.get("decision", {})
        context = params.get("context", {})

        if not decision_dict:
            return {"success": False, "error": "Missing decision data"}

        result = check_decision_approval(decision_dict, context)
        return {
            "success": True,
            **result,
        }
    except Exception as e:
        logger.exception(f"Error checking approval: {e}")
        return {"success": False, "error": str(e)}


async def approve_decision_handler(params: Dict) -> Dict[str, Any]:
    """Approve a pending decision."""
    try:
        from alpha_arena.core.hitl import approve_decision

        request_id = params.get("request_id")
        approved_by = params.get("approved_by", "user")
        notes = params.get("notes", "")

        if not request_id:
            return {"success": False, "error": "Missing request_id"}

        result = approve_decision(request_id, approved_by, notes)
        return {
            "success": True,
            **result,
        }
    except Exception as e:
        logger.exception(f"Error approving decision: {e}")
        return {"success": False, "error": str(e)}


async def reject_decision_handler(params: Dict) -> Dict[str, Any]:
    """Reject a pending decision."""
    try:
        from alpha_arena.core.hitl import reject_decision

        request_id = params.get("request_id")
        rejected_by = params.get("rejected_by", "user")
        notes = params.get("notes", "")

        if not request_id:
            return {"success": False, "error": "Missing request_id"}

        result = reject_decision(request_id, rejected_by, notes)
        return {
            "success": True,
            **result,
        }
    except Exception as e:
        logger.exception(f"Error rejecting decision: {e}")
        return {"success": False, "error": str(e)}


async def get_pending_approvals_handler(params: Dict) -> Dict[str, Any]:
    """Get all pending approval requests."""
    try:
        from alpha_arena.core.hitl import get_pending_approvals

        pending = get_pending_approvals()
        return {
            "success": True,
            "pending_approvals": pending,
            "count": len(pending),
        }
    except Exception as e:
        logger.exception(f"Error getting pending approvals: {e}")
        return {"success": False, "error": str(e)}


async def get_hitl_status_handler(params: Dict) -> Dict[str, Any]:
    """Get HITL manager status and configuration."""
    try:
        from alpha_arena.core.hitl import get_hitl_manager

        manager = get_hitl_manager()
        return {
            "success": True,
            "status": manager.to_dict(),
        }
    except Exception as e:
        logger.exception(f"Error getting HITL status: {e}")
        return {"success": False, "error": str(e)}


async def update_hitl_rule_handler(params: Dict) -> Dict[str, Any]:
    """Update a HITL rule configuration."""
    try:
        from alpha_arena.core.hitl import get_hitl_manager

        rule_name = params.get("rule_name")
        enabled = params.get("enabled")

        if not rule_name:
            return {"success": False, "error": "Missing rule_name"}

        if enabled is None:
            return {"success": False, "error": "Missing enabled flag"}

        manager = get_hitl_manager()
        manager.set_rule_enabled(rule_name, enabled)

        return {
            "success": True,
            "rule_name": rule_name,
            "enabled": enabled,
        }
    except Exception as e:
        logger.exception(f"Error updating HITL rule: {e}")
        return {"success": False, "error": str(e)}


# =============================================================================
# Portfolio Metrics Handlers
# =============================================================================

async def get_portfolio_metrics_handler(params: Dict) -> Dict[str, Any]:
    """Get portfolio metrics for a competition or model."""
    try:
        from alpha_arena.core.portfolio_metrics import PortfolioAnalyzer

        competition_id = params.get("competition_id")
        model_name = params.get("model_name")

        if not competition_id:
            return {"success": False, "error": "Missing competition_id"}

        db = get_database()
        comp_data = db.get_competition(competition_id)
        if not comp_data:
            return {"success": False, "error": f"Competition not found: {competition_id}"}

        initial_capital = comp_data.get("config", {}).get("initial_capital", 10000)

        # Get snapshots from database
        snapshots = db.get_snapshots(competition_id, model_name)
        if not snapshots:
            return {"success": False, "error": "No portfolio snapshots found"}

        # Group snapshots by model
        from collections import defaultdict
        by_model: Dict[str, list] = defaultdict(list)
        for snap in snapshots:
            by_model[snap.get("model_name", "unknown")].append(snap)

        # Calculate metrics per model
        all_metrics = {}
        for mname, model_snaps in by_model.items():
            # Sort by cycle number
            model_snaps.sort(key=lambda s: s.get("cycle_number", 0))
            analyzer = PortfolioAnalyzer(initial_capital=initial_capital)
            for snap in model_snaps:
                analyzer.record_value(snap.get("portfolio_value", initial_capital))
            all_metrics[mname] = analyzer.calculate_metrics().to_dict()

        # If a specific model was requested, return just that
        if model_name and model_name in all_metrics:
            return {
                "success": True,
                "model_name": model_name,
                "metrics": all_metrics[model_name],
            }

        return {
            "success": True,
            "metrics": all_metrics,
        }
    except Exception as e:
        logger.exception(f"Error getting portfolio metrics: {e}")
        return {"success": False, "error": str(e)}


async def get_equity_curve_handler(params: Dict) -> Dict[str, Any]:
    """Get equity curve data for charting."""
    try:
        from alpha_arena.core.portfolio_metrics import get_analyzer

        competition_id = params.get("competition_id")
        model_name = params.get("model_name")

        if not competition_id:
            return {"success": False, "error": "Missing competition_id"}

        # Get from database snapshots
        db = get_database()
        snapshots = db.get_snapshots(competition_id, model_name)

        if not snapshots:
            return {"success": False, "error": "No snapshots found"}

        # Build equity curve data
        equity_curve = []
        for snapshot in snapshots:
            equity_curve.append({
                "timestamp": snapshot.get("timestamp"),
                "cycle": snapshot.get("cycle_number"),
                "value": snapshot.get("portfolio_value"),
                "model": snapshot.get("model_name"),
            })

        return {
            "success": True,
            "equity_curve": equity_curve,
            "count": len(equity_curve),
        }
    except Exception as e:
        logger.exception(f"Error getting equity curve: {e}")
        return {"success": False, "error": str(e)}


# =============================================================================
# Grid Strategy Handlers
# =============================================================================

async def create_grid_agent_handler(params: Dict) -> Dict[str, Any]:
    """Create a grid trading agent."""
    try:
        from alpha_arena.core.grid_agent import GridStrategyAgent, GridConfig

        name = params.get("name", "GridAgent")
        symbol = params.get("symbol", "BTC/USD")

        # Grid configuration - support both flat params (from Rust) and nested grid_config
        grid_params = params.get("grid_config", {})
        config = GridConfig(
            symbol=symbol,
            lower_price=params.get("lower_price") or grid_params.get("lower_price", 90000),
            upper_price=params.get("upper_price") or grid_params.get("upper_price", 110000),
            num_grids=params.get("grid_levels") or grid_params.get("num_grids", 10),
            quantity_per_grid=grid_params.get("quantity_per_grid", 0.001),
            take_profit_pct=grid_params.get("take_profit_pct"),
            stop_loss_pct=grid_params.get("stop_loss_pct"),
        )

        # Create agent
        agent = GridStrategyAgent(name=name, grid_config=config)
        await agent.initialize()

        return {
            "success": True,
            "agent_name": name,
            "grid_config": {
                "symbol": config.symbol,
                "lower_price": config.lower_price,
                "upper_price": config.upper_price,
                "num_grids": config.num_grids,
                "quantity_per_grid": config.quantity_per_grid,
                "grid_spacing": config.grid_spacing,
            },
            "grid_levels": agent.state.grid_levels,
        }
    except Exception as e:
        logger.exception(f"Error creating grid agent: {e}")
        return {"success": False, "error": str(e)}


async def get_grid_status_handler(params: Dict) -> Dict[str, Any]:
    """Get grid agent status."""
    try:
        competition_id = params.get("competition_id")
        agent_name = params.get("agent_name")

        if not competition_id:
            return {"success": False, "error": "Missing competition_id"}

        competition = get_competition(competition_id)
        if not competition:
            return {"success": False, "error": f"Competition not found: {competition_id}"}

        # Find grid agent
        for agent in competition._agents:
            if hasattr(agent, 'state') and hasattr(agent.state, 'grid_levels'):
                if agent_name and agent.name != agent_name:
                    continue

                return {
                    "success": True,
                    "agent_name": agent.name,
                    "grid_status": {
                        "grid_levels": agent.state.grid_levels,
                        "triggered_levels": list(agent.state.triggered_levels),
                        "active_orders": agent.state.active_orders,
                        "total_profit": agent.state.total_profit,
                        "trades_executed": agent.state.trades_executed,
                    },
                }

        return {"success": False, "error": "No grid agent found"}
    except Exception as e:
        logger.exception(f"Error getting grid status: {e}")
        return {"success": False, "error": str(e)}


# =============================================================================
# Research Handlers
# =============================================================================

async def get_research_handler(params: Dict) -> Dict[str, Any]:
    """Get research report for a ticker."""
    try:
        from alpha_arena.core.research_agent import get_research_agent

        ticker = params.get("ticker")
        use_cache = params.get("use_cache", True)

        if not ticker:
            return {"success": False, "error": "Missing ticker"}

        agent = await get_research_agent()
        report = await agent.generate_report(ticker, use_cache=use_cache)

        return {
            "success": True,
            "report": report.to_dict(),
            "prompt_context": report.to_prompt_context(),
        }
    except Exception as e:
        logger.exception(f"Error getting research: {e}")
        return {"success": False, "error": str(e)}


async def get_sec_filings_handler(params: Dict) -> Dict[str, Any]:
    """Get SEC filings for a ticker."""
    try:
        from alpha_arena.core.research_agent import get_research_agent

        ticker = params.get("ticker")
        form_types = params.get("form_types")  # e.g., ["10-K", "10-Q"]
        limit = params.get("limit", 10)

        if not ticker:
            return {"success": False, "error": "Missing ticker"}

        agent = await get_research_agent()
        filings = await agent.get_recent_filings(ticker, form_types=form_types, limit=limit)

        return {
            "success": True,
            "ticker": ticker,
            "filings": [f.to_dict() for f in filings],
            "count": len(filings),
        }
    except Exception as e:
        logger.exception(f"Error getting SEC filings: {e}")
        return {"success": False, "error": str(e)}


# =============================================================================
# Features Pipeline Handlers
# =============================================================================

async def get_features_handler(params: Dict) -> Dict[str, Any]:
    """Get technical features for market analysis."""
    try:
        from alpha_arena.core.features_pipeline import get_features_pipeline
        from alpha_arena.types.models import MarketData
        from datetime import datetime

        symbol = params.get("symbol", "BTC/USD")
        price_history = params.get("price_history", [])
        current_price = params.get("current_price", 100000)

        # Build market data with required fields
        market_data = MarketData(
            symbol=symbol,
            price=current_price,
            bid=params.get("bid", current_price * 0.999),
            ask=params.get("ask", current_price * 1.001),
            high_24h=params.get("high_24h", current_price * 1.02),
            low_24h=params.get("low_24h", current_price * 0.98),
            volume_24h=params.get("volume_24h", 1000000),
            timestamp=datetime.fromisoformat(params["timestamp"]) if params.get("timestamp") else datetime.now(),
        )

        pipeline = get_features_pipeline()
        features = await pipeline.compute(market_data, price_history)

        return {
            "success": True,
            "features": {
                "technical": features.technical.to_dict() if features.technical else None,
                "sentiment": features.sentiment.to_dict() if features.sentiment else None,
            },
            "prompt_context": features.to_prompt_context(),
        }
    except Exception as e:
        logger.exception(f"Error computing features: {e}")
        return {"success": False, "error": str(e)}


# =============================================================================
# Broker Handlers
# =============================================================================

async def list_brokers_handler(params: Dict) -> Dict[str, Any]:
    """List all supported brokers."""
    try:
        from alpha_arena.core.broker_adapter import (
            get_supported_brokers,
            get_brokers_by_type,
            get_brokers_by_region,
            BrokerType,
            BrokerRegion,
        )

        broker_type = params.get("type")
        region = params.get("region")

        if broker_type:
            try:
                bt = BrokerType(broker_type)
                brokers = get_brokers_by_type(bt)
            except ValueError:
                return {"success": False, "error": f"Invalid broker type: {broker_type}"}
        elif region:
            try:
                br = BrokerRegion(region)
                brokers = get_brokers_by_region(br)
            except ValueError:
                return {"success": False, "error": f"Invalid region: {region}"}
        else:
            brokers = get_supported_brokers()

        return {
            "success": True,
            "brokers": [b.to_dict() for b in brokers],
            "count": len(brokers),
        }
    except Exception as e:
        logger.exception(f"Error listing brokers: {e}")
        return {"success": False, "error": str(e)}


async def get_broker_handler(params: Dict) -> Dict[str, Any]:
    """Get broker details by ID."""
    try:
        from alpha_arena.core.broker_adapter import get_broker_info

        broker_id = params.get("broker_id")
        if not broker_id:
            return {"success": False, "error": "Missing broker_id"}

        broker = get_broker_info(broker_id)
        if broker:
            return {
                "success": True,
                "broker": broker.to_dict(),
            }

        return {"success": False, "error": f"Broker not found: {broker_id}"}
    except Exception as e:
        logger.exception(f"Error getting broker: {e}")
        return {"success": False, "error": str(e)}


async def get_broker_ticker_handler(params: Dict) -> Dict[str, Any]:
    """Get ticker data from specific broker."""
    try:
        from alpha_arena.core.broker_adapter import get_multi_exchange_provider

        symbol = params.get("symbol")
        broker_id = params.get("broker_id", "binance")

        if not symbol:
            return {"success": False, "error": "Missing symbol"}

        provider = await get_multi_exchange_provider(broker_id)
        market_data = await provider.get_ticker(symbol, broker_id)

        return {
            "success": True,
            "ticker": {
                "symbol": market_data.symbol,
                "price": market_data.price,
                "bid": market_data.bid,
                "ask": market_data.ask,
                "high_24h": market_data.high_24h,
                "low_24h": market_data.low_24h,
                "volume_24h": market_data.volume_24h,
                "timestamp": market_data.timestamp.isoformat(),
            },
            "broker_id": broker_id,
        }
    except Exception as e:
        logger.exception(f"Error getting broker ticker: {e}")
        return {"success": False, "error": str(e)}


# =============================================================================
# Sentiment Handlers
# =============================================================================

async def get_sentiment_handler(params: Dict) -> Dict[str, Any]:
    """Get sentiment analysis for a symbol."""
    try:
        from alpha_arena.core.sentiment_agent import get_sentiment_agent

        symbol = params.get("symbol")
        use_cache = params.get("use_cache", True)
        max_articles = params.get("max_articles", 15)

        if not symbol:
            return {"success": False, "error": "Missing symbol"}

        agent = await get_sentiment_agent()
        result = await agent.analyze_sentiment(symbol, use_cache, max_articles)

        return {
            "success": True,
            "sentiment": result.to_dict(),
            "prompt_context": result.to_prompt_context(),
        }
    except Exception as e:
        logger.exception(f"Error getting sentiment: {e}")
        return {"success": False, "error": str(e)}


async def get_market_mood_handler(params: Dict) -> Dict[str, Any]:
    """Get overall market mood from multiple symbols."""
    try:
        from alpha_arena.core.sentiment_agent import get_sentiment_agent

        symbols = params.get("symbols", [])

        if not symbols:
            # Default to major market symbols
            symbols = ["SPY", "QQQ", "BTC", "ETH"]

        agent = await get_sentiment_agent()
        result = await agent.get_market_mood(symbols)

        return {
            "success": True,
            "market_mood": result,
        }
    except Exception as e:
        logger.exception(f"Error getting market mood: {e}")
        return {"success": False, "error": str(e)}


# =============================================================================
# Trading Styles Handlers
# =============================================================================

async def list_trading_styles_handler(params: Dict) -> Dict[str, Any]:
    """List all available trading styles."""
    try:
        from alpha_arena.config.trading_styles import list_trading_styles

        styles = list_trading_styles()

        return {
            "success": True,
            "styles": [style.to_dict() for style in styles],
            "count": len(styles),
        }
    except Exception as e:
        logger.exception(f"Error listing trading styles: {e}")
        return {"success": False, "error": str(e)}


async def create_styled_agents_handler(params: Dict) -> Dict[str, Any]:
    """
    Create multiple agents with different trading styles using the same provider.

    This allows running a competition with a single API key but different agent behaviors.

    Params:
        provider: LLM provider (openai, anthropic, etc.)
        model_id: Model identifier (gpt-4o-mini, claude-3-5-sonnet, etc.)
        api_key: API key for the provider (optional, can use env)
        styles: List of style IDs to use (optional, defaults to all styles)
        initial_capital: Starting capital per agent (default: 10000)
    """
    try:
        from alpha_arena.config.trading_styles import (
            list_trading_styles,
            get_trading_style,
            create_styled_agent_name,
        )

        provider = params.get("provider")
        model_id = params.get("model_id")
        api_key = params.get("api_key")
        style_ids = params.get("styles")
        initial_capital = params.get("initial_capital", 10000)

        if not provider:
            return {"success": False, "error": "Missing provider"}

        if not model_id:
            return {"success": False, "error": "Missing model_id"}

        # Get styles to use
        if style_ids:
            styles = []
            for style_id in style_ids:
                style = get_trading_style(style_id)
                if style:
                    styles.append(style)
                else:
                    logger.warning(f"Unknown trading style: {style_id}, skipping")
        else:
            # Default to a diverse set of contrasting styles
            default_style_ids = ["aggressive", "conservative", "momentum", "contrarian", "neutral"]
            styles = [get_trading_style(sid) for sid in default_style_ids if get_trading_style(sid)]

        if not styles:
            return {"success": False, "error": "No valid styles specified"}

        # Create agent configurations
        agents = []
        for style in styles:
            agent_name = create_styled_agent_name(provider, model_id, style)

            agents.append({
                "name": agent_name,
                "provider": provider,
                "model_id": model_id,
                "api_key": api_key,
                "initial_capital": initial_capital,
                "trading_style": style.id,
                "metadata": {
                    "style_config": style.to_dict(),
                },
            })

        return {
            "success": True,
            "agents": agents,
            "count": len(agents),
            "provider": provider,
            "model_id": model_id,
            "styles_used": [s.id for s in styles],
        }
    except Exception as e:
        logger.exception(f"Error creating styled agents: {e}")
        return {"success": False, "error": str(e)}


# =============================================================================
# Memory Handlers
# =============================================================================

async def get_agent_context_handler(params: Dict) -> Dict[str, Any]:
    """Get agent memory context for a symbol."""
    try:
        from alpha_arena.core.memory_adapter import get_agent_memory

        agent_name = params.get("agent_name")
        competition_id = params.get("competition_id")
        symbol = params.get("symbol")

        if not agent_name or not competition_id:
            return {"success": False, "error": "Missing agent_name or competition_id"}

        if not symbol:
            return {"success": False, "error": "Missing symbol"}

        memory = get_agent_memory(agent_name, competition_id)
        context = memory.get_relevant_context(symbol)

        return {
            "success": True,
            "context": context,
            "stats": memory.get_performance_stats(),
        }
    except Exception as e:
        logger.exception(f"Error getting agent context: {e}")
        return {"success": False, "error": str(e)}


async def get_agent_memory_summary_handler(params: Dict) -> Dict[str, Any]:
    """Get agent memory summary."""
    try:
        from alpha_arena.core.memory_adapter import get_agent_memory

        agent_name = params.get("agent_name")
        competition_id = params.get("competition_id")

        if not agent_name or not competition_id:
            return {"success": False, "error": "Missing agent_name or competition_id"}

        memory = get_agent_memory(agent_name, competition_id)

        return {
            "success": True,
            "summary": memory.get_summary(),
            "data": memory.to_dict(),
        }
    except Exception as e:
        logger.exception(f"Error getting agent memory summary: {e}")
        return {"success": False, "error": str(e)}


async def search_agent_memory_handler(params: Dict) -> Dict[str, Any]:
    """Search agent memory."""
    try:
        from alpha_arena.core.memory_adapter import get_agent_memory

        agent_name = params.get("agent_name")
        competition_id = params.get("competition_id")
        query = params.get("query")
        limit = params.get("limit", 5)

        if not agent_name or not competition_id:
            return {"success": False, "error": "Missing agent_name or competition_id"}

        if not query:
            return {"success": False, "error": "Missing search query"}

        memory = get_agent_memory(agent_name, competition_id)
        results = memory.search_memory(query, limit=limit)

        return {
            "success": True,
            "results": results,
            "count": len(results),
        }
    except Exception as e:
        logger.exception(f"Error searching agent memory: {e}")
        return {"success": False, "error": str(e)}


def main(args=None):
    """
    Main entry point - accepts single JSON payload from Rust/Tauri.

    Payload format:
    {
        "action": "create_competition|run_cycle|...",
        "api_keys": {"openai": "...", "anthropic": "..."},
        "params": {...}
    }
    """
    if args is None:
        args = sys.argv[1:]

    try:
        if len(args) == 0:
            return json.dumps({"success": False, "error": "No payload provided"})

        # Parse single JSON payload
        payload = json.loads(args[0])
        action = payload.get("action")
        api_keys = payload.get("api_keys", {})
        params = payload.get("params", {})

        if not action:
            return json.dumps({"success": False, "error": "Missing 'action' in payload"})

        # Run async handler
        result = asyncio.run(handle_action(action, params, api_keys))
        return json.dumps(result, default=str)

    except json.JSONDecodeError as e:
        return json.dumps({"success": False, "error": f"Invalid JSON: {str(e)}"})
    except Exception as e:
        logger.exception(f"Main error: {e}")
        return json.dumps({"success": False, "error": str(e)})


if __name__ == "__main__":
    result = main()
    print(result)
