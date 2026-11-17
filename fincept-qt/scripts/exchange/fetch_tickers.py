"""
Fetch several tickers in one call — a one-shot wrapper over the exchange daemon's own `fetch_tickers`
handler (exchange_daemon.py): the same symbol resolution, checks and reply
shape as the terminal, so this script cannot drift from it.

Usage: python fetch_tickers.py <exchange_id> <symbol1> [symbol2 ...]
Output: one JSON line in the daemon's reply shape —
  {"id":"cli","success":true,"data":{...}}  or  {"id":"cli","success":false,"error":"...","code":"..."}
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import exchange_daemon as D  # noqa: E402

USAGE = "fetch_tickers.py <exchange_id> <symbol1> [symbol2 ...]"


def main(argv):
    if len(argv) < 3:
        return D.cli_usage_error(USAGE)
    try:
        args = {"symbols": argv[2:]}
    except ValueError:
        return D.cli_usage_error(USAGE)
    return D.run_cli("fetch_tickers", argv[1], args)[0]


if __name__ == "__main__":
    sys.exit(main(sys.argv))
