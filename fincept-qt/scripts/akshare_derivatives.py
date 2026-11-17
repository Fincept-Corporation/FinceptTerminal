"""
AKShare Derivatives Data Wrapper
Wrapper for options and derivatives market data
Returns JSON output for Qt/C++ integration

NOTE: Cleaned to contain only VALID akshare option functions (46 total).
"""

import sys
import json
import pandas as pd
import akshare as ak

# safe_call: one akshare call -> {success, data, columns, count} or a plain-English error;
# run_wrapper_cli: get_all_endpoints / <endpoint> [field=value ...] -> one JSON line on stdout.
from akshare_screen_args import call_first_with_rows, china_today, recent_weekdays, run_wrapper_cli, safe_call
from typing import Dict, Any, List
from datetime import datetime, timedelta, date


# ── contract months that never expire ────────────────────────────────────────

def _nth_weekday(year: int, month: int, weekday: int, n: int) -> date:
    first = date(year, month, 1)
    return first + timedelta(days=(weekday - first.weekday()) % 7 + 7 * (n - 1))


def _near_month(expiry_weekday: int, nth: int):
    """(year, month) of the nearest contract month that has not expired yet."""
    today = china_today()
    y, m = today.year, today.month
    if today > _nth_weekday(y, m, expiry_weekday, nth):
        y, m = (y + 1, 1) if m == 12 else (y, m + 1)
    return y, m


def _cffex_near(prefix: str) -> str:
    """Near-month CFFEX index option series (io/ho/mo + YYMM); they expire on the 3rd Friday."""
    y, m = _near_month(4, 3)
    return f"{prefix}{y % 100:02d}{m:02d}"


def _sse_near_month() -> str:
    """Near expiry month (YYYYMM) of the SSE/SZSE ETF options; they expire on the 4th Wednesday."""
    y, m = _near_month(2, 4)
    return f"{y:04d}{m:02d}"


# SYMBOL may be the ETF code; akshare's option_finance_board wants the product name.
_BOARD_NAMES = {
    "510050": "华夏上证50ETF期权", "510300": "华泰柏瑞沪深300ETF期权", "510500": "南方中证500ETF期权",
    "588000": "华夏科创50ETF期权", "588080": "易方达科创50ETF期权", "159919": "嘉实沪深300ETF期权",
}


def _sse_option_code(symbol: str) -> str:
    """The option code typed in SYMBOL, else the first near-month 300ETF call (one Sina request)."""
    if symbol.strip():
        return symbol.strip()
    codes = ak.option_sse_codes_sina(symbol="看涨期权", trade_date=_sse_near_month(), underlying="510300")
    column = "期权代码" if "期权代码" in codes.columns else codes.columns[-1]
    return str(codes[column].iloc[0])


def _latest_day(func, trade_date: str, param: str = "trade_date", **kwargs) -> Dict[str, Any]:
    """One trading day of an exchange report: the day typed in START, else the newest weekday that
    has data (today first; holidays come back empty and the day before is tried)."""
    if trade_date:
        return safe_call(func, **{param: trade_date}, **kwargs)
    return call_first_with_rows(func, param, recent_weekdays(), **kwargs)


class DerivativesWrapper:
    """Derivatives data wrapper with VALIDATED akshare functions (46 total)"""

    def __init__(self):
        self.default_timeout = 30
        self.retry_delay = 2

    def _safe_call_with_retry(self, func, *args, max_retries: int = 2, **kwargs) -> Dict[str, Any]:
        """One akshare call -> {success, data, columns, count} or a plain-English error (shared
        akshare_screen_args.safe_call: only a dropped connection or a timeout is retried, never a
        rate limit or a refusal)."""
        return safe_call(func, *args, _retries=max_retries, **kwargs)

    # ==================== CFFEX OPTIONS ====================

    def get_option_cffex_hs300_daily_sina(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_cffex_hs300_daily_sina)

    def get_option_cffex_hs300_list_sina(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_cffex_hs300_list_sina)

    def get_option_cffex_hs300_spot_sina(self, symbol: str = "") -> Dict[str, Any]:
        """Option chain of one CSI 300 index option month (io + YYMM), default the near month"""
        return self._safe_call_with_retry(ak.option_cffex_hs300_spot_sina, symbol=symbol or _cffex_near("io"))

    def get_option_cffex_sz50_daily_sina(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_cffex_sz50_daily_sina)

    def get_option_cffex_sz50_list_sina(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_cffex_sz50_list_sina)

    def get_option_cffex_sz50_spot_sina(self, symbol: str = "") -> Dict[str, Any]:
        """Option chain of one SSE 50 index option month (ho + YYMM), default the near month"""
        return self._safe_call_with_retry(ak.option_cffex_sz50_spot_sina, symbol=symbol or _cffex_near("ho"))

    def get_option_cffex_zz1000_daily_sina(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_cffex_zz1000_daily_sina)

    def get_option_cffex_zz1000_list_sina(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_cffex_zz1000_list_sina)

    def get_option_cffex_zz1000_spot_sina(self, symbol: str = "") -> Dict[str, Any]:
        """Option chain of one CSI 1000 index option month (mo + YYMM), default the near month"""
        return self._safe_call_with_retry(ak.option_cffex_zz1000_spot_sina, symbol=symbol or _cffex_near("mo"))

    # ==================== COMMODITY OPTIONS ====================

    def get_option_comm_info(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_comm_info)

    def get_option_comm_symbol(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_comm_symbol)

    def get_option_commodity_contract_sina(self, symbol: str = "豆粕期权") -> Dict[str, Any]:
        """Listed contracts of one commodity option (product name: 豆粕期权, 玉米期权, 黄金期权, ...)"""
        return self._safe_call_with_retry(ak.option_commodity_contract_sina, symbol=symbol)

    def get_option_commodity_contract_table_sina(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_commodity_contract_table_sina)

    def get_option_commodity_hist_sina(self, symbol: str) -> Dict[str, Any]:
        """Daily history of one commodity option contract (e.g. m2701C3000 - see
        option_commodity_contract_sina); a contract code cannot be guessed, so SYMBOL is required"""
        return self._safe_call_with_retry(ak.option_commodity_hist_sina, symbol=symbol)

    def get_option_contract_info_ctp(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_contract_info_ctp)

    # ==================== SSE/SZSE OPTIONS ====================

    def get_option_current_day_sse(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_current_day_sse)

    def get_option_current_day_szse(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_current_day_szse)

    def get_option_current_em(self) -> Dict[str, Any]:
        """Quotes of every listed option (EastMoney); akshare no longer filters by underlying"""
        return self._safe_call_with_retry(ak.option_current_em)

    def get_option_daily_stats_sse(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_daily_stats_sse)

    def get_option_daily_stats_szse(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_daily_stats_szse)

    # ==================== FINANCE OPTIONS ====================

    def get_option_finance_board(self, symbol: str = "510300") -> Dict[str, Any]:
        """Today's board of one ETF/index option product (ETF code or product name), near month"""
        name = _BOARD_NAMES.get(symbol.strip(), symbol.strip())
        return self._safe_call_with_retry(ak.option_finance_board, symbol=name, end_month=_sse_near_month()[2:])

    def get_option_finance_minute_sina(self, symbol: str = "") -> Dict[str, Any]:
        """Intraday minutes of one SSE option contract (8-digit code), default a near-month 300ETF call"""
        return self._safe_call_with_retry(lambda: ak.option_finance_minute_sina(symbol=_sse_option_code(symbol)))

    def get_option_finance_sse_underlying(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_finance_sse_underlying)

    # ==================== HISTORICAL DATA ====================

    # SYMBOL is the exchange's option product name (白糖期权, 豆粕期权, 工业硅, 铜期权, ...);
    # START picks the trading day, default the latest one with data.

    def get_option_hist_czce(self, symbol: str = "白糖期权", trade_date: str = "") -> Dict[str, Any]:
        return _latest_day(ak.option_hist_czce, trade_date, symbol=symbol)

    def get_option_hist_dce(self, symbol: str = "豆粕期权", trade_date: str = "") -> Dict[str, Any]:
        return _latest_day(ak.option_hist_dce, trade_date, symbol=symbol)

    def get_option_hist_gfex(self, symbol: str = "工业硅", trade_date: str = "") -> Dict[str, Any]:
        return _latest_day(ak.option_hist_gfex, trade_date, symbol=symbol)

    def get_option_hist_shfe(self, symbol: str = "铜期权", trade_date: str = "") -> Dict[str, Any]:
        return _latest_day(ak.option_hist_shfe, trade_date, symbol=symbol)

    def get_option_hist_yearly_czce(self, symbol: str = "CF", year: str = "2024") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_hist_yearly_czce, symbol=symbol, year=year)

    # ==================== LHB & MARGIN ====================

    def get_option_lhb_em(self, symbol: str = "510300", trade_date: str = "") -> Dict[str, Any]:
        """Option leaderboard (put volume by member) for 510050 / 510300 / 159919, latest trading day"""
        return _latest_day(ak.option_lhb_em, trade_date, symbol=symbol, indicator="期权交易情况-认沽交易量")

    def get_option_margin(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_margin)

    def get_option_margin_symbol(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_margin_symbol)

    # ==================== MINUTE & ANALYSIS ====================

    def get_option_minute_em(self, symbol: str) -> Dict[str, Any]:
        """Intraday minutes of one option (EastMoney code, e.g. MO2701-P-6000 or 10009999); SYMBOL is required"""
        return self._safe_call_with_retry(ak.option_minute_em, symbol=symbol)

    # The EastMoney analysis tables cover every listed option; akshare dropped the underlying filter.

    def get_option_premium_analysis_em(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_premium_analysis_em)

    def get_option_risk_analysis_em(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_risk_analysis_em)

    def get_option_risk_indicator_sse(self, date: str = "") -> Dict[str, Any]:
        """SSE option risk indicators (greeks, implied vol) of one trading day; START picks the day"""
        return _latest_day(ak.option_risk_indicator_sse, date, param="date")

    def get_option_value_analysis_em(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_value_analysis_em)

    # ==================== SSE SINA OPTIONS ====================

    def get_option_sse_codes_sina(self, symbol: str = "510300") -> Dict[str, Any]:
        """Codes of the near-month calls on 510050 / 510300"""
        return self._safe_call_with_retry(ak.option_sse_codes_sina, symbol="看涨期权",
                                          trade_date=_sse_near_month(), underlying=symbol)

    def get_option_sse_daily_sina(self, symbol: str = "") -> Dict[str, Any]:
        """Daily history of one SSE option contract (8-digit code), default a near-month 300ETF call"""
        return self._safe_call_with_retry(lambda: ak.option_sse_daily_sina(symbol=_sse_option_code(symbol)))

    def get_option_sse_expire_day_sina(self, symbol: str = "300ETF") -> Dict[str, Any]:
        """Expiry date and days left of the near month (50ETF / 300ETF)"""
        underlying = {"510050": "50ETF", "510300": "300ETF"}.get(symbol.strip(), symbol.strip().upper())
        return self._safe_call_with_retry(
            lambda: [dict(zip(("expire_day", "days_left"),
                              ak.option_sse_expire_day_sina(trade_date=_sse_near_month(), symbol=underlying)))])

    def get_option_sse_greeks_sina(self, symbol: str = "") -> Dict[str, Any]:
        """Greeks of one SSE option contract (8-digit code), default a near-month 300ETF call"""
        return self._safe_call_with_retry(lambda: ak.option_sse_greeks_sina(symbol=_sse_option_code(symbol)))

    def get_option_sse_list_sina(self, symbol: str = "300ETF") -> Dict[str, Any]:
        """Expiry months listed for the SSE 50ETF or 300ETF options (510050 / 510300 accepted too)"""
        underlying = {"510050": "50ETF", "510300": "300ETF"}.get(symbol.strip(), symbol.strip().upper())
        return self._safe_call_with_retry(ak.option_sse_list_sina, symbol=underlying)

    def get_option_sse_minute_sina(self, symbol: str = "") -> Dict[str, Any]:
        """Intraday minutes of one SSE option contract (8-digit code), default a near-month 300ETF call"""
        return self._safe_call_with_retry(lambda: ak.option_sse_minute_sina(symbol=_sse_option_code(symbol)))

    def get_option_sse_spot_price_sina(self, symbol: str = "") -> Dict[str, Any]:
        """Real-time quote of one SSE option contract (8-digit code), default a near-month 300ETF call"""
        return self._safe_call_with_retry(lambda: ak.option_sse_spot_price_sina(symbol=_sse_option_code(symbol)))

    def get_option_sse_underlying_spot_price_sina(self, symbol: str = "510300") -> Dict[str, Any]:
        """Real-time quote of an option's underlying ETF (510300 or sh510300)"""
        code = symbol.strip().lower()
        code = code if code[:2] in ("sh", "sz") else ("sh" if code.startswith("5") else "sz") + code
        return self._safe_call_with_retry(ak.option_sse_underlying_spot_price_sina, symbol=code)

    # ==================== VOLATILITY ====================

    def get_option_vol_gfex(self, symbol: str = "碳酸锂", trade_date: str = "") -> Dict[str, Any]:
        return _latest_day(ak.option_vol_gfex, trade_date, symbol=symbol)

    def get_option_vol_shfe(self, symbol: str = "铜期权", trade_date: str = "") -> Dict[str, Any]:
        return _latest_day(ak.option_vol_shfe, trade_date, symbol=symbol)

    # ==================== UTILITY ====================

    def get_all_available_endpoints(self) -> Dict[str, Any]:
        """Get list of all available endpoints"""
        endpoints = [
            "option_cffex_hs300_daily_sina", "option_cffex_hs300_list_sina", "option_cffex_hs300_spot_sina",
            "option_cffex_sz50_daily_sina", "option_cffex_sz50_list_sina", "option_cffex_sz50_spot_sina",
            "option_cffex_zz1000_daily_sina", "option_cffex_zz1000_list_sina", "option_cffex_zz1000_spot_sina",
            "option_comm_info", "option_comm_symbol", "option_commodity_contract_sina",
            "option_commodity_contract_table_sina", "option_commodity_hist_sina", "option_contract_info_ctp",
            "option_current_day_sse", "option_current_day_szse", "option_current_em",
            "option_daily_stats_sse", "option_daily_stats_szse",
            "option_finance_board", "option_finance_minute_sina", "option_finance_sse_underlying",
            "option_hist_czce", "option_hist_dce", "option_hist_gfex", "option_hist_shfe", "option_hist_yearly_czce",
            "option_lhb_em", "option_margin", "option_margin_symbol",
            "option_minute_em", "option_premium_analysis_em", "option_risk_analysis_em",
            "option_risk_indicator_sse", "option_value_analysis_em",
            "option_sse_codes_sina", "option_sse_daily_sina", "option_sse_expire_day_sina",
            "option_sse_greeks_sina", "option_sse_list_sina", "option_sse_minute_sina",
            "option_sse_spot_price_sina", "option_sse_underlying_spot_price_sina",
            "option_vol_gfex", "option_vol_shfe"
        ]

        return {
            "success": True,
            "data": {
                "available_endpoints": endpoints,
                "total_count": len(endpoints),
                "categories": {
                    "Contract Info": ["option_comm_info", "option_comm_symbol", "option_contract_info_ctp"],
                    "Realtime": ["option_current_day_sse", "option_current_day_szse", "option_current_em"],
                    "Historical": ["option_hist_czce", "option_hist_dce", "option_hist_gfex", "option_hist_shfe"],
                    "Global": ["option_cffex_hs300_daily_sina", "option_cffex_sz50_daily_sina", "option_cffex_zz1000_daily_sina"],
                    "CFFEX Options": ["option_cffex_hs300_list_sina", "option_cffex_sz50_list_sina", "option_cffex_zz1000_list_sina"],
                    "SSE Options": ["option_sse_codes_sina", "option_sse_daily_sina", "option_sse_greeks_sina"],
                },
                "timestamp": int(datetime.now().timestamp())
            },
            "count": len(endpoints),
            "timestamp": int(datetime.now().timestamp())
        }


# ==================== CLI ====================

def main():
    wrapper = DerivativesWrapper()
    # One JSON line on stdout (NaN -> null, dates as ISO text); akshare's own prints go to stderr.
    return run_wrapper_cli(wrapper, wrapper.get_all_available_endpoints, ascii=True, iso_dates=True)


if __name__ == "__main__":
    sys.exit(main())
