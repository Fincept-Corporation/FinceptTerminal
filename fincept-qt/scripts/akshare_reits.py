#!/usr/bin/env python3
"""
AKShare REITs Data Wrapper
Provides access to REIT data: realtime, historical, minute data
"""

import sys
import json
import time
from datetime import datetime

# safe_call: one akshare call -> {success, data, columns, count} or a plain-English error.
# run_endpoint_table: the CLI (get_all_endpoints / <endpoint> [field=value ...]) printing one
# JSON line through fincept_json (NaN -> null) with akshare's own prints kept off stdout.
from akshare_screen_args import between_dates, run_endpoint_table, safe_call

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


# ==================== REIT DATA ====================

def get_reits_realtime():
    """Get REITs realtime data"""
    return safe_call(ak.reits_realtime_em)

def get_reits_hist(symbol="180101", start_date="", end_date=""):
    """Daily history of one REIT; START/END narrow the range (akshare returns the full history)"""
    return safe_call(lambda: between_dates(ak.reits_hist_em(symbol=symbol), start_date, end_date))

def get_reits_hist_min(symbol="180101"):
    """Get REITs minute historical data"""
    return safe_call(ak.reits_hist_min_em, symbol=symbol)


# ==================== ENDPOINT REGISTRY ====================

ENDPOINTS = {
    # REITs Data
    "reits_realtime": {"func": get_reits_realtime, "desc": "REITs realtime data", "category": "REITs"},
    "reits_hist": {"func": get_reits_hist, "desc": "REITs historical data", "category": "REITs"},
    "reits_hist_min": {"func": get_reits_hist_min, "desc": "REITs minute data", "category": "REITs"},
}


def main():
    return run_endpoint_table(ENDPOINTS)


if __name__ == "__main__":
    sys.exit(main())
