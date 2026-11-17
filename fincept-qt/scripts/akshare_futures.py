#!/usr/bin/env python3
"""
AKShare Futures Data Wrapper
Provides access to futures data: contracts, warehouse receipts, positions, spot prices
"""

import sys
import json
import time
from datetime import datetime

# safe_call: one akshare call -> {success, data, columns, count} or a plain-English error.
# run_endpoint_table: the CLI (get_all_endpoints / <endpoint> [field=value ...]) printing one
# JSON line through fincept_json (NaN -> null) with akshare's own prints kept off stdout.
from akshare_screen_args import (between_dates, call_first_with_rows, recent_weekdays, run_endpoint_table,
                                 safe_call)

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

def get_futures_contract_detail(symbol):
    """Contract specification of one listed contract (e.g. RB2701, M2705). A variety code alone ("A",
    the old default) is not a contract, so SYMBOL is required."""
    return safe_call(ak.futures_contract_detail, symbol=symbol.strip().upper())

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

# Sina's continuous main-contract codes (<variety>0) always point at the current main contract, so
# this default never expires (akshare's own default, V2309, expired in 2023).
MAIN_CONTRACTS = "RB0,HC0,I0,CU0,AL0,AU0,AG0,M0,Y0,V0,TA0,MA0,SR0,CF0"


def get_futures_zh_spot(symbol=MAIN_CONTRACTS):
    """Realtime quotes of Chinese commodity futures. SYMBOL: comma-separated contracts (RB2701,
    M2705) or continuous main contracts (RB0); default the main contract of 14 major varieties."""
    return safe_call(ak.futures_zh_spot, symbol=symbol.replace(" ", "").upper(), market="CF", adjust="0")

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

def get_futures_zh_minute_sina(symbol="RB0", period="1"):
    """Minute bars of one contract from Sina; RB0 = rebar main contract (the old default V2501 has
    expired). period is the bar length in minutes: 1, 5, 15, 30, 60."""
    return safe_call(ak.futures_zh_minute_sina, symbol=symbol.strip().upper(), period=period)

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

def get_futures_global_hist(symbol="HG00Y", start_date="", end_date=""):
    """Daily history of one global futures contract (EastMoney code from futures_global_spot, e.g.
    HG00Y = COMEX copper main); START/END narrow the range (akshare returns the full history)"""
    return safe_call(lambda: between_dates(ak.futures_global_hist_em(symbol=symbol), start_date, end_date))

# Sina codes of the most traded overseas contracts (NYMEX crude, COMEX gold/silver/copper, NYMEX gas,
# Brent); futures_foreign_symbols lists them all.
FOREIGN_DEFAULT = "CL,GC,SI,HG,NG,OIL"


def get_futures_foreign_commodity_realtime(symbol=FOREIGN_DEFAULT):
    """Realtime quotes of overseas commodity futures. SYMBOL: comma-separated Sina codes."""
    codes = [s.strip().upper() for s in symbol.split(",") if s.strip()]
    return safe_call(ak.futures_foreign_commodity_realtime, symbol=codes)

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
    return safe_call(ak.futures_warehouse_receipt_dce)

def get_futures_czce_warehouse_receipt():
    """Get CZCE warehouse receipts"""
    return safe_call(ak.futures_warehouse_receipt_czce)

def get_futures_gfex_warehouse_receipt():
    """Get GFEX warehouse receipts"""
    return safe_call(ak.futures_gfex_warehouse_receipt)


# ==================== POSITION RANK ====================

# Exchange position rankings of one trading day: START picks the day, default the newest weekday
# with data; SYMBOL narrows to one variety (M, I, SI, LC, ...). The rankings come back per contract
# and are joined into one table with a "contract" column.

def _ranking(func, date, **kwargs):
    if date:
        return safe_call(func, date=date, _key_column="contract", **kwargs)
    return call_first_with_rows(func, "date", recent_weekdays(), _key_column="contract", **kwargs)


def get_futures_dce_position_rank(symbol="", date=""):
    """DCE member position ranking"""
    return _ranking(ak.futures_dce_position_rank, date, **({"vars_list": [symbol.upper()]} if symbol else {}))

def get_futures_dce_position_rank_other(date=""):
    """DCE member position ranking (options and other products)"""
    return _ranking(ak.futures_dce_position_rank_other, date)

def get_futures_gfex_position_rank(symbol="", date=""):
    """GFEX member position ranking"""
    return _ranking(ak.futures_gfex_position_rank, date, **({"vars_list": [symbol.upper()]} if symbol else {}))

def get_futures_hold_pos_sina(symbol, date=""):
    """Member volume ranking of one contract (SYMBOL, e.g. RB2701) on one day (Sina); START picks the
    day, default the newest with data. A contract code cannot be guessed, so SYMBOL is required."""
    contract = symbol.strip().upper()
    if date:
        return safe_call(ak.futures_hold_pos_sina, symbol="成交量", contract=contract, date=date)
    return call_first_with_rows(ak.futures_hold_pos_sina, "date", recent_weekdays(), symbol="成交量",
                                contract=contract)


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

def get_futures_spot_price_daily(start_day="", end_day=""):
    """Spot vs futures prices per day; akshare fetches one page per day, so the default is the last 7
    days (the old 2020-2026 default meant ~1,700 requests and never finished)"""
    from datetime import timedelta
    from akshare_screen_args import china_today

    today = china_today()
    start = start_day or (today - timedelta(days=7)).strftime("%Y%m%d")
    return safe_call(ak.futures_spot_price_daily, start_day=start, end_day=end_day or today.strftime("%Y%m%d"))

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

def get_futures_contract_detail_em(symbol="A0"):
    """Get futures contract detail (EastMoney)"""
    return safe_call(ak.futures_contract_detail_em, symbol=symbol)


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

    # Additional
    "futures_contract_detail_em": {"func": get_futures_contract_detail_em, "desc": "Contract detail (EM)", "category": "Contract Info"},

    # Hog Futures
    "futures_hog_core": {"func": get_futures_hog_core, "desc": "Hog core data", "category": "Hog"},
    "futures_hog_cost": {"func": get_futures_hog_cost, "desc": "Hog cost data", "category": "Hog"},
    "futures_hog_supply": {"func": get_futures_hog_supply, "desc": "Hog supply data", "category": "Hog"},

    # Other
    "futures_news_shmet": {"func": get_futures_news_shmet, "desc": "SHMET news", "category": "Other"},
    "futures_settlement_sgx": {"func": get_futures_settlement_price_sgx, "desc": "SGX settlement prices", "category": "Other"},
}


def main():
    return run_endpoint_table(ENDPOINTS)


if __name__ == "__main__":
    sys.exit(main())
