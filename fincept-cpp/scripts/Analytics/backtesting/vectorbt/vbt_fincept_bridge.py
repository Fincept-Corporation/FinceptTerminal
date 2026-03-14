#!/usr/bin/env python3
"""
VectorBT <-> Fincept Strategy Bridge
Executes Fincept Engine strategies within VectorBT provider
"""

import sys
import json
from pathlib import Path

# Add base directory to path
BASE_DIR = Path(__file__).resolve().parent.parent / "base"
sys.path.insert(0, str(BASE_DIR))

from fincept_strategy_runner import FinceptStrategyRunner


def run_fincept_strategy(strategy_id: str, params_dict: dict) -> dict:
    """
    Execute Fincept strategy and return VectorBT-compatible results.

    Args:
        strategy_id: Fincept strategy ID (FCT-XXXXXXXX)
        params_dict: Backtest parameters

    Returns:
        dict: VectorBT-format results
    """
    runner = FinceptStrategyRunner()
    result = runner.execute_strategy(strategy_id, params_dict)

    if not result.get('success'):
        return result

    # Convert to VectorBT format (VectorBT expects similar structure)
    return result


if __name__ == '__main__':
    if len(sys.argv) < 3:
        print(json.dumps({
            'success': False,
            'error': 'Usage: vbt_fincept_bridge.py <strategy_id> <params_json>'
        }))
        sys.exit(1)

    strategy_id = sys.argv[1]
    params = json.loads(sys.argv[2])

    result = run_fincept_strategy(strategy_id, params)
    print(json.dumps(result, indent=2))
