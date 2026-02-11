#!/usr/bin/env python3
"""
py_vollib Worker Handler
Provides a main() function interface for worker pool execution.
Covers all 3 models: Black, Black-Scholes, Black-Scholes-Merton.
Each model supports: price, greeks, implied_volatility.
"""

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from black import calculate_black_price, calculate_black_greeks, calculate_black_iv
from black_scholes import calculate_bs_price, calculate_bs_greeks, calculate_bs_iv
from black_scholes_merton import calculate_bsm_price, calculate_bsm_greeks, calculate_bsm_iv


def main(args):
    """
    Main entry point for worker pool.
    Args: [operation, json_data]
    Returns: JSON string
    """
    try:
        if len(args) < 2:
            return json.dumps({"error": "Expected: [operation, json_data]"})

        operation = args[0]
        data = json.loads(args[1])

        dispatch = {
            # Black model (futures/forwards)
            "black_price": _black_price,
            "black_greeks": _black_greeks,
            "black_iv": _black_iv,
            # Black-Scholes model (equity, no dividends)
            "bs_price": _bs_price,
            "bs_greeks": _bs_greeks,
            "bs_iv": _bs_iv,
            # Black-Scholes-Merton model (equity with dividends)
            "bsm_price": _bsm_price,
            "bsm_greeks": _bsm_greeks,
            "bsm_iv": _bsm_iv,
        }

        handler = dispatch.get(operation)
        if handler is None:
            return json.dumps({"error": f"Unknown operation: {operation}"})

        return handler(data)

    except Exception as e:
        import traceback
        return json.dumps({
            "error": f"py_vollib error: {str(e)}",
            "traceback": traceback.format_exc()
        })


# ---------------------------------------------------------------------------
# Black Model (for futures/forwards pricing)
# ---------------------------------------------------------------------------

def _black_price(data):
    result = calculate_black_price(
        S=float(data["S"]),
        K=float(data["K"]),
        t=float(data["t"]),
        r=float(data["r"]),
        sigma=float(data["sigma"]),
        flag=data["flag"],
    )
    return json.dumps(result)


def _black_greeks(data):
    result = calculate_black_greeks(
        S=float(data["S"]),
        K=float(data["K"]),
        t=float(data["t"]),
        r=float(data["r"]),
        sigma=float(data["sigma"]),
        flag=data["flag"],
    )
    return json.dumps(result)


def _black_iv(data):
    result = calculate_black_iv(
        price=float(data["price"]),
        S=float(data["S"]),
        K=float(data["K"]),
        t=float(data["t"]),
        r=float(data["r"]),
        flag=data["flag"],
    )
    return json.dumps(result)


# ---------------------------------------------------------------------------
# Black-Scholes Model (equity options, no dividends)
# ---------------------------------------------------------------------------

def _bs_price(data):
    result = calculate_bs_price(
        S=float(data["S"]),
        K=float(data["K"]),
        t=float(data["t"]),
        r=float(data["r"]),
        sigma=float(data["sigma"]),
        flag=data["flag"],
    )
    return json.dumps(result)


def _bs_greeks(data):
    result = calculate_bs_greeks(
        S=float(data["S"]),
        K=float(data["K"]),
        t=float(data["t"]),
        r=float(data["r"]),
        sigma=float(data["sigma"]),
        flag=data["flag"],
    )
    return json.dumps(result)


def _bs_iv(data):
    result = calculate_bs_iv(
        price=float(data["price"]),
        S=float(data["S"]),
        K=float(data["K"]),
        t=float(data["t"]),
        r=float(data["r"]),
        flag=data["flag"],
    )
    return json.dumps(result)


# ---------------------------------------------------------------------------
# Black-Scholes-Merton Model (equity options with continuous dividends)
# ---------------------------------------------------------------------------

def _bsm_price(data):
    result = calculate_bsm_price(
        S=float(data["S"]),
        K=float(data["K"]),
        t=float(data["t"]),
        r=float(data["r"]),
        sigma=float(data["sigma"]),
        q=float(data["q"]),
        flag=data["flag"],
    )
    return json.dumps(result)


def _bsm_greeks(data):
    result = calculate_bsm_greeks(
        S=float(data["S"]),
        K=float(data["K"]),
        t=float(data["t"]),
        r=float(data["r"]),
        sigma=float(data["sigma"]),
        q=float(data["q"]),
        flag=data["flag"],
    )
    return json.dumps(result)


def _bsm_iv(data):
    result = calculate_bsm_iv(
        price=float(data["price"]),
        S=float(data["S"]),
        K=float(data["K"]),
        t=float(data["t"]),
        r=float(data["r"]),
        q=float(data["q"]),
        flag=data["flag"],
    )
    return json.dumps(result)


if __name__ == "__main__":
    if len(sys.argv) > 1:
        print(main(sys.argv[1:]))
    else:
        print("Usage: worker_handler.py <operation> <json_data>")
