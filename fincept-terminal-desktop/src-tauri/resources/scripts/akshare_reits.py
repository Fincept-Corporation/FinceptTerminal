#!/usr/bin/env python3
"""
AKShare REITs Data Wrapper
Provides access to REIT data: realtime, historical, minute data
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


# ==================== REIT DATA ====================

def get_reits_realtime():
    """Get REITs realtime data"""
    return safe_call(ak.reits_realtime_em)

def get_reits_hist(symbol="180101", period="daily", start_date="20200101", end_date="20251231", adjust=""):
    """Get REITs historical data"""
    return safe_call(ak.reits_hist_em, symbol=symbol, period=period, start_date=start_date, end_date=end_date, adjust=adjust)

def get_reits_hist_min(symbol="180101", period="1"):
    """Get REITs minute historical data"""
    return safe_call(ak.reits_hist_min_em, symbol=symbol, period=period)


# ==================== ENDPOINT REGISTRY ====================

ENDPOINTS = {
    # REITs Data
    "reits_realtime": {"func": get_reits_realtime, "desc": "REITs realtime data", "category": "REITs"},
    "reits_hist": {"func": get_reits_hist, "desc": "REITs historical data", "category": "REITs"},
    "reits_hist_min": {"func": get_reits_hist_min, "desc": "REITs minute data", "category": "REITs"},
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
