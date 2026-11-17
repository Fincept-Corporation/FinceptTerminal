#!/usr/bin/env python3
"""
AKShare Crypto Data Wrapper
Provides access to cryptocurrency data: Bitcoin, CME futures, spot prices
"""

import sys
import json
import time
from datetime import datetime

# safe_call: one akshare call -> {success, data, columns, count} or a plain-English error.
# run_endpoint_table: the CLI (get_all_endpoints / <endpoint> [field=value ...]) printing one
# JSON line through fincept_json (NaN -> null) with akshare's own prints kept off stdout.
from akshare_screen_args import run_endpoint_table, safe_call

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


# ==================== BITCOIN ====================

def get_crypto_bitcoin_cme():
    """Get Bitcoin CME futures data"""
    return safe_call(ak.crypto_bitcoin_cme)

def get_crypto_bitcoin_hold_report():
    """Get Bitcoin holding report"""
    return safe_call(ak.crypto_bitcoin_hold_report)


# ==================== SPOT PRICES ====================

def get_crypto_js_spot():
    """Get cryptocurrency spot prices"""
    return safe_call(ak.crypto_js_spot)


# ==================== ENDPOINT REGISTRY ====================

ENDPOINTS = {
    # Bitcoin
    "crypto_bitcoin_cme": {"func": get_crypto_bitcoin_cme, "desc": "Bitcoin CME futures", "category": "Bitcoin"},
    "crypto_bitcoin_hold_report": {"func": get_crypto_bitcoin_hold_report, "desc": "Bitcoin holding report", "category": "Bitcoin"},

    # Spot
    "crypto_spot": {"func": get_crypto_js_spot, "desc": "Crypto spot prices", "category": "Spot"},
}


def main():
    return run_endpoint_table(ENDPOINTS)


if __name__ == "__main__":
    sys.exit(main())
