"""
AKShare Stock Analysis Wrapper ("Market Analysis" source of the AkShare screen)

Valuation, fund-flow, cost-distribution, research and sentiment data for China A-shares, served by
the current top-level akshare functions (the old akshare.stock_feature package no longer exists).

Usage:  python akshare_analysis.py <endpoint> [symbol] [start_date] [end_date]
        python akshare_analysis.py get_all_endpoints
  symbol: 6-digit A-share code (e.g. 000001); only per-stock endpoints use it.
  dates:  YYYY-MM-DD or YYYYMMDD; only date-range endpoints use them.
Output: {"success": true, "data": [rows], "columns": [...], "count": N} or {"success": false, "error": ...}
All endpoints are free - no API key.
"""

import io
import json
import re
import sys
import time
from datetime import date, datetime, timedelta

try:
    import akshare as ak
    import pandas as pd
except ImportError as e:
    print(json.dumps({"success": False, "error": f"Missing dependency: {e}", "data": []}))
    sys.exit(1)


# ── argument helpers ─────────────────────────────────────────────────────────

def _symbol(args, default="000001"):
    """First argument when it is a 6-digit A-share code, else the default."""
    if args and re.fullmatch(r"\d{6}", args[0].strip()):
        return args[0].strip()
    return default


def _exchange_symbol(code):
    """000001 -> sz000001, 600000 -> sh600000, 830799 -> bj830799."""
    if code[0] in "69":
        return "sh" + code
    if code[0] in "48":
        return "bj" + code
    return "sz" + code


def _day(text):
    t = text.strip().replace("-", "")
    return t if re.fullmatch(r"\d{8}", t) else None


def _range(args, default_days):
    """(start, end) as YYYYMMDD from args[1], args[2]; default: the last default_days days."""
    end = (_day(args[2]) if len(args) > 2 else None) or date.today().strftime("%Y%m%d")
    start = (_day(args[1]) if len(args) > 1 else None)
    if not start:
        start = (datetime.strptime(end, "%Y%m%d") - timedelta(days=default_days)).strftime("%Y%m%d")
    return start, end


def _period_ends(months, count):
    """The last `count` period ends (YYYYMMDD) on the given month-ends, newest first."""
    out, d = [], date.today()
    while len(out) < count:
        for m in sorted(months, reverse=True):
            end = date(d.year, m, 31 if m in (3, 12) else 30)
            if end < date.today() and end.strftime("%Y%m%d") not in out:
                out.append(end.strftime("%Y%m%d"))
                if len(out) == count:
                    break
        d = date(d.year - 1, 12, 31)
    return out


def _first_nonempty(fetch, candidates):
    """Call fetch(c) for each candidate (newest report period / trading day first) until one has rows."""
    last = None
    for c in candidates:
        df = fetch(c)
        if isinstance(df, pd.DataFrame) and not df.empty:
            return df
        last = df
    return last


def _recent_days(n=10):
    return [(date.today() - timedelta(days=i)).strftime("%Y%m%d") for i in range(n)]


QUARTERS = (3, 6, 9, 12)


# ── endpoints: name -> (category, description, fetch(args) -> DataFrame) ─────

# Not offered (akshare 1.20.36 has the function, but the upstream answer no longer parses):
#   ESG ratings (stock_esg_rate_sina), Xueqiu hot stocks / insider trades (stock_hot_follow_xq,
#   stock_inner_trade_xq: Xueqiu now needs a logged-in session), Baidu bull/bear poll
#   (stock_zh_vote_baidu: KeyError 'voteRecords').
ENDPOINTS = {
    # Valuation
    "get_stock_valuation_metrics": ("Valuation", "Per-stock PE/PB/PS valuation history (EastMoney)",
                                    lambda a: ak.stock_value_em(symbol=_symbol(a))),
    "get_buffett_indicator": ("Valuation", "Buffett indicator: total market cap / GDP (Legulegu)",
                              lambda a: ak.stock_buffett_index_lg()),
    "get_ttm_lyr_ratios": ("Valuation", "A-share equal-weight and median PE (TTM / LYR)",
                           lambda a: ak.stock_a_ttm_lyr()),
    "get_market_pb_statistics": ("Valuation", "A-share equal-weight and median PB",
                                 lambda a: ak.stock_a_all_pb()),
    "get_below_net_asset_statistics": ("Valuation", "Stocks trading below net asset value - history",
                                       lambda a: ak.stock_a_below_net_asset_statistics(symbol="全部A股")),
    "get_high_low_analysis": ("Valuation", "Number of stocks at new highs / lows",
                              lambda a: ak.stock_a_high_low_statistics(symbol="all")),
    "get_valuation_analysis": ("Valuation", "Per-stock market capitalisation, last year (Baidu)",
                               lambda a: ak.stock_zh_valuation_baidu(symbol=_symbol(a), indicator="总市值",
                                                                     period="近一年")),
    # Fund flow
    "get_individual_stock_fund_flow": ("Fund Flow", "Individual stock fund flow, real time (THS)",
                                       lambda a: ak.stock_fund_flow_individual(symbol="即时")),
    "get_concept_fund_flow": ("Fund Flow", "Concept sector fund flow, real time (THS)",
                              lambda a: ak.stock_fund_flow_concept(symbol="即时")),
    "get_industry_fund_flow": ("Fund Flow", "Industry sector fund flow, real time (THS)",
                               lambda a: ak.stock_fund_flow_industry(symbol="即时")),
    "get_lhb_data": ("Fund Flow", "Dragon-tiger list (LHB) details for a date range, default last 7 days",
                     lambda a: ak.stock_lhb_detail_em(*_range(a, 7))),
    "get_hsgt_data": ("Fund Flow", "Shanghai/Shenzhen-Hong Kong Connect fund flow summary",
                      lambda a: ak.stock_hsgt_fund_flow_summary_em()),
    "get_margin_trading_data": ("Fund Flow", "SSE margin trading summary for a date range, default last 90 days",
                                lambda a: ak.stock_margin_sse(*_range(a, 90))),
    "get_investor_account_stats": ("Fund Flow", "Monthly new investor account statistics",
                                   lambda a: ak.stock_account_statistics_em()),
    # Cost distribution & order book
    "get_cost_layer_analysis": ("Cost Distribution & Order Book", "Per-stock chip (cost) distribution",
                                lambda a: ak.stock_cyq_em(symbol=_symbol(a))),
    "get_order_book_analysis": ("Cost Distribution & Order Book", "Per-stock bid/ask order book quote",
                                lambda a: ak.stock_bid_ask_em(symbol=_symbol(a))),
    # Corporate & research
    "get_profit_forecasts": ("Corporate & Research", "Analyst profit forecasts, all stocks",
                             lambda a: ak.stock_profit_forecast_em(symbol="")),
    "get_analyst_recommendations": ("Corporate & Research", "Analyst ranking for the current year",
                                    lambda a: ak.stock_analyst_rank_em(year=str(date.today().year))),
    "get_earnings_bulletins": ("Corporate & Research", "Earnings reports, latest reporting period",
                               lambda a: _first_nonempty(lambda d: ak.stock_yjbb_em(date=d), _period_ends(QUARTERS, 3))),
    "get_earnings_guidance": ("Corporate & Research", "Earnings forecasts (guidance), latest reporting period",
                              lambda a: _first_nonempty(lambda d: ak.stock_yjyg_em(date=d), _period_ends(QUARTERS, 3))),
    "get_shareholder_distribution": ("Corporate & Research", "Per-stock top-10 shareholders, latest reporting period",
                                     lambda a: _first_nonempty(
                                         lambda d: ak.stock_gdfx_top_10_em(symbol=_exchange_symbol(_symbol(a)), date=d),
                                         _period_ends(QUARTERS, 3))),
    "get_shareholder_count": ("Corporate & Research", "Number of shareholders, latest reporting period",
                              lambda a: _first_nonempty(lambda d: ak.stock_zh_a_gdhs(symbol=d), _period_ends(QUARTERS, 3))),
    "get_stock_pledge_info": ("Corporate & Research", "Share pledge market overview",
                              lambda a: ak.stock_gpzy_profile_em()),
    "get_research_reports": ("Corporate & Research", "Per-stock broker research reports",
                             lambda a: ak.stock_research_report_em(symbol=_symbol(a))),
    "get_disclosure_announcements": ("Corporate & Research", "Per-stock CNINFO disclosures, default last 365 days",
                                     lambda a: ak.stock_zh_a_disclosure_report_cninfo(
                                         symbol=_symbol(a), market="沪深京", start_date=_range(a, 365)[0],
                                         end_date=_range(a, 365)[1])),
    "get_dividend_distributions": ("Corporate & Research", "Dividend / bonus-share distributions, latest period",
                                   lambda a: _first_nonempty(lambda d: ak.stock_fhps_em(date=d), _period_ends((6, 12), 3))),
    "get_ipo_subscription_returns": ("Corporate & Research", "New-share (IPO) subscription returns",
                                     lambda a: ak.stock_dxsyl_em()),
    "get_secondary_offerings": ("Corporate & Research", "Secondary share offerings (增发)",
                                lambda a: ak.stock_qbzf_em()),
    "get_rights_issues": ("Corporate & Research", "Rights issues (配股)",
                          lambda a: ak.stock_pg_em()),
    # Sentiment
    "get_stock_comments": ("Sentiment", "Daily stock comments / scores, all stocks (EastMoney)",
                           lambda a: ak.stock_comment_em()),
    "get_limit_up_down_stats": ("Sentiment", "Limit-up stock pool, latest trading day",
                                lambda a: _first_nonempty(lambda d: ak.stock_zt_pool_em(date=d), _recent_days())),
}


def _to_result(df):
    if isinstance(df, pd.DataFrame):
        if df.empty:
            return {"success": True, "data": [], "columns": list(df.columns), "count": 0}
        df = df.replace([float("inf"), float("-inf")], None)
        df = df.astype(object).where(pd.notna(df), None)
        rows = df.to_dict(orient="records")
        return {"success": True, "data": rows, "columns": [str(c) for c in df.columns], "count": len(rows)}
    if isinstance(df, (list, dict)):
        return {"success": True, "data": df, "count": len(df) if isinstance(df, list) else 1}
    return {"success": False, "error": "No data returned", "data": []}


def run_endpoint(name, args):
    fetch = ENDPOINTS[name][2]
    for attempt in range(2):
        try:
            return _to_result(fetch(args))
        except Exception as e:  # network hiccups are retried once
            if attempt == 0:
                time.sleep(1)
                continue
            return {"success": False, "error": f"{name}: {e}", "data": []}


def get_all_endpoints():
    categories = {}
    for name, (category, _, _) in ENDPOINTS.items():
        categories.setdefault(category, []).append(name)
    return {
        "available_endpoints": list(ENDPOINTS),
        "categories": categories,
        "descriptions": {name: desc for name, (_, desc, _) in ENDPOINTS.items()},
        "total_count": len(ENDPOINTS),
    }


def main():
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")
    if len(sys.argv) < 2:
        result = {"success": False, "error": "Usage: python akshare_analysis.py <endpoint> [symbol] [start] [end]",
                  "available_endpoints": list(ENDPOINTS)}
    elif sys.argv[1] == "get_all_endpoints":
        result = get_all_endpoints()
    elif sys.argv[1] in ENDPOINTS:
        result = run_endpoint(sys.argv[1], sys.argv[2:])
    else:
        result = {"success": False, "error": f"Unknown endpoint: {sys.argv[1]}", "data": []}
    result["timestamp"] = int(time.time())
    print(json.dumps(result, ensure_ascii=False, default=str))


if __name__ == "__main__":
    main()
