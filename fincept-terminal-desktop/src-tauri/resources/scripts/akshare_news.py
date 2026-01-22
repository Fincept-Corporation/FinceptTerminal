#!/usr/bin/env python3
"""
AKShare News Data Wrapper
Provides access to financial news: CCTV, Baidu economic news, trade notifications
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


# ==================== NEWS FEEDS ====================

def get_news_cctv(date="20240101"):
    """Get CCTV news"""
    return safe_call(ak.news_cctv, date=date)

def get_news_economic_baidu():
    """Get Baidu economic news"""
    return safe_call(ak.news_economic_baidu)

def get_news_report_time_baidu():
    """Get Baidu report time news"""
    return safe_call(ak.news_report_time_baidu)


# ==================== TRADE NOTIFICATIONS ====================

def get_news_trade_notify_dividend_baidu():
    """Get dividend notifications from Baidu"""
    return safe_call(ak.news_trade_notify_dividend_baidu)

def get_news_trade_notify_suspend_baidu():
    """Get suspension notifications from Baidu"""
    return safe_call(ak.news_trade_notify_suspend_baidu)


# ==================== ENDPOINT REGISTRY ====================

ENDPOINTS = {
    # News Feeds
    "news_cctv": {"func": get_news_cctv, "desc": "CCTV news", "category": "News Feeds"},
    "news_economic_baidu": {"func": get_news_economic_baidu, "desc": "Baidu economic news", "category": "News Feeds"},
    "news_report_time": {"func": get_news_report_time_baidu, "desc": "Report time news", "category": "News Feeds"},

    # Trade Notifications
    "news_dividend_notify": {"func": get_news_trade_notify_dividend_baidu, "desc": "Dividend notifications", "category": "Trade Notifications"},
    "news_suspend_notify": {"func": get_news_trade_notify_suspend_baidu, "desc": "Suspension notifications", "category": "Trade Notifications"},
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
