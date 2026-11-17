"""
Fetch open positions (all markets without a symbol) — a one-shot wrapper over the exchange daemon's own `fetch_positions`
handler (exchange_daemon.py): the same symbol resolution, checks and reply
shape as the terminal, so this script cannot drift from it.
Credentials come as JSON on stdin, never on the command line.

Usage: echo '{"api_key":"...","secret":"..."}' | python fetch_positions.py <exchange_id> [symbol]
Output: one JSON line in the daemon's reply shape —
  {"id":"cli","success":true,"data":{...}}  or  {"id":"cli","success":false,"error":"...","code":"..."}
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import exchange_daemon as D  # noqa: E402

USAGE = "fetch_positions.py <exchange_id> [symbol]"


def main(argv):
    if len(argv) < 2:
        return D.cli_usage_error(USAGE)
    try:
        args = {"symbol": argv[2]} if len(argv) > 2 else {}
    except ValueError:
        return D.cli_usage_error(USAGE)
    return D.run_cli("fetch_positions", argv[1], args, D.cli_credentials())[0]


if __name__ == "__main__":
    sys.exit(main(sys.argv))
