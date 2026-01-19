#!/usr/bin/env python3
"""
AKShare Stocks Fund Flow & Capital Data Wrapper (Batch 5)
Provides access to fund flows, capital movement, block trades
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

def get_stock_fund_flow_big_deal(symbol="000001"):
    """Get big deal fund flow"""
    return safe_call(ak.stock_fund_flow_big_deal, symbol=symbol)

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

def get_stock_lhb_stock_detail_date_em(symbol="000001", date="20240101"):
    """Get LHB stock detail by date (EastMoney)"""
    return safe_call(ak.stock_lhb_stock_detail_date_em, symbol=symbol, date=date)

def get_stock_lhb_stock_statistic_em(symbol="近一月"):
    """Get LHB stock statistic (EastMoney)"""
    return safe_call(ak.stock_lhb_stock_statistic_em, symbol=symbol)

def get_stock_lhb_jgmx_sina(symbol="5"):
    """Get LHB institutional detail (Sina)"""
    return safe_call(ak.stock_lhb_jgmx_sina, recent_day=symbol)

def get_stock_lhb_jgzz_sina(symbol="5"):
    """Get LHB institutional positions (Sina)"""
    return safe_call(ak.stock_lhb_jgzz_sina, recent_day=symbol)

def get_stock_lhb_ggtj_sina(symbol="5"):
    """Get LHB stock statistics (Sina)"""
    return safe_call(ak.stock_lhb_ggtj_sina, recent_day=symbol)

def get_stock_lhb_yytj_sina(symbol="5"):
    """Get LHB broker statistics (Sina)"""
    return safe_call(ak.stock_lhb_yytj_sina, recent_day=symbol)

def get_stock_lhb_detail_daily_sina(trade_date="20240101"):
    """Get LHB daily detail (Sina)"""
    return safe_call(ak.stock_lhb_detail_daily_sina, trade_date=trade_date)

def get_stock_lhb_hyyyb_em():
    """Get LHB post-premium broker (EastMoney)"""
    return safe_call(ak.stock_lhb_hyyyb_em)

def get_stock_lhb_jgmmtj_em(symbol="近一月"):
    """Get LHB institutional buy-sell statistics (EastMoney)"""
    return safe_call(ak.stock_lhb_jgmmtj_em, symbol=symbol)

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

def get_stock_lh_yyb_capital(symbol="近一月"):
    """Get LHB broker capital"""
    return safe_call(ak.stock_lh_yyb_capital, symbol=symbol)

def get_stock_lh_yyb_control(symbol="近一月"):
    """Get LHB broker control"""
    return safe_call(ak.stock_lh_yyb_control, symbol=symbol)

def get_stock_lh_yyb_most(symbol="近一月"):
    """Get LHB broker most"""
    return safe_call(ak.stock_lh_yyb_most, symbol=symbol)


# ==================== REPURCHASE ====================

def get_stock_repurchase_em(symbol="A股"):
    """Get stock repurchase (EastMoney)"""
    return safe_call(ak.stock_repurchase_em, symbol=symbol)


# ==================== ALLOTMENT ====================

def get_stock_allotment_cninfo(symbol="000001"):
    """Get allotment data (CNINFO)"""
    return safe_call(ak.stock_allotment_cninfo, symbol=symbol)

def get_stock_pg_em(symbol="全部"):
    """Get allotment data (EastMoney)"""
    return safe_call(ak.stock_pg_em, symbol=symbol)


# ==================== IPO ====================

def get_stock_ipo_info(symbol="688981"):
    """Get IPO info"""
    return safe_call(ak.stock_ipo_info, stock=symbol)

def get_stock_ipo_declare():
    """Get IPO declaration"""
    return safe_call(ak.stock_ipo_declare)

def get_stock_ipo_summary_cninfo():
    """Get IPO summary (CNINFO)"""
    return safe_call(ak.stock_ipo_summary_cninfo)

def get_stock_ipo_benefit_ths():
    """Get IPO benefit (THS)"""
    return safe_call(ak.stock_ipo_benefit_ths)


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

    # New Issues
    "stock_new_gh_cninfo": {"func": get_stock_new_gh_cninfo, "desc": "New stock subscription", "category": "New Issues"},
    "stock_new_ipo_cninfo": {"func": get_stock_new_ipo_cninfo, "desc": "New IPO (CNINFO)", "category": "New Issues"},
    "stock_dxsyl_em": {"func": get_stock_dxsyl_em, "desc": "New stock yield", "category": "New Issues"},
    "stock_xgsr_ths": {"func": get_stock_xgsr_ths, "desc": "New stock return (THS)", "category": "New Issues"},
    "stock_xgsglb_em": {"func": get_stock_xgsglb_em, "desc": "New stock list", "category": "New Issues"},
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
