#!/usr/bin/env python3
"""
AKShare Stocks Margin Trading & HSGT Data Wrapper (Batch 7)
Provides access to margin trading, HSGT (Stock Connect), and related data
~50 endpoints
"""

import sys
import json
import time
from datetime import datetime, timedelta

# safe_call: one akshare call -> {success, data, columns, count} or a plain-English error.
# run_endpoint_table: the CLI (get_all_endpoints / <endpoint> [field=value ...]) printing one
# JSON line through fincept_json (NaN -> null) with akshare's own prints kept off stdout.
from akshare_screen_args import china_today, run_endpoint_table, safe_call

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


# ==================== MARGIN TRADING SSE ====================

def _window(start_date, end_date, days):
    """(start, end) as YYYYMMDD: the dates typed, else the last `days` days up to today."""
    today = china_today()
    return (start_date or (today - timedelta(days=days)).strftime("%Y%m%d"),
            end_date or today.strftime("%Y%m%d"))


def get_stock_margin_sse(start_date="", end_date=""):
    """SSE margin-trading totals per day; default the last 90 days"""
    start, end = _window(start_date, end_date, 90)
    return safe_call(ak.stock_margin_sse, start_date=start, end_date=end)

def get_stock_margin_detail_sse(date="20240101"):
    """Get SSE margin trading detail"""
    return safe_call(ak.stock_margin_detail_sse, date=date)


# ==================== MARGIN TRADING SZSE ====================

def get_stock_margin_szse(date="20240101"):
    """Get SZSE margin trading data"""
    return safe_call(ak.stock_margin_szse, date=date)

def get_stock_margin_detail_szse(date="20240101"):
    """Get SZSE margin trading detail"""
    return safe_call(ak.stock_margin_detail_szse, date=date)

def get_stock_margin_underlying_info_szse():
    """Get SZSE margin underlying info"""
    return safe_call(ak.stock_margin_underlying_info_szse)


# ==================== MARGIN ACCOUNT ====================

def get_stock_margin_account_info():
    """Get margin account info"""
    return safe_call(ak.stock_margin_account_info)

def get_stock_margin_ratio_pa():
    """Get margin ratio (PA)"""
    return safe_call(ak.stock_margin_ratio_pa)


# ==================== HSGT (沪深港通) ====================

def get_stock_hsgt_hist_em(symbol="沪股通"):
    """Get HSGT historical data (EastMoney)"""
    return safe_call(ak.stock_hsgt_hist_em, symbol=symbol)

def get_stock_hsgt_fund_flow_summary_em():
    """Get HSGT fund flow summary (EastMoney)"""
    return safe_call(ak.stock_hsgt_fund_flow_summary_em)

def get_stock_hsgt_fund_min_em():
    """Get HSGT fund minute data (EastMoney)"""
    return safe_call(ak.stock_hsgt_fund_min_em)

def get_stock_hsgt_board_rank_em(symbol="北向资金增持行业板块排行", indicator="今日"):
    """Get HSGT board ranking (EastMoney)"""
    return safe_call(ak.stock_hsgt_board_rank_em, symbol=symbol, indicator=indicator)

def get_stock_hsgt_hold_stock_em(market="北向", indicator="今日排行"):
    """Get HSGT stock holdings (EastMoney)"""
    return safe_call(ak.stock_hsgt_hold_stock_em, market=market, indicator=indicator)

def get_stock_hsgt_stock_statistics_em(symbol="北向持股", start_date="20240101", end_date="20241231"):
    """Get HSGT stock statistics (EastMoney)"""
    return safe_call(ak.stock_hsgt_stock_statistics_em, symbol=symbol, start_date=start_date, end_date=end_date)

def get_stock_hsgt_institution_statistics_em(market="北向持股", start_date="", end_date=""):
    """Stock Connect holdings by institution per day (EastMoney). market: 北向持股/南向持股/沪股通持股/
    深股通持股; default the last 30 days"""
    start, end = _window(start_date, end_date, 30)
    return safe_call(ak.stock_hsgt_institution_statistics_em, market=market, start_date=start, end_date=end)

def get_stock_hsgt_individual_em(symbol="000001"):
    """Get HSGT individual stock (EastMoney)"""
    return safe_call(ak.stock_hsgt_individual_em, symbol=symbol)

def get_stock_hsgt_individual_detail_em(symbol="000001", start_date="20240101", end_date="20241231"):
    """Get HSGT individual stock detail (EastMoney)"""
    return safe_call(ak.stock_hsgt_individual_detail_em, symbol=symbol, start_date=start_date, end_date=end_date)

def get_stock_hsgt_sh_hk_spot_em():
    """Get Shanghai-HK Stock Connect spot (EastMoney)"""
    return safe_call(ak.stock_hsgt_sh_hk_spot_em)


# ==================== SGT (深港通/沪港通) ====================

def get_stock_sgt_reference_exchange_rate_sse():
    """Get SSE reference exchange rate"""
    return safe_call(ak.stock_sgt_reference_exchange_rate_sse)

def get_stock_sgt_reference_exchange_rate_szse():
    """Get SZSE reference exchange rate"""
    return safe_call(ak.stock_sgt_reference_exchange_rate_szse)

def get_stock_sgt_settlement_exchange_rate_sse():
    """Get SSE settlement exchange rate"""
    return safe_call(ak.stock_sgt_settlement_exchange_rate_sse)

def get_stock_sgt_settlement_exchange_rate_szse():
    """Get SZSE settlement exchange rate"""
    return safe_call(ak.stock_sgt_settlement_exchange_rate_szse)


# ==================== REGISTER (注册制) ====================

def get_stock_register_kcb():
    """Get STAR Market registration"""
    return safe_call(ak.stock_register_kcb)

def get_stock_register_cyb():
    """Get ChiNext registration"""
    return safe_call(ak.stock_register_cyb)

def get_stock_register_bj():
    """Get Beijing Stock Exchange registration"""
    return safe_call(ak.stock_register_bj)

def get_stock_register_sh():
    """Get Shanghai registration"""
    return safe_call(ak.stock_register_sh)

def get_stock_register_sz():
    """Get Shenzhen registration"""
    return safe_call(ak.stock_register_sz)

def get_stock_register_db():
    """Get dual listing registration"""
    return safe_call(ak.stock_register_db)


# ==================== HK INDICATOR ====================

def get_stock_hk_indicator_eniu(symbol="00700", indicator="全部"):
    """Get HK stock indicator (Eniu)"""
    return safe_call(ak.stock_hk_indicator_eniu, symbol=symbol, indicator=indicator)

def get_stock_hk_gxl_lg():
    """Get HK dividend yield (LG)"""
    return safe_call(ak.stock_hk_gxl_lg)


# ==================== PE/PB MARKET DATA ====================

def get_stock_market_pe_lg(symbol="上证"):
    """Get market PE (LG)"""
    return safe_call(ak.stock_market_pe_lg, symbol=symbol)

def get_stock_market_pb_lg(symbol="上证"):
    """Get market PB (LG)"""
    return safe_call(ak.stock_market_pb_lg, symbol=symbol)

def get_stock_index_pe_lg(symbol="000001"):
    """Get index PE (LG)"""
    return safe_call(ak.stock_index_pe_lg, symbol=symbol)

def get_stock_index_pb_lg(symbol="000001"):
    """Get index PB (LG)"""
    return safe_call(ak.stock_index_pb_lg, symbol=symbol)

def get_stock_a_ttm_lyr():
    """Get A-share TTM and LYR data"""
    return safe_call(ak.stock_a_ttm_lyr)

def get_stock_a_all_pb():
    """Get A-share all PB data"""
    return safe_call(ak.stock_a_all_pb)

def get_stock_a_below_net_asset_statistics():
    """Get A-share below net asset statistics"""
    return safe_call(ak.stock_a_below_net_asset_statistics)


# ==================== BUFFETT INDEX ====================

def get_stock_buffett_index_lg():
    """Get Buffett index (LG)"""
    return safe_call(ak.stock_buffett_index_lg)


# ==================== EBS ====================

def get_stock_ebs_lg():
    """Get EBS data (LG)"""
    return safe_call(ak.stock_ebs_lg)


# ==================== CONGESTION ====================

def get_stock_a_congestion_lg():
    """Get A-share congestion index (LG)"""
    return safe_call(ak.stock_a_congestion_lg)

def get_stock_a_gxl_lg():
    """Get A-share dividend yield (LG)"""
    return safe_call(ak.stock_a_gxl_lg)


# ==================== A CODE UTILS ====================

def get_stock_a(symbol="沪A"):
    """Get A-shares list by market"""
    return safe_call(ak.stock_zh_a_spot_em)

def get_stock_a_code_to_symbol(symbol="000001"):
    """Get A-share code to symbol"""
    return safe_call(ak.stock_a_code_to_symbol, symbol=symbol)

def get_stock_a_high_low_statistics(market="all"):
    """Number of stocks at new highs / lows (Legulegu). market: all/sz50/hs300/zz500 (named `market` so
    a stock code in SYMBOL is not sent)"""
    return safe_call(ak.stock_a_high_low_statistics, symbol=market)


# ==================== MARKET ACTIVITY ====================

def get_stock_market_activity_legu():
    """Get market activity (LeGu)"""
    return safe_call(ak.stock_market_activity_legu)


# ==================== CYQE ====================

def get_stock_cyq_em(symbol="000001", adjust=""):
    """Get CYQ data (EastMoney)"""
    return safe_call(ak.stock_cyq_em, symbol=symbol, adjust=adjust)


# ==================== STAQ NET ====================

def get_stock_staq_net_stop():
    """Get STAQ Net stop data"""
    return safe_call(ak.stock_staq_net_stop)


# ==================== ENDPOINT REGISTRY ====================

ENDPOINTS = {
    # Margin Trading SSE
    "stock_margin_sse": {"func": get_stock_margin_sse, "desc": "SSE margin trading", "category": "Margin Trading"},
    "stock_margin_detail_sse": {"func": get_stock_margin_detail_sse, "desc": "SSE margin detail", "category": "Margin Trading"},

    # Margin Trading SZSE
    "stock_margin_szse": {"func": get_stock_margin_szse, "desc": "SZSE margin trading", "category": "Margin Trading"},
    "stock_margin_detail_szse": {"func": get_stock_margin_detail_szse, "desc": "SZSE margin detail", "category": "Margin Trading"},
    "stock_margin_underlying_info_szse": {"func": get_stock_margin_underlying_info_szse, "desc": "SZSE margin underlying", "category": "Margin Trading"},

    # Margin Account
    "stock_margin_account_info": {"func": get_stock_margin_account_info, "desc": "Margin account info", "category": "Margin Trading"},
    "stock_margin_ratio_pa": {"func": get_stock_margin_ratio_pa, "desc": "Margin ratio", "category": "Margin Trading"},

    # HSGT
    "stock_hsgt_hist_em": {"func": get_stock_hsgt_hist_em, "desc": "HSGT historical data", "category": "HSGT"},
    "stock_hsgt_fund_flow_summary_em": {"func": get_stock_hsgt_fund_flow_summary_em, "desc": "HSGT fund flow summary", "category": "HSGT"},
    "stock_hsgt_fund_min_em": {"func": get_stock_hsgt_fund_min_em, "desc": "HSGT fund minute", "category": "HSGT"},
    "stock_hsgt_board_rank_em": {"func": get_stock_hsgt_board_rank_em, "desc": "HSGT board ranking", "category": "HSGT"},
    "stock_hsgt_hold_stock_em": {"func": get_stock_hsgt_hold_stock_em, "desc": "HSGT holdings", "category": "HSGT"},
    "stock_hsgt_stock_statistics_em": {"func": get_stock_hsgt_stock_statistics_em, "desc": "HSGT stock statistics", "category": "HSGT"},
    "stock_hsgt_institution_statistics_em": {"func": get_stock_hsgt_institution_statistics_em, "desc": "HSGT institution stats", "category": "HSGT"},
    "stock_hsgt_individual_em": {"func": get_stock_hsgt_individual_em, "desc": "HSGT individual stock", "category": "HSGT"},
    "stock_hsgt_individual_detail_em": {"func": get_stock_hsgt_individual_detail_em, "desc": "HSGT individual detail", "category": "HSGT"},
    "stock_hsgt_sh_hk_spot_em": {"func": get_stock_hsgt_sh_hk_spot_em, "desc": "Shanghai-HK spot", "category": "HSGT"},

    # SGT Exchange Rate
    "stock_sgt_reference_exchange_rate_sse": {"func": get_stock_sgt_reference_exchange_rate_sse, "desc": "SSE reference rate", "category": "Exchange Rate"},
    "stock_sgt_reference_exchange_rate_szse": {"func": get_stock_sgt_reference_exchange_rate_szse, "desc": "SZSE reference rate", "category": "Exchange Rate"},
    "stock_sgt_settlement_exchange_rate_sse": {"func": get_stock_sgt_settlement_exchange_rate_sse, "desc": "SSE settlement rate", "category": "Exchange Rate"},
    "stock_sgt_settlement_exchange_rate_szse": {"func": get_stock_sgt_settlement_exchange_rate_szse, "desc": "SZSE settlement rate", "category": "Exchange Rate"},

    # Register
    "stock_register_kcb": {"func": get_stock_register_kcb, "desc": "STAR Market registration", "category": "Registration"},
    "stock_register_cyb": {"func": get_stock_register_cyb, "desc": "ChiNext registration", "category": "Registration"},
    "stock_register_bj": {"func": get_stock_register_bj, "desc": "Beijing registration", "category": "Registration"},
    "stock_register_sh": {"func": get_stock_register_sh, "desc": "Shanghai registration", "category": "Registration"},
    "stock_register_sz": {"func": get_stock_register_sz, "desc": "Shenzhen registration", "category": "Registration"},
    "stock_register_db": {"func": get_stock_register_db, "desc": "Dual listing registration", "category": "Registration"},

    # HK Indicator
    "stock_hk_indicator_eniu": {"func": get_stock_hk_indicator_eniu, "desc": "HK stock indicator (Eniu)", "category": "HK Indicator"},
    "stock_hk_gxl_lg": {"func": get_stock_hk_gxl_lg, "desc": "HK dividend yield", "category": "HK Indicator"},

    # PE/PB Market Data
    "stock_market_pe_lg": {"func": get_stock_market_pe_lg, "desc": "Market PE", "category": "PE/PB Data"},
    "stock_market_pb_lg": {"func": get_stock_market_pb_lg, "desc": "Market PB", "category": "PE/PB Data"},
    "stock_index_pe_lg": {"func": get_stock_index_pe_lg, "desc": "Index PE", "category": "PE/PB Data"},
    "stock_index_pb_lg": {"func": get_stock_index_pb_lg, "desc": "Index PB", "category": "PE/PB Data"},
    "stock_a_ttm_lyr": {"func": get_stock_a_ttm_lyr, "desc": "A-share TTM/LYR", "category": "PE/PB Data"},
    "stock_a_all_pb": {"func": get_stock_a_all_pb, "desc": "A-share all PB", "category": "PE/PB Data"},
    "stock_a_below_net_asset_statistics": {"func": get_stock_a_below_net_asset_statistics, "desc": "Below net asset stats", "category": "PE/PB Data"},

    # Buffett Index
    "stock_buffett_index_lg": {"func": get_stock_buffett_index_lg, "desc": "Buffett index", "category": "Market Indicators"},

    # EBS
    "stock_ebs_lg": {"func": get_stock_ebs_lg, "desc": "EBS data", "category": "Market Indicators"},

    # Congestion
    "stock_a_congestion_lg": {"func": get_stock_a_congestion_lg, "desc": "A-share congestion", "category": "Market Indicators"},
    "stock_a_gxl_lg": {"func": get_stock_a_gxl_lg, "desc": "A-share dividend yield", "category": "Market Indicators"},

    # A Code Utils
    "stock_a": {"func": get_stock_a, "desc": "A-shares by market", "category": "A-Share Utils"},
    "stock_a_code_to_symbol": {"func": get_stock_a_code_to_symbol, "desc": "Code to symbol", "category": "A-Share Utils"},
    "stock_a_high_low_statistics": {"func": get_stock_a_high_low_statistics, "desc": "High-low statistics", "category": "A-Share Utils"},

    # Market Activity
    "stock_market_activity_legu": {"func": get_stock_market_activity_legu, "desc": "Market activity", "category": "Market Activity"},

    # CYQ
    "stock_cyq_em": {"func": get_stock_cyq_em, "desc": "CYQ data", "category": "CYQ"},

    # STAQ
    "stock_staq_net_stop": {"func": get_stock_staq_net_stop, "desc": "STAQ Net stop", "category": "STAQ"},
}


def main():
    return run_endpoint_table(ENDPOINTS)


if __name__ == "__main__":
    sys.exit(main())
