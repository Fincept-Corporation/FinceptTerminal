"""
AKShare Company Information API
Fetches detailed company information for stocks

Usage: python akshare_company_info.py <cn_basic|cn_profile|hk_profile> symbol=<code>
Output: one JSON line {"success": true, "data": {field: value}} or {"success": false, "error": ...}
"""

import sys

import akshare as ak
import pandas as pd

from akshare_screen_args import XUEQIU_LOGIN, missing_param, run_cli, safe_call, screen_values, unavailable


def _first_row(df):
    """The profile functions return a one-row table: that row as {field: value}."""
    if not isinstance(df, pd.DataFrame) or df.empty:
        return None
    return df.iloc[0].to_dict()


def _profile(fetch, symbol):
    """{"success", "data": {field: value}} for one company, an explicit empty result, or the error."""
    holder = {}

    def call():
        holder["data"] = fetch(symbol)

    failed = safe_call(call)
    if "data" not in holder:
        return failed
    if not holder["data"]:
        return {"success": True, "data": [], "count": 0}
    return {"success": True, "data": holder["data"]}


def get_cn_basic_info(symbol):
    """Basic info of a Chinese A-share (EastMoney): its item/value table as {item: value}"""
    def fetch(code):
        df = ak.stock_individual_info_em(symbol=code)
        if isinstance(df, pd.DataFrame) and {"item", "value"} <= set(df.columns):
            return {str(row["item"]): row["value"] for _, row in df.iterrows()}
        return _first_row(df)
    return _profile(fetch, symbol)


def get_cn_profile(symbol):
    """Detailed profile of a Chinese A-share (CNINFO)"""
    return _profile(lambda code: _first_row(ak.stock_profile_cninfo(symbol=code)), symbol)


def get_hk_profile(symbol):
    """Profile of a Hong Kong stock (EastMoney)"""
    return _profile(lambda code: _first_row(ak.stock_hk_company_profile_em(symbol=code)), symbol)


ENDPOINTS = {"cn_basic": get_cn_basic_info, "cn_profile": get_cn_profile, "hk_profile": get_hk_profile}

# US profiles came from Xueqiu, which now needs a login token the screen cannot supply (upstream code
# 400016). No longer listed; a stale saved selection gets this answer.
RETIRED = {"us_info": XUEQIU_LOGIN.format("us_info")}


def get_all_endpoints():
    """Get list of all available endpoints"""
    return {
        "success": True,
        "data": {
            "available_endpoints": list(ENDPOINTS),
            "total_count": len(ENDPOINTS),
            "categories": {
                "CN Stocks": ["cn_basic", "cn_profile"],
                "HK Stocks": ["hk_profile"],
            },
        },
        "count": len(ENDPOINTS),
    }


def main():
    def handler():
        if len(sys.argv) < 2:
            return {"success": False, "error": "Missing endpoint parameter", "error_type": "usage"}
        endpoint = sys.argv[1]
        if endpoint == "get_all_endpoints":
            return get_all_endpoints()
        if endpoint in RETIRED:
            return unavailable(RETIRED[endpoint])
        if endpoint not in ENDPOINTS:
            return {"success": False, "error": f"Unknown endpoint: {endpoint}"}
        # The screens send symbol=<code>; a bare positional code (manual CLI use) is accepted too.
        symbol = screen_values(sys.argv[2:]).get("symbol", "")
        if not symbol:
            return missing_param(endpoint, ["symbol"])
        return ENDPOINTS[endpoint](symbol)

    # One JSON line on stdout through fincept_json (NaN -> null); akshare's own prints go to stderr.
    return run_cli(handler, ascii=True)


if __name__ == "__main__":
    sys.exit(main())
