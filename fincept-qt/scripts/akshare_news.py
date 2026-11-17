#!/usr/bin/env python3
"""
AKShare News Data Wrapper
Provides access to financial news: CCTV, Baidu economic news, trade notifications
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


def main():
    return run_endpoint_table(ENDPOINTS)


if __name__ == "__main__":
    sys.exit(main())
