#!/usr/bin/env python3
"""
AKShare Currency/Forex Data Wrapper
Provides access to forex data: exchange rates, currency pairs, historical data
"""

import sys
import json
import time
from datetime import datetime, timedelta

# safe_call: one akshare call -> {success, data, columns, count} or a plain-English error.
# run_endpoint_table: the CLI (get_all_endpoints / <endpoint> [field=value ...]) printing one
# JSON line through fincept_json (NaN -> null) with akshare's own prints kept off stdout.
from akshare_screen_args import china_today, run_endpoint_table, safe_call

try:
    import akshare as ak
    import pandas as pd
except ImportError as e:
    print(json.dumps({
        "success": False,
        "error": f"Missing dependency: {e}",
        "data": []
    }))
    sys.exit(1)


# ==================== EXCHANGE RATES ====================

def get_currency_boc_safe():
    """Get Bank of China exchange rates (SAFE)"""
    return safe_call(ak.currency_boc_safe)

def get_currency_boc_sina(symbol="美元", start_date="", end_date=""):
    """Bank of China rates for one currency (Chinese name: 美元, 欧元, 日元, 港币, 英镑 ...) from Sina;
    default the last 90 days (akshare's own default window is fixed in 2023). Sina serves one page
    per ~30 rows at up to ~10 s a page, so long ranges are slow."""
    today = china_today()
    start = start_date or (today - timedelta(days=90)).strftime("%Y%m%d")
    return safe_call(ak.currency_boc_sina, symbol=symbol, start_date=start, end_date=end_date or today.strftime("%Y%m%d"))


# ==================== CURRENCY DATA ====================

def get_currency_pair_map():
    """Get currency pair mapping"""
    return safe_call(ak.currency_pair_map)


# ==================== ENDPOINT REGISTRY ====================

ENDPOINTS = {
    # Exchange Rates
    "currency_boc_safe": {"func": get_currency_boc_safe, "desc": "Bank of China rates (SAFE)", "category": "Exchange Rates"},
    "currency_boc_sina": {"func": get_currency_boc_sina, "desc": "Bank of China rates (Sina)", "category": "Exchange Rates"},

    # Currency Data
    "currency_pair_map": {"func": get_currency_pair_map, "desc": "Currency pair mapping", "category": "Currency Data"},
}

# akshare's currency_currencies / currency_latest / currency_history / currency_time_series /
# currency_convert call CurrencyScoop, which now requires a paid API key (api_key=...) that the
# screen has no way to pass. They are no longer listed; a stale saved selection gets this answer.
_KEYED = ("{} needs a paid CurrencyScoop API key, which this screen cannot supply. For exchange rates "
          "use currency_boc_safe or currency_boc_sina (no key needed).")
RETIRED = {name: _KEYED.format(name) for name in
           ("currency_list", "currency_latest", "currency_history", "currency_time_series", "currency_convert")}


def main():
    return run_endpoint_table(ENDPOINTS, retired=RETIRED)


if __name__ == "__main__":
    sys.exit(main())
