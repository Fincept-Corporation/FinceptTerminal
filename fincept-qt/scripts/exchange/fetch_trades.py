"""
Fetch recent public trades — a one-shot wrapper over the exchange daemon's own `fetch_trades`
handler (exchange_daemon.py): the same symbol resolution, checks and reply
shape as the terminal, so this script cannot drift from it.

Usage: python fetch_trades.py <exchange_id> <symbol> [limit]
Output: one JSON line in the daemon's reply shape —
  {"id":"cli","success":true,"data":{...}}  or  {"id":"cli","success":false,"error":"...","code":"..."}
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import exchange_daemon as D  # noqa: E402

USAGE = "fetch_trades.py <exchange_id> <symbol> [limit]"


def main(argv):
    if len(argv) < 3:
        return D.cli_usage_error(USAGE)
    try:
        args = {"symbol": argv[2], "limit": int(argv[3]) if len(argv) > 3 else 50}
    except ValueError:
        return D.cli_usage_error(USAGE)
    return D.run_cli("fetch_trades", argv[1], args)[0]


if __name__ == "__main__":
    sys.exit(main(sys.argv))
