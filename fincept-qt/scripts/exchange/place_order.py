"""
Place an order on an exchange — a one-shot wrapper over the exchange daemon's
own prepare_order + place_order handlers (exchange_daemon.py), so it applies
exactly the rules the terminal applies: the market must exist exactly (no
quote substitution), the amount is in BASE COINS and converted to the
exchange's own unit (contracts on derivatives), prices must be on the price
grid, stops / SL / TP only where the exchange supports them, a client order id
is attached, and the outcome is classified (accepted / rejected / unknown).

Credentials come as JSON on stdin, never on the command line.

Usage: echo '{"api_key":"...","secret":"..."}' | python place_order.py <exchange_id> <symbol> <side> <type> <amount> [price]
         [--stop-price P] [--sl P] [--tp P] [--reduce-only] [--margin-mode cross|isolated]
         [--leverage N] [--contracts] [--prepare-only]
  <type>     market | limit | stop | stop_limit
  <amount>   base-currency quantity (coins); with --contracts, a number of contracts
  --prepare-only  print what would be sent (market, exchange quantity, price, params) and send nothing

Output: one JSON line in the daemon's reply shape, e.g.
  {"id":"cli","success":true,"data":{"outcome":"accepted","id":"12345","symbol":"BTC/USDT",...,"prepared":{...}}}
  {"id":"cli","success":false,"error":"...","code":"ORDER_REFUSED","outcome":"rejected","alternatives":[...]}
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import exchange_daemon as D  # noqa: E402

USAGE = ("place_order.py <exchange_id> <symbol> <side> <type> <amount> [price] [--stop-price P] [--sl P] "
         "[--tp P] [--reduce-only] [--margin-mode cross|isolated] [--leverage N] [--contracts] [--prepare-only]")
_VALUE_FLAGS = {"--stop-price": "stop_price", "--sl": "sl", "--tp": "tp", "--margin-mode": "margin_mode",
                "--leverage": "leverage"}


def main(argv):
    pos, args, prepare_only = [], {}, False
    i = 1
    while i < len(argv):
        a = argv[i]
        if a in _VALUE_FLAGS and i + 1 < len(argv):
            args[_VALUE_FLAGS[a]] = argv[i + 1]
            i += 2
            continue
        if a == "--reduce-only":
            args["reduce_only"] = True
        elif a == "--contracts":
            args["amount_unit"] = "contracts"
        elif a == "--prepare-only":
            prepare_only = True
        elif a.startswith("--"):
            return D.cli_usage_error(USAGE)
        else:
            pos.append(a)
        i += 1
    if len(pos) < 5:
        return D.cli_usage_error(USAGE)
    exchange_id = pos[0]
    args.update({"symbol": pos[1], "side": pos[2], "type": pos[3], "amount": pos[4]})
    if len(pos) > 5:
        args["price"] = pos[5]
    creds = D.cli_credentials()

    status, prepared = D.run_cli("prepare_order", exchange_id, args, creds, emit=prepare_only)
    if prepare_only or status != 0:
        if status != 0 and not prepare_only:
            D._write_line(D._dumps(prepared))
        return status
    p = prepared["data"]
    args.update(client_order_token=p["client_order_token"], confirm_hash=p["confirm_hash"],
                ref_price=p["ref_price"], account_id=p["account_id"])
    return D.run_cli("place_order", exchange_id, args, creds)[0]


if __name__ == "__main__":
    sys.exit(main(sys.argv))
