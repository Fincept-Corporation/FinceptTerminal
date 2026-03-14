#!/usr/bin/env python3
"""
AKShare Energy Data Wrapper
Provides access to energy data: carbon trading, oil prices, energy markets
"""

import sys
import json
import time
from datetime import datetime

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


def safe_call(func, *args, **kwargs):
    """Safely call AKShare function with error handling and retries"""
    max_retries = 2
    for attempt in range(max_retries):
        try:
            result = func(*args, **kwargs)
            if isinstance(result, pd.DataFrame):
                if result.empty:
                    return {"success": True, "data": [], "count": 0}
                for col in result.columns:
                    if result[col].dtype == 'datetime64[ns]':
                        result[col] = result[col].astype(str)
                # Replace NaN/Infinity with None for valid JSON
                result = result.replace([float("inf"), float("-inf")], None)
                result = result.where(pd.notna(result), None)
                data = result.to_dict(orient='records')
                return {"success": True, "data": data, "count": len(data)}
            elif isinstance(result, (list, dict)):
                return {"success": True, "data": result, "count": len(result) if isinstance(result, list) else 1}
            else:
                return {"success": True, "data": str(result), "count": 1}
        except Exception as e:
            if attempt < max_retries - 1:
                time.sleep(1)
                continue
            error_msg = str(e)
            # Add context for common errors
            if "find_all" in error_msg or "NoneType" in error_msg:
                error_msg = f"Data source unavailable or temporarily down: {error_msg}"
            return {"success": False, "error": error_msg, "data": []}
    return {"success": False, "error": "Data source unavailable after retries", "data": []}


# ==================== CARBON TRADING ====================

def get_energy_carbon_domestic():
    """Get domestic carbon trading data"""
    return safe_call(ak.energy_carbon_domestic)

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
    "energy_carbon_domestic": {"func": get_energy_carbon_domestic, "desc": "Domestic carbon trading", "category": "Carbon Trading"},
    "energy_carbon_eu": {"func": get_energy_carbon_eu, "desc": "EU carbon trading", "category": "Carbon Trading"},
    "energy_carbon_beijing": {"func": get_energy_carbon_bj, "desc": "Beijing carbon market", "category": "Carbon Trading"},
    "energy_carbon_guangzhou": {"func": get_energy_carbon_gz, "desc": "Guangzhou carbon market", "category": "Carbon Trading"},
    "energy_carbon_hubei": {"func": get_energy_carbon_hb, "desc": "Hubei carbon market", "category": "Carbon Trading"},
    "energy_carbon_shenzhen": {"func": get_energy_carbon_sz, "desc": "Shenzhen carbon market", "category": "Carbon Trading"},

    # Oil Prices
    "energy_oil_hist": {"func": get_energy_oil_hist, "desc": "Historical oil prices", "category": "Oil"},
    "energy_oil_detail": {"func": get_energy_oil_detail, "desc": "Detailed oil prices", "category": "Oil"},
}


def get_all_endpoints():
    """Return all available endpoints with descriptions"""
    endpoints = list(ENDPOINTS.keys())
    categories = {}
    for name, info in ENDPOINTS.items():
        cat = info.get("category", "Other")
        if cat not in categories:
            categories[cat] = []
        categories[cat].append(name)

    return {
        "success": True,
        "data": {
            "available_endpoints": endpoints,
            "total_count": len(endpoints),
            "categories": categories
        },
        "timestamp": int(time.time())
    }


def main():
    # Set stdout encoding to UTF-8
    import io
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

    if len(sys.argv) < 2:
        print(json.dumps({"success": False, "error": "No endpoint specified", "data": []}))
        sys.exit(1)

    endpoint = sys.argv[1]
    args = sys.argv[2:] if len(sys.argv) > 2 else []

    if endpoint == "get_all_endpoints":
        result = get_all_endpoints()
    elif endpoint in ENDPOINTS:
        func = ENDPOINTS[endpoint]["func"]
        if args:
            result = func(*args)
        else:
            result = func()
    else:
        result = {"success": False, "error": f"Unknown endpoint: {endpoint}", "data": []}

    result["timestamp"] = int(time.time())
    print(json.dumps(result, ensure_ascii=False, default=str))


if __name__ == "__main__":
    main()
