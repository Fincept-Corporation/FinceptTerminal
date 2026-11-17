#!/usr/bin/env python3
"""
AKShare Energy Data Wrapper
Provides access to energy data: carbon trading, oil prices, energy markets
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


# ==================== CARBON TRADING ====================


def get_energy_carbon_eu():
    """Get EU carbon trading data"""
    return safe_call(ak.energy_carbon_eu)

def get_energy_carbon_bj():
    """Get Beijing carbon trading data"""
    return safe_call(ak.energy_carbon_bj)

def get_energy_carbon_gz():
    """Get Guangzhou carbon trading data"""
    return safe_call(ak.energy_carbon_gz)

def get_energy_carbon_hb():
    """Get Hubei carbon trading data"""
    return safe_call(ak.energy_carbon_hb)

def get_energy_carbon_sz():
    """Get Shenzhen carbon trading data"""
    return safe_call(ak.energy_carbon_sz)


# ==================== OIL PRICES ====================

def get_energy_oil_hist():
    """Get historical oil price data"""
    return safe_call(ak.energy_oil_hist)

def get_energy_oil_detail():
    """Get detailed oil price data"""
    return safe_call(ak.energy_oil_detail)


# ==================== ENDPOINT REGISTRY ====================

ENDPOINTS = {
    # Carbon Trading
    "energy_carbon_eu": {"func": get_energy_carbon_eu, "desc": "EU carbon trading", "category": "Carbon Trading"},
    "energy_carbon_beijing": {"func": get_energy_carbon_bj, "desc": "Beijing carbon market", "category": "Carbon Trading"},
    "energy_carbon_guangzhou": {"func": get_energy_carbon_gz, "desc": "Guangzhou carbon market", "category": "Carbon Trading"},
    "energy_carbon_hubei": {"func": get_energy_carbon_hb, "desc": "Hubei carbon market", "category": "Carbon Trading"},
    "energy_carbon_shenzhen": {"func": get_energy_carbon_sz, "desc": "Shenzhen carbon market", "category": "Carbon Trading"},

    # Oil Prices
    "energy_oil_hist": {"func": get_energy_oil_hist, "desc": "Historical oil prices", "category": "Oil"},
    "energy_oil_detail": {"func": get_energy_oil_detail, "desc": "Detailed oil prices", "category": "Oil"},
}


def main():
    return run_endpoint_table(ENDPOINTS)


if __name__ == "__main__":
    sys.exit(main())
