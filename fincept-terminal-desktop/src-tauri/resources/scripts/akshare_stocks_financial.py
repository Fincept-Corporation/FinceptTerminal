#!/usr/bin/env python3
"""
AKShare Stocks Financial Data Wrapper (Batch 3)
Provides access to financial statements, analysis, and reports
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


# ==================== BALANCE SHEET ====================

def get_stock_balance_sheet_by_report_em(symbol="SH600519"):
    """Get balance sheet by report (EastMoney)"""
    return safe_call(ak.stock_balance_sheet_by_report_em, symbol=symbol)

def get_stock_balance_sheet_by_yearly_em(symbol="SH600519"):
    """Get balance sheet by yearly (EastMoney)"""
    return safe_call(ak.stock_balance_sheet_by_yearly_em, symbol=symbol)

def get_stock_balance_sheet_by_report_delisted_em(symbol="SH600432"):
    """Get delisted company balance sheet"""
    return safe_call(ak.stock_balance_sheet_by_report_delisted_em, symbol=symbol)

def get_stock_zcfz_em(date="20231231"):
    """Get balance sheet data (EastMoney)"""
    return safe_call(ak.stock_zcfz_em, date=date)

def get_stock_zcfz_bj_em(date="20231231"):
    """Get Beijing exchange balance sheet"""
    return safe_call(ak.stock_zcfz_bj_em, date=date)


# ==================== INCOME STATEMENT ====================

def get_stock_profit_sheet_by_report_em(symbol="SH600519"):
    """Get profit sheet by report (EastMoney)"""
    return safe_call(ak.stock_profit_sheet_by_report_em, symbol=symbol)

def get_stock_profit_sheet_by_yearly_em(symbol="SH600519"):
    """Get profit sheet by yearly (EastMoney)"""
    return safe_call(ak.stock_profit_sheet_by_yearly_em, symbol=symbol)

def get_stock_profit_sheet_by_quarterly_em(symbol="SH600519"):
    """Get profit sheet by quarterly (EastMoney)"""
    return safe_call(ak.stock_profit_sheet_by_quarterly_em, symbol=symbol)

def get_stock_profit_sheet_by_report_delisted_em(symbol="SH600432"):
    """Get delisted company profit sheet"""
    return safe_call(ak.stock_profit_sheet_by_report_delisted_em, symbol=symbol)

def get_stock_lrb_em(date="20231231"):
    """Get income statement data (EastMoney)"""
    return safe_call(ak.stock_lrb_em, date=date)


# ==================== CASH FLOW ====================

def get_stock_cash_flow_sheet_by_report_em(symbol="SH600519"):
    """Get cash flow sheet by report (EastMoney)"""
    return safe_call(ak.stock_cash_flow_sheet_by_report_em, symbol=symbol)

def get_stock_cash_flow_sheet_by_yearly_em(symbol="SH600519"):
    """Get cash flow sheet by yearly (EastMoney)"""
    return safe_call(ak.stock_cash_flow_sheet_by_yearly_em, symbol=symbol)

def get_stock_cash_flow_sheet_by_quarterly_em(symbol="SH600519"):
    """Get cash flow sheet by quarterly (EastMoney)"""
    return safe_call(ak.stock_cash_flow_sheet_by_quarterly_em, symbol=symbol)

def get_stock_cash_flow_sheet_by_report_delisted_em(symbol="SH600432"):
    """Get delisted company cash flow"""
    return safe_call(ak.stock_cash_flow_sheet_by_report_delisted_em, symbol=symbol)

def get_stock_xjll_em(date="20231231"):
    """Get cash flow data (EastMoney)"""
    return safe_call(ak.stock_xjll_em, date=date)


# ==================== FINANCIAL ANALYSIS ====================

def get_stock_financial_analysis_indicator(symbol="600519", start_year="2020"):
    """Get financial analysis indicators"""
    return safe_call(ak.stock_financial_analysis_indicator, symbol=symbol, start_year=start_year)

def get_stock_financial_analysis_indicator_em(symbol="600519"):
    """Get financial analysis indicators (EastMoney)"""
    return safe_call(ak.stock_financial_analysis_indicator_em, symbol=symbol)

def get_stock_financial_abstract(symbol="600519"):
    """Get financial abstract"""
    return safe_call(ak.stock_financial_abstract, symbol=symbol)

def get_stock_financial_abstract_ths(symbol="600519", indicator="按报告期"):
    """Get financial abstract (THS)"""
    return safe_call(ak.stock_financial_abstract_ths, symbol=symbol, indicator=indicator)

def get_stock_financial_report_sina(stock="sh600519", symbol="资产负债表"):
    """Get financial report (Sina)"""
    return safe_call(ak.stock_financial_report_sina, stock=stock, symbol=symbol)


# ==================== HK FINANCIAL ====================

def get_stock_financial_hk_report_em(stock="00700", symbol="资产负债表", date=""):
    """Get HK financial report (EastMoney)"""
    return safe_call(ak.stock_financial_hk_report_em, stock=stock, symbol=symbol, date=date)

def get_stock_financial_hk_analysis_indicator_em(symbol="00700"):
    """Get HK financial analysis indicators (EastMoney)"""
    return safe_call(ak.stock_financial_hk_analysis_indicator_em, symbol=symbol)

def get_stock_hk_financial_indicator_em(symbol="00700"):
    """Get HK financial indicators (EastMoney)"""
    return safe_call(ak.stock_hk_financial_indicator_em, symbol=symbol)


# ==================== US FINANCIAL ====================

def get_stock_financial_us_report_em(stock="AAPL", symbol="资产负债表", date=""):
    """Get US financial report (EastMoney)"""
    return safe_call(ak.stock_financial_us_report_em, stock=stock, symbol=symbol, date=date)

def get_stock_financial_us_analysis_indicator_em(symbol="AAPL"):
    """Get US financial analysis indicators (EastMoney)"""
    return safe_call(ak.stock_financial_us_analysis_indicator_em, symbol=symbol)


# ==================== THS FINANCIAL ====================

def get_stock_financial_benefit_ths(symbol="600519", indicator="按报告期"):
    """Get financial benefit (THS)"""
    return safe_call(ak.stock_financial_benefit_ths, symbol=symbol, indicator=indicator)

def get_stock_financial_cash_ths(symbol="600519", indicator="按报告期"):
    """Get financial cash (THS)"""
    return safe_call(ak.stock_financial_cash_ths, symbol=symbol, indicator=indicator)

def get_stock_financial_debt_ths(symbol="600519", indicator="按报告期"):
    """Get financial debt (THS)"""
    return safe_call(ak.stock_financial_debt_ths, symbol=symbol, indicator=indicator)


# ==================== EARNINGS ====================

def get_stock_yjbb_em(date="20231231"):
    """Get earnings report (EastMoney)"""
    return safe_call(ak.stock_yjbb_em, date=date)

def get_stock_yjkb_em(date="20231231"):
    """Get earnings express (EastMoney)"""
    return safe_call(ak.stock_yjkb_em, date=date)

def get_stock_yjyg_em(date="20231231"):
    """Get earnings forecast (EastMoney)"""
    return safe_call(ak.stock_yjyg_em, date=date)


# ==================== PROFIT FORECAST ====================

def get_stock_profit_forecast_em(symbol="000001"):
    """Get profit forecast (EastMoney)"""
    return safe_call(ak.stock_profit_forecast_em, symbol=symbol)

def get_stock_profit_forecast_ths(symbol="000001"):
    """Get profit forecast (THS)"""
    return safe_call(ak.stock_profit_forecast_ths, symbol=symbol)

def get_stock_hk_profit_forecast_et(symbol="00700"):
    """Get HK profit forecast"""
    return safe_call(ak.stock_hk_profit_forecast_et, symbol=symbol)


# ==================== DIVIDEND ====================

def get_stock_dividend_cninfo(symbol="000001", start_date="", end_date=""):
    """Get dividend data (CNINFO)"""
    return safe_call(ak.stock_dividend_cninfo, symbol=symbol, start_date=start_date, end_date=end_date)

def get_stock_history_dividend(symbol="000001", indicator="分红"):
    """Get dividend history"""
    return safe_call(ak.stock_history_dividend, symbol=symbol, indicator=indicator)

def get_stock_history_dividend_detail(symbol="000001", indicator="分红", date=""):
    """Get dividend history detail"""
    return safe_call(ak.stock_history_dividend_detail, symbol=symbol, indicator=indicator, date=date)

def get_stock_fhps_em(date="20231231"):
    """Get dividend/bonus data (EastMoney)"""
    return safe_call(ak.stock_fhps_em, date=date)

def get_stock_fhps_detail_em(symbol="000001"):
    """Get dividend/bonus detail (EastMoney)"""
    return safe_call(ak.stock_fhps_detail_em, symbol=symbol)

def get_stock_fhps_detail_ths(symbol="000001"):
    """Get dividend/bonus detail (THS)"""
    return safe_call(ak.stock_fhps_detail_ths, symbol=symbol)

def get_stock_hk_dividend_payout_em(symbol="00700"):
    """Get HK dividend payout (EastMoney)"""
    return safe_call(ak.stock_hk_dividend_payout_em, symbol=symbol)

def get_stock_hk_fhpx_detail_ths(symbol="00700"):
    """Get HK dividend detail (THS)"""
    return safe_call(ak.stock_hk_fhpx_detail_ths, symbol=symbol)


# ==================== COMPANY PROFILE ====================

def get_stock_profile_cninfo(symbol="000001"):
    """Get company profile (CNINFO)"""
    return safe_call(ak.stock_profile_cninfo, symbol=symbol)

def get_stock_hk_company_profile_em(symbol="00700"):
    """Get HK company profile (EastMoney)"""
    return safe_call(ak.stock_hk_company_profile_em, symbol=symbol)

def get_stock_hk_security_profile_em(symbol="00700"):
    """Get HK security profile (EastMoney)"""
    return safe_call(ak.stock_hk_security_profile_em, symbol=symbol)


# ==================== ENDPOINT REGISTRY ====================

ENDPOINTS = {
    # Balance Sheet
    "stock_balance_sheet_by_report_em": {"func": get_stock_balance_sheet_by_report_em, "desc": "Balance sheet by report", "category": "Balance Sheet"},
    "stock_balance_sheet_by_yearly_em": {"func": get_stock_balance_sheet_by_yearly_em, "desc": "Balance sheet by yearly", "category": "Balance Sheet"},
    "stock_balance_sheet_by_report_delisted_em": {"func": get_stock_balance_sheet_by_report_delisted_em, "desc": "Delisted balance sheet", "category": "Balance Sheet"},
    "stock_zcfz_em": {"func": get_stock_zcfz_em, "desc": "Balance sheet data", "category": "Balance Sheet"},
    "stock_zcfz_bj_em": {"func": get_stock_zcfz_bj_em, "desc": "Beijing balance sheet", "category": "Balance Sheet"},

    # Income Statement
    "stock_profit_sheet_by_report_em": {"func": get_stock_profit_sheet_by_report_em, "desc": "Profit sheet by report", "category": "Income Statement"},
    "stock_profit_sheet_by_yearly_em": {"func": get_stock_profit_sheet_by_yearly_em, "desc": "Profit sheet by yearly", "category": "Income Statement"},
    "stock_profit_sheet_by_quarterly_em": {"func": get_stock_profit_sheet_by_quarterly_em, "desc": "Profit sheet by quarterly", "category": "Income Statement"},
    "stock_profit_sheet_by_report_delisted_em": {"func": get_stock_profit_sheet_by_report_delisted_em, "desc": "Delisted profit sheet", "category": "Income Statement"},
    "stock_lrb_em": {"func": get_stock_lrb_em, "desc": "Income statement data", "category": "Income Statement"},

    # Cash Flow
    "stock_cash_flow_sheet_by_report_em": {"func": get_stock_cash_flow_sheet_by_report_em, "desc": "Cash flow by report", "category": "Cash Flow"},
    "stock_cash_flow_sheet_by_yearly_em": {"func": get_stock_cash_flow_sheet_by_yearly_em, "desc": "Cash flow by yearly", "category": "Cash Flow"},
    "stock_cash_flow_sheet_by_quarterly_em": {"func": get_stock_cash_flow_sheet_by_quarterly_em, "desc": "Cash flow by quarterly", "category": "Cash Flow"},
    "stock_cash_flow_sheet_by_report_delisted_em": {"func": get_stock_cash_flow_sheet_by_report_delisted_em, "desc": "Delisted cash flow", "category": "Cash Flow"},
    "stock_xjll_em": {"func": get_stock_xjll_em, "desc": "Cash flow data", "category": "Cash Flow"},

    # Financial Analysis
    "stock_financial_analysis_indicator": {"func": get_stock_financial_analysis_indicator, "desc": "Financial analysis indicators", "category": "Financial Analysis"},
    "stock_financial_analysis_indicator_em": {"func": get_stock_financial_analysis_indicator_em, "desc": "Financial analysis (EastMoney)", "category": "Financial Analysis"},
    "stock_financial_abstract": {"func": get_stock_financial_abstract, "desc": "Financial abstract", "category": "Financial Analysis"},
    "stock_financial_abstract_ths": {"func": get_stock_financial_abstract_ths, "desc": "Financial abstract (THS)", "category": "Financial Analysis"},
    "stock_financial_report_sina": {"func": get_stock_financial_report_sina, "desc": "Financial report (Sina)", "category": "Financial Analysis"},

    # HK Financial
    "stock_financial_hk_report_em": {"func": get_stock_financial_hk_report_em, "desc": "HK financial report", "category": "HK Financial"},
    "stock_financial_hk_analysis_indicator_em": {"func": get_stock_financial_hk_analysis_indicator_em, "desc": "HK financial analysis", "category": "HK Financial"},
    "stock_hk_financial_indicator_em": {"func": get_stock_hk_financial_indicator_em, "desc": "HK financial indicators", "category": "HK Financial"},

    # US Financial
    "stock_financial_us_report_em": {"func": get_stock_financial_us_report_em, "desc": "US financial report", "category": "US Financial"},
    "stock_financial_us_analysis_indicator_em": {"func": get_stock_financial_us_analysis_indicator_em, "desc": "US financial analysis", "category": "US Financial"},

    # THS Financial
    "stock_financial_benefit_ths": {"func": get_stock_financial_benefit_ths, "desc": "Financial benefit (THS)", "category": "THS Financial"},
    "stock_financial_cash_ths": {"func": get_stock_financial_cash_ths, "desc": "Financial cash (THS)", "category": "THS Financial"},
    "stock_financial_debt_ths": {"func": get_stock_financial_debt_ths, "desc": "Financial debt (THS)", "category": "THS Financial"},

    # Earnings
    "stock_yjbb_em": {"func": get_stock_yjbb_em, "desc": "Earnings report", "category": "Earnings"},
    "stock_yjkb_em": {"func": get_stock_yjkb_em, "desc": "Earnings express", "category": "Earnings"},
    "stock_yjyg_em": {"func": get_stock_yjyg_em, "desc": "Earnings forecast", "category": "Earnings"},

    # Profit Forecast
    "stock_profit_forecast_em": {"func": get_stock_profit_forecast_em, "desc": "Profit forecast (EastMoney)", "category": "Profit Forecast"},
    "stock_profit_forecast_ths": {"func": get_stock_profit_forecast_ths, "desc": "Profit forecast (THS)", "category": "Profit Forecast"},
    "stock_hk_profit_forecast_et": {"func": get_stock_hk_profit_forecast_et, "desc": "HK profit forecast", "category": "Profit Forecast"},

    # Dividend
    "stock_dividend_cninfo": {"func": get_stock_dividend_cninfo, "desc": "Dividend data (CNINFO)", "category": "Dividend"},
    "stock_history_dividend": {"func": get_stock_history_dividend, "desc": "Dividend history", "category": "Dividend"},
    "stock_history_dividend_detail": {"func": get_stock_history_dividend_detail, "desc": "Dividend history detail", "category": "Dividend"},
    "stock_fhps_em": {"func": get_stock_fhps_em, "desc": "Dividend/bonus data", "category": "Dividend"},
    "stock_fhps_detail_em": {"func": get_stock_fhps_detail_em, "desc": "Dividend/bonus detail", "category": "Dividend"},
    "stock_fhps_detail_ths": {"func": get_stock_fhps_detail_ths, "desc": "Dividend detail (THS)", "category": "Dividend"},
    "stock_hk_dividend_payout_em": {"func": get_stock_hk_dividend_payout_em, "desc": "HK dividend payout", "category": "Dividend"},
    "stock_hk_fhpx_detail_ths": {"func": get_stock_hk_fhpx_detail_ths, "desc": "HK dividend detail (THS)", "category": "Dividend"},

    # Company Profile
    "stock_profile_cninfo": {"func": get_stock_profile_cninfo, "desc": "Company profile (CNINFO)", "category": "Company Profile"},
    "stock_hk_company_profile_em": {"func": get_stock_hk_company_profile_em, "desc": "HK company profile", "category": "Company Profile"},
    "stock_hk_security_profile_em": {"func": get_stock_hk_security_profile_em, "desc": "HK security profile", "category": "Company Profile"},
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
