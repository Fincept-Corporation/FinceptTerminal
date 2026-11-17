"""
AKShare Expanded Funds Data Wrapper
Comprehensive wrapper for enhanced fund market data and analysis
Returns JSON output for Qt/C++ integration

NOTE: Refactored to contain only VALID akshare fund functions (70 total).
"""

import sys
import json
import pandas as pd
import akshare as ak

# safe_call: one akshare call -> {success, data, columns, count} or a plain-English error;
# run_wrapper_cli: get_all_endpoints / <endpoint> [field=value ...] -> one JSON line on stdout.
from akshare_screen_args import (call_first_with_rows, china_today, recent_period_ends, run_wrapper_cli,
                                 safe_call)
from typing import Dict, Any, List, Optional, Union
from datetime import datetime, timedelta
import time
import traceback


class ExpandedFundsWrapper:
    """Comprehensive expanded funds data wrapper with 70+ validated endpoints"""

    def __init__(self):
        self.session = None
        self.default_timeout = 30
        self.retry_delay = 2

        # Common date parameters
        self.default_start_date = (datetime.now() - timedelta(days=3650)).strftime('%Y%m%d')  # 10 years
        self.default_end_date = datetime.now().strftime('%Y%m%d')

    def _safe_call_with_retry(self, func, *args, max_retries: int = 2, **kwargs) -> Dict[str, Any]:
        """One akshare call -> {success, data, columns, count} or a plain-English error (shared
        akshare_screen_args.safe_call: only a dropped connection or a timeout is retried, never a
        rate limit or a refusal)."""
        return safe_call(func, *args, _retries=max_retries, **kwargs)

    # ==================== FUND OVERVIEW & INFO ====================

    def get_fund_overview_em(self) -> Dict[str, Any]:
        """Get fund market overview"""
        return self._safe_call_with_retry(ak.fund_overview_em)

    def get_fund_fee_em(self, symbol: str = "000001") -> Dict[str, Any]:
        """Get fund fee information"""
        return self._safe_call_with_retry(ak.fund_fee_em, symbol=symbol)

    def get_fund_name_em(self) -> Dict[str, Any]:
        """Get fund name list"""
        return self._safe_call_with_retry(ak.fund_name_em)

    def get_fund_info_index_em(self, symbol: str = "000001", indicator: str = "单位净值走势") -> Dict[str, Any]:
        """Get fund index information"""
        return self._safe_call_with_retry(ak.fund_info_index_em, symbol=symbol, indicator=indicator)

    # ==================== OPEN FUNDS ====================

    def get_fund_open_fund_daily_em(self) -> Dict[str, Any]:
        """Latest NAV of every open-end fund (akshare no longer takes a fund code here)"""
        return self._safe_call_with_retry(ak.fund_open_fund_daily_em)

    def get_fund_open_fund_info_em(self, symbol: str = "000001", indicator: str = "基金档案") -> Dict[str, Any]:
        """Get open fund information"""
        return self._safe_call_with_retry(ak.fund_open_fund_info_em, symbol=symbol, indicator=indicator)

    # ==================== ETF FUNDS ====================

    def get_fund_etf_fund_daily_em(self) -> Dict[str, Any]:
        """Latest NAV of every exchange-traded fund (akshare no longer takes a fund code here)"""
        return self._safe_call_with_retry(ak.fund_etf_fund_daily_em)

    def get_fund_etf_fund_info_em(self, symbol: str = "512690", start_date: str = "20000101",
                                  end_date: str = "20500101") -> Dict[str, Any]:
        """NAV history of one exchange-traded fund; START/END narrow the range"""
        return self._safe_call_with_retry(ak.fund_etf_fund_info_em, fund=symbol, start_date=start_date,
                                          end_date=end_date)

    def get_fund_etf_hist_em(self, symbol: str = "512690", period: str = "daily", start_date: str = "19700101", end_date: str = "20500101", adjust: str = "") -> Dict[str, Any]:
        """Get ETF historical data"""
        return self._safe_call_with_retry(ak.fund_etf_hist_em, symbol=symbol, period=period, start_date=start_date, end_date=end_date, adjust=adjust)

    def get_fund_etf_hist_min_em(self, symbol: str = "512690", period: str = "1", adjust: str = "", start_date: str = "1979-09-01 09:32:00", end_date: str = "2222-01-01 09:32:00") -> Dict[str, Any]:
        """Get ETF minute historical data"""
        return self._safe_call_with_retry(ak.fund_etf_hist_min_em, symbol=symbol, period=period, adjust=adjust, start_date=start_date, end_date=end_date)

    def get_fund_etf_spot_em(self) -> Dict[str, Any]:
        """Get ETF spot data"""
        return self._safe_call_with_retry(ak.fund_etf_spot_em)

    def get_fund_etf_hist_sina(self, symbol: str = "sh510300") -> Dict[str, Any]:
        """Get ETF historical data from Sina"""
        return self._safe_call_with_retry(ak.fund_etf_hist_sina, symbol=symbol)

    def get_fund_etf_category_sina(self, symbol: str = "封闭式基金") -> Dict[str, Any]:
        """Get ETF category from Sina"""
        return self._safe_call_with_retry(ak.fund_etf_category_sina, symbol=symbol)

    def get_fund_etf_dividend_sina(self) -> Dict[str, Any]:
        """Get ETF dividend from Sina"""
        return self._safe_call_with_retry(ak.fund_etf_dividend_sina)

    def get_fund_etf_spot_ths(self) -> Dict[str, Any]:
        """Get ETF spot data from THS"""
        return self._safe_call_with_retry(ak.fund_etf_spot_ths)

    def get_fund_etf_scale_sse(self) -> Dict[str, Any]:
        """Get ETF scale from SSE"""
        return self._safe_call_with_retry(ak.fund_etf_scale_sse)

    def get_fund_etf_scale_szse(self) -> Dict[str, Any]:
        """Get ETF scale from SZSE"""
        return self._safe_call_with_retry(ak.fund_etf_scale_szse)

    # ==================== FINANCIAL FUNDS ====================


    def get_fund_financial_fund_info_em(self, symbol: str = "000134") -> Dict[str, Any]:
        """NAV history of one wealth-management fund"""
        return self._safe_call_with_retry(ak.fund_financial_fund_info_em, symbol=symbol)

    # ==================== GRADED FUNDS ====================

    def get_fund_graded_fund_daily_em(self) -> Dict[str, Any]:
        """Latest NAV of every graded (structured) fund"""
        return self._safe_call_with_retry(ak.fund_graded_fund_daily_em)

    def get_fund_graded_fund_info_em(self, symbol: str = "150232") -> Dict[str, Any]:
        """NAV history of one graded (structured) fund"""
        return self._safe_call_with_retry(ak.fund_graded_fund_info_em, symbol=symbol)

    # ==================== MONEY FUNDS ====================

    def get_fund_money_fund_daily_em(self) -> Dict[str, Any]:
        """Latest yields of every money-market fund"""
        return self._safe_call_with_retry(ak.fund_money_fund_daily_em)

    def get_fund_money_fund_info_em(self, symbol: str = "000009") -> Dict[str, Any]:
        """Yield history of one money-market fund"""
        return self._safe_call_with_retry(ak.fund_money_fund_info_em, symbol=symbol)

    # ==================== HK FUNDS ====================

    def get_fund_hk_fund_hist_em(self, symbol: str = "1002200683") -> Dict[str, Any]:
        """NAV history of one Hong Kong fund (EastMoney HK fund code, see fund_hk_rank_em)"""
        return self._safe_call_with_retry(ak.fund_hk_fund_hist_em, code=symbol, symbol="历史净值明细")

    # ==================== LOF FUNDS ====================

    def get_fund_lof_hist_em(self, symbol: str = "166009", period: str = "daily", start_date: str = "19700101", end_date: str = "20500101", adjust: str = "") -> Dict[str, Any]:
        """Get LOF historical data"""
        return self._safe_call_with_retry(ak.fund_lof_hist_em, symbol=symbol, period=period, start_date=start_date, end_date=end_date, adjust=adjust)

    def get_fund_lof_spot_em(self) -> Dict[str, Any]:
        """Get LOF spot data"""
        return self._safe_call_with_retry(ak.fund_lof_spot_em)

    def get_fund_lof_hist_min_em(self, symbol: str = "166009", period: str = "1", adjust: str = "", start_date: str = "1979-09-01 09:32:00", end_date: str = "2222-01-01 09:32:00") -> Dict[str, Any]:
        """Get LOF minute historical data"""
        return self._safe_call_with_retry(ak.fund_lof_hist_min_em, symbol=symbol, period=period, adjust=adjust, start_date=start_date, end_date=end_date)

    # ==================== FUND PORTFOLIO ====================

    def get_fund_portfolio_hold_em(self, symbol: str = "000001", date: str = "2024") -> Dict[str, Any]:
        """Get fund portfolio holdings"""
        return self._safe_call_with_retry(ak.fund_portfolio_hold_em, symbol=symbol, date=date)

    def get_fund_portfolio_change_em(self, symbol: str = "000001", date: str = "2024") -> Dict[str, Any]:
        """Get fund portfolio changes"""
        return self._safe_call_with_retry(ak.fund_portfolio_change_em, symbol=symbol, date=date)

    def get_fund_portfolio_bond_hold_em(self, symbol: str = "000001", date: str = "2024") -> Dict[str, Any]:
        """Get fund bond portfolio holdings"""
        return self._safe_call_with_retry(ak.fund_portfolio_bond_hold_em, symbol=symbol, date=date)

    def get_fund_portfolio_industry_allocation_em(self, symbol: str = "000001", date: str = "2024") -> Dict[str, Any]:
        """Get fund industry allocation"""
        return self._safe_call_with_retry(ak.fund_portfolio_industry_allocation_em, symbol=symbol, date=date)

    # ==================== FUND RATINGS ====================

    def get_fund_rating_sh(self) -> Dict[str, Any]:
        """Get fund ratings from Shanghai"""
        return self._safe_call_with_retry(ak.fund_rating_sh)

    def get_fund_rating_zs(self) -> Dict[str, Any]:
        """Get fund ratings from Zhishu"""
        return self._safe_call_with_retry(ak.fund_rating_zs)

    def get_fund_rating_ja(self) -> Dict[str, Any]:
        """Get fund ratings from JiAn"""
        return self._safe_call_with_retry(ak.fund_rating_ja)

    def get_fund_rating_all(self) -> Dict[str, Any]:
        """Get all fund ratings"""
        return self._safe_call_with_retry(ak.fund_rating_all)

    def get_fund_rating_morningstar(self) -> Dict[str, Any]:
        """Get Morningstar fund ratings - Alias for fund_rating_all"""
        return self.get_fund_rating_all()

    def get_fund_rating_zhishu(self) -> Dict[str, Any]:
        """Get Zhishu fund ratings - Alias for fund_rating_zs"""
        return self.get_fund_rating_zs()

    # ==================== FUND RANKINGS ====================

    def get_fund_exchange_rank_em(self) -> Dict[str, Any]:
        """Get exchange fund rankings"""
        return self._safe_call_with_retry(ak.fund_exchange_rank_em)

    def get_fund_money_rank_em(self) -> Dict[str, Any]:
        """Money-market fund ranking"""
        return self._safe_call_with_retry(ak.fund_money_rank_em)

    def get_fund_open_fund_rank_em(self, symbol: str = "全部") -> Dict[str, Any]:
        """Get open fund rankings"""
        return self._safe_call_with_retry(ak.fund_open_fund_rank_em, symbol=symbol)

    def get_fund_hk_rank_em(self) -> Dict[str, Any]:
        """Get HK fund rankings"""
        return self._safe_call_with_retry(ak.fund_hk_rank_em)

    def get_fund_lcx_rank_em(self) -> Dict[str, Any]:
        """Get LCX fund rankings"""
        return self._safe_call_with_retry(ak.fund_lcx_rank_em)

    # ==================== FUND SCALE ====================

    def get_fund_scale_em(self) -> Dict[str, Any]:
        """Get fund scale - Alias for fund_aum_em"""
        return self._safe_call_with_retry(ak.fund_aum_em)

    def get_fund_scale_change_em(self) -> Dict[str, Any]:
        """Market-wide fund share changes by report period"""
        return self._safe_call_with_retry(ak.fund_scale_change_em)

    def get_fund_hold_structure_em(self) -> Dict[str, Any]:
        """Market-wide fund holder structure (institutional / individual) by report period"""
        return self._safe_call_with_retry(ak.fund_hold_structure_em)

    def get_fund_scale_open_sina(self) -> Dict[str, Any]:
        """Get open fund scale from Sina"""
        return self._safe_call_with_retry(ak.fund_scale_open_sina)

    def get_fund_scale_close_sina(self) -> Dict[str, Any]:
        """Get closed fund scale from Sina"""
        return self._safe_call_with_retry(ak.fund_scale_close_sina)

    def get_fund_scale_structured_sina(self) -> Dict[str, Any]:
        """Get structured fund scale from Sina"""
        return self._safe_call_with_retry(ak.fund_scale_structured_sina)

    def get_fund_scale_trend(self) -> Dict[str, Any]:
        """Get fund scale trend - Alias for fund_scale_change_em"""
        return self.get_fund_scale_change_em()

    # ==================== FUND REPORTS ====================
    # Market-wide tables of one report period (quarter end); START picks it, default the newest
    # quarter that has been published.

    def _report_period(self, func, date: str) -> Dict[str, Any]:
        if date:
            return self._safe_call_with_retry(func, date=date)
        return call_first_with_rows(func, "date", recent_period_ends(count=4))

    def get_fund_report_stock_cninfo(self, date: str = "") -> Dict[str, Any]:
        """Stocks held by funds, all funds, one report period (CNInfo)"""
        return self._report_period(ak.fund_report_stock_cninfo, date)

    def get_fund_report_industry_allocation_cninfo(self, date: str = "") -> Dict[str, Any]:
        """Industry allocation of all funds, one report period (CNInfo)"""
        return self._report_period(ak.fund_report_industry_allocation_cninfo, date)

    def get_fund_report_asset_allocation_cninfo(self) -> Dict[str, Any]:
        """Asset allocation of all funds by report period (CNInfo)"""
        return self._safe_call_with_retry(ak.fund_report_asset_allocation_cninfo)

    # ==================== FUND AUM ====================

    def get_fund_aum_em(self) -> Dict[str, Any]:
        """Get fund assets under management"""
        return self._safe_call_with_retry(ak.fund_aum_em)

    def get_fund_aum_trend_em(self) -> Dict[str, Any]:
        """Market-wide fund AUM trend"""
        return self._safe_call_with_retry(ak.fund_aum_trend_em)

    def get_fund_aum_hist_em(self) -> Dict[str, Any]:
        """Get fund AUM historical data"""
        return self._safe_call_with_retry(ak.fund_aum_hist_em)

    # ==================== FUND POSITION (LG) ====================

    def get_fund_stock_position_lg(self) -> Dict[str, Any]:
        """Get stock fund positions"""
        return self._safe_call_with_retry(ak.fund_stock_position_lg)

    def get_fund_balance_position_lg(self) -> Dict[str, Any]:
        """Get balanced fund positions"""
        return self._safe_call_with_retry(ak.fund_balance_position_lg)

    def get_fund_linghuo_position_lg(self) -> Dict[str, Any]:
        """Get flexible fund positions"""
        return self._safe_call_with_retry(ak.fund_linghuo_position_lg)

    # ==================== FUND MISC ====================

    def get_fund_purchase_em(self) -> Dict[str, Any]:
        """Get fund purchase information"""
        return self._safe_call_with_retry(ak.fund_purchase_em)

    def get_fund_value_estimation_em(self, symbol: str = "000001") -> Dict[str, Any]:
        """Get fund value estimation"""
        return self._safe_call_with_retry(ak.fund_value_estimation_em, symbol=symbol)

    def get_fund_new_issue(self) -> Dict[str, Any]:
        """Get new fund issues"""
        return self._safe_call_with_retry(ak.fund_new_found_em)

    def get_fund_new_issue_detail(self, fund_code: str = "000001") -> Dict[str, Any]:
        """Get new fund issue details - Uses fund info endpoint"""
        return self.get_fund_open_fund_info_em(symbol=fund_code, indicator="基金档案")

    def get_fund_manager_em(self) -> Dict[str, Any]:
        """Get fund manager information"""
        return self._safe_call_with_retry(ak.fund_manager_em)

    def get_fund_manager_detail(self, manager_name: str = "") -> Dict[str, Any]:
        """Get fund manager details - Uses fund manager endpoint"""
        return self.get_fund_manager_em()

    def get_fund_manager_performance(self, manager_name: str = "") -> Dict[str, Any]:
        """Get fund manager performance - Uses fund manager endpoint"""
        return self.get_fund_manager_em()

    def get_fund_manager_ranking(self) -> Dict[str, Any]:
        """Get fund manager rankings - Uses fund manager endpoint"""
        return self.get_fund_manager_em()

    # `date` so the screen's START box can pick the year (any day of it); default this year.

    def get_fund_cf_em(self, date: str = "") -> Dict[str, Any]:
        """Fund share splits of one year, all funds"""
        return self._safe_call_with_retry(ak.fund_cf_em, year=(date or str(china_today().year))[:4])

    def get_fund_fh_em(self, date: str = "") -> Dict[str, Any]:
        """Fund dividend distributions of one year, all funds"""
        return self._safe_call_with_retry(ak.fund_fh_em, year=(date or str(china_today().year))[:4])

    def get_fund_fh_rank_em(self) -> Dict[str, Any]:
        """Get fund dividend rankings"""
        return self._safe_call_with_retry(ak.fund_fh_rank_em)

    def get_fund_flow_em(self) -> Dict[str, Any]:
        """Get fund flow - Uses AUM data as proxy"""
        return self.get_fund_aum_em()

    def get_fund_flow_daily(self) -> Dict[str, Any]:
        """Get daily fund flow - Uses AUM data as proxy"""
        return self.get_fund_aum_em()

    def get_fund_flow_weekly(self) -> Dict[str, Any]:
        """Get weekly fund flow - Uses AUM data as proxy"""
        return self.get_fund_aum_em()

    def get_fund_flow_monthly(self) -> Dict[str, Any]:
        """Get monthly fund flow - Uses AUM data as proxy"""
        return self.get_fund_aum_em()

    # ==================== FUND XQ (XUEQIU) ====================

    def get_fund_individual_basic_info_xq(self, symbol: str = "SH501018") -> Dict[str, Any]:
        """Get fund basic info from Xueqiu"""
        return self._safe_call_with_retry(ak.fund_individual_basic_info_xq, symbol=symbol)

    def get_fund_individual_achievement_xq(self, symbol: str = "SH501018") -> Dict[str, Any]:
        """Get fund achievement from Xueqiu"""
        return self._safe_call_with_retry(ak.fund_individual_achievement_xq, symbol=symbol)

    def get_fund_individual_analysis_xq(self, symbol: str = "SH501018") -> Dict[str, Any]:
        """Get fund analysis from Xueqiu"""
        return self._safe_call_with_retry(ak.fund_individual_analysis_xq, symbol=symbol)

    def get_fund_individual_profit_probability_xq(self, symbol: str = "SH501018") -> Dict[str, Any]:
        """Get fund profit probability from Xueqiu"""
        return self._safe_call_with_retry(ak.fund_individual_profit_probability_xq, symbol=symbol)

    def get_fund_individual_detail_info_xq(self, symbol: str = "SH501018") -> Dict[str, Any]:
        """Get fund detail info from Xueqiu"""
        return self._safe_call_with_retry(ak.fund_individual_detail_info_xq, symbol=symbol)

    def get_fund_individual_detail_hold_xq(self, symbol: str = "SH501018") -> Dict[str, Any]:
        """Get fund holdings from Xueqiu"""
        return self._safe_call_with_retry(ak.fund_individual_detail_hold_xq, symbol=symbol)

    # ==================== UTILITY FUNCTIONS ====================

    def get_all_available_endpoints(self) -> Dict[str, Any]:
        """Get list of all available fund endpoints"""
        endpoints = [
            "get_fund_overview_em", "get_fund_fee_em", "get_fund_name_em", "get_fund_info_index_em",
            "get_fund_open_fund_daily_em", "get_fund_open_fund_info_em",
            "get_fund_etf_fund_daily_em", "get_fund_etf_fund_info_em", "get_fund_etf_hist_em",
            "get_fund_etf_hist_min_em", "get_fund_etf_spot_em", "get_fund_etf_hist_sina",
            "get_fund_etf_category_sina", "get_fund_etf_dividend_sina", "get_fund_etf_spot_ths",
            "get_fund_etf_scale_sse", "get_fund_etf_scale_szse",
            "get_fund_financial_fund_info_em",
            "get_fund_graded_fund_daily_em", "get_fund_graded_fund_info_em",
            "get_fund_money_fund_daily_em", "get_fund_money_fund_info_em",
            "get_fund_hk_fund_hist_em",
            "get_fund_lof_hist_em", "get_fund_lof_spot_em", "get_fund_lof_hist_min_em",
            "get_fund_portfolio_hold_em", "get_fund_portfolio_change_em",
            "get_fund_portfolio_bond_hold_em", "get_fund_portfolio_industry_allocation_em",
            "get_fund_rating_sh", "get_fund_rating_zs", "get_fund_rating_ja", "get_fund_rating_all",
            "get_fund_rating_morningstar", "get_fund_rating_zhishu",
            "get_fund_exchange_rank_em", "get_fund_money_rank_em", "get_fund_open_fund_rank_em",
            "get_fund_hk_rank_em", "get_fund_lcx_rank_em",
            "get_fund_scale_em", "get_fund_scale_change_em", "get_fund_hold_structure_em",
            "get_fund_scale_open_sina", "get_fund_scale_close_sina", "get_fund_scale_structured_sina",
            "get_fund_scale_trend",
            "get_fund_report_stock_cninfo", "get_fund_report_industry_allocation_cninfo",
            "get_fund_report_asset_allocation_cninfo",
            "get_fund_aum_em", "get_fund_aum_trend_em", "get_fund_aum_hist_em",
            "get_fund_stock_position_lg", "get_fund_balance_position_lg", "get_fund_linghuo_position_lg",
            "get_fund_purchase_em", "get_fund_value_estimation_em", "get_fund_new_issue",
            "get_fund_new_issue_detail", "get_fund_manager_em", "get_fund_manager_detail",
            "get_fund_manager_performance", "get_fund_manager_ranking",
            "get_fund_cf_em", "get_fund_fh_em", "get_fund_fh_rank_em",
            "get_fund_flow_em", "get_fund_flow_daily", "get_fund_flow_weekly", "get_fund_flow_monthly",
            "get_fund_individual_basic_info_xq", "get_fund_individual_achievement_xq",
            "get_fund_individual_analysis_xq", "get_fund_individual_profit_probability_xq",
            "get_fund_individual_detail_info_xq", "get_fund_individual_detail_hold_xq"
        ]

        return {
            "success": True,
            "data": {
                "available_endpoints": endpoints,
                "total_count": len(endpoints),
                "categories": {
                    "Fund Rankings": ["get_fund_open_fund_rank_em", "get_fund_etf_fund_info_em", "get_fund_money_fund_daily_em"],
                    "Fund Info": ["get_fund_info_index_em", "get_fund_individual_basic_info_xq", "get_fund_name_em"],
                    "Fund Holdings": ["get_fund_portfolio_hold_em", "get_fund_stock_position_lg", "get_fund_balance_position_lg"],
                    "Fund Managers": ["get_fund_manager_em", "get_fund_manager_detail", "get_fund_manager_performance"],
                    "Fund Flows": ["get_fund_flow_em", "get_fund_flow_daily", "get_fund_flow_weekly"],
                    "Fund Dividends": ["get_fund_fh_em", "get_fund_fh_rank_em"],
                },
                "timestamp": int(datetime.now().timestamp())
            },
            "count": len(endpoints),
            "timestamp": int(datetime.now().timestamp())
        }


# ==================== CLI ====================

def main():
    wrapper = ExpandedFundsWrapper()
    # One JSON line on stdout (NaN -> null, dates as ISO text); akshare's own prints go to stderr.
    return run_wrapper_cli(wrapper, wrapper.get_all_available_endpoints, ascii=True, iso_dates=False)


if __name__ == "__main__":
    sys.exit(main())
