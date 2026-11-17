"""
AKShare Data Wrapper ("Market Data" source of the AkShare screen)
A small cross-asset sampler: A/US/HK stock lists, ETFs, China macro, bonds, FX and futures.
Returns JSON output for Qt/C++ integration. All endpoints are free - no API keys required.

Usage:  python akshare_data.py get_all_endpoints
        python akshare_data.py <endpoint> [symbol=..] [start_date=..] [end_date=..]
  <endpoint> is one of the advertised names (stock_zh_spot, china_gdp, ...) or, for manual use,
  any wrapper method name with or without its get_ prefix (stock_zh_a_daily symbol=600519).
Output: one JSON line {"success", "data", "columns", "count"} or {"success": false, "error"}.
"""

import sys
from datetime import timedelta
from typing import Any, Dict

import akshare as ak

# safe_call: one akshare call -> {success, data, columns, count} or a plain-English error;
# run_wrapper_cli: get_all_endpoints / <endpoint> [field=value ...] -> one JSON line on stdout.
from akshare_screen_args import (between_dates, call_first_with_rows, china_today, recent_weekdays,
                                 run_wrapper_cli, safe_call)

# Sina's continuous main-contract codes (<variety>0) always point at the current main contract
# (akshare's own futures_zh_spot default, V2309, expired in 2023). Same list as akshare_futures.py.
MAIN_CONTRACTS = "RB0,HC0,I0,CU0,AL0,AU0,AG0,M0,Y0,V0,TA0,MA0,SR0,CF0"


def _ymd(day) -> str:
    return day.strftime("%Y%m%d")


def _window(start_date: str, end_date: str, days: int):
    """(start, end) as YYYYMMDD: the dates typed, else the last `days` days up to today."""
    today = china_today()
    return start_date or _ymd(today - timedelta(days=days)), end_date or _ymd(today)


class AKShareDataWrapper:
    """Cross-asset sampler of akshare functions"""

    def _safe_call(self, func, *args, **kwargs) -> Dict[str, Any]:
        return safe_call(func, *args, **kwargs)

    # ==================== STOCK MARKET DATA ====================

    def get_stock_zh_a_spot(self) -> Dict[str, Any]:
        """Get all Chinese A-shares real-time quotes"""
        return self._safe_call(ak.stock_zh_a_spot_em)

    def get_stock_zh_a_daily(self, symbol: str, start_date: str = "", end_date: str = "",
                             adjust: str = "") -> Dict[str, Any]:
        """Daily history of one A-share (6-digit code); default the last 365 days.
        adjust: "" (none), "qfq" or "hfq". (akshare renamed stock_zh_a_hist_em to stock_zh_a_hist.)"""
        start, end = _window(start_date, end_date, 365)
        return self._safe_call(ak.stock_zh_a_hist, symbol=symbol, period="daily", start_date=start,
                               end_date=end, adjust=adjust)

    def get_stock_us_spot(self) -> Dict[str, Any]:
        """Get all US stocks real-time quotes (15-min delayed)"""
        return self._safe_call(ak.stock_us_spot)

    def get_stock_us_daily(self, symbol: str, start_date: str = "", end_date: str = "",
                           adjust: str = "") -> Dict[str, Any]:
        """Daily history of one US stock (AAPL, .DJI); akshare returns it all, START/END narrow it"""
        return self._safe_call(lambda: between_dates(ak.stock_us_daily(symbol=symbol, adjust=adjust),
                                                     start_date, end_date))

    def get_stock_hk_spot(self) -> Dict[str, Any]:
        """Get all Hong Kong stocks real-time quotes"""
        return self._safe_call(ak.stock_hk_spot)

    def get_stock_hk_daily(self, symbol: str, start_date: str = "", end_date: str = "",
                           adjust: str = "") -> Dict[str, Any]:
        """Daily history of one HK stock (00700); akshare returns it all, START/END narrow it"""
        return self._safe_call(lambda: between_dates(ak.stock_hk_daily(symbol=symbol, adjust=adjust),
                                                     start_date, end_date))

    def get_stock_info_a_code_name(self) -> Dict[str, Any]:
        """Get all A-share symbols and names"""
        return self._safe_call(ak.stock_info_a_code_name)

    # ==================== FUND DATA ====================

    def get_fund_etf_spot(self) -> Dict[str, Any]:
        """Get all ETFs real-time quotes"""
        return self._safe_call(ak.fund_etf_spot_em)

    def get_fund_etf_hist(self, symbol: str, start_date: str = "", end_date: str = "",
                          adjust: str = "") -> Dict[str, Any]:
        """Daily history of one ETF (513500, 159901); default the last 365 days"""
        start, end = _window(start_date, end_date, 365)
        return self._safe_call(ak.fund_etf_hist_em, symbol=symbol, period="daily", start_date=start,
                               end_date=end, adjust=adjust)

    def get_fund_open_fund_rank(self) -> Dict[str, Any]:
        """Get open-end fund performance rankings"""
        return self._safe_call(ak.fund_open_fund_rank_em)

    def get_fund_manager(self) -> Dict[str, Any]:
        """Get fund manager information"""
        return self._safe_call(ak.fund_manager_em)

    # ==================== ECONOMIC INDICATORS ====================

    def get_macro_china_gdp(self) -> Dict[str, Any]:
        """Get China GDP data"""
        return self._safe_call(ak.macro_china_gdp)

    def get_macro_china_cpi(self) -> Dict[str, Any]:
        """Get China Consumer Price Index"""
        return self._safe_call(ak.macro_china_cpi)

    def get_macro_china_ppi(self) -> Dict[str, Any]:
        """Get China Producer Price Index"""
        return self._safe_call(ak.macro_china_ppi)

    def get_macro_china_pmi(self) -> Dict[str, Any]:
        """Get China Purchasing Managers Index"""
        return self._safe_call(ak.macro_china_pmi)

    def get_macro_china_shibor(self) -> Dict[str, Any]:
        """Get China SHIBOR interest rates"""
        return self._safe_call(ak.macro_china_shibor_all)

    # ==================== BOND MARKET DATA ====================

    def get_bond_zh_hs_spot(self) -> Dict[str, Any]:
        """Get China bond spot quotes"""
        return self._safe_call(ak.bond_zh_hs_spot)

    def get_bond_china_yield(self, start_date: str = "", end_date: str = "") -> Dict[str, Any]:
        """China bond yield curves (ChinaBond); default the last 180 days (akshare's default is 2020)"""
        start, end = _window(start_date, end_date, 180)
        return self._safe_call(ak.bond_china_yield, start_date=start, end_date=end)

    def get_bond_cb_index_jsl(self) -> Dict[str, Any]:
        """Get convertible bond index"""
        return self._safe_call(ak.bond_cb_index_jsl)

    # ==================== CURRENCY & FOREX DATA ====================

    def get_currency_boc_sina(self, start_date: str = "", end_date: str = "") -> Dict[str, Any]:
        """Bank of China USD/CNY rates (Sina); default the last 90 days (akshare's default is 2023;
        Sina pages slowly, so long ranges take minutes)"""
        start, end = _window(start_date, end_date, 90)
        return self._safe_call(ak.currency_boc_sina, symbol="美元", start_date=start, end_date=end)

    def get_forex_spot(self) -> Dict[str, Any]:
        """Get forex spot rates"""
        return self._safe_call(ak.forex_spot_em)

    # ==================== FUTURES & COMMODITIES DATA ====================

    def get_futures_zh_spot(self, symbol: str = MAIN_CONTRACTS) -> Dict[str, Any]:
        """Realtime quotes of Chinese commodity futures; SYMBOL: contracts (RB2701) or main contracts
        (RB0), comma-separated; default the main contract of 14 major varieties"""
        return self._safe_call(ak.futures_zh_spot, symbol=symbol.replace(" ", "").upper(), market="CF", adjust="0")

    def get_futures_zh_daily(self, symbol: str, start_date: str = "", end_date: str = "") -> Dict[str, Any]:
        """Daily history of one Chinese futures contract (RB0, CU2701) from Sina; START/END narrow it"""
        return self._safe_call(lambda: between_dates(ak.futures_zh_daily_sina(symbol=symbol), start_date, end_date))

    def get_futures_global_spot(self) -> Dict[str, Any]:
        """Get global futures spot data"""
        return self._safe_call(ak.futures_global_spot_em)

    # ==================== ALTERNATIVE DATA ====================

    def get_air_quality_hebei(self) -> Dict[str, Any]:
        """Get Hebei province air quality data"""
        return self._safe_call(ak.air_quality_hebei)

    def get_energy_oil(self) -> Dict[str, Any]:
        """Get oil price data"""
        return self._safe_call(ak.energy_oil_hist)

    # ==================== MARKET INDICES & ANALYTICS ====================

    def get_stock_industry_pe(self, date: str = "") -> Dict[str, Any]:
        """Industry PE ratios (CNINFO, CSRC classification) of one day; default the newest with data"""
        if date:
            return self._safe_call(ak.stock_industry_pe_ratio_cninfo, symbol="证监会行业分类", date=date)
        return call_first_with_rows(ak.stock_industry_pe_ratio_cninfo, "date", recent_weekdays(),
                                    symbol="证监会行业分类")

    def get_stock_industry_sw(self) -> Dict[str, Any]:
        """Get SW industry classification"""
        return self._safe_call(ak.stock_industry_clf_hist_sw)

    def get_stock_board_concept(self) -> Dict[str, Any]:
        """Get concept board classification"""
        return self._safe_call(ak.stock_board_concept_name_em)

    def get_stock_board_industry(self) -> Dict[str, Any]:
        """Get industry board classification"""
        return self._safe_call(ak.stock_board_industry_name_em)

    def get_stock_hot_rank(self) -> Dict[str, Any]:
        """Get hot stocks ranking"""
        return self._safe_call(ak.stock_hot_rank_em)

    def get_stock_hsgt(self) -> Dict[str, Any]:
        """Get North-South trading flow data"""
        return self._safe_call(ak.stock_hsgt_hist_em)

    # Removed: get_us_stock_name, get_macro_usa, get_energy_carbon called the akshare functions
    # get_us_stock_name, macro_usa and energy_carbon_domestic, which akshare no longer has (US names:
    # stock_us_spot; US macro series: akshare_macro.py; carbon markets: akshare_energy.py).

    # ==================== UTILITY FUNCTIONS ====================

    def get_all_available_endpoints(self) -> Dict[str, Any]:
        """The advertised endpoints (ENDPOINTS below)"""
        categories: Dict[str, list] = {}
        for name, (category, _) in ENDPOINTS.items():
            categories.setdefault(category, []).append(name)
        return {
            "success": True,
            "data": {
                "available_endpoints": list(ENDPOINTS),
                "total_count": len(ENDPOINTS),
                "categories": categories,
            },
        }


# Advertised endpoint -> (category, wrapper method). Any other wrapper method is reachable by name.
ENDPOINTS = {
    "stock_zh_spot": ("Stocks", "get_stock_zh_a_spot"),
    "stock_us_spot": ("Stocks", "get_stock_us_spot"),
    "stock_hk_spot": ("Stocks", "get_stock_hk_spot"),
    "stock_symbols": ("Stocks", "get_stock_info_a_code_name"),
    "fund_etf_spot": ("Funds", "get_fund_etf_spot"),
    "fund_manager": ("Funds", "get_fund_manager"),
    "china_gdp": ("Macro", "get_macro_china_gdp"),
    "china_cpi": ("Macro", "get_macro_china_cpi"),
    "china_pmi": ("Macro", "get_macro_china_pmi"),
    "bond_spot": ("Bonds", "get_bond_zh_hs_spot"),
    "bond_yield": ("Bonds", "get_bond_china_yield"),
    "currency_rates": ("Forex", "get_currency_boc_sina"),
    "forex_spot": ("Forex", "get_forex_spot"),
    "futures_spot": ("Futures", "get_futures_zh_spot"),
}


# ==================== CLI ====================

def main():
    wrapper = AKShareDataWrapper()
    # One JSON line on stdout (NaN -> null, dates as ISO text as before); akshare's prints go to stderr.
    return run_wrapper_cli(wrapper, wrapper.get_all_available_endpoints,
                           aliases={name: method for name, (_, method) in ENDPOINTS.items()},
                           ascii=True, iso_dates=True)


if __name__ == "__main__":
    sys.exit(main())
