"""
Fetch maker / taker fees (one market, or the account's table) — a one-shot wrapper over the exchange daemon's own `fetch_trading_fees`
handler (exchange_daemon.py): the same symbol resolution, checks and reply
shape as the terminal, so this script cannot drift from it.

Usage: python fetch_trading_fees.py <exchange_id> [symbol]
Output: one JSON line in the daemon's reply shape —
  {"id":"cli","success":true,"data":{...}}  or  {"id":"cli","success":false,"error":"...","code":"..."}
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import exchange_daemon as D  # noqa: E402

USAGE = "fetch_trading_fees.py <exchange_id> [symbol]"


def main(argv):
    if len(argv) < 2:
        return D.cli_usage_error(USAGE)
    try:
        args = {"symbol": argv[2]} if len(argv) > 2 else {}
    except ValueError:
        return D.cli_usage_error(USAGE)
    return D.run_cli("fetch_trading_fees", argv[1], args)[0]


if __name__ == "__main__":
    sys.exit(main(sys.argv))
