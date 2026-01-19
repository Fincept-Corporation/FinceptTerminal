#!/usr/bin/env python3
"""
AKShare Stocks Board Data Wrapper (Batch 6)
Provides access to industry boards, concept boards, sector data
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


# ==================== CONCEPT BOARD (概念板块) ====================

def get_stock_board_concept_name_em():
    """Get concept board names (EastMoney)"""
    return safe_call(ak.stock_board_concept_name_em)

def get_stock_board_concept_name_ths():
    """Get concept board names (THS)"""
    return safe_call(ak.stock_board_concept_name_ths)

def get_stock_board_concept_spot_em():
    """Get concept board spot data (EastMoney)"""
    return safe_call(ak.stock_board_concept_spot_em)

def get_stock_board_concept_cons_em(symbol="人工智能"):
    """Get concept board constituents (EastMoney)"""
    return safe_call(ak.stock_board_concept_cons_em, symbol=symbol)

def get_stock_board_concept_hist_em(symbol="人工智能", period="daily", start_date="20200101", end_date="20251231", adjust=""):
    """Get concept board historical data (EastMoney)"""
    return safe_call(ak.stock_board_concept_hist_em, symbol=symbol, period=period, start_date=start_date, end_date=end_date, adjust=adjust)

def get_stock_board_concept_hist_min_em(symbol="人工智能", period="5"):
    """Get concept board minute data (EastMoney)"""
    return safe_call(ak.stock_board_concept_hist_min_em, symbol=symbol, period=period)

def get_stock_board_concept_index_ths(symbol="人工智能"):
    """Get concept board index (THS)"""
    return safe_call(ak.stock_board_concept_index_ths, symbol=symbol)

def get_stock_board_concept_info_ths(symbol="人工智能"):
    """Get concept board info (THS)"""
    return safe_call(ak.stock_board_concept_info_ths, symbol=symbol)

def get_stock_board_concept_summary_ths():
    """Get concept board summary (THS)"""
    return safe_call(ak.stock_board_concept_summary_ths)

def get_stock_concept_cons_futu(symbol="人工智能"):
    """Get concept constituents (Futu)"""
    return safe_call(ak.stock_concept_cons_futu, symbol=symbol)


# ==================== INDUSTRY BOARD (行业板块) ====================

def get_stock_board_industry_name_em():
    """Get industry board names (EastMoney)"""
    return safe_call(ak.stock_board_industry_name_em)

def get_stock_board_industry_name_ths():
    """Get industry board names (THS)"""
    return safe_call(ak.stock_board_industry_name_ths)

def get_stock_board_industry_spot_em():
    """Get industry board spot data (EastMoney)"""
    return safe_call(ak.stock_board_industry_spot_em)

def get_stock_board_industry_cons_em(symbol="小金属"):
    """Get industry board constituents (EastMoney)"""
    return safe_call(ak.stock_board_industry_cons_em, symbol=symbol)

def get_stock_board_industry_hist_em(symbol="小金属", period="daily", start_date="20200101", end_date="20251231", adjust=""):
    """Get industry board historical data (EastMoney)"""
    return safe_call(ak.stock_board_industry_hist_em, symbol=symbol, period=period, start_date=start_date, end_date=end_date, adjust=adjust)

def get_stock_board_industry_hist_min_em(symbol="小金属", period="5"):
    """Get industry board minute data (EastMoney)"""
    return safe_call(ak.stock_board_industry_hist_min_em, symbol=symbol, period=period)

def get_stock_board_industry_index_ths(symbol="采掘服务"):
    """Get industry board index (THS)"""
    return safe_call(ak.stock_board_industry_index_ths, symbol=symbol)

def get_stock_board_industry_info_ths(symbol="采掘服务"):
    """Get industry board info (THS)"""
    return safe_call(ak.stock_board_industry_info_ths, symbol=symbol)

def get_stock_board_industry_summary_ths():
    """Get industry board summary (THS)"""
    return safe_call(ak.stock_board_industry_summary_ths)


# ==================== BOARD CHANGES ====================

def get_stock_board_change_em():
    """Get board change data (EastMoney)"""
    return safe_call(ak.stock_board_change_em)


# ==================== INDUSTRY CATEGORY ====================

def get_stock_industry_category_cninfo(symbol="巨潮行业分类标准"):
    """Get industry category (CNINFO)"""
    return safe_call(ak.stock_industry_category_cninfo, symbol=symbol)

def get_stock_industry_change_cninfo(symbol="000001", start_date="", end_date=""):
    """Get industry change (CNINFO)"""
    return safe_call(ak.stock_industry_change_cninfo, symbol=symbol, start_date=start_date, end_date=end_date)

def get_stock_industry_clf_hist_sw(symbol="801010"):
    """Get Shenwan industry historical data"""
    return safe_call(ak.stock_industry_clf_hist_sw, symbol=symbol)

def get_stock_industry_pe_ratio_cninfo(symbol="证监会行业分类", date="2024-01-01"):
    """Get industry PE ratio (CNINFO)"""
    return safe_call(ak.stock_industry_pe_ratio_cninfo, symbol=symbol, date=date)


# ==================== ZT POOL (涨停板) ====================

def get_stock_zt_pool_em(date="20240101"):
    """Get limit-up pool (EastMoney)"""
    return safe_call(ak.stock_zt_pool_em, date=date)

def get_stock_zt_pool_previous_em(date="20240101"):
    """Get previous limit-up pool (EastMoney)"""
    return safe_call(ak.stock_zt_pool_previous_em, date=date)

def get_stock_zt_pool_strong_em(date="20240101"):
    """Get strong limit-up pool (EastMoney)"""
    return safe_call(ak.stock_zt_pool_strong_em, date=date)

def get_stock_zt_pool_sub_new_em(date="20240101"):
    """Get sub-new limit-up pool (EastMoney)"""
    return safe_call(ak.stock_zt_pool_sub_new_em, date=date)

def get_stock_zt_pool_zbgc_em(date="20240101"):
    """Get limit-up failure pool (EastMoney)"""
    return safe_call(ak.stock_zt_pool_zbgc_em, date=date)

def get_stock_zt_pool_dtgc_em(date="20240101"):
    """Get limit-down pool (EastMoney)"""
    return safe_call(ak.stock_zt_pool_dtgc_em, date=date)


# ==================== ANALYST ====================

def get_stock_analyst_rank_em(year="2024"):
    """Get analyst ranking (EastMoney)"""
    return safe_call(ak.stock_analyst_rank_em, year=year)

def get_stock_analyst_detail_em(analyst_id="11000200589"):
    """Get analyst detail (EastMoney)"""
    return safe_call(ak.stock_analyst_detail_em, analyst_id=analyst_id)


# ==================== RESEARCH ====================

def get_stock_institute_recommend(symbol="000001"):
    """Get institutional recommendations"""
    return safe_call(ak.stock_institute_recommend, symbol=symbol)

def get_stock_institute_recommend_detail(symbol="000001"):
    """Get institutional recommendation detail"""
    return safe_call(ak.stock_institute_recommend_detail, symbol=symbol)

def get_stock_research_report_em(symbol="000001"):
    """Get research report (EastMoney)"""
    return safe_call(ak.stock_research_report_em, symbol=symbol)

def get_stock_jgdy_tj_em(date="20240101"):
    """Get institutional research statistics (EastMoney)"""
    return safe_call(ak.stock_jgdy_tj_em, date=date)

def get_stock_jgdy_detail_em(date="20240101"):
    """Get institutional research detail (EastMoney)"""
    return safe_call(ak.stock_jgdy_detail_em, date=date)


# ==================== RANK ====================

def get_stock_rank_cxg_ths():
    """Get new high ranking (THS)"""
    return safe_call(ak.stock_rank_cxg_ths)

def get_stock_rank_cxd_ths():
    """Get new low ranking (THS)"""
    return safe_call(ak.stock_rank_cxd_ths)

def get_stock_rank_cxfl_ths():
    """Get volume breakthrough ranking (THS)"""
    return safe_call(ak.stock_rank_cxfl_ths)

def get_stock_rank_cxsl_ths():
    """Get volume shrink ranking (THS)"""
    return safe_call(ak.stock_rank_cxsl_ths)

def get_stock_rank_ljqd_ths():
    """Get strong rise ranking (THS)"""
    return safe_call(ak.stock_rank_ljqd_ths)

def get_stock_rank_ljqs_ths():
    """Get strong fall ranking (THS)"""
    return safe_call(ak.stock_rank_ljqs_ths)

def get_stock_rank_lxsz_ths():
    """Get continuous rise ranking (THS)"""
    return safe_call(ak.stock_rank_lxsz_ths)

def get_stock_rank_lxxd_ths():
    """Get continuous fall ranking (THS)"""
    return safe_call(ak.stock_rank_lxxd_ths)

def get_stock_rank_xstp_ths():
    """Get price up ranking (THS)"""
    return safe_call(ak.stock_rank_xstp_ths)

def get_stock_rank_xxtp_ths():
    """Get price down ranking (THS)"""
    return safe_call(ak.stock_rank_xxtp_ths)

def get_stock_rank_xzjp_ths():
    """Get active trading ranking (THS)"""
    return safe_call(ak.stock_rank_xzjp_ths)

def get_stock_rank_forecast_cninfo(date="2024-01-01"):
    """Get forecast ranking (CNINFO)"""
    return safe_call(ak.stock_rank_forecast_cninfo, date=date)


# ==================== ENDPOINT REGISTRY ====================

ENDPOINTS = {
    # Concept Board
    "stock_board_concept_name_em": {"func": get_stock_board_concept_name_em, "desc": "Concept board names (EastMoney)", "category": "Concept Board"},
    "stock_board_concept_name_ths": {"func": get_stock_board_concept_name_ths, "desc": "Concept board names (THS)", "category": "Concept Board"},
    "stock_board_concept_spot_em": {"func": get_stock_board_concept_spot_em, "desc": "Concept board spot", "category": "Concept Board"},
    "stock_board_concept_cons_em": {"func": get_stock_board_concept_cons_em, "desc": "Concept constituents", "category": "Concept Board"},
    "stock_board_concept_hist_em": {"func": get_stock_board_concept_hist_em, "desc": "Concept historical data", "category": "Concept Board"},
    "stock_board_concept_hist_min_em": {"func": get_stock_board_concept_hist_min_em, "desc": "Concept minute data", "category": "Concept Board"},
    "stock_board_concept_index_ths": {"func": get_stock_board_concept_index_ths, "desc": "Concept index (THS)", "category": "Concept Board"},
    "stock_board_concept_info_ths": {"func": get_stock_board_concept_info_ths, "desc": "Concept info (THS)", "category": "Concept Board"},
    "stock_board_concept_summary_ths": {"func": get_stock_board_concept_summary_ths, "desc": "Concept summary (THS)", "category": "Concept Board"},
    "stock_concept_cons_futu": {"func": get_stock_concept_cons_futu, "desc": "Concept constituents (Futu)", "category": "Concept Board"},

    # Industry Board
    "stock_board_industry_name_em": {"func": get_stock_board_industry_name_em, "desc": "Industry board names (EastMoney)", "category": "Industry Board"},
    "stock_board_industry_name_ths": {"func": get_stock_board_industry_name_ths, "desc": "Industry board names (THS)", "category": "Industry Board"},
    "stock_board_industry_spot_em": {"func": get_stock_board_industry_spot_em, "desc": "Industry board spot", "category": "Industry Board"},
    "stock_board_industry_cons_em": {"func": get_stock_board_industry_cons_em, "desc": "Industry constituents", "category": "Industry Board"},
    "stock_board_industry_hist_em": {"func": get_stock_board_industry_hist_em, "desc": "Industry historical data", "category": "Industry Board"},
    "stock_board_industry_hist_min_em": {"func": get_stock_board_industry_hist_min_em, "desc": "Industry minute data", "category": "Industry Board"},
    "stock_board_industry_index_ths": {"func": get_stock_board_industry_index_ths, "desc": "Industry index (THS)", "category": "Industry Board"},
    "stock_board_industry_info_ths": {"func": get_stock_board_industry_info_ths, "desc": "Industry info (THS)", "category": "Industry Board"},
    "stock_board_industry_summary_ths": {"func": get_stock_board_industry_summary_ths, "desc": "Industry summary (THS)", "category": "Industry Board"},

    # Board Changes
    "stock_board_change_em": {"func": get_stock_board_change_em, "desc": "Board change data", "category": "Board Changes"},

    # Industry Category
    "stock_industry_category_cninfo": {"func": get_stock_industry_category_cninfo, "desc": "Industry category (CNINFO)", "category": "Industry Category"},
    "stock_industry_change_cninfo": {"func": get_stock_industry_change_cninfo, "desc": "Industry change (CNINFO)", "category": "Industry Category"},
    "stock_industry_clf_hist_sw": {"func": get_stock_industry_clf_hist_sw, "desc": "Shenwan industry history", "category": "Industry Category"},
    "stock_industry_pe_ratio_cninfo": {"func": get_stock_industry_pe_ratio_cninfo, "desc": "Industry PE ratio", "category": "Industry Category"},

    # ZT Pool
    "stock_zt_pool_em": {"func": get_stock_zt_pool_em, "desc": "Limit-up pool", "category": "ZT Pool"},
    "stock_zt_pool_previous_em": {"func": get_stock_zt_pool_previous_em, "desc": "Previous limit-up", "category": "ZT Pool"},
    "stock_zt_pool_strong_em": {"func": get_stock_zt_pool_strong_em, "desc": "Strong limit-up", "category": "ZT Pool"},
    "stock_zt_pool_sub_new_em": {"func": get_stock_zt_pool_sub_new_em, "desc": "Sub-new limit-up", "category": "ZT Pool"},
    "stock_zt_pool_zbgc_em": {"func": get_stock_zt_pool_zbgc_em, "desc": "Limit-up failure", "category": "ZT Pool"},
    "stock_zt_pool_dtgc_em": {"func": get_stock_zt_pool_dtgc_em, "desc": "Limit-down pool", "category": "ZT Pool"},

    # Analyst
    "stock_analyst_rank_em": {"func": get_stock_analyst_rank_em, "desc": "Analyst ranking", "category": "Analyst"},
    "stock_analyst_detail_em": {"func": get_stock_analyst_detail_em, "desc": "Analyst detail", "category": "Analyst"},

    # Research
    "stock_institute_recommend": {"func": get_stock_institute_recommend, "desc": "Institution recommendations", "category": "Research"},
    "stock_institute_recommend_detail": {"func": get_stock_institute_recommend_detail, "desc": "Recommendation detail", "category": "Research"},
    "stock_research_report_em": {"func": get_stock_research_report_em, "desc": "Research report", "category": "Research"},
    "stock_jgdy_tj_em": {"func": get_stock_jgdy_tj_em, "desc": "Research statistics", "category": "Research"},
    "stock_jgdy_detail_em": {"func": get_stock_jgdy_detail_em, "desc": "Research detail", "category": "Research"},

    # Rank
    "stock_rank_cxg_ths": {"func": get_stock_rank_cxg_ths, "desc": "New high ranking", "category": "Rankings"},
    "stock_rank_cxd_ths": {"func": get_stock_rank_cxd_ths, "desc": "New low ranking", "category": "Rankings"},
    "stock_rank_cxfl_ths": {"func": get_stock_rank_cxfl_ths, "desc": "Volume breakthrough", "category": "Rankings"},
    "stock_rank_cxsl_ths": {"func": get_stock_rank_cxsl_ths, "desc": "Volume shrink", "category": "Rankings"},
    "stock_rank_ljqd_ths": {"func": get_stock_rank_ljqd_ths, "desc": "Strong rise ranking", "category": "Rankings"},
    "stock_rank_ljqs_ths": {"func": get_stock_rank_ljqs_ths, "desc": "Strong fall ranking", "category": "Rankings"},
    "stock_rank_lxsz_ths": {"func": get_stock_rank_lxsz_ths, "desc": "Continuous rise", "category": "Rankings"},
    "stock_rank_lxxd_ths": {"func": get_stock_rank_lxxd_ths, "desc": "Continuous fall", "category": "Rankings"},
    "stock_rank_xstp_ths": {"func": get_stock_rank_xstp_ths, "desc": "Price up ranking", "category": "Rankings"},
    "stock_rank_xxtp_ths": {"func": get_stock_rank_xxtp_ths, "desc": "Price down ranking", "category": "Rankings"},
    "stock_rank_xzjp_ths": {"func": get_stock_rank_xzjp_ths, "desc": "Active trading ranking", "category": "Rankings"},
    "stock_rank_forecast_cninfo": {"func": get_stock_rank_forecast_cninfo, "desc": "Forecast ranking", "category": "Rankings"},
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
