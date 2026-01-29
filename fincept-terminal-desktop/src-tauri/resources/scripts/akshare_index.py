#!/usr/bin/env python3
"""
AKShare Index Data Wrapper
Provides access to index data: constituents, weights, historical data, global indices
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
                # Convert timestamps and handle NaN values
                for col in result.columns:
                    if result[col].dtype == 'datetime64[ns]':
                        result[col] = result[col].astype(str)
                # Replace NaN/Infinity with None for valid JSON
                result = result.replace([float('inf'), float('-inf')], None)
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


# ==================== CHINESE INDEX DATA ====================

def get_index_zh_a_hist(symbol="000001", period="daily", start_date="20200101", end_date="20261231"):
    """Get Chinese A-share index historical data"""
    return safe_call(ak.index_zh_a_hist, symbol=symbol, period=period, start_date=start_date, end_date=end_date)

def get_index_zh_a_hist_min(symbol="000001", period="1"):
    """Get Chinese A-share index minute data"""
    return safe_call(ak.index_zh_a_hist_min_em, symbol=symbol, period=period)

def get_index_stock_cons(symbol="000300"):
    """Get index constituent stocks (e.g., CSI 300)"""
    return safe_call(ak.index_stock_cons, symbol=symbol)

def get_index_stock_cons_csindex(symbol="000300"):
    """Get index constituents from CSIndex"""
    return safe_call(ak.index_stock_cons_csindex, symbol=symbol)

def get_index_stock_cons_weight_csindex(symbol="000300"):
    """Get index constituent weights from CSIndex"""
    return safe_call(ak.index_stock_cons_weight_csindex, symbol=symbol)

def get_index_stock_info():
    """Get all index information"""
    return safe_call(ak.index_stock_info)

def get_index_code_id_map():
    """Get index code to ID mapping"""
    return safe_call(ak.index_code_id_map_em)


# ==================== SHENWAN INDEX ====================

def get_index_realtime_sw():
    """Get Shenwan index realtime data"""
    return safe_call(ak.index_realtime_sw)

def get_index_hist_sw(symbol="801010", period="day"):
    """Get Shenwan index historical data"""
    return safe_call(ak.index_hist_sw, symbol=symbol, period=period)

def get_index_min_sw(symbol="801010"):
    """Get Shenwan index minute data"""
    return safe_call(ak.index_min_sw, symbol=symbol)

def get_index_component_sw(symbol="801010"):
    """Get Shenwan index components"""
    return safe_call(ak.index_component_sw, symbol=symbol)

def get_index_analysis_daily_sw(symbol="801010", start_date="20200101", end_date="20261231"):
    """Get Shenwan index daily analysis"""
    return safe_call(ak.index_analysis_daily_sw, symbol=symbol, start_date=start_date, end_date=end_date)

def get_index_analysis_weekly_sw(symbol="801010"):
    """Get Shenwan index weekly analysis"""
    return safe_call(ak.index_analysis_weekly_sw, symbol=symbol)

def get_index_analysis_monthly_sw(symbol="801010"):
    """Get Shenwan index monthly analysis"""
    return safe_call(ak.index_analysis_monthly_sw, symbol=symbol)

def get_index_analysis_week_month_sw(symbol="801010"):
    """Get Shenwan index week/month analysis"""
    return safe_call(ak.index_analysis_week_month_sw, symbol=symbol)

def get_index_realtime_fund_sw():
    """Get Shenwan fund index realtime"""
    return safe_call(ak.index_realtime_fund_sw)

def get_index_hist_fund_sw(symbol="801010", period="day"):
    """Get Shenwan fund index historical"""
    return safe_call(ak.index_hist_fund_sw, symbol=symbol, period=period)


# ==================== CNI INDEX ====================

def get_index_all_cni():
    """Get all CNI indices"""
    return safe_call(ak.index_all_cni)

def get_index_hist_cni(symbol="399001"):
    """Get CNI index historical data"""
    return safe_call(ak.index_hist_cni, symbol=symbol)

def get_index_detail_cni(symbol="399001"):
    """Get CNI index details"""
    return safe_call(ak.index_detail_cni, symbol=symbol)

def get_index_detail_hist_cni(symbol="399001", start_date="20200101", end_date="20261231"):
    """Get CNI index historical details"""
    return safe_call(ak.index_detail_hist_cni, symbol=symbol, date=start_date)

def get_index_detail_hist_adjust_cni(symbol="399001", start_date="20200101"):
    """Get CNI index adjusted historical details"""
    return safe_call(ak.index_detail_hist_adjust_cni, symbol=symbol, date=start_date)


# ==================== GLOBAL INDEX ====================

def get_index_global_spot():
    """Get global index spot data"""
    return safe_call(ak.index_global_spot_em)

def get_index_global_hist(symbol=".DJI", period="daily", start_date="20200101", end_date="20261231"):
    """Get global index historical data (e.g., .DJI, .IXIC, .INX)"""
    return safe_call(ak.index_global_hist_em, symbol=symbol, period=period, start_date=start_date, end_date=end_date)

def get_index_global_name_table():
    """Get global index name table"""
    return safe_call(ak.index_global_name_table)

def get_index_us_stock_sina():
    """Get US stock indices from Sina"""
    return safe_call(ak.index_us_stock_sina)

def get_index_global_hist_sina(symbol="DJI"):
    """Get global index historical data from Sina"""
    return safe_call(ak.index_global_hist_sina, symbol=symbol)

def get_index_stock_cons_sina(symbol="000300"):
    """Get index constituent stocks from Sina"""
    return safe_call(ak.index_stock_cons_sina, symbol=symbol)


# ==================== CS INDEX ====================

def get_index_csindex_all():
    """Get all CS indices"""
    return safe_call(ak.index_csindex_all)


# ==================== OPTION VOLATILITY INDEX (QVIX) ====================

def get_index_option_50etf_qvix():
    """Get 50ETF option QVIX"""
    return safe_call(ak.index_option_50etf_qvix)

def get_index_option_300etf_qvix():
    """Get 300ETF option QVIX"""
    return safe_call(ak.index_option_300etf_qvix)

def get_index_option_500etf_qvix():
    """Get 500ETF option QVIX"""
    return safe_call(ak.index_option_500etf_qvix)

def get_index_option_100etf_qvix():
    """Get 100ETF option QVIX"""
    return safe_call(ak.index_option_100etf_qvix)

def get_index_option_cyb_qvix():
    """Get ChiNext option QVIX"""
    return safe_call(ak.index_option_cyb_qvix)

def get_index_option_kcb_qvix():
    """Get STAR Market option QVIX"""
    return safe_call(ak.index_option_kcb_qvix)

def get_index_option_50index_qvix():
    """Get SSE 50 Index option QVIX"""
    return safe_call(ak.index_option_50index_qvix)

def get_index_option_300index_qvix():
    """Get CSI 300 Index option QVIX"""
    return safe_call(ak.index_option_300index_qvix)

def get_index_option_500etf_min_qvix():
    """Get 500ETF option QVIX minute data"""
    return safe_call(ak.index_option_500etf_min_qvix)

def get_index_option_1000index_qvix():
    """Get CSI 1000 Index option QVIX"""
    return safe_call(ak.index_option_1000index_qvix)

def get_index_option_50etf_min_qvix():
    """Get 50ETF option QVIX minute data"""
    return safe_call(ak.index_option_50etf_min_qvix)

def get_index_option_100etf_min_qvix():
    """Get 100ETF option QVIX minute data"""
    return safe_call(ak.index_option_100etf_min_qvix)

def get_index_option_300etf_min_qvix():
    """Get 300ETF option QVIX minute data"""
    return safe_call(ak.index_option_300etf_min_qvix)

def get_index_option_50index_min_qvix():
    """Get SSE 50 Index option QVIX minute data"""
    return safe_call(ak.index_option_50index_min_qvix)

def get_index_option_300index_min_qvix():
    """Get CSI 300 Index option QVIX minute data"""
    return safe_call(ak.index_option_300index_min_qvix)

def get_index_option_1000index_min_qvix():
    """Get CSI 1000 Index option QVIX minute data"""
    return safe_call(ak.index_option_1000index_min_qvix)

def get_index_option_cyb_min_qvix():
    """Get ChiNext option QVIX minute data"""
    return safe_call(ak.index_option_cyb_min_qvix)

def get_index_option_kcb_min_qvix():
    """Get STAR Market option QVIX minute data"""
    return safe_call(ak.index_option_kcb_min_qvix)


# ==================== ECONOMIC INDICES (CX) ====================

def get_index_pmi_com_cx():
    """Get Composite PMI (Caixin)"""
    return safe_call(ak.index_pmi_com_cx)

def get_index_pmi_man_cx():
    """Get Manufacturing PMI (Caixin)"""
    return safe_call(ak.index_pmi_man_cx)

def get_index_pmi_ser_cx():
    """Get Services PMI (Caixin)"""
    return safe_call(ak.index_pmi_ser_cx)

def get_index_cci_cx():
    """Get Consumer Confidence Index (Caixin)"""
    return safe_call(ak.index_cci_cx)

def get_index_bei_cx():
    """Get Business Environment Index (Caixin)"""
    return safe_call(ak.index_bei_cx)

def get_index_nei_cx():
    """Get New Economy Index (Caixin)"""
    return safe_call(ak.index_nei_cx)

def get_index_li_cx():
    """Get Labor Index (Caixin)"""
    return safe_call(ak.index_li_cx)

def get_index_si_cx():
    """Get Sentiment Index (Caixin)"""
    return safe_call(ak.index_si_cx)

def get_index_fi_cx():
    """Get Finance Index (Caixin)"""
    return safe_call(ak.index_fi_cx)

def get_index_bi_cx():
    """Get Business Index (Caixin)"""
    return safe_call(ak.index_bi_cx)

def get_index_ci_cx():
    """Get Consumption Index (Caixin)"""
    return safe_call(ak.index_ci_cx)

def get_index_ti_cx():
    """Get Technology Index (Caixin)"""
    return safe_call(ak.index_ti_cx)

def get_index_dei_cx():
    """Get Digital Economy Index (Caixin)"""
    return safe_call(ak.index_dei_cx)

def get_index_ii_cx():
    """Get Investment Index (Caixin)"""
    return safe_call(ak.index_ii_cx)

def get_index_neei_cx():
    """Get New Energy Economy Index (Caixin)"""
    return safe_call(ak.index_neei_cx)

def get_index_neaw_cx():
    """Get New Economy Asset Wealth Index (Caixin)"""
    return safe_call(ak.index_neaw_cx)

def get_index_qli_cx():
    """Get Quality of Life Index (Caixin)"""
    return safe_call(ak.index_qli_cx)

def get_index_ai_cx():
    """Get AI Index (Caixin)"""
    return safe_call(ak.index_ai_cx)

def get_index_awpr_cx():
    """Get Asset-Weighted Price Return Index (Caixin)"""
    return safe_call(ak.index_awpr_cx)


# ==================== COMMODITY/INDUSTRY INDICES ====================

def get_index_price_cflp():
    """Get CFLP commodity price index"""
    return safe_call(ak.index_price_cflp)

def get_index_volume_cflp():
    """Get CFLP commodity volume index"""
    return safe_call(ak.index_volume_cflp)

def get_index_hog_spot_price():
    """Get hog spot price index"""
    return safe_call(ak.index_hog_spot_price)

def get_index_sugar_msweet():
    """Get sugar price index (MSWeet)"""
    return safe_call(ak.index_sugar_msweet)

def get_index_inner_quote_sugar_msweet():
    """Get domestic sugar quote index"""
    return safe_call(ak.index_inner_quote_sugar_msweet)

def get_index_outer_quote_sugar_msweet():
    """Get international sugar quote index"""
    return safe_call(ak.index_outer_quote_sugar_msweet)


# ==================== OTHER INDICES ====================

def get_index_eri():
    """Get Economic Recovery Index"""
    return safe_call(ak.index_eri)

def get_index_yw():
    """Get YiWu Small Commodity Index"""
    return safe_call(ak.index_yw)

def get_index_kq_fashion():
    """Get KQ Fashion Index"""
    return safe_call(ak.index_kq_fashion)

def get_index_kq_fz():
    """Get KQ FZ Index"""
    return safe_call(ak.index_kq_fz)

def get_index_news_sentiment_scope():
    """Get news sentiment scope index"""
    return safe_call(ak.index_news_sentiment_scope)

def get_index_bloomberg_billionaires():
    """Get Bloomberg Billionaires Index"""
    return safe_call(ak.index_bloomberg_billionaires)

def get_index_bloomberg_billionaires_hist(symbol="�马化腾"):
    """Get Bloomberg Billionaires historical data"""
    return safe_call(ak.index_bloomberg_billionaires_hist, symbol=symbol)


# ==================== ENDPOINT REGISTRY ====================

ENDPOINTS = {
    # Chinese Index
    "index_zh_a_hist": {"func": get_index_zh_a_hist, "desc": "Chinese A-share index historical data", "category": "Chinese Index"},
    "index_zh_a_hist_min": {"func": get_index_zh_a_hist_min, "desc": "Chinese A-share index minute data", "category": "Chinese Index"},
    "index_stock_cons": {"func": get_index_stock_cons, "desc": "Index constituent stocks", "category": "Chinese Index"},
    "index_stock_cons_csindex": {"func": get_index_stock_cons_csindex, "desc": "Index constituents from CSIndex", "category": "Chinese Index"},
    "index_stock_cons_weight": {"func": get_index_stock_cons_weight_csindex, "desc": "Index constituent weights", "category": "Chinese Index"},
    "index_stock_info": {"func": get_index_stock_info, "desc": "All index information", "category": "Chinese Index"},
    "index_code_map": {"func": get_index_code_id_map, "desc": "Index code to ID mapping", "category": "Chinese Index"},

    # Shenwan Index
    "index_realtime_sw": {"func": get_index_realtime_sw, "desc": "Shenwan index realtime", "category": "Shenwan Index"},
    "index_hist_sw": {"func": get_index_hist_sw, "desc": "Shenwan index historical", "category": "Shenwan Index"},
    "index_min_sw": {"func": get_index_min_sw, "desc": "Shenwan index minute data", "category": "Shenwan Index"},
    "index_component_sw": {"func": get_index_component_sw, "desc": "Shenwan index components", "category": "Shenwan Index"},
    "index_analysis_daily_sw": {"func": get_index_analysis_daily_sw, "desc": "Shenwan daily analysis", "category": "Shenwan Index"},
    "index_analysis_weekly_sw": {"func": get_index_analysis_weekly_sw, "desc": "Shenwan weekly analysis", "category": "Shenwan Index"},
    "index_analysis_monthly_sw": {"func": get_index_analysis_monthly_sw, "desc": "Shenwan monthly analysis", "category": "Shenwan Index"},
    "index_analysis_week_month_sw": {"func": get_index_analysis_week_month_sw, "desc": "Shenwan week/month analysis", "category": "Shenwan Index"},
    "index_realtime_fund_sw": {"func": get_index_realtime_fund_sw, "desc": "Shenwan fund index realtime", "category": "Shenwan Index"},
    "index_hist_fund_sw": {"func": get_index_hist_fund_sw, "desc": "Shenwan fund index historical", "category": "Shenwan Index"},

    # CNI Index
    "index_all_cni": {"func": get_index_all_cni, "desc": "All CNI indices", "category": "CNI Index"},
    "index_hist_cni": {"func": get_index_hist_cni, "desc": "CNI index historical", "category": "CNI Index"},
    "index_detail_cni": {"func": get_index_detail_cni, "desc": "CNI index details", "category": "CNI Index"},
    "index_detail_hist_cni": {"func": get_index_detail_hist_cni, "desc": "CNI index historical details", "category": "CNI Index"},
    "index_detail_hist_adjust_cni": {"func": get_index_detail_hist_adjust_cni, "desc": "CNI adjusted hist details", "category": "CNI Index"},

    # Global Index
    "index_global_spot": {"func": get_index_global_spot, "desc": "Global index spot data", "category": "Global Index"},
    "index_global_hist": {"func": get_index_global_hist, "desc": "Global index historical (DJI, IXIC, etc.)", "category": "Global Index"},
    "index_global_name_table": {"func": get_index_global_name_table, "desc": "Global index name table", "category": "Global Index"},
    "index_us_stock": {"func": get_index_us_stock_sina, "desc": "US stock indices", "category": "Global Index"},
    "index_global_hist_sina": {"func": get_index_global_hist_sina, "desc": "Global index hist (Sina)", "category": "Global Index"},
    "index_stock_cons_sina": {"func": get_index_stock_cons_sina, "desc": "Index constituents (Sina)", "category": "Global Index"},
    "index_csindex_all": {"func": get_index_csindex_all, "desc": "All CS indices", "category": "Global Index"},

    # Option QVIX
    "index_qvix_50etf": {"func": get_index_option_50etf_qvix, "desc": "50ETF option QVIX", "category": "Option QVIX"},
    "index_qvix_300etf": {"func": get_index_option_300etf_qvix, "desc": "300ETF option QVIX", "category": "Option QVIX"},
    "index_qvix_500etf": {"func": get_index_option_500etf_qvix, "desc": "500ETF option QVIX", "category": "Option QVIX"},
    "index_qvix_100etf": {"func": get_index_option_100etf_qvix, "desc": "100ETF option QVIX", "category": "Option QVIX"},
    "index_qvix_cyb": {"func": get_index_option_cyb_qvix, "desc": "ChiNext option QVIX", "category": "Option QVIX"},
    "index_qvix_kcb": {"func": get_index_option_kcb_qvix, "desc": "STAR Market option QVIX", "category": "Option QVIX"},
    "index_qvix_50index": {"func": get_index_option_50index_qvix, "desc": "SSE 50 Index QVIX", "category": "Option QVIX"},
    "index_qvix_300index": {"func": get_index_option_300index_qvix, "desc": "CSI 300 Index QVIX", "category": "Option QVIX"},
    "index_qvix_1000index": {"func": get_index_option_1000index_qvix, "desc": "CSI 1000 Index QVIX", "category": "Option QVIX"},
    "index_qvix_50etf_min": {"func": get_index_option_50etf_min_qvix, "desc": "50ETF QVIX minute", "category": "Option QVIX"},
    "index_qvix_100etf_min": {"func": get_index_option_100etf_min_qvix, "desc": "100ETF QVIX minute", "category": "Option QVIX"},
    "index_qvix_300etf_min": {"func": get_index_option_300etf_min_qvix, "desc": "300ETF QVIX minute", "category": "Option QVIX"},
    "index_qvix_50index_min": {"func": get_index_option_50index_min_qvix, "desc": "SSE 50 QVIX minute", "category": "Option QVIX"},
    "index_qvix_300index_min": {"func": get_index_option_300index_min_qvix, "desc": "CSI 300 QVIX minute", "category": "Option QVIX"},
    "index_qvix_1000index_min": {"func": get_index_option_1000index_min_qvix, "desc": "CSI 1000 QVIX minute", "category": "Option QVIX"},
    "index_qvix_cyb_min": {"func": get_index_option_cyb_min_qvix, "desc": "ChiNext QVIX minute", "category": "Option QVIX"},
    "index_qvix_kcb_min": {"func": get_index_option_kcb_min_qvix, "desc": "STAR QVIX minute", "category": "Option QVIX"},

    # Economic Indices (Caixin)
    "index_pmi_composite": {"func": get_index_pmi_com_cx, "desc": "Composite PMI (Caixin)", "category": "Economic Index"},
    "index_pmi_manufacturing": {"func": get_index_pmi_man_cx, "desc": "Manufacturing PMI (Caixin)", "category": "Economic Index"},
    "index_pmi_services": {"func": get_index_pmi_ser_cx, "desc": "Services PMI (Caixin)", "category": "Economic Index"},
    "index_consumer_confidence": {"func": get_index_cci_cx, "desc": "Consumer Confidence Index", "category": "Economic Index"},
    "index_business_environment": {"func": get_index_bei_cx, "desc": "Business Environment Index", "category": "Economic Index"},
    "index_new_economy": {"func": get_index_nei_cx, "desc": "New Economy Index", "category": "Economic Index"},
    "index_labor": {"func": get_index_li_cx, "desc": "Labor Index", "category": "Economic Index"},
    "index_sentiment": {"func": get_index_si_cx, "desc": "Sentiment Index", "category": "Economic Index"},
    "index_finance": {"func": get_index_fi_cx, "desc": "Finance Index", "category": "Economic Index"},
    "index_business": {"func": get_index_bi_cx, "desc": "Business Index", "category": "Economic Index"},
    "index_consumption": {"func": get_index_ci_cx, "desc": "Consumption Index", "category": "Economic Index"},
    "index_technology": {"func": get_index_ti_cx, "desc": "Technology Index", "category": "Economic Index"},
    "index_digital_economy": {"func": get_index_dei_cx, "desc": "Digital Economy Index", "category": "Economic Index"},
    "index_investment": {"func": get_index_ii_cx, "desc": "Investment Index", "category": "Economic Index"},
    "index_new_energy": {"func": get_index_neei_cx, "desc": "New Energy Economy Index", "category": "Economic Index"},
    "index_quality_life": {"func": get_index_qli_cx, "desc": "Quality of Life Index", "category": "Economic Index"},
    "index_ai": {"func": get_index_ai_cx, "desc": "AI Index", "category": "Economic Index"},

    # Commodity Indices
    "index_cflp_price": {"func": get_index_price_cflp, "desc": "CFLP commodity price index", "category": "Commodity Index"},
    "index_cflp_volume": {"func": get_index_volume_cflp, "desc": "CFLP commodity volume index", "category": "Commodity Index"},
    "index_hog_price": {"func": get_index_hog_spot_price, "desc": "Hog spot price index", "category": "Commodity Index"},
    "index_sugar": {"func": get_index_sugar_msweet, "desc": "Sugar price index", "category": "Commodity Index"},
    "index_sugar_domestic": {"func": get_index_inner_quote_sugar_msweet, "desc": "Domestic sugar quote", "category": "Commodity Index"},
    "index_sugar_international": {"func": get_index_outer_quote_sugar_msweet, "desc": "International sugar quote", "category": "Commodity Index"},

    # Other Indices
    "index_economic_recovery": {"func": get_index_eri, "desc": "Economic Recovery Index", "category": "Other Index"},
    "index_yiwu": {"func": get_index_yw, "desc": "YiWu Small Commodity Index", "category": "Other Index"},
    "index_fashion": {"func": get_index_kq_fashion, "desc": "KQ Fashion Index", "category": "Other Index"},
    "index_news_sentiment": {"func": get_index_news_sentiment_scope, "desc": "News sentiment index", "category": "Other Index"},
    "index_billionaires": {"func": get_index_bloomberg_billionaires, "desc": "Bloomberg Billionaires", "category": "Other Index"},
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
