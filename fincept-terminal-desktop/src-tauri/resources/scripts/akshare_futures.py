#!/usr/bin/env python3
"""
AKShare Futures Data Wrapper
Provides access to futures data: contracts, warehouse receipts, positions, spot prices
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


# ==================== CONTRACT INFO ====================

def get_futures_comm_info():
    """Get futures commodity information"""
    return safe_call(ak.futures_comm_info)

def get_futures_fees_info():
    """Get futures fees information"""
    return safe_call(ak.futures_fees_info)

def get_futures_rule():
    """Get futures trading rules"""
    return safe_call(ak.futures_rule)

def get_futures_contract_detail(symbol="A"):
    """Get futures contract details"""
    return safe_call(ak.futures_contract_detail, symbol=symbol)

def get_futures_symbol_mark():
    """Get futures symbol marks"""
    return safe_call(ak.futures_symbol_mark)

def get_futures_contract_info_shfe():
    """Get SHFE contract info"""
    return safe_call(ak.futures_contract_info_shfe)

def get_futures_contract_info_dce():
    """Get DCE contract info"""
    return safe_call(ak.futures_contract_info_dce)

def get_futures_contract_info_czce():
    """Get CZCE contract info"""
    return safe_call(ak.futures_contract_info_czce)

def get_futures_contract_info_cffex():
    """Get CFFEX contract info"""
    return safe_call(ak.futures_contract_info_cffex)

def get_futures_contract_info_ine():
    """Get INE contract info"""
    return safe_call(ak.futures_contract_info_ine)

def get_futures_contract_info_gfex():
    """Get GFEX contract info"""
    return safe_call(ak.futures_contract_info_gfex)


# ==================== REALTIME DATA ====================

def get_futures_zh_realtime():
    """Get Chinese futures realtime data"""
    return safe_call(ak.futures_zh_realtime)

def get_futures_zh_spot():
    """Get Chinese futures spot data"""
    return safe_call(ak.futures_zh_spot)

def get_futures_main_sina():
    """Get main futures contracts from Sina"""
    return safe_call(ak.futures_main_sina)

def get_futures_display_main_sina():
    """Get main futures display from Sina"""
    return safe_call(ak.futures_display_main_sina)

def get_futures_hq_subscribe_exchange_symbol():
    """Get futures HQ subscription symbols"""
    return safe_call(ak.futures_hq_subscribe_exchange_symbol)


# ==================== HISTORICAL DATA ====================

def get_futures_zh_daily_sina(symbol="V0"):
    """Get Chinese futures daily data from Sina"""
    return safe_call(ak.futures_zh_daily_sina, symbol=symbol)

def get_futures_zh_minute_sina(symbol="V2501", period="1"):
    """Get Chinese futures minute data from Sina"""
    return safe_call(ak.futures_zh_minute_sina, symbol=symbol, period=period)

def get_futures_hist_em(symbol="豆粕主力"):
    """Get futures historical data from EastMoney"""
    return safe_call(ak.futures_hist_em, symbol=symbol)

def get_futures_hist_table_em():
    """Get futures historical table from EastMoney"""
    return safe_call(ak.futures_hist_table_em)


# ==================== GLOBAL FUTURES ====================

def get_futures_global_spot():
    """Get global futures spot data"""
    return safe_call(ak.futures_global_spot_em)

def get_futures_global_hist(symbol="101", period="daily", start_date="20200101", end_date="20261231"):
    """Get global futures historical data"""
    return safe_call(ak.futures_global_hist_em, symbol=symbol, period=period, start_date=start_date, end_date=end_date)

def get_futures_foreign_commodity_realtime():
    """Get foreign commodity futures realtime"""
    return safe_call(ak.futures_foreign_commodity_realtime)

def get_futures_foreign_commodity_subscribe():
    """Get foreign commodity subscription symbols"""
    return safe_call(ak.futures_foreign_commodity_subscribe_exchange_symbol)

def get_futures_foreign_hist(symbol="BRN"):
    """Get foreign futures historical data"""
    return safe_call(ak.futures_foreign_hist, symbol=symbol)

def get_futures_foreign_detail(symbol="BRN"):
    """Get foreign futures details"""
    return safe_call(ak.futures_foreign_detail, symbol=symbol)


# ==================== WAREHOUSE RECEIPTS ====================

def get_futures_shfe_warehouse_receipt():
    """Get SHFE warehouse receipts"""
    return safe_call(ak.futures_shfe_warehouse_receipt)

def get_futures_dce_warehouse_receipt():
    """Get DCE warehouse receipts"""
    return safe_call(ak.futures_dce_warehouse_receipt)

def get_futures_czce_warehouse_receipt():
    """Get CZCE warehouse receipts"""
    return safe_call(ak.futures_czce_warehouse_receipt)

def get_futures_gfex_warehouse_receipt():
    """Get GFEX warehouse receipts"""
    return safe_call(ak.futures_gfex_warehouse_receipt)


# ==================== POSITION RANK ====================

def get_futures_dce_position_rank(symbol="a", date="20240101"):
    """Get DCE position rank"""
    return safe_call(ak.futures_dce_position_rank, symbol=symbol, date=date)

def get_futures_dce_position_rank_other(symbol="a", date="20240101"):
    """Get DCE position rank (other)"""
    return safe_call(ak.futures_dce_position_rank_other, symbol=symbol, date=date)

def get_futures_gfex_position_rank(symbol="si", date="20240101"):
    """Get GFEX position rank"""
    return safe_call(ak.futures_gfex_position_rank, symbol=symbol, date=date)

def get_futures_hold_pos_sina(symbol="RB"):
    """Get futures holding positions from Sina"""
    return safe_call(ak.futures_hold_pos_sina, symbol=symbol)


# ==================== DELIVERY ====================

def get_futures_delivery_shfe():
    """Get SHFE delivery data"""
    return safe_call(ak.futures_delivery_shfe)

def get_futures_delivery_dce():
    """Get DCE delivery data"""
    return safe_call(ak.futures_delivery_dce)

def get_futures_delivery_czce():
    """Get CZCE delivery data"""
    return safe_call(ak.futures_delivery_czce)

def get_futures_delivery_match_dce():
    """Get DCE delivery match"""
    return safe_call(ak.futures_delivery_match_dce)

def get_futures_delivery_match_czce():
    """Get CZCE delivery match"""
    return safe_call(ak.futures_delivery_match_czce)


# ==================== SPOT PRICES ====================

def get_futures_spot_price():
    """Get futures spot prices"""
    return safe_call(ak.futures_spot_price)

def get_futures_spot_price_daily(start_day="20200101", end_day="20261231", vars_list=None):
    """Get daily spot prices"""
    return safe_call(ak.futures_spot_price_daily, start_day=start_day, end_day=end_day)

def get_futures_spot_price_previous():
    """Get previous spot prices"""
    return safe_call(ak.futures_spot_price_previous)

def get_futures_spot_stock():
    """Get spot stock data"""
    return safe_call(ak.futures_spot_stock)

def get_futures_spot_sys():
    """Get spot system data"""
    return safe_call(ak.futures_spot_sys)

def get_futures_to_spot_shfe():
    """Get SHFE futures to spot data"""
    return safe_call(ak.futures_to_spot_shfe)

def get_futures_to_spot_dce():
    """Get DCE futures to spot data"""
    return safe_call(ak.futures_to_spot_dce)

def get_futures_to_spot_czce():
    """Get CZCE futures to spot data"""
    return safe_call(ak.futures_to_spot_czce)


# ==================== INVENTORY ====================

def get_futures_inventory_em(symbol="沪铜"):
    """Get futures inventory from EastMoney"""
    return safe_call(ak.futures_inventory_em, symbol=symbol)

def get_futures_inventory_99(symbol="铜"):
    """Get futures inventory from 99"""
    return safe_call(ak.futures_inventory_99, symbol=symbol)

def get_futures_comex_inventory():
    """Get COMEX inventory"""
    return safe_call(ak.futures_comex_inventory)

def get_futures_stock_shfe_js():
    """Get SHFE stock from JS"""
    return safe_call(ak.futures_stock_shfe_js)


# ==================== INDEX ====================

def get_futures_index_ccidx():
    """Get CCIDX futures index"""
    return safe_call(ak.futures_index_ccidx)

def get_futures_index_min_ccidx(symbol="CCIDX"):
    """Get CCIDX futures index minute data"""
    return safe_call(ak.futures_index_min_ccidx, symbol=symbol)


# ==================== HOG FUTURES ====================

def get_futures_hog_core():
    """Get hog futures core data"""
    return safe_call(ak.futures_hog_core)

def get_futures_hog_cost():
    """Get hog futures cost data"""
    return safe_call(ak.futures_hog_cost)

def get_futures_hog_supply():
    """Get hog futures supply data"""
    return safe_call(ak.futures_hog_supply)


# ==================== OTHER ====================

def get_futures_news_shmet():
    """Get SHMET futures news"""
    return safe_call(ak.futures_news_shmet)

def get_futures_settlement_price_sgx():
    """Get SGX settlement prices"""
    return safe_call(ak.futures_settlement_price_sgx)


# ==================== ENDPOINT REGISTRY ====================

ENDPOINTS = {
    # Contract Info
    "futures_comm_info": {"func": get_futures_comm_info, "desc": "Futures commodity information", "category": "Contract Info"},
    "futures_fees_info": {"func": get_futures_fees_info, "desc": "Futures fees information", "category": "Contract Info"},
    "futures_rule": {"func": get_futures_rule, "desc": "Futures trading rules", "category": "Contract Info"},
    "futures_contract_detail": {"func": get_futures_contract_detail, "desc": "Futures contract details", "category": "Contract Info"},
    "futures_symbol_mark": {"func": get_futures_symbol_mark, "desc": "Futures symbol marks", "category": "Contract Info"},
    "futures_contract_shfe": {"func": get_futures_contract_info_shfe, "desc": "SHFE contract info", "category": "Contract Info"},
    "futures_contract_dce": {"func": get_futures_contract_info_dce, "desc": "DCE contract info", "category": "Contract Info"},
    "futures_contract_czce": {"func": get_futures_contract_info_czce, "desc": "CZCE contract info", "category": "Contract Info"},
    "futures_contract_cffex": {"func": get_futures_contract_info_cffex, "desc": "CFFEX contract info", "category": "Contract Info"},
    "futures_contract_ine": {"func": get_futures_contract_info_ine, "desc": "INE contract info", "category": "Contract Info"},
    "futures_contract_gfex": {"func": get_futures_contract_info_gfex, "desc": "GFEX contract info", "category": "Contract Info"},

    # Realtime Data
    "futures_zh_realtime": {"func": get_futures_zh_realtime, "desc": "Chinese futures realtime", "category": "Realtime"},
    "futures_zh_spot": {"func": get_futures_zh_spot, "desc": "Chinese futures spot", "category": "Realtime"},
    "futures_main_sina": {"func": get_futures_main_sina, "desc": "Main futures from Sina", "category": "Realtime"},
    "futures_display_main": {"func": get_futures_display_main_sina, "desc": "Main futures display", "category": "Realtime"},
    "futures_hq_symbols": {"func": get_futures_hq_subscribe_exchange_symbol, "desc": "HQ subscription symbols", "category": "Realtime"},

    # Historical Data
    "futures_zh_daily": {"func": get_futures_zh_daily_sina, "desc": "Chinese futures daily", "category": "Historical"},
    "futures_zh_minute": {"func": get_futures_zh_minute_sina, "desc": "Chinese futures minute", "category": "Historical"},
    "futures_hist_em": {"func": get_futures_hist_em, "desc": "Futures historical (EM)", "category": "Historical"},
    "futures_hist_table": {"func": get_futures_hist_table_em, "desc": "Futures historical table", "category": "Historical"},

    # Global Futures
    "futures_global_spot": {"func": get_futures_global_spot, "desc": "Global futures spot", "category": "Global"},
    "futures_global_hist": {"func": get_futures_global_hist, "desc": "Global futures historical", "category": "Global"},
    "futures_foreign_realtime": {"func": get_futures_foreign_commodity_realtime, "desc": "Foreign commodity realtime", "category": "Global"},
    "futures_foreign_symbols": {"func": get_futures_foreign_commodity_subscribe, "desc": "Foreign commodity symbols", "category": "Global"},
    "futures_foreign_hist": {"func": get_futures_foreign_hist, "desc": "Foreign futures historical", "category": "Global"},
    "futures_foreign_detail": {"func": get_futures_foreign_detail, "desc": "Foreign futures details", "category": "Global"},

    # Warehouse Receipts
    "futures_warehouse_shfe": {"func": get_futures_shfe_warehouse_receipt, "desc": "SHFE warehouse receipts", "category": "Warehouse"},
    "futures_warehouse_dce": {"func": get_futures_dce_warehouse_receipt, "desc": "DCE warehouse receipts", "category": "Warehouse"},
    "futures_warehouse_czce": {"func": get_futures_czce_warehouse_receipt, "desc": "CZCE warehouse receipts", "category": "Warehouse"},
    "futures_warehouse_gfex": {"func": get_futures_gfex_warehouse_receipt, "desc": "GFEX warehouse receipts", "category": "Warehouse"},

    # Position Rank
    "futures_position_dce": {"func": get_futures_dce_position_rank, "desc": "DCE position rank", "category": "Positions"},
    "futures_position_dce_other": {"func": get_futures_dce_position_rank_other, "desc": "DCE position rank (other)", "category": "Positions"},
    "futures_position_gfex": {"func": get_futures_gfex_position_rank, "desc": "GFEX position rank", "category": "Positions"},
    "futures_hold_pos": {"func": get_futures_hold_pos_sina, "desc": "Holding positions", "category": "Positions"},

    # Delivery
    "futures_delivery_shfe": {"func": get_futures_delivery_shfe, "desc": "SHFE delivery", "category": "Delivery"},
    "futures_delivery_dce": {"func": get_futures_delivery_dce, "desc": "DCE delivery", "category": "Delivery"},
    "futures_delivery_czce": {"func": get_futures_delivery_czce, "desc": "CZCE delivery", "category": "Delivery"},
    "futures_delivery_match_dce": {"func": get_futures_delivery_match_dce, "desc": "DCE delivery match", "category": "Delivery"},
    "futures_delivery_match_czce": {"func": get_futures_delivery_match_czce, "desc": "CZCE delivery match", "category": "Delivery"},

    # Spot Prices
    "futures_spot_price": {"func": get_futures_spot_price, "desc": "Spot prices", "category": "Spot"},
    "futures_spot_daily": {"func": get_futures_spot_price_daily, "desc": "Daily spot prices", "category": "Spot"},
    "futures_spot_previous": {"func": get_futures_spot_price_previous, "desc": "Previous spot prices", "category": "Spot"},
    "futures_spot_stock": {"func": get_futures_spot_stock, "desc": "Spot stock data", "category": "Spot"},
    "futures_spot_sys": {"func": get_futures_spot_sys, "desc": "Spot system data", "category": "Spot"},
    "futures_to_spot_shfe": {"func": get_futures_to_spot_shfe, "desc": "SHFE futures to spot", "category": "Spot"},
    "futures_to_spot_dce": {"func": get_futures_to_spot_dce, "desc": "DCE futures to spot", "category": "Spot"},
    "futures_to_spot_czce": {"func": get_futures_to_spot_czce, "desc": "CZCE futures to spot", "category": "Spot"},

    # Inventory
    "futures_inventory_em": {"func": get_futures_inventory_em, "desc": "Futures inventory (EM)", "category": "Inventory"},
    "futures_inventory_99": {"func": get_futures_inventory_99, "desc": "Futures inventory (99)", "category": "Inventory"},
    "futures_comex_inventory": {"func": get_futures_comex_inventory, "desc": "COMEX inventory", "category": "Inventory"},
    "futures_stock_shfe": {"func": get_futures_stock_shfe_js, "desc": "SHFE stock", "category": "Inventory"},

    # Index
    "futures_index_ccidx": {"func": get_futures_index_ccidx, "desc": "CCIDX futures index", "category": "Index"},
    "futures_index_min": {"func": get_futures_index_min_ccidx, "desc": "CCIDX index minute", "category": "Index"},

    # Hog Futures
    "futures_hog_core": {"func": get_futures_hog_core, "desc": "Hog core data", "category": "Hog"},
    "futures_hog_cost": {"func": get_futures_hog_cost, "desc": "Hog cost data", "category": "Hog"},
    "futures_hog_supply": {"func": get_futures_hog_supply, "desc": "Hog supply data", "category": "Hog"},

    # Other
    "futures_news_shmet": {"func": get_futures_news_shmet, "desc": "SHMET news", "category": "Other"},
    "futures_settlement_sgx": {"func": get_futures_settlement_price_sgx, "desc": "SGX settlement prices", "category": "Other"},
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
