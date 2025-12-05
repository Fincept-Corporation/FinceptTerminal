"""
AKShare Stock Analysis Wrapper
Comprehensive wrapper for AKShare stock technical analysis, fund flow, and fundamental data
Returns JSON output for Rust integration
Modular, fault-tolerant design - each endpoint works independently
All endpoints are FREE - no API keys required
"""

import sys
import json
import pandas as pd
import akshare as ak
from typing import Dict, Any, List, Optional, Union
from datetime import datetime, timedelta
import traceback
import time

# Import stock feature modules for analysis
try:
    from akshare.stock_feature import stock_a_pe_and_pb, stock_a_indicator, stock_value_em, stock_buffett_index_lg
    from akshare.stock_feature import stock_fund_flow_individual, stock_fund_flow_concept, stock_fund_flow_industry
    from akshare.stock_feature import stock_lhb_em, stock_lhb_sina, stock_hsgt_em, stock_margin_em, stock_account_em
    from akshare.stock_feature import stock_cyq_em, stock_congestion_lg, stock_pankou_em
    from akshare.stock_feature import stock_three_report_em, stock_profit_forecast_em, stock_analyst_em
    from akshare.stock_feature import stock_yjbb_em, stock_gdfx_em, stock_yjyg_em, stock_research_report_em
    from akshare.stock_feature import stock_a_below_net_asset_statistics, stock_a_high_low, stock_all_pb
    from akshare.stock_feature import stock_disclosure_cninfo, stock_dxsyl_em, stock_esg_sina
    from akshare.stock_feature import stock_fhps_em, stock_fhps_ths, stock_gdhs, stock_gpzy_em
    from akshare.stock_feature import stock_hot_xq, stock_inner_trade_xq, stock_comment_em, stock_zh_vote_baidu
    from akshare.stock_feature import stock_ttm_lyr, stock_zh_valuation_baidu, stock_zf_pg, stock_ztb_em
    AKSHARE_FEATURES_AVAILABLE = True
except ImportError:
    AKSHARE_FEATURES_AVAILABLE = False

from akshare.stock.stock_info import stock_info_a_code_name
from akshare.stock.stock_overview_em import stock_overview_em


class StockAnalysisError:
    """Custom error class for AKShare stock analysis errors"""
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
            "type": "StockAnalysisError"
        }


class StockAnalysisWrapper:
    """Comprehensive AKShare stock analysis data wrapper with fault-tolerant endpoints"""

    def __init__(self):
        self.default_timeout = 30
        self.default_start_date = (datetime.now() - timedelta(days=365)).strftime('%Y%m%d')
        self.default_end_date = datetime.now().strftime('%Y%m%d')
        self.retry_attempts = 3
        self.retry_delay = 2

    def _safe_call_with_retry(self, func, *args, max_retries: int = 3, **kwargs) -> Dict[str, Any]:
        """Safely call AKShare function with retry logic and enhanced error handling"""
        last_error = None

        for attempt in range(max_retries):
            try:
                result = func(*args, **kwargs)

                if result is not None and hasattr(result, '__len__') and len(result) > 0:
                    if hasattr(result, 'empty') and not result.empty:
                        return {
                            "success": True,
                            "data": result.to_dict('records') if hasattr(result, 'to_dict') else str(result),
                            "count": len(result),
                            "timestamp": int(datetime.now().timestamp()),
                            "data_quality": "high",
                            "source": getattr(func, '__module__', 'akshare')
                        }
                    else:
                        return {
                            "success": False,
                            "error": "Empty or invalid data returned",
                            "data": [],
                            "count": 0,
                            "timestamp": int(datetime.now().timestamp())
                        }
                else:
                    return {
                        "success": False,
                        "error": "No data returned",
                        "data": [],
                        "count": 0,
                        "timestamp": int(datetime.now().timestamp())
                    }

            except Exception as e:
                last_error = str(e)
                if attempt < max_retries - 1:
                    time.sleep(self.retry_delay ** attempt)
                    continue
                else:
                    break

        error_obj = StockAnalysisError(
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

    def _validate_and_format_dataframe(self, df: pd.DataFrame, min_rows: int = 1) -> Dict[str, Any]:
        """Validate and format DataFrame for consistent output"""
        if df is None or df.empty:
            return {
                "success": False,
                "error": "Empty DataFrame returned",
                "data": [],
                "count": 0,
                "timestamp": int(datetime.now().timestamp())
            }

        if len(df) < min_rows:
            return {
                "success": False,
                "error": f"Insufficient data: only {len(df)} rows returned",
                "data": [],
                "count": 0,
                "timestamp": int(datetime.now().timestamp())
            }

        # Clean data
        df_clean = df.dropna(how='all').drop_duplicates()

        return {
            "success": True,
            "data": df_clean.to_dict('records'),
            "count": len(df_clean),
            "timestamp": int(datetime.now().timestamp()),
            "data_quality": "high",
            "columns": list(df_clean.columns)
        }

    # ==================== TECHNICAL INDICATORS ====================

    def get_technical_indicators_pe_pb(self) -> Dict[str, Any]:
        """Get stock market P/E and P/B ratios

        Returns market-wide P/E and P/B analysis data
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_a_pe_and_pb()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_technical_indicators_analysis(self) -> Dict[str, Any]:
        """Get comprehensive stock technical indicators

        Returns detailed technical analysis for stocks
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_a_indicator()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_stock_valuation_metrics(self) -> Dict[str, Any]:
        """Get stock valuation analysis metrics

        Returns comprehensive valuation metrics and analysis
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_value_em()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_buffett_indicator(self) -> Dict[str, Any]:
        """Get Buffett market valuation indicator

        Returns Buffett indicator for market valuation assessment
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_buffett_index_lg()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_ttm_lyr_ratios(self) -> Dict[str, Any]:
        """Get TTM and L-year financial ratios

        Returns trailing twelve months and long-term financial ratios
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_ttm_lyr()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_market_pb_statistics(self) -> Dict[str, Any]:
        """Get market-wide P/B ratio statistics

        Returns comprehensive P/B ratio analysis
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_all_pb()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_below_net_asset_statistics(self) -> Dict[str, Any]:
        """Get stocks trading below net asset value

        Returns analysis of stocks trading below book value
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_a_below_net_asset_statistics()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_high_low_analysis(self) -> Dict[str, Any]:
        """Get high/low price analysis

        Returns 52-week high/low price analysis
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_a_high_low()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    # ==================== FUND FLOW ANALYSIS ====================

    def get_individual_stock_fund_flow(self, period: str = "即时") -> Dict[str, Any]:
        """Get individual stock fund flow analysis

        Args:
            period: Time period - "即时", "3日排行", "5日排行", "10日排行", "20日排行"

        Returns individual stock money flow analysis
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_fund_flow_individual(symbol=period)
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_concept_fund_flow(self) -> Dict[str, Any]:
        """Get concept sector fund flow analysis

        Returns money flow analysis for concept boards/sectors
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_fund_flow_concept()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_industry_fund_flow(self) -> Dict[str, Any]:
        """Get industry sector fund flow analysis

        Returns money flow analysis for industry sectors
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_fund_flow_industry()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_lhb_data(self) -> Dict[str, Any]:
        """Get Long-Hu-Bang (龙虎榜) data

        Returns top trading activity and large trade data
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_lhb_em()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_lhb_sina_data(self) -> Dict[str, Any]:
        """Get Sina Long-Hu-Bang data

        Returns alternative source for large trade data
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_lhb_sina()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_hsgt_data(self) -> Dict[str, Any]:
        """Get Hong Kong Stock Connect (HSGT) flow data

        Returns northbound/southbound trading flow data
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_hsgt_em()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_margin_trading_data(self) -> Dict[str, Any]:
        """Get margin trading data

        Returns margin trading statistics and outstanding balances
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_margin_em()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_investor_account_stats(self) -> Dict[str, Any]:
        """Get investor account statistics

        Returns trading account statistics and trends
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_account_em()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    # ==================== COST LAYER & ORDER BOOK ANALYSIS ====================

    def get_cost_layer_analysis(self, symbol: str = "sh600000") -> Dict[str, Any]:
        """Get cost layer analysis (筹码分布)

        Args:
            symbol: Stock symbol (e.g., "sh600000", "sz000001")

        Returns detailed cost distribution and position analysis
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_cyq_em(symbol=symbol)
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_market_congestion_indicator(self) -> Dict[str, Any]:
        """Get market congestion indicator

        Returns market congestion and momentum analysis
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_congestion_lg()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_order_book_analysis(self, symbol: str = "sh600000") -> Dict[str, Any]:
        """Get order book (盘口) analysis

        Args:
            symbol: Stock symbol (e.g., "sh600000", "sz000001")

        Returns order book depth and analysis
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_pankou_em(symbol=symbol)
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    # ==================== CORPORATE FINANCIALS ====================

    def get_financial_reports(self) -> Dict[str, Any]:
        """Get three major financial reports

        Returns quarterly and annual financial reports
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_three_report_em()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_profit_forecasts(self) -> Dict[str, Any]:
        """Get earnings profit forecasts

        Returns analyst profit forecasts and expectations
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_profit_forecast_em()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_analyst_recommendations(self) -> Dict[str, Any]:
        """Get analyst recommendations

        Returns buy/sell/hold recommendations from analysts
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_analyst_em()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_earnings_bulletins(self) -> Dict[str, Any]:
        """Get earnings announcements bulletins

        Returns YJBB earnings announcements and guidance
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_yjbb_em()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_shareholder_distribution(self) -> Dict[str, Any]:
        """Get shareholder distribution analysis

        Returns detailed shareholder structure and changes
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_gdfx_em()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_stock_pledge_info(self) -> Dict[str, Any]:
        """Get stock pledge information

        Returns share pledge and collateral information
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_gpzy_em()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_research_reports(self) -> Dict[str, Any]:
        """Get institutional research reports

        Returns research reports from institutions
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_research_report_em()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_disclosure_announcements(self) -> Dict[str, Any]:
        """Get company disclosure announcements

        Returns mandatory disclosures and announcements
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_disclosure_cninfo()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_earnings_guidance(self) -> Dict[str, Any]:
        """Get earnings guidance announcements

        Returns YJYG earnings guidance and forecasts
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_yjyg_em()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_dividend_yield_stats(self) -> Dict[str, Any]:
        """Get dividend yield and statistics

        Returns dividend yield analysis and payout statistics
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_dxsyl_em()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_esg_data(self) -> Dict[str, Any]:
        """Get ESG (Environmental, Social, Governance) data

        Returns ESG ratings and sustainability metrics
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_esg_sina()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_financial_highlights(self) -> Dict[str, Any]:
        """Get financial performance highlights

        Returns FHPS (financial performance highlights) analysis
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_fhps_em()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_top_shareholders(self) -> Dict[str, Any]:
        """Get top shareholders information

        Returns top 10 shareholders and holdings
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_gdhs()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    # ==================== MARKET SENTIMENT & ALTERNATIVE DATA ====================

    def get_xueqiu_hot_stocks(self) -> Dict[str, Any]:
        """Get hot stocks from Xueqiu

        Returns trending stocks and social media sentiment
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_hot_xq()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_inner_trading_data(self) -> Dict[str, Any]:
        """Get insider trading information

        Returns insider trading activities and patterns
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_inner_trade_xq()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_stock_comments(self, symbol: str = None) -> Dict[str, Any]:
        """Get stock comments and sentiment analysis

        Args:
            symbol: Optional stock symbol filter

        Returns user comments and sentiment analysis
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_comment_em(symbol=symbol)
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_baidu_poll_data(self) -> Dict[str, Any]:
        """Get Baidu stock poll/voting data

        Returns investor sentiment and voting results
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_zh_vote_baidu()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_valuation_analysis(self, market: str = "sh") -> Dict[str, Any]:
        """Get Baidu valuation analysis

        Args:
            market: Market - "sh" (Shanghai) or "sz" (Shenzhen)

        Returns comprehensive valuation analysis and metrics
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_zh_valuation_baidu(market=market)
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_turnover_analysis(self) -> Dict[str, Any]:
        """Get stock turnover analysis

        Returns ZFPG (turnover rate) and trading activity analysis
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_zf_pg()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    def get_limit_up_down_stats(self) -> Dict[str, Any]:
        """Get limit up/down statistics

        Returns ZTB (limit order) and trading constraints
        """
        if not AKSHARE_FEATURES_AVAILABLE:
            return {"success": False, "error": "AKShare features module not available", "data": []}

        try:
            df = stock_ztb_em()
            return self._validate_and_format_dataframe(df)
        except Exception as e:
            return {"success": False, "error": str(e), "data": []}

    # ==================== UTILITY FUNCTIONS ====================

    def get_all_stock_analysis_endpoints(self) -> Dict[str, Any]:
        """Get list of all available stock analysis endpoints"""
        endpoints = [
            # Technical Indicators
            "get_technical_indicators_pe_pb",
            "get_technical_indicators_analysis",
            "get_stock_valuation_metrics",
            "get_buffett_indicator",
            "get_ttm_lyr_ratios",
            "get_market_pb_statistics",
            "get_below_net_asset_statistics",
            "get_high_low_analysis",

            # Fund Flow Analysis
            "get_individual_stock_fund_flow",
            "get_concept_fund_flow",
            "get_industry_fund_flow",
            "get_lhb_data",
            "get_lhb_sina_data",
            "get_hsgt_data",
            "get_margin_trading_data",
            "get_investor_account_stats",

            # Cost Layer & Order Book
            "get_cost_layer_analysis",
            "get_market_congestion_indicator",
            "get_order_book_analysis",

            # Corporate Financials
            "get_financial_reports",
            "get_profit_forecasts",
            "get_analyst_recommendations",
            "get_earnings_bulletins",
            "get_shareholder_distribution",
            "get_stock_pledge_info",
            "get_research_reports",
            "get_disclosure_announcements",
            "get_earnings_guidance",

            # Additional Financial Metrics
            "get_dividend_yield_stats",
            "get_esg_data",
            "get_financial_highlights",
            "get_top_shareholders",

            # Market Sentiment
            "get_xueqiu_hot_stocks",
            "get_inner_trading_data",
            "get_stock_comments",
            "get_baidu_poll_data",
            "get_valuation_analysis",
            "get_turnover_analysis",
            "get_limit_up_down_stats"
        ]

        return {
            "available_endpoints": endpoints,
            "total_count": len(endpoints),
            "categories": {
                "Technical Indicators": [
                    "get_technical_indicators_pe_pb",
                    "get_technical_indicators_analysis",
                    "get_stock_valuation_metrics",
                    "get_buffett_indicator",
                    "get_ttm_lyr_ratios",
                    "get_market_pb_statistics",
                    "get_below_net_asset_statistics",
                    "get_high_low_analysis"
                ],
                "Fund Flow Analysis": [
                    "get_individual_stock_fund_flow",
                    "get_concept_fund_flow",
                    "get_industry_fund_flow",
                    "get_lhb_data",
                    "get_lhb_sina_data",
                    "get_hsgt_data",
                    "get_margin_trading_data",
                    "get_investor_account_stats"
                ],
                "Cost Layer & Order Book": [
                    "get_cost_layer_analysis",
                    "get_market_congestion_indicator",
                    "get_order_book_analysis"
                ],
                "Corporate Financials": [
                    "get_financial_reports",
                    "get_profit_forecasts",
                    "get_analyst_recommendations",
                    "get_earnings_bulletins",
                    "get_shareholder_distribution",
                    "get_stock_pledge_info",
                    "get_research_reports",
                    "get_disclosure_announcements",
                    "get_earnings_guidance"
                ],
                "Additional Financial Metrics": [
                    "get_dividend_yield_stats",
                    "get_esg_data",
                    "get_financial_highlights",
                    "get_top_shareholders"
                ],
                "Market Sentiment": [
                    "get_xueqiu_hot_stocks",
                    "get_inner_trading_data",
                    "get_stock_comments",
                    "get_baidu_poll_data",
                    "get_valuation_analysis",
                    "get_turnover_analysis",
                    "get_limit_up_down_stats"
                ]
            },
            "timestamp": int(datetime.now().timestamp()),
            "features_available": AKSHARE_FEATURES_AVAILABLE
        }

    
    def get_analysis_summary(self) -> Dict[str, Any]:
        """Get summary of analysis capabilities"""
        return {
            "endpoint_count": 42 if AKSHARE_FEATURES_AVAILABLE else 0,
            "categories": [
                "Technical Indicators (8 functions)",
                "Fund Flow Analysis (8 functions)",
                "Cost Layer & Order Book (3 functions)",
                "Corporate Financials (10 functions)",
                "Additional Financial Metrics (4 functions)",
                "Market Sentiment (7 functions)"
            ] if AKSHARE_FEATURES_AVAILABLE else [
                "Basic functions only (AKShare features not available)"
            ],
            "data_quality": "High - Includes validation and error handling",
            "coverage": "Comprehensive Chinese market analysis",
            "timestamp": int(datetime.now().timestamp())
        }


# ==================== COMMAND LINE INTERFACE ====================

def main():
    """Command line interface for the AKShare stock analysis wrapper"""
    wrapper = StockAnalysisWrapper()

    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python akshare_analysis.py <endpoint> [args...]",
            "available_endpoints": wrapper.get_all_stock_analysis_endpoints()["available_endpoints"]
        }, indent=2))
        return

    endpoint = sys.argv[1]
    args = sys.argv[2:] if len(sys.argv) > 2 else []

    # Map endpoint names to method calls
    endpoint_map = {
                "get_all_endpoints": wrapper.get_all_stock_analysis_endpoints,
        "get_analysis_summary": wrapper.get_analysis_summary,

        # Technical Indicators
        "technical_indicators": wrapper.get_technical_indicators_analysis,
        "pe_pb_ratios": wrapper.get_technical_indicators_pe_pb,
        "valuation_metrics": wrapper.get_stock_valuation_metrics,
        "buffett_indicator": wrapper.get_buffett_indicator,
        "ttm_ratios": wrapper.get_ttm_lyr_ratios,
        "market_pb_stats": wrapper.get_market_pb_statistics,

        # Fund Flow
        "individual_fund_flow": wrapper.get_individual_stock_fund_flow,
        "concept_fund_flow": wrapper.get_concept_fund_flow,
        "industry_fund_flow": wrapper.get_industry_fund_flow,
        "lhb_data": wrapper.get_lhb_data,
        "hsgt_data": wrapper.get_hsgt_data,
        "margin_trading": wrapper.get_margin_trading_data,

        # Cost Layer
        "cost_layer": wrapper.get_cost_layer_analysis,
        "market_congestion": wrapper.get_market_congestion_indicator,
        "order_book": wrapper.get_order_book_analysis,

        # Financials
        "financial_reports": wrapper.get_financial_reports,
        "profit_forecasts": wrapper.get_profit_forecasts,
        "analyst_recommendations": wrapper.get_analyst_recommendations,
        "earnings_bulletins": wrapper.get_earnings_bulletins,
        "shareholder_distribution": wrapper.get_shareholder_distribution,
        "stock_pledge": wrapper.get_stock_pledge_info,

        # Market Sentiment
        "hot_stocks": wrapper.get_xueqiu_hot_stocks,
        "inner_trading": wrapper.get_inner_trading_data,
        "stock_comments": wrapper.get_stock_comments,
        "baidu_polls": wrapper.get_baidu_poll_data
    }

    method = endpoint_map.get(endpoint)
    if method:
        if args:
            # For endpoints that require parameters
            try:
                if endpoint in ["individual_fund_flow", "cost_layer", "order_book"]:
                    result = method(symbol=args[0] if args else "sh600000")
                elif endpoint in ["valuation_analysis"]:
                    result = method(market=args[0] if args else "sh")
                elif endpoint == "stock_comments":
                    result = method(symbol=args[0] if args else None)
                else:
                    result = method(*args)
            except Exception as e:
                result = {"error": str(e), "endpoint": endpoint}
        else:
            result = method()
        print(json.dumps(result, indent=2, ensure_ascii=True))
    else:
        print(json.dumps({
            "error": f"Unknown endpoint: {endpoint}",
            "available_endpoints": list(endpoint_map.keys())
        }, indent=2))


if __name__ == "__main__":
    main()