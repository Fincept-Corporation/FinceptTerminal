#!/usr/bin/env python3
"""
AKShare Stocks Realtime Data Wrapper (Batch 1)
Provides access to realtime/spot stock data: A-shares, HK, US, B-shares, indices
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


# ==================== A-SHARES REALTIME ====================

def get_stock_zh_a_spot_em():
    """Get A-shares realtime spot data (EastMoney)"""
    return safe_call(ak.stock_zh_a_spot_em)

def get_stock_sh_a_spot_em():
    """Get Shanghai A-shares realtime spot"""
    return safe_call(ak.stock_sh_a_spot_em)

def get_stock_sz_a_spot_em():
    """Get Shenzhen A-shares realtime spot"""
    return safe_call(ak.stock_sz_a_spot_em)

def get_stock_bj_a_spot_em():
    """Get Beijing A-shares realtime spot"""
    return safe_call(ak.stock_bj_a_spot_em)

def get_stock_kc_a_spot_em():
    """Get STAR Market (科创板) realtime spot"""
    return safe_call(ak.stock_kc_a_spot_em)

def get_stock_cy_a_spot_em():
    """Get ChiNext (创业板) realtime spot"""
    return safe_call(ak.stock_cy_a_spot_em)

def get_stock_new_a_spot_em():
    """Get new stocks realtime spot"""
    return safe_call(ak.stock_new_a_spot_em)

def get_stock_zh_a_spot():
    """Get A-shares realtime spot (Sina)"""
    return safe_call(ak.stock_zh_a_spot)

def get_stock_zh_a_new():
    """Get new A-shares listing"""
    return safe_call(ak.stock_zh_a_new)

def get_stock_zh_a_new_em():
    """Get new A-shares listing (EastMoney)"""
    return safe_call(ak.stock_zh_a_new_em)

def get_stock_zh_a_st_em():
    """Get ST stocks realtime"""
    return safe_call(ak.stock_zh_a_st_em)

def get_stock_zh_a_stop_em():
    """Get suspended stocks"""
    return safe_call(ak.stock_zh_a_stop_em)


# ==================== B-SHARES REALTIME ====================

def get_stock_zh_b_spot():
    """Get B-shares realtime spot (Sina)"""
    return safe_call(ak.stock_zh_b_spot)

def get_stock_zh_b_spot_em():
    """Get B-shares realtime spot (EastMoney)"""
    return safe_call(ak.stock_zh_b_spot_em)


# ==================== HONG KONG STOCKS ====================

def get_stock_hk_spot():
    """Get HK stocks realtime spot"""
    return safe_call(ak.stock_hk_spot)

def get_stock_hk_spot_em():
    """Get HK stocks realtime spot (EastMoney)"""
    return safe_call(ak.stock_hk_spot_em)

def get_stock_hk_main_board_spot_em():
    """Get HK main board stocks"""
    return safe_call(ak.stock_hk_main_board_spot_em)

def get_stock_hk_famous_spot_em():
    """Get HK famous stocks"""
    return safe_call(ak.stock_hk_famous_spot_em)

def get_stock_hk_ggt_components_em():
    """Get HK Stock Connect components"""
    return safe_call(ak.stock_hk_ggt_components_em)


# ==================== US STOCKS ====================

def get_stock_us_spot():
    """Get US stocks realtime spot"""
    return safe_call(ak.stock_us_spot)

def get_stock_us_spot_em():
    """Get US stocks realtime spot (EastMoney)"""
    return safe_call(ak.stock_us_spot_em)

def get_stock_us_famous_spot_em():
    """Get US famous stocks"""
    return safe_call(ak.stock_us_famous_spot_em)

def get_stock_us_pink_spot_em():
    """Get US pink sheet stocks"""
    return safe_call(ak.stock_us_pink_spot_em)


# ==================== A-H SHARES ====================

def get_stock_zh_ah_spot():
    """Get A+H shares realtime spot"""
    return safe_call(ak.stock_zh_ah_spot)

def get_stock_zh_ah_spot_em():
    """Get A+H shares realtime spot (EastMoney)"""
    return safe_call(ak.stock_zh_ah_spot_em)

def get_stock_zh_ah_name():
    """Get A+H shares name mapping"""
    return safe_call(ak.stock_zh_ah_name)


# ==================== INDEX REALTIME ====================

def get_stock_zh_index_spot_em():
    """Get Chinese index realtime spot (EastMoney)"""
    return safe_call(ak.stock_zh_index_spot_em)

def get_stock_zh_index_spot_sina():
    """Get Chinese index realtime spot (Sina)"""
    return safe_call(ak.stock_zh_index_spot_sina)

def get_stock_hk_index_spot_em():
    """Get HK index realtime spot (EastMoney)"""
    return safe_call(ak.stock_hk_index_spot_em)

def get_stock_hk_index_spot_sina():
    """Get HK index realtime spot (Sina)"""
    return safe_call(ak.stock_hk_index_spot_sina)


# ==================== KCHUANG (科创板) ====================

def get_stock_zh_kcb_spot():
    """Get STAR Market realtime spot"""
    return safe_call(ak.stock_zh_kcb_spot)


# ==================== STOCK INFO ====================

def get_stock_info_a_code_name():
    """Get A-shares code and name list"""
    return safe_call(ak.stock_info_a_code_name)

def get_stock_info_sh_name_code():
    """Get Shanghai stocks code and name"""
    return safe_call(ak.stock_info_sh_name_code)

def get_stock_info_sz_name_code():
    """Get Shenzhen stocks code and name"""
    return safe_call(ak.stock_info_sz_name_code)

def get_stock_info_bj_name_code():
    """Get Beijing stocks code and name"""
    return safe_call(ak.stock_info_bj_name_code)

def get_stock_info_sh_delist():
    """Get Shanghai delisted stocks"""
    return safe_call(ak.stock_info_sh_delist)

def get_stock_info_sz_delist():
    """Get Shenzhen delisted stocks"""
    return safe_call(ak.stock_info_sz_delist)

def get_stock_info_sz_change_name():
    """Get Shenzhen stocks name change history"""
    return safe_call(ak.stock_info_sz_change_name)

def get_stock_info_change_name():
    """Get stocks name change history"""
    return safe_call(ak.stock_info_change_name)


# ==================== GLOBAL STOCK INFO ====================

def get_stock_info_global_cls():
    """Get global stock info (CLS)"""
    return safe_call(ak.stock_info_global_cls)

def get_stock_info_global_em():
    """Get global stock info (EastMoney)"""
    return safe_call(ak.stock_info_global_em)

def get_stock_info_global_futu():
    """Get global stock info (Futu)"""
    return safe_call(ak.stock_info_global_futu)

def get_stock_info_global_sina():
    """Get global stock info (Sina)"""
    return safe_call(ak.stock_info_global_sina)

def get_stock_info_global_ths():
    """Get global stock info (THS)"""
    return safe_call(ak.stock_info_global_ths)


# ==================== INDIVIDUAL STOCK INFO ====================

def get_stock_individual_info_em(symbol="000001"):
    """Get individual stock info (EastMoney)"""
    return safe_call(ak.stock_individual_info_em, symbol=symbol)

def get_stock_individual_spot_xq(symbol="SZ000001"):
    """Get individual stock spot (XueQiu)"""
    return safe_call(ak.stock_individual_spot_xq, symbol=symbol)

def get_stock_individual_basic_info_xq(symbol="SZ000001"):
    """Get individual stock basic info (XueQiu)"""
    return safe_call(ak.stock_individual_basic_info_xq, symbol=symbol)

def get_stock_individual_basic_info_hk_xq(symbol="00700"):
    """Get HK individual stock basic info (XueQiu)"""
    return safe_call(ak.stock_individual_basic_info_hk_xq, symbol=symbol)

def get_stock_individual_basic_info_us_xq(symbol="AAPL"):
    """Get US individual stock basic info (XueQiu)"""
    return safe_call(ak.stock_individual_basic_info_us_xq, symbol=symbol)


# ==================== ENDPOINT REGISTRY ====================

ENDPOINTS = {
    # A-Shares Realtime
    "stock_zh_a_spot_em": {"func": get_stock_zh_a_spot_em, "desc": "A-shares realtime (EastMoney)", "category": "A-Shares Realtime"},
    "stock_sh_a_spot_em": {"func": get_stock_sh_a_spot_em, "desc": "Shanghai A-shares realtime", "category": "A-Shares Realtime"},
    "stock_sz_a_spot_em": {"func": get_stock_sz_a_spot_em, "desc": "Shenzhen A-shares realtime", "category": "A-Shares Realtime"},
    "stock_bj_a_spot_em": {"func": get_stock_bj_a_spot_em, "desc": "Beijing A-shares realtime", "category": "A-Shares Realtime"},
    "stock_kc_a_spot_em": {"func": get_stock_kc_a_spot_em, "desc": "STAR Market realtime", "category": "A-Shares Realtime"},
    "stock_cy_a_spot_em": {"func": get_stock_cy_a_spot_em, "desc": "ChiNext realtime", "category": "A-Shares Realtime"},
    "stock_new_a_spot_em": {"func": get_stock_new_a_spot_em, "desc": "New stocks realtime", "category": "A-Shares Realtime"},
    "stock_zh_a_spot": {"func": get_stock_zh_a_spot, "desc": "A-shares realtime (Sina)", "category": "A-Shares Realtime"},
    "stock_zh_a_new": {"func": get_stock_zh_a_new, "desc": "New A-shares listing", "category": "A-Shares Realtime"},
    "stock_zh_a_new_em": {"func": get_stock_zh_a_new_em, "desc": "New A-shares (EastMoney)", "category": "A-Shares Realtime"},
    "stock_zh_a_st_em": {"func": get_stock_zh_a_st_em, "desc": "ST stocks realtime", "category": "A-Shares Realtime"},
    "stock_zh_a_stop_em": {"func": get_stock_zh_a_stop_em, "desc": "Suspended stocks", "category": "A-Shares Realtime"},

    # B-Shares
    "stock_zh_b_spot": {"func": get_stock_zh_b_spot, "desc": "B-shares realtime (Sina)", "category": "B-Shares"},
    "stock_zh_b_spot_em": {"func": get_stock_zh_b_spot_em, "desc": "B-shares realtime (EastMoney)", "category": "B-Shares"},

    # HK Stocks
    "stock_hk_spot": {"func": get_stock_hk_spot, "desc": "HK stocks realtime", "category": "HK Stocks"},
    "stock_hk_spot_em": {"func": get_stock_hk_spot_em, "desc": "HK stocks realtime (EastMoney)", "category": "HK Stocks"},
    "stock_hk_main_board_spot_em": {"func": get_stock_hk_main_board_spot_em, "desc": "HK main board stocks", "category": "HK Stocks"},
    "stock_hk_famous_spot_em": {"func": get_stock_hk_famous_spot_em, "desc": "HK famous stocks", "category": "HK Stocks"},
    "stock_hk_ggt_components_em": {"func": get_stock_hk_ggt_components_em, "desc": "HK Stock Connect components", "category": "HK Stocks"},

    # US Stocks
    "stock_us_spot": {"func": get_stock_us_spot, "desc": "US stocks realtime", "category": "US Stocks"},
    "stock_us_spot_em": {"func": get_stock_us_spot_em, "desc": "US stocks realtime (EastMoney)", "category": "US Stocks"},
    "stock_us_famous_spot_em": {"func": get_stock_us_famous_spot_em, "desc": "US famous stocks", "category": "US Stocks"},
    "stock_us_pink_spot_em": {"func": get_stock_us_pink_spot_em, "desc": "US pink sheet stocks", "category": "US Stocks"},

    # A+H Shares
    "stock_zh_ah_spot": {"func": get_stock_zh_ah_spot, "desc": "A+H shares realtime", "category": "A+H Shares"},
    "stock_zh_ah_spot_em": {"func": get_stock_zh_ah_spot_em, "desc": "A+H shares realtime (EastMoney)", "category": "A+H Shares"},
    "stock_zh_ah_name": {"func": get_stock_zh_ah_name, "desc": "A+H shares name mapping", "category": "A+H Shares"},

    # Index Realtime
    "stock_zh_index_spot_em": {"func": get_stock_zh_index_spot_em, "desc": "Chinese index realtime (EastMoney)", "category": "Index Realtime"},
    "stock_zh_index_spot_sina": {"func": get_stock_zh_index_spot_sina, "desc": "Chinese index realtime (Sina)", "category": "Index Realtime"},
    "stock_hk_index_spot_em": {"func": get_stock_hk_index_spot_em, "desc": "HK index realtime (EastMoney)", "category": "Index Realtime"},
    "stock_hk_index_spot_sina": {"func": get_stock_hk_index_spot_sina, "desc": "HK index realtime (Sina)", "category": "Index Realtime"},

    # STAR Market
    "stock_zh_kcb_spot": {"func": get_stock_zh_kcb_spot, "desc": "STAR Market realtime", "category": "STAR Market"},

    # Stock Info
    "stock_info_a_code_name": {"func": get_stock_info_a_code_name, "desc": "A-shares code and name", "category": "Stock Info"},
    "stock_info_sh_name_code": {"func": get_stock_info_sh_name_code, "desc": "Shanghai stocks list", "category": "Stock Info"},
    "stock_info_sz_name_code": {"func": get_stock_info_sz_name_code, "desc": "Shenzhen stocks list", "category": "Stock Info"},
    "stock_info_bj_name_code": {"func": get_stock_info_bj_name_code, "desc": "Beijing stocks list", "category": "Stock Info"},
    "stock_info_sh_delist": {"func": get_stock_info_sh_delist, "desc": "Shanghai delisted stocks", "category": "Stock Info"},
    "stock_info_sz_delist": {"func": get_stock_info_sz_delist, "desc": "Shenzhen delisted stocks", "category": "Stock Info"},
    "stock_info_sz_change_name": {"func": get_stock_info_sz_change_name, "desc": "Shenzhen name changes", "category": "Stock Info"},
    "stock_info_change_name": {"func": get_stock_info_change_name, "desc": "Stock name changes", "category": "Stock Info"},

    # Global Stock Info
    "stock_info_global_cls": {"func": get_stock_info_global_cls, "desc": "Global stock info (CLS)", "category": "Global Info"},
    "stock_info_global_em": {"func": get_stock_info_global_em, "desc": "Global stock info (EastMoney)", "category": "Global Info"},
    "stock_info_global_futu": {"func": get_stock_info_global_futu, "desc": "Global stock info (Futu)", "category": "Global Info"},
    "stock_info_global_sina": {"func": get_stock_info_global_sina, "desc": "Global stock info (Sina)", "category": "Global Info"},
    "stock_info_global_ths": {"func": get_stock_info_global_ths, "desc": "Global stock info (THS)", "category": "Global Info"},

    # Individual Stock Info
    "stock_individual_info_em": {"func": get_stock_individual_info_em, "desc": "Individual stock info", "category": "Individual Info"},
    "stock_individual_spot_xq": {"func": get_stock_individual_spot_xq, "desc": "Individual spot (XueQiu)", "category": "Individual Info"},
    "stock_individual_basic_info_xq": {"func": get_stock_individual_basic_info_xq, "desc": "Individual basic info (XueQiu)", "category": "Individual Info"},
    "stock_individual_basic_info_hk_xq": {"func": get_stock_individual_basic_info_hk_xq, "desc": "HK individual info (XueQiu)", "category": "Individual Info"},
    "stock_individual_basic_info_us_xq": {"func": get_stock_individual_basic_info_us_xq, "desc": "US individual info (XueQiu)", "category": "Individual Info"},
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
