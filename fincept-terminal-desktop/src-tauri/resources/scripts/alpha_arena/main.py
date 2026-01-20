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
            )
            models.append(model)
            logger.info(f"Configured model: {model.name} ({model.provider}/{model.model_id})")

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

            return {
                "success": True,
                "competition_id": competition_id,
                "config": config.model_dump(),
                "status": "created",
                "models_count": len(models),
            }
        else:
            return {
                "success": False,
                "error": "Failed to initialize competition. Check that LLM providers are configured correctly and API keys are valid.",
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


def get_system_info() -> Dict[str, Any]:
    """Get system information."""
    return {
        "success": True,
        "version": "2.0.0",
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
        "default_exchange": "kraken",
    }


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
