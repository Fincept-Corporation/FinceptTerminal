"""
Set the margin mode of a contract market (changes the account; on some exchanges account-wide) — a one-shot wrapper over the exchange daemon's own `set_margin_mode`
handler (exchange_daemon.py): the same symbol resolution, checks and reply
shape as the terminal, so this script cannot drift from it.
Credentials come as JSON on stdin, never on the command line.

Usage: echo '{"api_key":"...","secret":"..."}' | python set_margin_mode.py <exchange_id> <symbol> <cross|isolated> [leverage]
Output: one JSON line in the daemon's reply shape —
  {"id":"cli","success":true,"data":{...}}  or  {"id":"cli","success":false,"error":"...","code":"..."}
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import exchange_daemon as D  # noqa: E402

USAGE = "set_margin_mode.py <exchange_id> <symbol> <cross|isolated> [leverage]"


def main(argv):
    if len(argv) < 4:
        return D.cli_usage_error(USAGE)
    try:
        args = {"symbol": argv[2], "mode": argv[3], **({"leverage": argv[4]} if len(argv) > 4 else {})}
    except ValueError:
        return D.cli_usage_error(USAGE)
    return D.run_cli("set_margin_mode", argv[1], args, D.cli_credentials())[0]


if __name__ == "__main__":
    sys.exit(main(sys.argv))
