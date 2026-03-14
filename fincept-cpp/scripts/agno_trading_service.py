#!/usr/bin/env python
"""
Alpha Arena Trading Service - New Framework
===========================================

Streamlined service using the modular trading framework.
Clean separation of concerns, extensible architecture.

Commands:
    - create_competition: Create a new multi-LLM competition
    - run_cycle: Execute a single trading cycle
    - start_competition: Start continuous trading
    - stop_competition: Stop the competition
    - get_leaderboard_alpha: Get current rankings
    - get_model_decisions_alpha: Get recent decisions
"""

import sys
import json
import os
import asyncio
from typing import Dict, Any, Optional

# Configure logging - stderr only
import logging
logging.basicConfig(
    level=logging.ERROR,
    format='%(levelname)s: %(message)s',
    stream=sys.stderr,
    force=True
)

# Redirect stdout to stderr (preserve stdout for JSON)
class StdoutToStderr:
    def write(self, text):
        sys.stderr.write(text)
    def flush(self):
        sys.stderr.flush()

_original_stdout = sys.stdout

def _suppress_stdout():
    sys.stdout = StdoutToStderr()

def _restore_stdout():
    sys.stdout = _original_stdout

_suppress_stdout()

# Disable loguru
os.environ['LOGURU_LEVEL'] = 'CRITICAL'
os.environ['LOGURU_AUTOINIT'] = 'False'

# Add to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), 'agno_trading'))

try:
    from loguru import logger
    logger.remove()
    logger.add(sys.stderr, level="CRITICAL")
except ImportError:
    pass

# Import framework
from framework import CompetitionRuntime, ModelConfig
from db.database_manager import DatabaseManager

# Global competition instance
_active_competition: Optional[CompetitionRuntime] = None
_db_manager: Optional[DatabaseManager] = None


def get_db_manager() -> DatabaseManager:
    """Get or create database manager."""
    global _db_manager
    if _db_manager is None:
        _db_manager = DatabaseManager()
    return _db_manager


def create_competition_command(args: list) -> Dict[str, Any]:
    """
    Create a new Alpha Arena competition.

    Args:
        args[0]: JSON string with competition config
            {
                "competition_id": "comp_123",
                "competition_name": "My Competition",
                "models": [
                    {
                        "name": "GPT-4",
                        "provider": "openai",
                        "model_id": "gpt-4",
                        "api_key": "sk-..."
                    },
                    ...
                ],
                "symbols": ["BTC/USD"],
                "initial_capital": 10000.0,
                "exchange_id": "kraken",
                "custom_prompt": "Optional strategy instructions"
            }
    """
    global _active_competition

    try:
        config = json.loads(args[0])

        competition_id = config["competition_id"]
        competition_name = config.get("competition_name", "Untitled")
        symbols = config.get("symbols", ["BTC/USD"])
        initial_capital = float(config.get("initial_capital", 10000.0))
        exchange_id = config.get("exchange_id", "kraken")
        custom_prompt = config.get("custom_prompt")

        # Parse model configs
        models = []
        for model_data in config["models"]:
            models.append(ModelConfig(
                name=model_data["name"],
                provider=model_data["provider"],
                model_id=model_data["model_id"],
                api_key=model_data["api_key"],
                temperature=float(model_data.get("temperature", 0.7)),
                max_tokens=int(model_data.get("max_tokens", 1000)),
                timeout_seconds=int(model_data.get("timeout_seconds", 30))
            ))

        # Create competition runtime
        _active_competition = CompetitionRuntime(
            competition_id=competition_id,
            models=models,
            symbols=symbols,
            initial_capital=initial_capital,
            exchange_id=exchange_id,
            custom_prompt=custom_prompt
        )

        # Save to database
        db = get_db_manager()
        models_json = json.dumps([{
            "name": m.name,
            "provider": m.provider,
            "model_id": m.model_id
        } for m in models])

        db.save_competition_config(
            competition_id=competition_id,
            name=competition_name,
            models_json=models_json,
            symbol=symbols[0],  # Primary symbol
            mode="baseline",  # Default mode
            api_keys_json=json.dumps({m.name: m.api_key for m in models})
        )

        # Save model states
        for model in models:
            db.save_model_state(
                competition_id=competition_id,
                model_name=model.name,
                capital=initial_capital,
                positions_json="{}",
                trades_count=0,
                total_pnl=0.0,
                portfolio_value=initial_capital
            )

        return {
            "success": True,
            "competition_id": competition_id,
            "models": [m.name for m in models],
            "symbols": symbols
        }

    except Exception as e:
        print(f"[ERROR] create_competition failed: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc(file=sys.stderr)
        return {"success": False, "error": str(e)}


def load_competition_from_db(competition_id: str) -> Optional[CompetitionRuntime]:
    """Load competition from database."""
    try:
        db = get_db_manager()

        # Get competition config
        conn = db.get_connection()
        cursor = conn.cursor()
        cursor.execute("""
            SELECT name, models_json, symbol, api_keys_json
            FROM competition_configs
            WHERE competition_id = ?
        """, (competition_id,))

        row = cursor.fetchone()
        if not row:
            conn.close()
            print(f"[ERROR] Competition {competition_id} not found in database", file=sys.stderr)
            return None

        name, models_json, symbol, api_keys_json = row

        # Parse models
        models_data = json.loads(models_json)
        api_keys = json.loads(api_keys_json) if api_keys_json else {}

        models = []
        for model_data in models_data:
            model_name = model_data["name"]
            api_key = api_keys.get(model_name, "")

            models.append(ModelConfig(
                name=model_name,
                provider=model_data["provider"],
                model_id=model_data["model_id"],
                api_key=api_key,
                temperature=0.7,
                max_tokens=1000,
                timeout_seconds=30
            ))

        # Get model states to restore portfolios
        cursor.execute("""
            SELECT model_name, capital, positions_json, trades_count, total_pnl, portfolio_value
            FROM alpha_model_states
            WHERE competition_id = ?
        """, (competition_id,))

        model_states = {}
        for row in cursor.fetchall():
            model_name, capital, positions_json, trades_count, total_pnl, portfolio_value = row
            model_states[model_name] = {
                "capital": capital,
                "positions": json.loads(positions_json) if positions_json else {},
                "trades_count": trades_count,
                "total_pnl": total_pnl,
                "portfolio_value": portfolio_value
            }

        conn.close()

        # Create competition
        competition = CompetitionRuntime(
            competition_id=competition_id,
            models=models,
            symbols=[symbol],
            initial_capital=10000.0,  # Will restore from states
            exchange_id="kraken",
            custom_prompt=None
        )

        # Restore portfolio states
        for model_name, state in model_states.items():
            if model_name in competition.portfolios:
                portfolio = competition.portfolios[model_name]
                portfolio.cash = state["capital"]
                portfolio.trades_count = state["trades_count"]
                portfolio.total_realized_pnl = state["total_pnl"]
                # TODO: Restore positions from state["positions"]

        print(f"[INFO] Loaded competition {competition_id} from database", file=sys.stderr)
        return competition

    except Exception as e:
        print(f"[ERROR] Failed to load competition: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc(file=sys.stderr)
        return None


async def run_cycle_async(args: list) -> Dict[str, Any]:
    """Run a single trading cycle (async)."""
    global _active_competition

    try:
        # Get competition ID from args
        competition_id = args[0] if args else None

        # If no active competition, try to load from database
        if _active_competition is None:
            if not competition_id:
                return {"success": False, "error": "No competition ID provided"}

            print(f"[INFO] Loading competition {competition_id} from database", file=sys.stderr)
            _active_competition = load_competition_from_db(competition_id)

            if _active_competition is None:
                return {"success": False, "error": "Failed to load competition from database"}

        # Verify competition ID matches if provided
        if competition_id and _active_competition.competition_id != competition_id:
            print(f"[INFO] Switching to competition {competition_id}", file=sys.stderr)
            _active_competition = load_competition_from_db(competition_id)

            if _active_competition is None:
                return {"success": False, "error": "Failed to load competition from database"}

        # Run cycle
        result = await _active_competition.run_cycle()

        # Save to database
        db = get_db_manager()
        competition_id = _active_competition.competition_id

        # Save ALL decisions (including HOLD/WAIT)
        for compose_result in result.compose_results:
            # Get portfolio value before
            portfolio_before = result.portfolio_snapshots.get(compose_result.model_name)
            portfolio_value_before = portfolio_before.total_value if portfolio_before else 0.0

            # Get the decision type (BUY/SELL/HOLD/WAIT)
            decision_type = compose_result.decision_type or 'WAIT'

            if compose_result.instructions:
                # BUY or SELL decision with trade execution
                for instruction in compose_result.instructions:
                    db.save_decision_log(
                        competition_id=competition_id,
                        model_name=instruction.model_name or compose_result.model_name,
                        cycle_number=result.cycle_number,
                        action=instruction.side.value,
                        symbol=instruction.symbol,
                        quantity=instruction.quantity,
                        confidence=instruction.confidence or 0.5,
                        reasoning=instruction.reasoning or "",
                        trade_executed=True,
                        price_at_decision=0.0,  # Will be updated with tx result
                        portfolio_value_before=portfolio_value_before,
                        portfolio_value_after=0.0  # Will be updated below
                    )
            else:
                # HOLD or WAIT decision (no trade execution)
                # Get symbol from features or use competition default
                symbol = _active_competition.symbols[0] if _active_competition.symbols else 'BTC/USD'

                db.save_decision_log(
                    competition_id=competition_id,
                    model_name=compose_result.model_name,
                    cycle_number=result.cycle_number,
                    action=decision_type,  # HOLD or WAIT
                    symbol=symbol,
                    quantity=0.0,
                    confidence=0.5,
                    reasoning=compose_result.rationale or "",
                    trade_executed=False,
                    price_at_decision=0.0,
                    portfolio_value_before=portfolio_value_before,
                    portfolio_value_after=portfolio_value_before  # No change
                )

        # Save portfolio snapshots
        for model_name, portfolio_view in result.portfolio_snapshots.items():
            db.save_performance_snapshot(
                competition_id=competition_id,
                cycle_number=result.cycle_number,
                model_name=model_name,
                portfolio_value=portfolio_view.total_value,
                cash=portfolio_view.cash,
                pnl=portfolio_view.total_realized_pnl + portfolio_view.total_unrealized_pnl,
                return_pct=((portfolio_view.total_value - _active_competition.initial_capital) / _active_competition.initial_capital) * 100.0,
                positions_count=len(portfolio_view.positions),
                trades_count=portfolio_view.trades_count
            )

            # Update model state
            db.save_model_state(
                competition_id=competition_id,
                model_name=model_name,
                capital=portfolio_view.cash,
                positions_json=json.dumps(portfolio_view.to_dict()["positions"]),
                trades_count=portfolio_view.trades_count,
                total_pnl=portfolio_view.total_realized_pnl + portfolio_view.total_unrealized_pnl,
                portfolio_value=portfolio_view.total_value
            )

        return {
            "success": True,
            "cycle_number": result.cycle_number,
            "leaderboard": result.leaderboard,
            "decisions_count": len(result.tx_results)
        }

    except Exception as e:
        print(f"[ERROR] run_cycle failed: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc(file=sys.stderr)
        return {"success": False, "error": str(e)}


def run_cycle_command(args: list) -> Dict[str, Any]:
    """Run a single trading cycle (sync wrapper)."""
    return asyncio.run(run_cycle_async(args))


def start_competition_command(args: list) -> Dict[str, Any]:
    """Start continuous trading (runs one cycle then returns)."""
    return run_cycle_command(args)


def stop_competition_command(args: list) -> Dict[str, Any]:
    """Stop the competition."""
    global _active_competition

    try:
        if _active_competition:
            _active_competition.stop()
            asyncio.run(_active_competition.close())
            _active_competition = None

        return {"success": True}

    except Exception as e:
        print(f"[ERROR] stop_competition failed: {e}", file=sys.stderr)
        return {"success": False, "error": str(e)}


async def get_leaderboard_async(args: list) -> Dict[str, Any]:
    """Get current leaderboard (async)."""
    global _active_competition

    try:
        # Get competition ID from args
        competition_id = args[0] if args else None

        # If no active competition, try to load from database
        if _active_competition is None:
            if not competition_id:
                return {"success": False, "error": "No competition ID provided"}

            print(f"[INFO] Loading competition {competition_id} for leaderboard", file=sys.stderr)
            _active_competition = load_competition_from_db(competition_id)

            if _active_competition is None:
                return {"success": False, "error": "Failed to load competition from database"}

        # Verify competition ID matches if provided - but DON'T reload if match
        if competition_id and _active_competition.competition_id != competition_id:
            print(f"[INFO] Switching to competition {competition_id}", file=sys.stderr)
            _active_competition = load_competition_from_db(competition_id)

            if _active_competition is None:
                return {"success": False, "error": "Failed to load competition from database"}
        elif competition_id and _active_competition.competition_id == competition_id:
            # Competition matches - sync portfolio states from DB to ensure consistency
            db = get_db_manager()
            conn = db.get_connection()
            cursor = conn.cursor()
            cursor.execute("""
                SELECT model_name, capital, positions_json, trades_count, total_pnl, portfolio_value
                FROM alpha_model_states
                WHERE competition_id = ?
            """, (competition_id,))

            for row in cursor.fetchall():
                model_name, capital, positions_json, trades_count, total_pnl, portfolio_value = row
                if model_name in _active_competition.portfolios:
                    portfolio = _active_competition.portfolios[model_name]
                    portfolio.cash = capital
                    portfolio.trades_count = trades_count
                    portfolio.total_realized_pnl = total_pnl

            conn.close()

        leaderboard = await _active_competition.get_leaderboard()

        return {
            "success": True,
            "leaderboard": leaderboard
        }

    except Exception as e:
        print(f"[ERROR] get_leaderboard failed: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc(file=sys.stderr)
        return {"success": False, "error": str(e)}


def get_leaderboard_command(args: list) -> Dict[str, Any]:
    """Get current leaderboard (sync wrapper)."""
    return asyncio.run(get_leaderboard_async(args))


def get_model_decisions_command(args: list) -> Dict[str, Any]:
    """Get recent model decisions from database."""
    try:
        competition_id = args[0] if args else None
        if not competition_id:
            return {"success": False, "error": "competition_id required"}

        db = get_db_manager()
        decisions = db.get_decision_logs(competition_id, limit=50)

        return {
            "success": True,
            "decisions": decisions
        }

    except Exception as e:
        print(f"[ERROR] get_model_decisions failed: {e}", file=sys.stderr)
        return {"success": False, "error": str(e)}


# Command registry
COMMANDS = {
    "create_competition": create_competition_command,
    "run_cycle": run_cycle_command,
    "start_competition": start_competition_command,
    "stop_competition": stop_competition_command,
    "get_leaderboard_alpha": get_leaderboard_command,
    "get_model_decisions_alpha": get_model_decisions_command,
}


def main():
    """Main entry point."""
    if len(sys.argv) < 2:
        _restore_stdout()
        print(json.dumps({
            "success": False,
            "error": "No command specified"
        }))
        return

    command = sys.argv[1]
    args = sys.argv[2:]

    if command not in COMMANDS:
        _restore_stdout()
        print(json.dumps({
            "success": False,
            "error": f"Unknown command: {command}"
        }))
        return

    # Execute command
    result = COMMANDS[command](args)

    # Output JSON result
    _restore_stdout()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
