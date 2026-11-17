"""
AKShare Bonds Data Wrapper
Wrapper for bond market data (treasury, corporate, convertible bonds)
Returns JSON output for Qt/C++ integration

NOTE: Cleaned to contain only VALID akshare bond functions (33 total).
"""

import sys
import json
import pandas as pd
import akshare as ak

# safe_call: one akshare call -> {success, data, columns, count} or a plain-English error;
# run_wrapper_cli: get_all_endpoints / <endpoint> [field=value ...] -> one JSON line on stdout.
from akshare_screen_args import run_wrapper_cli, safe_call
from typing import Dict, Any, List
from datetime import datetime, timedelta
from datetime import date

# A liquid convertible bond that trades until Dec 2027 (Industrial Bank, 兴业转债); the SYMBOL box
# overrides it. Codes may be typed bare (113052) or with the exchange prefix (sh113052).
DEFAULT_CB = "113052"


def _cb_bare(symbol: str) -> str:
    s = symbol.strip().lower()
    return s[2:] if s[:2] in ("sh", "sz") else s


def _cb_prefixed(symbol: str) -> str:
    """sh/sz + code: Shanghai convertibles are 110/111/113/118xxx, Shenzhen ones 12xxxx."""
    s = symbol.strip().lower()
    if s[:2] in ("sh", "sz"):
        return s
    return ("sh" if s.startswith("11") else "sz") + s


def _latest_report_year() -> str:
    """Fund portfolio reports are filed per year; the first quarterly report of a year is out by May."""
    today = date.today()
    return str(today.year if today.month >= 5 else today.year - 1)


class BondsWrapper:
    """Bonds data wrapper with VALIDATED akshare functions"""

    def __init__(self):
        self.default_timeout = 30
        self.retry_delay = 2

    def _safe_call_with_retry(self, func, *args, max_retries: int = 2, **kwargs) -> Dict[str, Any]:
        """One akshare call -> {success, data, columns, count} or a plain-English error (shared
        akshare_screen_args.safe_call: only a dropped connection or a timeout is retried, never a
        rate limit or a refusal)."""
        return safe_call(func, *args, _retries=max_retries, **kwargs)

    # ==================== CONVERTIBLE BONDS ====================

    def get_bond_cb_jsl(self) -> Dict[str, Any]:
        """Get convertible bonds list from JSL"""
        return self._safe_call_with_retry(ak.bond_cb_jsl)

    def get_bond_cb_index_jsl(self) -> Dict[str, Any]:
        """Get convertible bond index from JSL"""
        return self._safe_call_with_retry(ak.bond_cb_index_jsl)

    def get_bond_cb_redeem_jsl(self) -> Dict[str, Any]:
        """Get convertible bond redemption data from JSL"""
        return self._safe_call_with_retry(ak.bond_cb_redeem_jsl)

    def get_bond_cb_adj_logs_jsl(self, symbol: str = "128013") -> Dict[str, Any]:
        """Downward conversion-price revisions of one convertible bond (JSL). Most bonds never had one
        (empty answer), so the default is a bond that did; DEFAULT_CB has none."""
        return self._safe_call_with_retry(ak.bond_cb_adj_logs_jsl, symbol=_cb_bare(symbol))

    def get_bond_cb_profile_sina(self) -> Dict[str, Any]:
        """Get convertible bond profile from Sina"""
        return self._safe_call_with_retry(ak.bond_cb_profile_sina)

    def get_bond_cb_summary_sina(self) -> Dict[str, Any]:
        """Get convertible bond summary from Sina"""
        return self._safe_call_with_retry(ak.bond_cb_summary_sina)

    def get_bond_zh_cov(self) -> Dict[str, Any]:
        """Get convertible bond list (China)"""
        return self._safe_call_with_retry(ak.bond_zh_cov)

    def get_bond_zh_cov_info(self, symbol: str = DEFAULT_CB) -> Dict[str, Any]:
        """Profile of one convertible bond (EastMoney)"""
        return self._safe_call_with_retry(ak.bond_zh_cov_info, symbol=_cb_bare(symbol), indicator="基本信息")

    def get_bond_zh_cov_info_ths(self) -> Dict[str, Any]:
        """Get convertible bond info from THS"""
        return self._safe_call_with_retry(ak.bond_zh_cov_info_ths)

    def get_bond_zh_cov_spot(self) -> Dict[str, Any]:
        """Get convertible bond spot prices (China)"""
        return self._safe_call_with_retry(ak.bond_zh_hs_cov_spot)

    def get_bond_zh_cov_daily(self, symbol: str = DEFAULT_CB) -> Dict[str, Any]:
        """Daily price history of one convertible bond (Sina)"""
        return self._safe_call_with_retry(ak.bond_zh_hs_cov_daily, symbol=_cb_prefixed(symbol))

    def get_bond_zh_cov_min(self, symbol: str = DEFAULT_CB, start_date: str = "1979-09-01 09:32:00",
                            end_date: str = "2222-01-01 09:32:00") -> Dict[str, Any]:
        """15-minute bars of one convertible bond (EastMoney); START/END narrow the range"""
        return self._safe_call_with_retry(ak.bond_zh_hs_cov_min, symbol=_cb_prefixed(symbol), period="15",
                                          start_date=start_date, end_date=end_date)

    def get_bond_zh_cov_pre_min(self, symbol: str = DEFAULT_CB) -> Dict[str, Any]:
        """Intraday minute bars of one convertible bond, latest session (EastMoney)"""
        return self._safe_call_with_retry(ak.bond_zh_hs_cov_pre_min, symbol=_cb_prefixed(symbol))

    def get_bond_zh_cov_value_analysis(self, symbol: str = DEFAULT_CB) -> Dict[str, Any]:
        """Value analysis history (pure-bond value, conversion value, premiums) of one convertible bond"""
        return self._safe_call_with_retry(ak.bond_zh_cov_value_analysis, symbol=_cb_bare(symbol))

    # ==================== GOVERNMENT BONDS ====================

    def get_bond_treasure_issue_cninfo(self) -> Dict[str, Any]:
        """Get treasury bond issuance data from CNInfo"""
        return self._safe_call_with_retry(ak.bond_treasure_issue_cninfo)

    def get_bond_local_government_issue_cninfo(self) -> Dict[str, Any]:
        """Get local government bond issuance from CNInfo"""
        return self._safe_call_with_retry(ak.bond_local_government_issue_cninfo)

    # ==================== CORPORATE BONDS ====================

    def get_bond_corporate_issue_cninfo(self) -> Dict[str, Any]:
        """Get corporate bond issuance from CNInfo"""
        return self._safe_call_with_retry(ak.bond_corporate_issue_cninfo)

    def get_bond_cov_issue_cninfo(self) -> Dict[str, Any]:
        """Get convertible bond issuance from CNInfo"""
        return self._safe_call_with_retry(ak.bond_cov_issue_cninfo)

    def get_bond_cov_stock_issue_cninfo(self) -> Dict[str, Any]:
        """Get convertible bond stock issuance from CNInfo"""
        return self._safe_call_with_retry(ak.bond_cov_stock_issue_cninfo)

    # ==================== BOND MARKET DATA ====================

    def get_bond_zh_hs_daily(self) -> Dict[str, Any]:
        """Get bond daily data (Shanghai/Shenzhen)"""
        return self._safe_call_with_retry(ak.bond_zh_hs_daily)

    def get_bond_spot_deal(self) -> Dict[str, Any]:
        """Get bond spot deal data"""
        return self._safe_call_with_retry(ak.bond_spot_deal)

    def get_bond_spot_quote(self) -> Dict[str, Any]:
        """Get bond spot quotes"""
        return self._safe_call_with_retry(ak.bond_spot_quote)

    # ==================== BOND BUYBACK ====================

    def get_bond_buy_back_hist_em(self) -> Dict[str, Any]:
        """Get bond buyback history from EM"""
        return self._safe_call_with_retry(ak.bond_buy_back_hist_em)

    def get_bond_sh_buy_back_em(self) -> Dict[str, Any]:
        """Get Shanghai bond buyback from EM"""
        return self._safe_call_with_retry(ak.bond_sh_buy_back_em)

    def get_bond_sz_buy_back_em(self) -> Dict[str, Any]:
        """Get Shenzhen bond buyback from EM"""
        return self._safe_call_with_retry(ak.bond_sz_buy_back_em)

    # ==================== EXCHANGE DATA ====================

    def get_bond_cash_summary_sse(self) -> Dict[str, Any]:
        """Get bond cash summary from SSE"""
        return self._safe_call_with_retry(ak.bond_cash_summary_sse)

    def get_bond_deal_summary_sse(self) -> Dict[str, Any]:
        """Get bond deal summary from SSE"""
        return self._safe_call_with_retry(ak.bond_deal_summary_sse)

    # ==================== BOND INFO ====================

    def get_bond_debt_nafmii(self) -> Dict[str, Any]:
        """Get bond debt data from NAFMII"""
        return self._safe_call_with_retry(ak.bond_debt_nafmii)

    def get_amac_person_bond_org_list(self) -> Dict[str, Any]:
        """Get AMAC person bond organization list"""
        return self._safe_call_with_retry(ak.amac_person_bond_org_list)

    def get_fund_portfolio_bond_hold_em(self, symbol: str = "000001", date: str = _latest_report_year()) -> Dict[str, Any]:
        """Bond holdings of one fund (fund code), all quarterly reports of a year (START: the year)"""
        return self._safe_call_with_retry(ak.fund_portfolio_bond_hold_em, symbol=symbol, date=date)

    def get_macro_china_bond_public(self) -> Dict[str, Any]:
        """Get China macro bond public data"""
        return self._safe_call_with_retry(ak.macro_china_bond_public)

    # ==================== UTILITY ====================

    def get_all_available_endpoints(self) -> Dict[str, Any]:
        """Get list of all available endpoints"""
        categories = {
            "Convertible Bonds": [
                "bond_cb_jsl", "bond_cb_index_jsl", "bond_cb_redeem_jsl", "bond_cb_adj_logs_jsl",
                "bond_cb_profile_sina", "bond_cb_summary_sina", "bond_zh_cov", "bond_zh_cov_info",
                "bond_zh_cov_info_ths", "bond_zh_cov_spot", "bond_zh_cov_daily", "bond_zh_cov_min",
                "bond_zh_cov_pre_min", "bond_zh_cov_value_analysis",
            ],
            "Issuance": [
                "bond_treasure_issue_cninfo", "bond_local_government_issue_cninfo",
                "bond_corporate_issue_cninfo", "bond_cov_issue_cninfo", "bond_cov_stock_issue_cninfo",
            ],
            "Market Data": ["bond_zh_hs_daily", "bond_spot_deal", "bond_spot_quote"],
            "Repo / Buyback": ["bond_buy_back_hist_em", "bond_sh_buy_back_em", "bond_sz_buy_back_em"],
            "Exchange Summaries": ["bond_cash_summary_sse", "bond_deal_summary_sse"],
            "Bond Info": [
                "bond_debt_nafmii", "amac_person_bond_org_list", "fund_portfolio_bond_hold_em",
                "macro_china_bond_public",
            ],
        }
        endpoints = [name for names in categories.values() for name in names]

        return {
            "success": True,
            "data": {
                "available_endpoints": endpoints,
                "total_count": len(endpoints),
                "categories": categories,
                "timestamp": int(datetime.now().timestamp())
            },
            "count": len(endpoints),
            "timestamp": int(datetime.now().timestamp())
        }


# ==================== CLI ====================

def main():
    wrapper = BondsWrapper()
    # One JSON line on stdout (NaN -> null, dates as ISO text); akshare's own prints go to stderr.
    return run_wrapper_cli(wrapper, wrapper.get_all_available_endpoints, ascii=True, iso_dates=True)


if __name__ == "__main__":
    sys.exit(main())
