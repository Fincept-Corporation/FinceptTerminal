#!/usr/bin/env python3
"""
AKShare Stocks Fund Flow & Capital Data Wrapper (Batch 5)
Provides access to fund flows, capital movement, block trades
~50 endpoints
"""

import sys
import json
import time
from datetime import datetime, timedelta

# safe_call: one akshare call -> {success, data, columns, count} or a plain-English error.
# run_endpoint_table: the CLI (get_all_endpoints / <endpoint> [field=value ...]) printing one
# JSON line through fincept_json (NaN -> null) with akshare's own prints kept off stdout.
from akshare_screen_args import call_first_with_rows, china_today, recent_weekdays, run_endpoint_table, safe_call

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


# ==================== INDIVIDUAL FUND FLOW ====================

def get_stock_individual_fund_flow(symbol="000001", market="sh"):
    """Get individual stock fund flow"""
    return safe_call(ak.stock_individual_fund_flow, stock=symbol, market=market)

def get_stock_individual_fund_flow_rank(indicator="今日"):
    """Get individual fund flow ranking"""
    return safe_call(ak.stock_individual_fund_flow_rank, indicator=indicator)


# ==================== MARKET FUND FLOW ====================

def get_stock_market_fund_flow():
    """Get market fund flow"""
    return safe_call(ak.stock_market_fund_flow)

def get_stock_main_fund_flow(symbol="全部股票"):
    """Get main fund flow"""
    return safe_call(ak.stock_main_fund_flow, symbol=symbol)


# ==================== FUND FLOW BY TYPE ====================

def get_stock_fund_flow_big_deal():
    """Large-order trades across the market (THS); akshare no longer filters by stock"""
    return safe_call(ak.stock_fund_flow_big_deal)

def get_stock_fund_flow_concept(symbol="3日排行"):
    """Get concept fund flow"""
    return safe_call(ak.stock_fund_flow_concept, symbol=symbol)

def get_stock_fund_flow_individual(symbol="即时"):
    """Get individual fund flow"""
    return safe_call(ak.stock_fund_flow_individual, symbol=symbol)

def get_stock_fund_flow_industry(symbol="即时"):
    """Get industry fund flow"""
    return safe_call(ak.stock_fund_flow_industry, symbol=symbol)

def get_stock_concept_fund_flow_hist(symbol="人工智能"):
    """Get concept fund flow history"""
    return safe_call(ak.stock_concept_fund_flow_hist, symbol=symbol)


# ==================== BLOCK TRADE (大宗交易) ====================

def get_stock_dzjy_mrmx(symbol="A股", start_date="2024-01-01", end_date="2024-12-31"):
    """Get block trade buy-sell details"""
    return safe_call(ak.stock_dzjy_mrmx, symbol=symbol, start_date=start_date, end_date=end_date)

def get_stock_dzjy_mrtj(start_date="2024-01-01", end_date="2024-12-31"):
    """Get block trade daily statistics"""
    return safe_call(ak.stock_dzjy_mrtj, start_date=start_date, end_date=end_date)

def get_stock_dzjy_sctj():
    """Get block trade market statistics"""
    return safe_call(ak.stock_dzjy_sctj)

def get_stock_dzjy_hygtj(symbol="近一月"):
    """Get block trade post-premium statistics"""
    return safe_call(ak.stock_dzjy_hygtj, symbol=symbol)

def get_stock_dzjy_hyyybtj(symbol="近三月"):
    """Get block trade broker statistics"""
    return safe_call(ak.stock_dzjy_hyyybtj, symbol=symbol)

def get_stock_dzjy_yybph(symbol="买入"):
    """Get block trade broker ranking"""
    return safe_call(ak.stock_dzjy_yybph, symbol=symbol)


# ==================== LHB (龙虎榜) ====================

def get_stock_lhb_detail_em(start_date="20240101", end_date="20241231"):
    """Get LHB detail (EastMoney)"""
    return safe_call(ak.stock_lhb_detail_em, start_date=start_date, end_date=end_date)

def get_stock_lhb_stock_detail_em(symbol="000001"):
    """Get LHB stock detail (EastMoney)"""
    return safe_call(ak.stock_lhb_stock_detail_em, symbol=symbol)

def get_stock_lhb_stock_detail_date_em(symbol="600077"):
    """Dates on which one stock was on the dragon-tiger list (EastMoney)"""
    return safe_call(ak.stock_lhb_stock_detail_date_em, symbol=symbol)

def get_stock_lhb_stock_statistic_em(symbol="近一月"):
    """Get LHB stock statistic (EastMoney)"""
    return safe_call(ak.stock_lhb_stock_statistic_em, symbol=symbol)

# The Sina dragon-tiger statistics take a look-back window in trading days ("5", "10", "30", "60").
# The parameter is named recent_day here so a stock code in the SYMBOL box is not sent as the window.

def get_stock_lhb_jgmx_sina():
    """Institutional seat trades on the dragon-tiger list (Sina)"""
    return safe_call(ak.stock_lhb_jgmx_sina)

def get_stock_lhb_jgzz_sina(recent_day="5"):
    """Institutional seat statistics over the last N trading days (Sina)"""
    return safe_call(ak.stock_lhb_jgzz_sina, symbol=recent_day)

def get_stock_lhb_ggtj_sina(recent_day="5"):
    """Per-stock dragon-tiger statistics over the last N trading days (Sina)"""
    return safe_call(ak.stock_lhb_ggtj_sina, symbol=recent_day)

def get_stock_lhb_yytj_sina(recent_day="5"):
    """Broker-branch statistics over the last N trading days (Sina)"""
    return safe_call(ak.stock_lhb_yytj_sina, symbol=recent_day)

def get_stock_lhb_detail_daily_sina(date=""):
    """Dragon-tiger list of one trading day (Sina); START picks the day, default the latest"""
    if date:
        return safe_call(ak.stock_lhb_detail_daily_sina, date=date)
    return call_first_with_rows(ak.stock_lhb_detail_daily_sina, "date", recent_weekdays())

def get_stock_lhb_hyyyb_em():
    """Get LHB post-premium broker (EastMoney)"""
    return safe_call(ak.stock_lhb_hyyyb_em)

def get_stock_lhb_jgmmtj_em(start_date="", end_date=""):
    """Institutional buy/sell statistics on the dragon-tiger list (EastMoney); default the last 30 days"""
    end = end_date or china_today().strftime("%Y%m%d")
    start = start_date or (china_today() - timedelta(days=30)).strftime("%Y%m%d")
    return safe_call(ak.stock_lhb_jgmmtj_em, start_date=start, end_date=end)

def get_stock_lhb_jgstatistic_em():
    """Get LHB institutional statistics (EastMoney)"""
    return safe_call(ak.stock_lhb_jgstatistic_em)

def get_stock_lhb_traderstatistic_em(symbol="近一月"):
    """Get LHB trader statistics (EastMoney)"""
    return safe_call(ak.stock_lhb_traderstatistic_em, symbol=symbol)

def get_stock_lhb_yyb_detail_em():
    """Get LHB broker detail (EastMoney)"""
    return safe_call(ak.stock_lhb_yyb_detail_em)

def get_stock_lhb_yybph_em(symbol="近一月"):
    """Get LHB broker ranking (EastMoney)"""
    return safe_call(ak.stock_lhb_yybph_em, symbol=symbol)


# ==================== LH YYB (龙虎榜营业部) ====================

def get_stock_lh_yyb_capital():
    """Dragon-tiger broker branches ranked by capital (THS)"""
    return safe_call(ak.stock_lh_yyb_capital)

def get_stock_lh_yyb_control():
    """Dragon-tiger broker branches ranked by control (THS)"""
    return safe_call(ak.stock_lh_yyb_control)

def get_stock_lh_yyb_most():
    """Most active dragon-tiger broker branches (THS)"""
    return safe_call(ak.stock_lh_yyb_most)


# ==================== REPURCHASE ====================

def get_stock_repurchase_em():
    """Share repurchases of all A-shares (EastMoney)"""
    return safe_call(ak.stock_repurchase_em)


# ==================== ALLOTMENT ====================

def get_stock_allotment_cninfo(symbol="000001"):
    """Get allotment data (CNINFO)"""
    return safe_call(ak.stock_allotment_cninfo, symbol=symbol)

def get_stock_pg_em():
    """Rights issues of all A-shares (EastMoney)"""
    return safe_call(ak.stock_pg_em)


# ==================== IPO ====================

def get_stock_ipo_info(symbol="688981"):
    """Get IPO info"""
    return safe_call(ak.stock_ipo_info, stock=symbol)

def get_stock_ipo_declare():
    """Get IPO declaration"""
    return safe_call(ak.stock_ipo_declare_em)

def get_stock_ipo_summary_cninfo():
    """Get IPO summary (CNINFO)"""
    return safe_call(ak.stock_ipo_summary_cninfo)

def get_stock_ipo_benefit_ths():
    """Get IPO benefit (THS)"""
    return safe_call(ak.stock_ipo_benefit_ths)

def get_stock_ipo_review_em():
    """Get IPO review (EastMoney)"""
    return safe_call(ak.stock_ipo_review_em)

def get_stock_ipo_tutor_em():
    """Get IPO tutor (EastMoney)"""
    return safe_call(ak.stock_ipo_tutor_em)


# ==================== NEW ISSUES ====================

def get_stock_new_gh_cninfo():
    """Get new stock subscription (CNINFO)"""
    return safe_call(ak.stock_new_gh_cninfo)

def get_stock_new_ipo_cninfo():
    """Get new IPO (CNINFO)"""
    return safe_call(ak.stock_new_ipo_cninfo)

def get_stock_dxsyl_em():
    """Get new stock yield (EastMoney)"""
    return safe_call(ak.stock_dxsyl_em)

def get_stock_xgsr_ths():
    """Get new stock subscription return (THS)"""
    return safe_call(ak.stock_xgsr_ths)

def get_stock_xgsglb_em():
    """Get new stock subscription list (EastMoney)"""
    return safe_call(ak.stock_xgsglb_em)

def get_stock_zh_a_new():
    """Get new A-share stocks"""
    return safe_call(ak.stock_zh_a_new)

def get_stock_zh_a_new_em():
    """Get new A-share stocks (EastMoney)"""
    return safe_call(ak.stock_zh_a_new_em)

def get_stock_new_a_spot_em():
    """Get new A-share spot data (EastMoney)"""
    return safe_call(ak.stock_new_a_spot_em)

def get_stock_zt_pool_sub_new_em():
    """Get new stock limit-up pool (EastMoney)"""
    return safe_call(ak.stock_zt_pool_sub_new_em)


# ==================== ADDITIONAL FUND FLOW ====================

def get_stock_hsgt_fund_flow_summary_em():
    """Get HSGT fund flow summary (EastMoney)"""
    return safe_call(ak.stock_hsgt_fund_flow_summary_em)

def get_stock_sector_fund_flow_hist(symbol="汽车服务"):
    """Fund-flow history of one industry board (EastMoney board name, see stock_sector_fund_flow_rank)"""
    return safe_call(ak.stock_sector_fund_flow_hist, symbol=symbol)

def get_stock_sector_fund_flow_rank(indicator="5日排行", sector_type="行业资金流"):
    """Get sector fund flow ranking"""
    return safe_call(ak.stock_sector_fund_flow_rank, indicator=indicator, sector_type=sector_type)

def get_stock_sector_fund_flow_summary(symbol="全部"):
    """Get sector fund flow summary"""
    return safe_call(ak.stock_sector_fund_flow_summary, symbol=symbol)


# ==================== OPTION LHB ====================

def get_option_lhb_em(symbol="沪"):
    """Get option LHB data (EastMoney)"""
    return safe_call(ak.option_lhb_em, symbol=symbol)


# ==================== ENDPOINT REGISTRY ====================

ENDPOINTS = {
    # Individual Fund Flow
    "stock_individual_fund_flow": {"func": get_stock_individual_fund_flow, "desc": "Individual fund flow", "category": "Individual Fund Flow"},
    "stock_individual_fund_flow_rank": {"func": get_stock_individual_fund_flow_rank, "desc": "Individual fund flow ranking", "category": "Individual Fund Flow"},

    # Market Fund Flow
    "stock_market_fund_flow": {"func": get_stock_market_fund_flow, "desc": "Market fund flow", "category": "Market Fund Flow"},
    "stock_main_fund_flow": {"func": get_stock_main_fund_flow, "desc": "Main fund flow", "category": "Market Fund Flow"},

    # Fund Flow by Type
    "stock_fund_flow_big_deal": {"func": get_stock_fund_flow_big_deal, "desc": "Big deal fund flow", "category": "Fund Flow Types"},
    "stock_fund_flow_concept": {"func": get_stock_fund_flow_concept, "desc": "Concept fund flow", "category": "Fund Flow Types"},
    "stock_fund_flow_individual": {"func": get_stock_fund_flow_individual, "desc": "Individual fund flow", "category": "Fund Flow Types"},
    "stock_fund_flow_industry": {"func": get_stock_fund_flow_industry, "desc": "Industry fund flow", "category": "Fund Flow Types"},
    "stock_concept_fund_flow_hist": {"func": get_stock_concept_fund_flow_hist, "desc": "Concept fund flow history", "category": "Fund Flow Types"},

    # Block Trade
    "stock_dzjy_mrmx": {"func": get_stock_dzjy_mrmx, "desc": "Block trade buy-sell details", "category": "Block Trade"},
    "stock_dzjy_mrtj": {"func": get_stock_dzjy_mrtj, "desc": "Block trade daily stats", "category": "Block Trade"},
    "stock_dzjy_sctj": {"func": get_stock_dzjy_sctj, "desc": "Block trade market stats", "category": "Block Trade"},
    "stock_dzjy_hygtj": {"func": get_stock_dzjy_hygtj, "desc": "Block trade premium stats", "category": "Block Trade"},
    "stock_dzjy_hyyybtj": {"func": get_stock_dzjy_hyyybtj, "desc": "Block trade broker stats", "category": "Block Trade"},
    "stock_dzjy_yybph": {"func": get_stock_dzjy_yybph, "desc": "Block trade broker ranking", "category": "Block Trade"},

    # LHB (龙虎榜)
    "stock_lhb_detail_em": {"func": get_stock_lhb_detail_em, "desc": "LHB detail (EastMoney)", "category": "LHB"},
    "stock_lhb_stock_detail_em": {"func": get_stock_lhb_stock_detail_em, "desc": "LHB stock detail", "category": "LHB"},
    "stock_lhb_stock_detail_date_em": {"func": get_stock_lhb_stock_detail_date_em, "desc": "LHB stock by date", "category": "LHB"},
    "stock_lhb_stock_statistic_em": {"func": get_stock_lhb_stock_statistic_em, "desc": "LHB stock statistics", "category": "LHB"},
    "stock_lhb_jgmx_sina": {"func": get_stock_lhb_jgmx_sina, "desc": "LHB institutional (Sina)", "category": "LHB"},
    "stock_lhb_jgzz_sina": {"func": get_stock_lhb_jgzz_sina, "desc": "LHB positions (Sina)", "category": "LHB"},
    "stock_lhb_ggtj_sina": {"func": get_stock_lhb_ggtj_sina, "desc": "LHB stock stats (Sina)", "category": "LHB"},
    "stock_lhb_yytj_sina": {"func": get_stock_lhb_yytj_sina, "desc": "LHB broker stats (Sina)", "category": "LHB"},
    "stock_lhb_detail_daily_sina": {"func": get_stock_lhb_detail_daily_sina, "desc": "LHB daily detail (Sina)", "category": "LHB"},
    "stock_lhb_hyyyb_em": {"func": get_stock_lhb_hyyyb_em, "desc": "LHB premium broker", "category": "LHB"},
    "stock_lhb_jgmmtj_em": {"func": get_stock_lhb_jgmmtj_em, "desc": "LHB institutional stats", "category": "LHB"},
    "stock_lhb_jgstatistic_em": {"func": get_stock_lhb_jgstatistic_em, "desc": "LHB institution statistics", "category": "LHB"},
    "stock_lhb_traderstatistic_em": {"func": get_stock_lhb_traderstatistic_em, "desc": "LHB trader statistics", "category": "LHB"},
    "stock_lhb_yyb_detail_em": {"func": get_stock_lhb_yyb_detail_em, "desc": "LHB broker detail", "category": "LHB"},
    "stock_lhb_yybph_em": {"func": get_stock_lhb_yybph_em, "desc": "LHB broker ranking", "category": "LHB"},

    # LH YYB
    "stock_lh_yyb_capital": {"func": get_stock_lh_yyb_capital, "desc": "LHB broker capital", "category": "LHB Broker"},
    "stock_lh_yyb_control": {"func": get_stock_lh_yyb_control, "desc": "LHB broker control", "category": "LHB Broker"},
    "stock_lh_yyb_most": {"func": get_stock_lh_yyb_most, "desc": "LHB broker most", "category": "LHB Broker"},

    # Repurchase
    "stock_repurchase_em": {"func": get_stock_repurchase_em, "desc": "Stock repurchase", "category": "Repurchase"},

    # Allotment
    "stock_allotment_cninfo": {"func": get_stock_allotment_cninfo, "desc": "Allotment (CNINFO)", "category": "Allotment"},
    "stock_pg_em": {"func": get_stock_pg_em, "desc": "Allotment (EastMoney)", "category": "Allotment"},

    # IPO
    "stock_ipo_info": {"func": get_stock_ipo_info, "desc": "IPO info", "category": "IPO"},
    "stock_ipo_declare": {"func": get_stock_ipo_declare, "desc": "IPO declaration", "category": "IPO"},
    "stock_ipo_summary_cninfo": {"func": get_stock_ipo_summary_cninfo, "desc": "IPO summary (CNINFO)", "category": "IPO"},
    "stock_ipo_benefit_ths": {"func": get_stock_ipo_benefit_ths, "desc": "IPO benefit (THS)", "category": "IPO"},
    "stock_ipo_review_em": {"func": get_stock_ipo_review_em, "desc": "IPO review", "category": "IPO"},
    "stock_ipo_tutor_em": {"func": get_stock_ipo_tutor_em, "desc": "IPO tutor", "category": "IPO"},

    # New Issues
    "stock_new_gh_cninfo": {"func": get_stock_new_gh_cninfo, "desc": "New stock subscription", "category": "New Issues"},
    "stock_new_ipo_cninfo": {"func": get_stock_new_ipo_cninfo, "desc": "New IPO (CNINFO)", "category": "New Issues"},
    "stock_dxsyl_em": {"func": get_stock_dxsyl_em, "desc": "New stock yield", "category": "New Issues"},
    "stock_xgsr_ths": {"func": get_stock_xgsr_ths, "desc": "New stock return (THS)", "category": "New Issues"},
    "stock_xgsglb_em": {"func": get_stock_xgsglb_em, "desc": "New stock list", "category": "New Issues"},
    "stock_zh_a_new": {"func": get_stock_zh_a_new, "desc": "New A-share stocks", "category": "New Issues"},
    "stock_zh_a_new_em": {"func": get_stock_zh_a_new_em, "desc": "New A-share (EastMoney)", "category": "New Issues"},
    "stock_new_a_spot_em": {"func": get_stock_new_a_spot_em, "desc": "New A-share spot", "category": "New Issues"},
    "stock_zt_pool_sub_new_em": {"func": get_stock_zt_pool_sub_new_em, "desc": "New stock limit-up pool", "category": "New Issues"},

    # Additional Fund Flow
    "stock_hsgt_fund_flow_summary_em": {"func": get_stock_hsgt_fund_flow_summary_em, "desc": "HSGT fund flow summary", "category": "Fund Flow Types"},
    "stock_sector_fund_flow_hist": {"func": get_stock_sector_fund_flow_hist, "desc": "Sector fund flow history", "category": "Fund Flow Types"},
    "stock_sector_fund_flow_rank": {"func": get_stock_sector_fund_flow_rank, "desc": "Sector fund flow ranking", "category": "Fund Flow Types"},
    "stock_sector_fund_flow_summary": {"func": get_stock_sector_fund_flow_summary, "desc": "Sector fund flow summary", "category": "Fund Flow Types"},

    # Option LHB
    "option_lhb_em": {"func": get_option_lhb_em, "desc": "Option LHB data", "category": "LHB"},
}


def main():
    return run_endpoint_table(ENDPOINTS)


if __name__ == "__main__":
    sys.exit(main())
