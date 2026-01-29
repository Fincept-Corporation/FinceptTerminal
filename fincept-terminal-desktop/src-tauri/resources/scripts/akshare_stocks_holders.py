#!/usr/bin/env python3
"""
AKShare Stocks Shareholders & Holdings Data Wrapper (Batch 4)
Provides access to shareholder data, holdings, institutional ownership
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


# ==================== MAJOR SHAREHOLDERS ====================

def get_stock_main_stock_holder(symbol="000001"):
    """Get main stock holders"""
    return safe_call(ak.stock_main_stock_holder, stock=symbol)

def get_stock_circulate_stock_holder(symbol="000001"):
    """Get circulating stock holders"""
    return safe_call(ak.stock_circulate_stock_holder, symbol=symbol)

def get_stock_zh_a_gdhs(symbol="000001"):
    """Get A-shares shareholder count"""
    return safe_call(ak.stock_zh_a_gdhs, symbol=symbol)

def get_stock_zh_a_gdhs_detail_em(symbol="000001"):
    """Get A-shares shareholder count detail (EastMoney)"""
    return safe_call(ak.stock_zh_a_gdhs_detail_em, symbol=symbol)

def get_stock_zh_a_gbjg_em(symbol="000001"):
    """Get A-shares shareholding structure (EastMoney)"""
    return safe_call(ak.stock_zh_a_gbjg_em, symbol=symbol)


# ==================== GDFX (股东分析) ====================

def get_stock_gdfx_top_10_em(symbol="000001"):
    """Get top 10 shareholders (EastMoney)"""
    return safe_call(ak.stock_gdfx_top_10_em, symbol=symbol)

def get_stock_gdfx_free_top_10_em(symbol="000001"):
    """Get top 10 free float shareholders (EastMoney)"""
    return safe_call(ak.stock_gdfx_free_top_10_em, symbol=symbol)

def get_stock_gdfx_holding_analyse_em(date="20231231"):
    """Get shareholder holding analysis (EastMoney)"""
    return safe_call(ak.stock_gdfx_holding_analyse_em, date=date)

def get_stock_gdfx_holding_change_em(symbol="000001"):
    """Get shareholder holding change (EastMoney)"""
    return safe_call(ak.stock_gdfx_holding_change_em, symbol=symbol)

def get_stock_gdfx_holding_detail_em(symbol="000001"):
    """Get shareholder holding detail (EastMoney)"""
    return safe_call(ak.stock_gdfx_holding_detail_em, symbol=symbol)

def get_stock_gdfx_holding_statistics_em(symbol="000001"):
    """Get shareholder holding statistics (EastMoney)"""
    return safe_call(ak.stock_gdfx_holding_statistics_em, symbol=symbol)

def get_stock_gdfx_holding_teamwork_em(symbol="000001"):
    """Get shareholder teamwork (EastMoney)"""
    return safe_call(ak.stock_gdfx_holding_teamwork_em, symbol=symbol)

def get_stock_gdfx_free_holding_analyse_em(date="20231231"):
    """Get free float shareholder analysis (EastMoney)"""
    return safe_call(ak.stock_gdfx_free_holding_analyse_em, date=date)

def get_stock_gdfx_free_holding_change_em(symbol="000001"):
    """Get free float shareholder change (EastMoney)"""
    return safe_call(ak.stock_gdfx_free_holding_change_em, symbol=symbol)

def get_stock_gdfx_free_holding_detail_em(symbol="000001"):
    """Get free float shareholder detail (EastMoney)"""
    return safe_call(ak.stock_gdfx_free_holding_detail_em, symbol=symbol)

def get_stock_gdfx_free_holding_statistics_em(symbol="000001"):
    """Get free float shareholder statistics (EastMoney)"""
    return safe_call(ak.stock_gdfx_free_holding_statistics_em, symbol=symbol)

def get_stock_gdfx_free_holding_teamwork_em(symbol="000001"):
    """Get free float shareholder teamwork (EastMoney)"""
    return safe_call(ak.stock_gdfx_free_holding_teamwork_em, symbol=symbol)


# ==================== SHAREHOLDER CHANGES ====================

def get_stock_shareholder_change_ths(symbol="000001"):
    """Get shareholder change (THS)"""
    return safe_call(ak.stock_shareholder_change_ths, symbol=symbol)

def get_stock_gddh_em():
    """Get shareholder appointment (EastMoney)"""
    return safe_call(ak.stock_gddh_em)


# ==================== FUND HOLDINGS ====================

def get_stock_fund_stock_holder(symbol="000001"):
    """Get fund stock holdings"""
    return safe_call(ak.stock_fund_stock_holder, symbol=symbol)

def get_stock_report_fund_hold(symbol="000001", date=""):
    """Get fund holding report"""
    return safe_call(ak.stock_report_fund_hold, symbol=symbol, date=date)

def get_stock_report_fund_hold_detail(symbol="000001", date=""):
    """Get fund holding detail"""
    return safe_call(ak.stock_report_fund_hold_detail, symbol=symbol, date=date)


# ==================== INSTITUTIONAL HOLDINGS ====================

def get_stock_institute_hold(symbol="20231231"):
    """Get institutional holdings"""
    return safe_call(ak.stock_institute_hold, quarter=symbol)

def get_stock_institute_hold_detail(stock="000001", quarter=""):
    """Get institutional holding detail"""
    return safe_call(ak.stock_institute_hold_detail, stock=stock, quarter=quarter)


# ==================== HOLD MANAGEMENT ====================

def get_stock_hold_management_detail_cninfo(symbol="000001"):
    """Get management holding detail (CNINFO)"""
    return safe_call(ak.stock_hold_management_detail_cninfo, symbol=symbol)

def get_stock_hold_management_detail_em(symbol="000001"):
    """Get management holding detail (EastMoney)"""
    return safe_call(ak.stock_hold_management_detail_em, symbol=symbol)

def get_stock_hold_management_person_em():
    """Get management holding person (EastMoney)"""
    return safe_call(ak.stock_hold_management_person_em)

def get_stock_hold_num_cninfo(date="2024-01-01"):
    """Get shareholder number (CNINFO)"""
    return safe_call(ak.stock_hold_num_cninfo, date=date)

def get_stock_hold_control_cninfo(symbol="000001"):
    """Get actual controller (CNINFO)"""
    return safe_call(ak.stock_hold_control_cninfo, symbol=symbol)

def get_stock_hold_change_cninfo(symbol="增持", start_date="2024-01-01", end_date="2024-12-31"):
    """Get holding change (CNINFO)"""
    return safe_call(ak.stock_hold_change_cninfo, symbol=symbol, start_date=start_date, end_date=end_date)


# ==================== SHARE CHANGES ====================

def get_stock_share_change_cninfo(symbol="000001", start_date="", end_date=""):
    """Get share change (CNINFO)"""
    return safe_call(ak.stock_share_change_cninfo, symbol=symbol, start_date=start_date, end_date=end_date)

def get_stock_share_hold_change_bse():
    """Get BSE share holding change"""
    return safe_call(ak.stock_share_hold_change_bse)

def get_stock_share_hold_change_sse():
    """Get SSE share holding change"""
    return safe_call(ak.stock_share_hold_change_sse)

def get_stock_share_hold_change_szse():
    """Get SZSE share holding change"""
    return safe_call(ak.stock_share_hold_change_szse)


# ==================== GGCG (高管持股) ====================

def get_stock_ggcg_em(symbol="全部"):
    """Get executive shareholding (EastMoney)"""
    return safe_call(ak.stock_ggcg_em, symbol=symbol)


# ==================== RESTRICTED SHARES ====================

def get_stock_restricted_release_summary_em():
    """Get restricted release summary (EastMoney)"""
    return safe_call(ak.stock_restricted_release_summary_em)

def get_stock_restricted_release_queue_em(symbol="全部"):
    """Get restricted release queue (EastMoney)"""
    return safe_call(ak.stock_restricted_release_queue_em, symbol=symbol)

def get_stock_restricted_release_queue_sina(symbol="全部"):
    """Get restricted release queue (Sina)"""
    return safe_call(ak.stock_restricted_release_queue_sina, symbol=symbol)

def get_stock_restricted_release_detail_em(symbol="全部"):
    """Get restricted release detail (EastMoney)"""
    return safe_call(ak.stock_restricted_release_detail_em, symbol=symbol)

def get_stock_restricted_release_stockholder_em(symbol="全部"):
    """Get restricted release stockholder (EastMoney)"""
    return safe_call(ak.stock_restricted_release_stockholder_em, symbol=symbol)


# ==================== PLEDGE ====================

def get_stock_gpzy_profile_em():
    """Get equity pledge profile (EastMoney)"""
    return safe_call(ak.stock_gpzy_profile_em)

def get_stock_gpzy_pledge_ratio_em(date="20231231"):
    """Get equity pledge ratio (EastMoney)"""
    return safe_call(ak.stock_gpzy_pledge_ratio_em, date=date)

def get_stock_gpzy_pledge_ratio_detail_em(symbol="000001"):
    """Get equity pledge ratio detail (EastMoney)"""
    return safe_call(ak.stock_gpzy_pledge_ratio_detail_em, symbol=symbol)

def get_stock_gpzy_distribute_statistics_bank_em():
    """Get equity pledge bank statistics (EastMoney)"""
    return safe_call(ak.stock_gpzy_distribute_statistics_bank_em)

def get_stock_gpzy_distribute_statistics_company_em():
    """Get equity pledge company statistics (EastMoney)"""
    return safe_call(ak.stock_gpzy_distribute_statistics_company_em)

def get_stock_gpzy_industry_data_em():
    """Get equity pledge industry data (EastMoney)"""
    return safe_call(ak.stock_gpzy_industry_data_em)


# ==================== CNINFO HOLDINGS ====================

def get_stock_cg_equity_mortgage_cninfo(symbol="000001"):
    """Get equity mortgage (CNINFO)"""
    return safe_call(ak.stock_cg_equity_mortgage_cninfo, symbol=symbol)

def get_stock_cg_guarantee_cninfo(symbol="000001"):
    """Get guarantee data (CNINFO)"""
    return safe_call(ak.stock_cg_guarantee_cninfo, symbol=symbol)

def get_stock_cg_lawsuit_cninfo(symbol="000001"):
    """Get lawsuit data (CNINFO)"""
    return safe_call(ak.stock_cg_lawsuit_cninfo, symbol=symbol)


# ==================== ENDPOINT REGISTRY ====================

ENDPOINTS = {
    # Major Shareholders
    "stock_main_stock_holder": {"func": get_stock_main_stock_holder, "desc": "Main stock holders", "category": "Major Shareholders"},
    "stock_circulate_stock_holder": {"func": get_stock_circulate_stock_holder, "desc": "Circulating stock holders", "category": "Major Shareholders"},
    "stock_zh_a_gdhs": {"func": get_stock_zh_a_gdhs, "desc": "A-shares shareholder count", "category": "Major Shareholders"},
    "stock_zh_a_gdhs_detail_em": {"func": get_stock_zh_a_gdhs_detail_em, "desc": "Shareholder count detail", "category": "Major Shareholders"},
    "stock_zh_a_gbjg_em": {"func": get_stock_zh_a_gbjg_em, "desc": "Shareholding structure", "category": "Major Shareholders"},

    # GDFX (股东分析)
    "stock_gdfx_top_10_em": {"func": get_stock_gdfx_top_10_em, "desc": "Top 10 shareholders", "category": "Shareholder Analysis"},
    "stock_gdfx_free_top_10_em": {"func": get_stock_gdfx_free_top_10_em, "desc": "Top 10 free float shareholders", "category": "Shareholder Analysis"},
    "stock_gdfx_holding_analyse_em": {"func": get_stock_gdfx_holding_analyse_em, "desc": "Holding analysis", "category": "Shareholder Analysis"},
    "stock_gdfx_holding_change_em": {"func": get_stock_gdfx_holding_change_em, "desc": "Holding change", "category": "Shareholder Analysis"},
    "stock_gdfx_holding_detail_em": {"func": get_stock_gdfx_holding_detail_em, "desc": "Holding detail", "category": "Shareholder Analysis"},
    "stock_gdfx_holding_statistics_em": {"func": get_stock_gdfx_holding_statistics_em, "desc": "Holding statistics", "category": "Shareholder Analysis"},
    "stock_gdfx_holding_teamwork_em": {"func": get_stock_gdfx_holding_teamwork_em, "desc": "Shareholder teamwork", "category": "Shareholder Analysis"},
    "stock_gdfx_free_holding_analyse_em": {"func": get_stock_gdfx_free_holding_analyse_em, "desc": "Free float analysis", "category": "Shareholder Analysis"},
    "stock_gdfx_free_holding_change_em": {"func": get_stock_gdfx_free_holding_change_em, "desc": "Free float change", "category": "Shareholder Analysis"},
    "stock_gdfx_free_holding_detail_em": {"func": get_stock_gdfx_free_holding_detail_em, "desc": "Free float detail", "category": "Shareholder Analysis"},
    "stock_gdfx_free_holding_statistics_em": {"func": get_stock_gdfx_free_holding_statistics_em, "desc": "Free float statistics", "category": "Shareholder Analysis"},
    "stock_gdfx_free_holding_teamwork_em": {"func": get_stock_gdfx_free_holding_teamwork_em, "desc": "Free float teamwork", "category": "Shareholder Analysis"},

    # Shareholder Changes
    "stock_shareholder_change_ths": {"func": get_stock_shareholder_change_ths, "desc": "Shareholder change (THS)", "category": "Shareholder Changes"},
    "stock_gddh_em": {"func": get_stock_gddh_em, "desc": "Shareholder appointment", "category": "Shareholder Changes"},

    # Fund Holdings
    "stock_fund_stock_holder": {"func": get_stock_fund_stock_holder, "desc": "Fund stock holdings", "category": "Fund Holdings"},
    "stock_report_fund_hold": {"func": get_stock_report_fund_hold, "desc": "Fund holding report", "category": "Fund Holdings"},
    "stock_report_fund_hold_detail": {"func": get_stock_report_fund_hold_detail, "desc": "Fund holding detail", "category": "Fund Holdings"},

    # Institutional Holdings
    "stock_institute_hold": {"func": get_stock_institute_hold, "desc": "Institutional holdings", "category": "Institutional Holdings"},
    "stock_institute_hold_detail": {"func": get_stock_institute_hold_detail, "desc": "Institutional holding detail", "category": "Institutional Holdings"},

    # Hold Management
    "stock_hold_management_detail_cninfo": {"func": get_stock_hold_management_detail_cninfo, "desc": "Management holding (CNINFO)", "category": "Management Holdings"},
    "stock_hold_management_detail_em": {"func": get_stock_hold_management_detail_em, "desc": "Management holding (EastMoney)", "category": "Management Holdings"},
    "stock_hold_management_person_em": {"func": get_stock_hold_management_person_em, "desc": "Management holding person", "category": "Management Holdings"},
    "stock_hold_num_cninfo": {"func": get_stock_hold_num_cninfo, "desc": "Shareholder number", "category": "Management Holdings"},
    "stock_hold_control_cninfo": {"func": get_stock_hold_control_cninfo, "desc": "Actual controller", "category": "Management Holdings"},
    "stock_hold_change_cninfo": {"func": get_stock_hold_change_cninfo, "desc": "Holding change (CNINFO)", "category": "Management Holdings"},

    # Share Changes
    "stock_share_change_cninfo": {"func": get_stock_share_change_cninfo, "desc": "Share change (CNINFO)", "category": "Share Changes"},
    "stock_share_hold_change_bse": {"func": get_stock_share_hold_change_bse, "desc": "BSE share change", "category": "Share Changes"},
    "stock_share_hold_change_sse": {"func": get_stock_share_hold_change_sse, "desc": "SSE share change", "category": "Share Changes"},
    "stock_share_hold_change_szse": {"func": get_stock_share_hold_change_szse, "desc": "SZSE share change", "category": "Share Changes"},

    # Executive Holdings
    "stock_ggcg_em": {"func": get_stock_ggcg_em, "desc": "Executive shareholding", "category": "Executive Holdings"},

    # Restricted Shares
    "stock_restricted_release_summary_em": {"func": get_stock_restricted_release_summary_em, "desc": "Restricted release summary", "category": "Restricted Shares"},
    "stock_restricted_release_queue_em": {"func": get_stock_restricted_release_queue_em, "desc": "Restricted release queue", "category": "Restricted Shares"},
    "stock_restricted_release_queue_sina": {"func": get_stock_restricted_release_queue_sina, "desc": "Restricted queue (Sina)", "category": "Restricted Shares"},
    "stock_restricted_release_detail_em": {"func": get_stock_restricted_release_detail_em, "desc": "Restricted release detail", "category": "Restricted Shares"},
    "stock_restricted_release_stockholder_em": {"func": get_stock_restricted_release_stockholder_em, "desc": "Restricted stockholder", "category": "Restricted Shares"},

    # Pledge
    "stock_gpzy_profile_em": {"func": get_stock_gpzy_profile_em, "desc": "Equity pledge profile", "category": "Equity Pledge"},
    "stock_gpzy_pledge_ratio_em": {"func": get_stock_gpzy_pledge_ratio_em, "desc": "Equity pledge ratio", "category": "Equity Pledge"},
    "stock_gpzy_pledge_ratio_detail_em": {"func": get_stock_gpzy_pledge_ratio_detail_em, "desc": "Pledge ratio detail", "category": "Equity Pledge"},
    "stock_gpzy_distribute_statistics_bank_em": {"func": get_stock_gpzy_distribute_statistics_bank_em, "desc": "Pledge bank statistics", "category": "Equity Pledge"},
    "stock_gpzy_distribute_statistics_company_em": {"func": get_stock_gpzy_distribute_statistics_company_em, "desc": "Pledge company statistics", "category": "Equity Pledge"},
    "stock_gpzy_industry_data_em": {"func": get_stock_gpzy_industry_data_em, "desc": "Pledge industry data", "category": "Equity Pledge"},

    # CNINFO Holdings
    "stock_cg_equity_mortgage_cninfo": {"func": get_stock_cg_equity_mortgage_cninfo, "desc": "Equity mortgage", "category": "CNINFO Holdings"},
    "stock_cg_guarantee_cninfo": {"func": get_stock_cg_guarantee_cninfo, "desc": "Guarantee data", "category": "CNINFO Holdings"},
    "stock_cg_lawsuit_cninfo": {"func": get_stock_cg_lawsuit_cninfo, "desc": "Lawsuit data", "category": "CNINFO Holdings"},
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
