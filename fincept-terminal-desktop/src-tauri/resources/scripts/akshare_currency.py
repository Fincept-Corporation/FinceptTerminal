#!/usr/bin/env python3
"""
AKShare Currency/Forex Data Wrapper
Provides access to forex data: exchange rates, currency pairs, historical data
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
            return {"success": False, "error": str(e), "data": []}
    return {"success": False, "error": "Max retries exceeded", "data": []}


# ==================== EXCHANGE RATES ====================

def get_currency_boc_safe():
    """Get Bank of China exchange rates (SAFE)"""
    return safe_call(ak.currency_boc_safe)

def get_currency_boc_sina():
    """Get Bank of China exchange rates from Sina"""
    return safe_call(ak.currency_boc_sina)


# ==================== CURRENCY DATA ====================

def get_currency_currencies():
    """Get list of available currencies"""
    return safe_call(ak.currency_currencies)

def get_currency_pair_map():
    """Get currency pair mapping"""
    return safe_call(ak.currency_pair_map)

def get_currency_latest(base="USD"):
    """Get latest exchange rates for base currency"""
    return safe_call(ak.currency_latest, base=base)

def get_currency_history(base="USD", symbol="CNY", start_date="2020-01-01", end_date="2025-12-31"):
    """Get historical exchange rate data"""
    return safe_call(ak.currency_history, base=base, symbol=symbol, start_date=start_date, end_date=end_date)

def get_currency_time_series(base="USD", symbol="CNY", start_date="2020-01-01", end_date="2025-12-31"):
    """Get currency time series data"""
    return safe_call(ak.currency_time_series, base=base, symbol=symbol, start_date=start_date, end_date=end_date)

def get_currency_convert(base="USD", symbol="CNY", amount=100):
    """Convert currency amount"""
    return safe_call(ak.currency_convert, base=base, symbol=symbol, amount=amount)


# ==================== ENDPOINT REGISTRY ====================

ENDPOINTS = {
    # Exchange Rates
    "currency_boc_safe": {"func": get_currency_boc_safe, "desc": "Bank of China rates (SAFE)", "category": "Exchange Rates"},
    "currency_boc_sina": {"func": get_currency_boc_sina, "desc": "Bank of China rates (Sina)", "category": "Exchange Rates"},

    # Currency Data
    "currency_list": {"func": get_currency_currencies, "desc": "List of currencies", "category": "Currency Data"},
    "currency_pair_map": {"func": get_currency_pair_map, "desc": "Currency pair mapping", "category": "Currency Data"},
    "currency_latest": {"func": get_currency_latest, "desc": "Latest exchange rates", "category": "Currency Data"},
    "currency_history": {"func": get_currency_history, "desc": "Historical exchange rates", "category": "Currency Data"},
    "currency_time_series": {"func": get_currency_time_series, "desc": "Currency time series", "category": "Currency Data"},
    "currency_convert": {"func": get_currency_convert, "desc": "Currency converter", "category": "Currency Data"},
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
