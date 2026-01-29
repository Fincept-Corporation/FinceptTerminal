"""
AKShare Expanded Funds Data Wrapper
Comprehensive wrapper for enhanced fund market data and analysis
Returns JSON output for Rust integration

NOTE: Refactored to contain only VALID akshare fund functions (70 total).
"""

import sys
import json
import pandas as pd
import akshare as ak
from typing import Dict, Any, List, Optional, Union
from datetime import datetime, timedelta
import time
import traceback


class AKShareError:
    """Custom error class for AKShare API errors"""
    def __init__(self, endpoint: str, error: str, data_source: Optional[str] = None):
        self.endpoint = endpoint
        self.error = error
        self.data_source = data_source
        self.timestamp = int(datetime.now().timestamp())

    def to_dict(self) -> Dict[str, Any]:
        return {
            "endpoint": self.endpoint,
            "error": self.error,
            "data_source": self.data_source,
            "timestamp": self.timestamp,
            "type": "AKShareError"
        }


class ExpandedFundsWrapper:
    """Comprehensive expanded funds data wrapper with 70+ validated endpoints"""

    def __init__(self):
        self.session = None
        self.default_timeout = 30
        self.retry_delay = 2

        # Common date parameters
        self.default_start_date = (datetime.now() - timedelta(days=3650)).strftime('%Y%m%d')  # 10 years
        self.default_end_date = datetime.now().strftime('%Y%m%d')

    def _convert_dataframe_to_json_safe(self, df: pd.DataFrame) -> List[Dict[str, Any]]:
        """Convert DataFrame to JSON-safe format, handling dates and other non-serializable types"""
        df_copy = df.copy()
        for col in df_copy.columns:
            if pd.api.types.is_datetime64_any_dtype(df_copy[col]):
                df_copy[col] = df_copy[col].astype(str)
            elif df_copy[col].dtype == 'object':
                try:
                    df_copy[col] = df_copy[col].apply(lambda x: str(x) if hasattr(x, 'strftime') else x)
                except:
                    pass
        return df_copy.to_dict('records')

    def _safe_call_with_retry(self, func, *args, max_retries: int = 3, **kwargs) -> Dict[str, Any]:
        """Safely call AKShare function with enhanced error handling and retry logic"""
        last_error = None

        for attempt in range(max_retries):
            try:
                result = func(*args, **kwargs)

                # Enhanced data validation
                if result is not None:
                    if hasattr(result, 'empty') and not result.empty:
                        return {
                            "success": True,
                            "data": self._convert_dataframe_to_json_safe(result),
                            "count": len(result),
                            "timestamp": int(datetime.now().timestamp()),
                            "data_quality": "high",
                            "source": f"akshare.{func.__name__}"
                        }
                    elif hasattr(result, '__len__') and len(result) > 0:
                        # Handle non-DataFrame results
                        return {
                            "success": True,
                            "data": result if isinstance(result, list) else [result],
                            "count": len(result) if isinstance(result, list) else 1,
                            "timestamp": int(datetime.now().timestamp()),
                            "data_quality": "high",
                            "source": f"akshare.{func.__name__}"
                        }
                    else:
                        return {
                            "success": False,
                            "error": "No data returned",
                            "data": [],
                            "count": 0,
                            "timestamp": int(datetime.now().timestamp()),
                            "data_quality": "low"
                        }

            except Exception as e:
                last_error = str(e)
                if attempt < max_retries - 1:
                    time.sleep(self.retry_delay ** attempt)
                    continue
                else:
                    break

        error_obj = AKShareError(
            endpoint=func.__name__ if hasattr(func, '__name__') else 'unknown',
            error=last_error or "Unknown error",
            data_source=getattr(func, '__module__', 'unknown')
        )
        return {
            "success": False,
            "error": error_obj.to_dict(),
            "data": [],
            "count": 0,
            "timestamp": int(datetime.now().timestamp())
        }

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

    def get_fund_open_fund_daily_em(self, symbol: str = "000001", period: str = "daily") -> Dict[str, Any]:
        """Get open fund daily data"""
        return self._safe_call_with_retry(ak.fund_open_fund_daily_em, symbol=symbol, period=period)

    def get_fund_open_fund_info_em(self, symbol: str = "000001", indicator: str = "基金档案") -> Dict[str, Any]:
        """Get open fund information"""
        return self._safe_call_with_retry(ak.fund_open_fund_info_em, symbol=symbol, indicator=indicator)

    # ==================== ETF FUNDS ====================

    def get_fund_etf_fund_daily_em(self, symbol: str = "512690", period: str = "daily") -> Dict[str, Any]:
        """Get ETF fund daily data"""
        return self._safe_call_with_retry(ak.fund_etf_fund_daily_em, symbol=symbol, period=period)

    def get_fund_etf_fund_info_em(self, symbol: str = "512690", indicator: str = "基金档案") -> Dict[str, Any]:
        """Get ETF fund information"""
        return self._safe_call_with_retry(ak.fund_etf_fund_info_em, symbol=symbol, indicator=indicator)

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

    def get_fund_financial_fund_daily_em(self, symbol: str = "000647", period: str = "daily") -> Dict[str, Any]:
        """Get financial fund daily data"""
        return self._safe_call_with_retry(ak.fund_financial_fund_daily_em, symbol=symbol, period=period)

    def get_fund_financial_fund_info_em(self, symbol: str = "000647", indicator: str = "基金档案") -> Dict[str, Any]:
        """Get financial fund information"""
        return self._safe_call_with_retry(ak.fund_financial_fund_info_em, symbol=symbol, indicator=indicator)

    # ==================== GRADED FUNDS ====================

    def get_fund_graded_fund_daily_em(self, symbol: str = "150232", period: str = "daily") -> Dict[str, Any]:
        """Get graded fund daily data"""
        return self._safe_call_with_retry(ak.fund_graded_fund_daily_em, symbol=symbol, period=period)

    def get_fund_graded_fund_info_em(self, symbol: str = "150232", indicator: str = "基金档案") -> Dict[str, Any]:
        """Get graded fund information"""
        return self._safe_call_with_retry(ak.fund_graded_fund_info_em, symbol=symbol, indicator=indicator)

    # ==================== MONEY FUNDS ====================

    def get_fund_money_fund_daily_em(self, symbol: str = "000009", period: str = "daily") -> Dict[str, Any]:
        """Get money fund daily data"""
        return self._safe_call_with_retry(ak.fund_money_fund_daily_em, symbol=symbol, period=period)

    def get_fund_money_fund_info_em(self, symbol: str = "000009", indicator: str = "基金档案") -> Dict[str, Any]:
        """Get money fund information"""
        return self._safe_call_with_retry(ak.fund_money_fund_info_em, symbol=symbol, indicator=indicator)

    # ==================== HK FUNDS ====================

    def get_fund_hk_fund_hist_em(self, symbol: str = "968001", period: str = "daily") -> Dict[str, Any]:
        """Get HK fund historical data"""
        return self._safe_call_with_retry(ak.fund_hk_fund_hist_em, symbol=symbol, period=period)

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

    def get_fund_money_rank_em(self, symbol: str = "全部") -> Dict[str, Any]:
        """Get money fund rankings"""
        return self._safe_call_with_retry(ak.fund_money_rank_em, symbol=symbol)

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

    def get_fund_scale_change_em(self, symbol: str = "000001") -> Dict[str, Any]:
        """Get fund scale changes"""
        return self._safe_call_with_retry(ak.fund_scale_change_em, symbol=symbol)

    def get_fund_hold_structure_em(self, symbol: str = "000001") -> Dict[str, Any]:
        """Get fund holding structure"""
        return self._safe_call_with_retry(ak.fund_hold_structure_em, symbol=symbol)

    def get_fund_scale_open_sina(self) -> Dict[str, Any]:
        """Get open fund scale from Sina"""
        return self._safe_call_with_retry(ak.fund_scale_open_sina)

    def get_fund_scale_close_sina(self) -> Dict[str, Any]:
        """Get closed fund scale from Sina"""
        return self._safe_call_with_retry(ak.fund_scale_close_sina)

    def get_fund_scale_structured_sina(self) -> Dict[str, Any]:
        """Get structured fund scale from Sina"""
        return self._safe_call_with_retry(ak.fund_scale_structured_sina)

    def get_fund_scale_trend(self, symbol: str = "000001") -> Dict[str, Any]:
        """Get fund scale trend - Alias for fund_scale_change_em"""
        return self.get_fund_scale_change_em(symbol=symbol)

    # ==================== FUND REPORTS ====================

    def get_fund_report_stock_cninfo(self, symbol: str = "000001", date: str = "20240630") -> Dict[str, Any]:
        """Get fund stock report from CNInfo"""
        return self._safe_call_with_retry(ak.fund_report_stock_cninfo, symbol=symbol, date=date)

    def get_fund_report_industry_allocation_cninfo(self, symbol: str = "000001", date: str = "20240630") -> Dict[str, Any]:
        """Get fund industry allocation report from CNInfo"""
        return self._safe_call_with_retry(ak.fund_report_industry_allocation_cninfo, symbol=symbol, date=date)

    def get_fund_report_asset_allocation_cninfo(self, symbol: str = "000001", date: str = "20240630") -> Dict[str, Any]:
        """Get fund asset allocation report from CNInfo"""
        return self._safe_call_with_retry(ak.fund_report_asset_allocation_cninfo, symbol=symbol, date=date)

    # ==================== FUND AUM ====================

    def get_fund_aum_em(self) -> Dict[str, Any]:
        """Get fund assets under management"""
        return self._safe_call_with_retry(ak.fund_aum_em)

    def get_fund_aum_trend_em(self, symbol: str = "全部") -> Dict[str, Any]:
        """Get fund AUM trend"""
        return self._safe_call_with_retry(ak.fund_aum_trend_em, symbol=symbol)

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

    def get_fund_cf_em(self, symbol: str = "000001") -> Dict[str, Any]:
        """Get fund cash flow"""
        return self._safe_call_with_retry(ak.fund_cf_em, symbol=symbol)

    def get_fund_fh_em(self, symbol: str = "000001") -> Dict[str, Any]:
        """Get fund dividend distribution"""
        return self._safe_call_with_retry(ak.fund_fh_em, symbol=symbol)

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
            "get_fund_financial_fund_daily_em", "get_fund_financial_fund_info_em",
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
    import sys
    wrapper = ExpandedFundsWrapper()

    if len(sys.argv) < 2:
        print(json.dumps({"error": "Usage: python akshare_funds_expanded.py <endpoint>"}))
        return

    endpoint = sys.argv[1]

    endpoint_map = {
        "get_all_endpoints": wrapper.get_all_available_endpoints,
        "get_fund_open_fund_rank_em": wrapper.get_fund_open_fund_rank_em,
        "get_fund_etf_fund_info_em": wrapper.get_fund_etf_fund_info_em,
        "get_fund_name_em": wrapper.get_fund_name_em,
    }

    method = endpoint_map.get(endpoint)
    if method:
        result = method()
        print(json.dumps(result, ensure_ascii=True))
    else:
        print(json.dumps({"error": f"Unknown endpoint: {endpoint}"}))

if __name__ == "__main__":
    main()
