#!/usr/bin/env python3
"""
AKShare Stocks Historical Data Wrapper (Batch 2)
Provides access to historical price data: daily, minute, intraday
~50 endpoints
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
            return {"success": False, "error": str(e), "data": []}
    return {"success": False, "error": "Max retries exceeded", "data": []}


# ==================== A-SHARES HISTORICAL ====================

def get_stock_zh_a_hist(symbol="000001", period="daily", start_date="20200101", end_date="20261231", adjust=""):
    """Get A-shares historical data"""
    return safe_call(ak.stock_zh_a_hist, symbol=symbol, period=period, start_date=start_date, end_date=end_date, adjust=adjust)

def get_stock_zh_a_hist_min_em(symbol="000001", period="1", start_date="2024-01-01 09:30:00", end_date="2026-12-31 15:00:00", adjust=""):
    """Get A-shares minute historical data (EastMoney)"""
    return safe_call(ak.stock_zh_a_hist_min_em, symbol=symbol, period=period, start_date=start_date, end_date=end_date, adjust=adjust)

def get_stock_zh_a_hist_pre_min_em(symbol="000001"):
    """Get A-shares pre-market minute data"""
    return safe_call(ak.stock_zh_a_hist_pre_min_em, symbol=symbol)

def get_stock_zh_a_hist_tx(symbol="sz000001", start_date="20200101", end_date="20261231"):
    """Get A-shares historical data (Tencent)"""
    return safe_call(ak.stock_zh_a_hist_tx, symbol=symbol, start_date=start_date, end_date=end_date)

def get_stock_zh_a_daily(symbol="sz000001", start_date="20200101", end_date="20261231", adjust="qfq"):
    """Get A-shares daily data"""
    return safe_call(ak.stock_zh_a_daily, symbol=symbol, start_date=start_date, end_date=end_date, adjust=adjust)

def get_stock_zh_a_minute(symbol="sz000001", period="1", adjust=""):
    """Get A-shares minute data"""
    return safe_call(ak.stock_zh_a_minute, symbol=symbol, period=period, adjust=adjust)

def get_stock_zh_a_cdr_daily(symbol="689009", start_date="20200101", end_date="20261231"):
    """Get A-shares CDR daily data"""
    return safe_call(ak.stock_zh_a_cdr_daily, symbol=symbol, start_date=start_date, end_date=end_date)


# ==================== B-SHARES HISTORICAL ====================

def get_stock_zh_b_daily(symbol="sh900901", start_date="20200101", end_date="20261231", adjust="qfq"):
    """Get B-shares daily data"""
    return safe_call(ak.stock_zh_b_daily, symbol=symbol, start_date=start_date, end_date=end_date, adjust=adjust)

def get_stock_zh_b_minute(symbol="sh900901", period="1", adjust=""):
    """Get B-shares minute data"""
    return safe_call(ak.stock_zh_b_minute, symbol=symbol, period=period, adjust=adjust)


# ==================== HK STOCKS HISTORICAL ====================

def get_stock_hk_hist(symbol="00700", period="daily", start_date="20200101", end_date="20261231", adjust=""):
    """Get HK stocks historical data"""
    return safe_call(ak.stock_hk_hist, symbol=symbol, period=period, start_date=start_date, end_date=end_date, adjust=adjust)

def get_stock_hk_hist_min_em(symbol="00700", period="1", start_date="2024-01-01 09:30:00", end_date="2026-12-31 16:00:00", adjust=""):
    """Get HK stocks minute historical data"""
    return safe_call(ak.stock_hk_hist_min_em, symbol=symbol, period=period, start_date=start_date, end_date=end_date, adjust=adjust)

def get_stock_hk_daily(symbol="00700", adjust="qfq"):
    """Get HK stocks daily data"""
    return safe_call(ak.stock_hk_daily, symbol=symbol, adjust=adjust)


# ==================== US STOCKS HISTORICAL ====================

def get_stock_us_hist(symbol="105.AAPL", period="daily", start_date="20200101", end_date="20261231", adjust=""):
    """Get US stocks historical data"""
    return safe_call(ak.stock_us_hist, symbol=symbol, period=period, start_date=start_date, end_date=end_date, adjust=adjust)

def get_stock_us_hist_min_em(symbol="105.AAPL", period="1", start_date="2024-01-01 09:30:00", end_date="2026-12-31 16:00:00", adjust=""):
    """Get US stocks minute historical data"""
    return safe_call(ak.stock_us_hist_min_em, symbol=symbol, period=period, start_date=start_date, end_date=end_date, adjust=adjust)

def get_stock_us_daily(symbol="AAPL", adjust="qfq"):
    """Get US stocks daily data"""
    return safe_call(ak.stock_us_daily, symbol=symbol, adjust=adjust)


# ==================== A+H SHARES HISTORICAL ====================

def get_stock_zh_ah_daily(symbol="02318", start_date="20200101", end_date="20261231"):
    """Get A+H shares daily data"""
    return safe_call(ak.stock_zh_ah_daily, symbol=symbol, start_date=start_date, end_date=end_date)


# ==================== INDEX HISTORICAL ====================

def get_stock_zh_index_daily(symbol="sh000001"):
    """Get Chinese index daily data"""
    return safe_call(ak.stock_zh_index_daily, symbol=symbol)

def get_stock_zh_index_daily_em(symbol="000001", start_date="20200101", end_date="20261231"):
    """Get Chinese index daily data (EastMoney)"""
    return safe_call(ak.stock_zh_index_daily_em, symbol=symbol, start_date=start_date, end_date=end_date)

def get_stock_zh_index_daily_tx(symbol="sh000001"):
    """Get Chinese index daily data (Tencent)"""
    return safe_call(ak.stock_zh_index_daily_tx, symbol=symbol)

def get_stock_zh_index_hist_csindex(symbol="000001", start_date="20200101", end_date="20261231"):
    """Get CSIndex historical data"""
    return safe_call(ak.stock_zh_index_hist_csindex, symbol=symbol, start_date=start_date, end_date=end_date)

def get_stock_zh_index_value_csindex(symbol="000001"):
    """Get CSIndex value data"""
    return safe_call(ak.stock_zh_index_value_csindex, symbol=symbol)

def get_stock_hk_index_daily_em(symbol="HSI", start_date="20200101", end_date="20261231"):
    """Get HK index daily data (EastMoney)"""
    return safe_call(ak.stock_hk_index_daily_em, symbol=symbol, start_date=start_date, end_date=end_date)

def get_stock_hk_index_daily_sina(symbol="HSI"):
    """Get HK index daily data (Sina)"""
    return safe_call(ak.stock_hk_index_daily_sina, symbol=symbol)


# ==================== STAR MARKET HISTORICAL ====================

def get_stock_zh_kcb_daily(symbol="688001", adjust="qfq"):
    """Get STAR Market daily data"""
    return safe_call(ak.stock_zh_kcb_daily, symbol=symbol, adjust=adjust)

def get_stock_zh_kcb_report_em(symbol="688001"):
    """Get STAR Market report data"""
    return safe_call(ak.stock_zh_kcb_report_em, symbol=symbol)


# ==================== INTRADAY ====================

def get_stock_intraday_em(symbol="000001"):
    """Get intraday data (EastMoney)"""
    return safe_call(ak.stock_intraday_em, symbol=symbol)

def get_stock_intraday_sina(symbol="sz000001"):
    """Get intraday data (Sina)"""
    return safe_call(ak.stock_intraday_sina, symbol=symbol)


# ==================== TICK DATA ====================

def get_stock_zh_a_tick_tx_js(symbol="sz000001"):
    """Get A-shares tick data (Tencent)"""
    return safe_call(ak.stock_zh_a_tick_tx_js, symbol=symbol)


# ==================== PRICE DATA ====================

def get_stock_price_js(symbol="000001"):
    """Get stock price data (JS)"""
    return safe_call(ak.stock_price_js, symbol=symbol)


# ==================== BID-ASK ====================

def get_stock_bid_ask_em(symbol="000001"):
    """Get bid-ask data (EastMoney)"""
    return safe_call(ak.stock_bid_ask_em, symbol=symbol)


# ==================== SECTOR HISTORICAL ====================

def get_stock_sector_detail(sector="new_blhy"):
    """Get sector detail data"""
    return safe_call(ak.stock_sector_detail, sector=sector)

def get_stock_sector_spot(indicator="new_blhy"):
    """Get sector spot data"""
    return safe_call(ak.stock_sector_spot, indicator=indicator)

def get_stock_sector_fund_flow_hist(symbol="即时", indicator="新浪行业"):
    """Get sector fund flow historical data"""
    return safe_call(ak.stock_sector_fund_flow_hist, symbol=symbol, indicator=indicator)

def get_stock_sector_fund_flow_rank(indicator="今日", sector_type="行业资金流"):
    """Get sector fund flow ranking"""
    return safe_call(ak.stock_sector_fund_flow_rank, indicator=indicator, sector_type=sector_type)

def get_stock_sector_fund_flow_summary(indicator="今日", sector_type="行业资金流"):
    """Get sector fund flow summary"""
    return safe_call(ak.stock_sector_fund_flow_summary, indicator=indicator, sector_type=sector_type)


# ==================== CLASSIFICATION ====================

def get_stock_classify_sina(symbol="industry"):
    """Get stock classification (Sina)"""
    return safe_call(ak.stock_classify_sina, symbol=symbol)


# ==================== SSE/SZSE SUMMARY ====================

def get_stock_sse_summary():
    """Get SSE market summary"""
    return safe_call(ak.stock_sse_summary)

def get_stock_sse_deal_daily(date="20240101"):
    """Get SSE daily deal data"""
    return safe_call(ak.stock_sse_deal_daily, date=date)

def get_stock_szse_summary(date="20240101"):
    """Get SZSE market summary"""
    return safe_call(ak.stock_szse_summary, date=date)

def get_stock_szse_area_summary(date="202401"):
    """Get SZSE area summary"""
    return safe_call(ak.stock_szse_area_summary, date=date)

def get_stock_szse_sector_summary(symbol="当日行业", date="20240101"):
    """Get SZSE sector summary"""
    return safe_call(ak.stock_szse_sector_summary, symbol=symbol, date=date)


# ==================== VALUATION ====================

def get_stock_zh_valuation_baidu(symbol="000001", indicator="总市值", period="全部"):
    """Get stock valuation (Baidu)"""
    return safe_call(ak.stock_zh_valuation_baidu, symbol=symbol, indicator=indicator, period=period)

def get_stock_hk_valuation_baidu(symbol="00700", indicator="总市值", period="全部"):
    """Get HK stock valuation (Baidu)"""
    return safe_call(ak.stock_hk_valuation_baidu, symbol=symbol, indicator=indicator, period=period)

def get_stock_value_em(symbol="000001"):
    """Get stock value (EastMoney)"""
    return safe_call(ak.stock_value_em, symbol=symbol)


# ==================== COMPARISON ====================

def get_stock_zh_ab_comparison_em():
    """Get A-B shares comparison (EastMoney)"""
    return safe_call(ak.stock_zh_ab_comparison_em)

def get_stock_zh_dupont_comparison_em(symbol="000001"):
    """Get Dupont comparison (EastMoney)"""
    return safe_call(ak.stock_zh_dupont_comparison_em, symbol=symbol)

def get_stock_zh_growth_comparison_em(symbol="000001"):
    """Get growth comparison (EastMoney)"""
    return safe_call(ak.stock_zh_growth_comparison_em, symbol=symbol)

def get_stock_zh_scale_comparison_em(symbol="000001"):
    """Get scale comparison (EastMoney)"""
    return safe_call(ak.stock_zh_scale_comparison_em, symbol=symbol)

def get_stock_zh_valuation_comparison_em(symbol="000001"):
    """Get valuation comparison (EastMoney)"""
    return safe_call(ak.stock_zh_valuation_comparison_em, symbol=symbol)

def get_stock_hk_growth_comparison_em(symbol="00700"):
    """Get HK growth comparison (EastMoney)"""
    return safe_call(ak.stock_hk_growth_comparison_em, symbol=symbol)

def get_stock_hk_scale_comparison_em(symbol="00700"):
    """Get HK scale comparison (EastMoney)"""
    return safe_call(ak.stock_hk_scale_comparison_em, symbol=symbol)

def get_stock_hk_valuation_comparison_em(symbol="00700"):
    """Get HK valuation comparison (EastMoney)"""
    return safe_call(ak.stock_hk_valuation_comparison_em, symbol=symbol)


# ==================== ENDPOINT REGISTRY ====================

ENDPOINTS = {
    # A-Shares Historical
    "stock_zh_a_hist": {"func": get_stock_zh_a_hist, "desc": "A-shares historical data", "category": "A-Shares Historical"},
    "stock_zh_a_hist_min_em": {"func": get_stock_zh_a_hist_min_em, "desc": "A-shares minute data (EastMoney)", "category": "A-Shares Historical"},
    "stock_zh_a_hist_pre_min_em": {"func": get_stock_zh_a_hist_pre_min_em, "desc": "A-shares pre-market minute", "category": "A-Shares Historical"},
    "stock_zh_a_hist_tx": {"func": get_stock_zh_a_hist_tx, "desc": "A-shares historical (Tencent)", "category": "A-Shares Historical"},
    "stock_zh_a_daily": {"func": get_stock_zh_a_daily, "desc": "A-shares daily data", "category": "A-Shares Historical"},
    "stock_zh_a_minute": {"func": get_stock_zh_a_minute, "desc": "A-shares minute data", "category": "A-Shares Historical"},
    "stock_zh_a_cdr_daily": {"func": get_stock_zh_a_cdr_daily, "desc": "A-shares CDR daily", "category": "A-Shares Historical"},

    # B-Shares Historical
    "stock_zh_b_daily": {"func": get_stock_zh_b_daily, "desc": "B-shares daily data", "category": "B-Shares Historical"},
    "stock_zh_b_minute": {"func": get_stock_zh_b_minute, "desc": "B-shares minute data", "category": "B-Shares Historical"},

    # HK Historical
    "stock_hk_hist": {"func": get_stock_hk_hist, "desc": "HK stocks historical", "category": "HK Historical"},
    "stock_hk_hist_min_em": {"func": get_stock_hk_hist_min_em, "desc": "HK stocks minute data", "category": "HK Historical"},
    "stock_hk_daily": {"func": get_stock_hk_daily, "desc": "HK stocks daily", "category": "HK Historical"},

    # US Historical
    "stock_us_hist": {"func": get_stock_us_hist, "desc": "US stocks historical", "category": "US Historical"},
    "stock_us_hist_min_em": {"func": get_stock_us_hist_min_em, "desc": "US stocks minute data", "category": "US Historical"},
    "stock_us_daily": {"func": get_stock_us_daily, "desc": "US stocks daily", "category": "US Historical"},

    # A+H Historical
    "stock_zh_ah_daily": {"func": get_stock_zh_ah_daily, "desc": "A+H shares daily", "category": "A+H Historical"},

    # Index Historical
    "stock_zh_index_daily": {"func": get_stock_zh_index_daily, "desc": "Chinese index daily", "category": "Index Historical"},
    "stock_zh_index_daily_em": {"func": get_stock_zh_index_daily_em, "desc": "Chinese index daily (EastMoney)", "category": "Index Historical"},
    "stock_zh_index_daily_tx": {"func": get_stock_zh_index_daily_tx, "desc": "Chinese index daily (Tencent)", "category": "Index Historical"},
    "stock_zh_index_hist_csindex": {"func": get_stock_zh_index_hist_csindex, "desc": "CSIndex historical", "category": "Index Historical"},
    "stock_zh_index_value_csindex": {"func": get_stock_zh_index_value_csindex, "desc": "CSIndex value", "category": "Index Historical"},
    "stock_hk_index_daily_em": {"func": get_stock_hk_index_daily_em, "desc": "HK index daily (EastMoney)", "category": "Index Historical"},
    "stock_hk_index_daily_sina": {"func": get_stock_hk_index_daily_sina, "desc": "HK index daily (Sina)", "category": "Index Historical"},

    # STAR Market Historical
    "stock_zh_kcb_daily": {"func": get_stock_zh_kcb_daily, "desc": "STAR Market daily", "category": "STAR Market Historical"},
    "stock_zh_kcb_report_em": {"func": get_stock_zh_kcb_report_em, "desc": "STAR Market report", "category": "STAR Market Historical"},

    # Intraday
    "stock_intraday_em": {"func": get_stock_intraday_em, "desc": "Intraday data (EastMoney)", "category": "Intraday"},
    "stock_intraday_sina": {"func": get_stock_intraday_sina, "desc": "Intraday data (Sina)", "category": "Intraday"},

    # Tick Data
    "stock_zh_a_tick_tx_js": {"func": get_stock_zh_a_tick_tx_js, "desc": "A-shares tick data", "category": "Tick Data"},

    # Price Data
    "stock_price_js": {"func": get_stock_price_js, "desc": "Stock price data", "category": "Price Data"},

    # Bid-Ask
    "stock_bid_ask_em": {"func": get_stock_bid_ask_em, "desc": "Bid-ask data", "category": "Bid-Ask"},

    # Sector Historical
    "stock_sector_detail": {"func": get_stock_sector_detail, "desc": "Sector detail data", "category": "Sector Data"},
    "stock_sector_spot": {"func": get_stock_sector_spot, "desc": "Sector spot data", "category": "Sector Data"},
    "stock_sector_fund_flow_hist": {"func": get_stock_sector_fund_flow_hist, "desc": "Sector fund flow history", "category": "Sector Data"},
    "stock_sector_fund_flow_rank": {"func": get_stock_sector_fund_flow_rank, "desc": "Sector fund flow rank", "category": "Sector Data"},
    "stock_sector_fund_flow_summary": {"func": get_stock_sector_fund_flow_summary, "desc": "Sector fund flow summary", "category": "Sector Data"},

    # Classification
    "stock_classify_sina": {"func": get_stock_classify_sina, "desc": "Stock classification (Sina)", "category": "Classification"},

    # SSE/SZSE Summary
    "stock_sse_summary": {"func": get_stock_sse_summary, "desc": "SSE market summary", "category": "Exchange Summary"},
    "stock_sse_deal_daily": {"func": get_stock_sse_deal_daily, "desc": "SSE daily deal", "category": "Exchange Summary"},
    "stock_szse_summary": {"func": get_stock_szse_summary, "desc": "SZSE market summary", "category": "Exchange Summary"},
    "stock_szse_area_summary": {"func": get_stock_szse_area_summary, "desc": "SZSE area summary", "category": "Exchange Summary"},
    "stock_szse_sector_summary": {"func": get_stock_szse_sector_summary, "desc": "SZSE sector summary", "category": "Exchange Summary"},

    # Valuation
    "stock_zh_valuation_baidu": {"func": get_stock_zh_valuation_baidu, "desc": "Stock valuation (Baidu)", "category": "Valuation"},
    "stock_hk_valuation_baidu": {"func": get_stock_hk_valuation_baidu, "desc": "HK valuation (Baidu)", "category": "Valuation"},
    "stock_value_em": {"func": get_stock_value_em, "desc": "Stock value (EastMoney)", "category": "Valuation"},

    # Comparison
    "stock_zh_ab_comparison_em": {"func": get_stock_zh_ab_comparison_em, "desc": "A-B shares comparison", "category": "Comparison"},
    "stock_zh_dupont_comparison_em": {"func": get_stock_zh_dupont_comparison_em, "desc": "Dupont comparison", "category": "Comparison"},
    "stock_zh_growth_comparison_em": {"func": get_stock_zh_growth_comparison_em, "desc": "Growth comparison", "category": "Comparison"},
    "stock_zh_scale_comparison_em": {"func": get_stock_zh_scale_comparison_em, "desc": "Scale comparison", "category": "Comparison"},
    "stock_zh_valuation_comparison_em": {"func": get_stock_zh_valuation_comparison_em, "desc": "Valuation comparison", "category": "Comparison"},
    "stock_hk_growth_comparison_em": {"func": get_stock_hk_growth_comparison_em, "desc": "HK growth comparison", "category": "Comparison"},
    "stock_hk_scale_comparison_em": {"func": get_stock_hk_scale_comparison_em, "desc": "HK scale comparison", "category": "Comparison"},
    "stock_hk_valuation_comparison_em": {"func": get_stock_hk_valuation_comparison_em, "desc": "HK valuation comparison", "category": "Comparison"},
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
