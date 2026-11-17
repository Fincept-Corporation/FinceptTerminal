"""
List the exchange ids ccxt supports — a one-shot wrapper over the exchange daemon's own `list_exchange_ids`
handler (exchange_daemon.py): the same symbol resolution, checks and reply
shape as the terminal, so this script cannot drift from it.

Usage: python list_exchange_ids.py
Output: one JSON line in the daemon's reply shape —
  {"id":"cli","success":true,"data":{...}}  or  {"id":"cli","success":false,"error":"...","code":"..."}
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import exchange_daemon as D  # noqa: E402

USAGE = "list_exchange_ids.py "


def main(argv):
    if len(argv) < 1:
        return D.cli_usage_error(USAGE)
    try:
        args = {}
    except ValueError:
        return D.cli_usage_error(USAGE)
    return D.run_cli("list_exchange_ids", "", args)[0]


if __name__ == "__main__":
    sys.exit(main(sys.argv))
