"""
List all ccxt exchange IDs (fast — no instantiation).

Usage: python list_exchange_ids.py

Output JSON:
{
  "success": true,
  "data": {
    "ids": ["ace", "alpaca", "ascendex", "bequant", "bigone", "binance", ...],
    "count": 110
  }
}
"""

import ccxt
from exchange_client import output_success, run_with_error_handling


@run_with_error_handling
def main():
    ids = sorted(ccxt.exchanges)
    output_success({
        "ids": ids,
        "count": len(ids),
    })


if __name__ == "__main__":
    main()
