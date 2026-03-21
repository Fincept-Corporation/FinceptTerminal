#!/usr/bin/env python3
"""
Fincept Engine Backtesting Provider
Executes strategies from the 418+ Fincept Terminal strategy registry
"""

import sys
import json
from pathlib import Path
from datetime import datetime, timedelta
from typing import Dict, Any, List

# Add paths
BASE_DIR = Path(__file__).resolve().parent.parent
STRATEGIES_DIR = BASE_DIR.parent.parent / "strategies"
sys.path.insert(0, str(STRATEGIES_DIR))
sys.path.insert(0, str(BASE_DIR / "base"))

from fincept_strategy_runner import FinceptStrategyRunner


class FinceptProvider:
    """
    Fincept Engine backtesting provider.
    Bridges Fincept Terminal frontend to 418+ QCAlgorithm strategies.
    """

    def __init__(self):
        self.runner = FinceptStrategyRunner()

    def run_backtest(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """
        Execute a backtest for a Fincept Engine strategy.

        Request format:
        {
            "strategy": {
                "type": "FCT-XXXXX",  # Strategy ID from registry
                "name": "Strategy Name",
                "parameters": {...}    # Strategy-specific params
            },
            "symbols": ["BTC-USD", "ETH-USD"],
            "startDate": "2025-01-01",
            "endDate": "2026-01-01",
            "initialCapital": 100000,
            "assets": [{"symbol": "BTC-USD", ...}]
        }
        """
        try:
            strategy_id = request.get("strategy", {}).get("type")
            if not strategy_id:
                return {
                    "success": False,
                    "error": "strategy.type (strategy ID) is required"
                }

            # Validate strategy exists
            strategy_info = self.runner.get_strategy_info(strategy_id)
            if not strategy_info:
                return {
                    "success": False,
                    "error": f"Strategy {strategy_id} not found in registry (418 strategies available)"
                }

            # Extract parameters
            symbols = request.get("symbols", [])
            if not symbols and "assets" in request:
                symbols = [asset["symbol"] for asset in request["assets"]]

            start_date = request.get("startDate", "2025-01-01")
            end_date = request.get("endDate", (datetime.now() - timedelta(days=1)).strftime("%Y-%m-%d"))
            initial_capital = request.get("initialCapital", 100000)
            strategy_params = request.get("strategy", {}).get("parameters", {})

            print(f"[Fincept Engine] Executing strategy: {strategy_info['name']}")
            print(f"[Fincept Engine] ID: {strategy_id}")
            print(f"[Fincept Engine] Category: {strategy_info['category']}")
            print(f"[Fincept Engine] Symbols: {symbols}")
            print(f"[Fincept Engine] Period: {start_date} to {end_date}")

            # Execute strategy
            result = self.runner.execute_strategy(
                strategy_id=strategy_id,
                params={
                    "symbols": symbols,
                    "start_date": start_date,
                    "end_date": end_date,
                    "initial_cash": initial_capital,
                    "resolution": "daily",
                    "strategy_params": strategy_params
                }
            )

            # Format response
            if result.get("success"):
                print(f"[Fincept Engine] Backtest completed successfully")
                return {
                    "success": True,
                    "message": f"Backtest completed for {strategy_info['name']}",
                    "data": result
                }
            else:
                return {
                    "success": False,
                    "error": result.get("error", "Unknown error"),
                    "details": result
                }

        except Exception as e:
            import traceback
            error_details = traceback.format_exc()
            print(f"[Fincept Engine] Error: {e}", file=sys.stderr)
            print(error_details, file=sys.stderr)
            return {
                "success": False,
                "error": str(e),
                "traceback": error_details
            }


def main():
    """CLI entry point"""
    if len(sys.argv) < 3:
        print(json.dumps({
            "success": False,
            "error": "Usage: fincept_provider.py <command> <args_json>"
        }))
        sys.exit(1)

    command = sys.argv[1]
    args_json = sys.argv[2]

    try:
        args = json.loads(args_json)
    except json.JSONDecodeError as e:
        print(json.dumps({
            "success": False,
            "error": f"Invalid JSON: {e}"
        }))
        sys.exit(1)

    provider = FinceptProvider()

    # Route command
    if command == "run_backtest" or command == "execute_fincept_strategy":
        result = provider.run_backtest(args)
    else:
        result = {
            "success": False,
            "error": f"Unknown command: {command}. Supported: run_backtest"
        }

    print(json.dumps(result))


if __name__ == "__main__":
    main()
