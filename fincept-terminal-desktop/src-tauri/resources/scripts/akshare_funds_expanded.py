"""
AKShare Expanded Funds Data Wrapper
Comprehensive wrapper for enhanced fund market data and analysis
Returns JSON output for Rust integration
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
    """Comprehensive expanded funds data wrapper with 40+ endpoints"""

    def __init__(self):
        self.session = None
        self.default_timeout = 30
        self.retry_delay = 2

        # Common date parameters
        self.default_start_date = (datetime.now() - timedelta(days=3650)).strftime('%Y%m%d')  # 10 years
        self.default_end_date = datetime.now().strftime('%Y%m%d')

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
                            "data": result.to_dict('records'),
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

                error_obj = AKShareError(
                    endpoint=func.__name__,
                    error=last_error,
                    data_source=getattr(func, '__module__', 'unknown')
                )
                return {
                    "success": False,
                    "error": error_obj.to_dict(),
                    "data": [],
                    "count": 0,
                    "timestamp": int(datetime.now().timestamp()),
                    "data_quality": "experimental"
                }

        # Should never reach here
        return {
            "success": False,
            "error": f"Max retries exceeded: {last_error}",
            "data": [],
            "count": 0,
            "timestamp": int(datetime.now().timestamp())
        }

    # ==================== ENHANCED ETF DATA ====================

    def get_fund_etf_ths(self) -> Dict[str, Any]:
        """Get enhanced ETF data from THS"""
        return self._safe_call_with_retry(ak.fund_etf_ths)

    def get_fund_etf_category(self) -> Dict[str, Any]:
        """Get ETF category data"""
        return self._safe_call_with_retry(ak.fund_etf_category)

    def get_fund_etf_rank(self) -> Dict[str, Any]:
        """Get ETF ranking data"""
        return self._safe_call_with_retry(ak.fund_etf_rank)

    def get_fund_etf_portfolio(self) -> Dict[str, Any]:
        """Get ETF portfolio holdings"""
        return self._safe_call_with_retry(ak.fund_etf_portfolio)

    def get_fund_etf_dividend(self) -> Dict[str, Any]:
        """Get ETF dividend data"""
        return self._safe_call_with_retry(ak.fund_etf_dividend)

    def get_fund_etf_split(self) -> Dict[str, Any]:
        """Get ETF split data"""
        return self._safe_call_with_retry(ak.fund_etf_split)

    def get_fund_etf_expense_ratio(self) -> Dict[str, Any]:
        """Get ETF expense ratio data"""
        return self._safe_call_with_retry(ak.fund_etf_expense_ratio)

    def get_fund_etf_tracking_error(self) -> Dict[str, Any]:
        """Get ETF tracking error data"""
        return self._safe_call_with_retry(ak.fund_etf_tracking_error)

    def get_fund_etf_premium_discount(self) -> Dict[str, Any]:
        """Get ETF premium/discount data"""
        return self._safe_call_with_retry(ak.fund_etf_premium_discount)

    def get_fund_etf_creation_redemption(self) -> Dict[str, Any]:
        """Get ETF creation/redemption data"""
        return self._safe_call_with_retry(ak.fund_etf_creation_redemption)

    # ==================== FUND PORTFOLIO AND HOLDINGS ====================

    def get_fund_portfolio_em(self, symbol: str) -> Dict[str, Any]:
        """Get fund portfolio holdings from Eastmoney"""
        return self._safe_call_with_retry(ak.fund_portfolio_em, symbol=symbol)

    def get_fund_holdings_em(self, symbol: str) -> Dict[str, Any]:
        """Get fund holdings from Eastmoney"""
        return self._safe_call_with_retry(ak.fund_holdings_em, symbol=symbol)

    def get_fund_holdings_change(self, symbol: str) -> Dict[str, Any]:
        """Get fund holdings changes"""
        return self._safe_call_with_retry(ak.fund_holdings_change, symbol=symbol)

    def get_fund_top_holdings(self, symbol: str) -> Dict[str, Any]:
        """Get fund top holdings"""
        return self._safe_call_with_retry(ak.fund_top_holdings, symbol=symbol)

    def get_fund_sector_allocation(self, symbol: str) -> Dict[str, Any]:
        """Get fund sector allocation"""
        return self._safe_call_with_retry(ak.fund_sector_allocation, symbol=symbol)

    def get_fund_asset_allocation(self, symbol: str) -> Dict[str, Any]:
        """Get fund asset allocation"""
        return self._safe_call_with_retry(ak.fund_asset_allocation, symbol=symbol)

    def get_fund_geographic_allocation(self, symbol: str) -> Dict[str, Any]:
        """Get fund geographic allocation"""
        return self._safe_call_with_retry(ak.fund_geographic_allocation, symbol=symbol)

    def get_fund_style_box(self, symbol: str) -> Dict[str, Any]:
        """Get fund style box data"""
        return self._safe_call_with_retry(ak.fund_style_box, symbol=symbol)

    # ==================== FUND RATINGS AND PERFORMANCE ====================

    def get_fund_rating(self) -> Dict[str, Any]:
        """Get fund ratings"""
        return self._safe_call_with_retry(ak.fund_rating)

    def get_fund_rating_morningstar(self) -> Dict[str, Any]:
        """Get Morningstar fund ratings"""
        return self._safe_call_with_retry(ak.fund_rating_morningstar)

    def get_fund_rating_zhishu(self) -> Dict[str, Any]:
        """Get Zhishu fund ratings"""
        return self._safe_call_with_retry(ak.fund_rating_zhishu)

    def get_fund_performance_attribution(self, symbol: str) -> Dict[str, Any]:
        """Get fund performance attribution"""
        return self._safe_call_with_retry(ak.fund_performance_attribution, symbol=symbol)

    def get_fund_risk_metrics(self, symbol: str) -> Dict[str, Any]:
        """Get fund risk metrics"""
        return self._safe_call_with_retry(ak.fund_risk_metrics, symbol=symbol)

    def get_fund_sharpe_ratio(self, symbol: str) -> Dict[str, Any]:
        """Get fund Sharpe ratio"""
        return self._safe_call_with_retry(ak.fund_sharpe_ratio, symbol=symbol)

    def get_fund_information_ratio(self, symbol: str) -> Dict[str, Any]:
        """Get fund information ratio"""
        return self._safe_call_with_retry(ak.fund_information_ratio, symbol=symbol)

    def get_fund_sortino_ratio(self, symbol: str) -> Dict[str, Any]:
        """Get fund Sortino ratio"""
        return self._safe_call_with_retry(ak.fund_sortino_ratio, symbol=symbol)

    def get_fund_beta(self, symbol: str) -> Dict[str, Any]:
        """Get fund beta"""
        return self._safe_call_with_retry(ak.fund_beta, symbol=symbol)

    def get_fund_alpha(self, symbol: str) -> Dict[str, Any]:
        """Get fund alpha"""
        return self._safe_call_with_retry(ak.fund_alpha, symbol=symbol)

    def get_fund_standard_deviation(self, symbol: str) -> Dict[str, Any]:
        """Get fund standard deviation"""
        return self._safe_call_with_retry(ak.fund_standard_deviation, symbol=symbol)

    def get_fund_max_drawdown(self, symbol: str) -> Dict[str, Any]:
        """Get fund maximum drawdown"""
        return self._safe_call_with_retry(ak.fund_max_drawdown, symbol=symbol)

    def get_fund_var(self, symbol: str) -> Dict[str, Any]:
        """Get fund Value at Risk"""
        return self._safe_call_with_retry(ak.fund_var, symbol=symbol)

    # ==================== FUND SCALE AND FLOWS ====================

    def get_fund_scale_em(self) -> Dict[str, Any]:
        """Get fund scale data from Eastmoney"""
        return self._safe_call_with_retry(ak.fund_scale_em)

    def get_fund_scale_trend(self, symbol: str) -> Dict[str, Any]:
        """Get fund scale trend"""
        return self._safe_call_with_retry(ak.fund_scale_trend, symbol=symbol)

    def get_fund_flow_em(self) -> Dict[str, Any]:
        """Get fund flow data from Eastmoney"""
        return self._safe_call_with_retry(ak.fund_flow_em)

    def get_fund_flow_daily(self) -> Dict[str, Any]:
        """Get daily fund flow data"""
        return self._safe_call_with_retry(ak.fund_flow_daily)

    def get_fund_flow_weekly(self) -> Dict[str, Any]:
        """Get weekly fund flow data"""
        return self._safe_call_with_retry(ak.fund_flow_weekly)

    def get_fund_flow_monthly(self) -> Dict[str, Any]:
        """Get monthly fund flow data"""
        return self._safe_call_with_retry(ak.fund_flow_monthly)

    def get_fund_subscription_redemption(self, symbol: str) -> Dict[str, Any]:
        """Get fund subscription/redemption data"""
        return self._safe_call_with_retry(ak.fund_subscription_redemption, symbol=symbol)

    def get_fund_new_issue(self) -> Dict[str, Any]:
        """Get new fund issue data"""
        return self._safe_call_with_retry(ak.fund_new_issue)

    def get_fund_new_issue_detail(self, fund_code: str) -> Dict[str, Any]:
        """Get new fund issue details"""
        return self._safe_call_with_retry(ak.fund_new_issue_detail, fund_code=fund_code)

    # ==================== AMAC AND REGULATORY DATA ====================

    def get_fund_amac(self) -> Dict[str, Any]:
        """Get AMAC (Asset Management Association of China) data"""
        return self._safe_call_with_retry(ak.fund_amac)

    def get_fund_amac_statistics(self) -> Dict[str, Any]:
        """Get AMAC statistics"""
        return self._safe_call_with_retry(ak.fund_amac_statistics)

    def get_fund_amac_private(self) -> Dict[str, Any]:
        """Get AMAC private fund data"""
        return self._safe_call_with_retry(ak.fund_amac_private)

    def get_fund_amac_securities(self) -> Dict[str, Any]:
        """Get AMAC securities fund data"""
        return self._safe_call_with_retry(ak.fund_amac_securities)

    def get_fund_amac_futures(self) -> Dict[str, Any]:
        """Get AMAC futures fund data"""
        return self._safe_call_with_retry(ak.fund_amac_futures)

    def get_fund_regulatory_filing(self) -> Dict[str, Any]:
        """Get fund regulatory filing data"""
        return self._safe_call_with_retry(ak.fund_regulatory_filing)

    def get_fund_disclosure(self) -> Dict[str, Any]:
        """Get fund disclosure data"""
        return self._safe_call_with_retry(ak.fund_disclosure)

    # ==================== SPECIALIZED FUNDS ====================

    def get_fund_financial_fund_daily_em(self) -> Dict[str, Any]:
        """Get financial fund daily data from Eastmoney"""
        return self._safe_call_with_retry(ak.fund_financial_fund_daily_em)

    def get_fund_money_fund_daily_em(self) -> Dict[str, Any]:
        """Get money market fund daily data from Eastmoney"""
        return self._safe_call_with_retry(ak.fund_money_fund_daily_em)

    def get_fund_hedge_fund(self) -> Dict[str, Any]:
        """Get hedge fund data"""
        return self._safe_call_with_retry(ak.fund_hedge_fund)

    def get_fund_private_equity(self) -> Dict[str, Any]:
        """Get private equity fund data"""
        return self._safe_call_with_retry(ak.fund_private_equity)

    def get_fund_venture_capital(self) -> Dict[str, Any]:
        """Get venture capital fund data"""
        return self._safe_call_with_retry(ak.fund_venture_capital)

    def get_fund_qfii(self) -> Dict[str, Any]:
        """Get QFII fund data"""
        return self._safe_call_with_retry(ak.fund_qfii)

    def get_fund_rqfii(self) -> Dict[str, Any]:
        """Get RQFII fund data"""
        return self._safe_call_with_retry(ak.fund_rqfii)

    def get_fund_stock_connect(self) -> Dict[str, Any]:
        """Get Stock Connect fund data"""
        return self._safe_call_with_retry(ak.fund_stock_connect)

    def get_fund_bond_connect(self) -> Dict[str, Any]:
        """Get Bond Connect fund data"""
        return self._safe_call_with_retry(ak.fund_bond_connect)

    # ==================== FUND MANAGER AND ANALYST DATA ====================

    def get_fund_manager_detail(self, manager_name: str) -> Dict[str, Any]:
        """Get fund manager details"""
        return self._safe_call_with_retry(ak.fund_manager_detail, manager_name=manager_name)

    def get_fund_manager_performance(self, manager_name: str) -> Dict[str, Any]:
        """Get fund manager performance"""
        return self._safe_call_with_retry(ak.fund_manager_performance, manager_name=manager_name)

    def get_fund_manager_ranking(self) -> Dict[str, Any]:
        """Get fund manager ranking"""
        return self._safe_call_with_retry(ak.fund_manager_ranking)

    def get_fund_analyst_recommendation(self, symbol: str) -> Dict[str, Any]:
        """Get analyst recommendations for funds"""
        return self._safe_call_with_retry(ak.fund_analyst_recommendation, symbol=symbol)

    def get_fund_research_report(self) -> Dict[str, Any]:
        """Get fund research reports"""
        return self._safe_call_with_retry(ak.fund_research_report)

    def get_fund_commentary(self, symbol: str) -> Dict[str, Any]:
        """Get fund commentary"""
        return self._safe_call_with_retry(ak.fund_commentary, symbol=symbol)

    # ==================== FUND COMPARISON AND ANALYSIS ====================

    def get_fund_comparison(self, symbols: List[str]) -> Dict[str, Any]:
        """Compare multiple funds"""
        return self._safe_call_with_retry(ak.fund_comparison, symbols=symbols)

    def get_fund_correlation(self, symbols: List[str]) -> Dict[str, Any]:
        """Get fund correlation analysis"""
        return self._safe_call_with_retry(ak.fund_correlation, symbols=symbols)

    def get_fund_cluster_analysis(self) -> Dict[str, Any]:
        """Get fund cluster analysis"""
        return self._safe_call_with_retry(ak.fund_cluster_analysis)

    def get_fund_factor_analysis(self, symbol: str) -> Dict[str, Any]:
        """Get fund factor analysis"""
        return self._safe_call_with_retry(ak.fund_factor_analysis, symbol=symbol)

    def get_fund_regression_analysis(self, symbol: str) -> Dict[str, Any]:
        """Get fund regression analysis"""
        return self._safe_call_with_retry(ak.fund_regression_analysis, symbol=symbol)

    # ==================== UTILITIES ====================

    def get_all_available_endpoints(self) -> Dict[str, Any]:
        """Get list of all available endpoints in this wrapper"""
        methods = [method for method in dir(self) if method.startswith('get_') and callable(getattr(self, method))]
        return {
            "available_endpoints": methods,
            "total_count": len(methods),
            "categories": {
                "Enhanced ETF Data": [m for m in methods if "fund_etf" in m],
                "Fund Portfolio & Holdings": [m for m in methods if any(x in m for x in ["fund_portfolio", "fund_holdings", "fund_top_holdings", "fund_sector_allocation", "fund_asset_allocation", "fund_geographic_allocation", "fund_style_box"])],
                "Fund Ratings & Performance": [m for m in methods if any(x in m for x in ["fund_rating", "fund_performance", "fund_sharpe", "fund_information", "fund_sortino", "fund_beta", "fund_alpha", "fund_standard", "fund_max", "fund_var"])],
                "Fund Scale & Flows": [m for m in methods if any(x in m for x in ["fund_scale", "fund_flow", "fund_subscription", "fund_new"])],
                "AMAC & Regulatory": [m for m in methods if any(x in m for x in ["fund_amac", "fund_regulatory", "fund_disclosure"])],
                "Specialized Funds": [m for m in methods if any(x in m for x in ["fund_financial", "fund_money", "fund_hedge", "fund_private", "fund_venture", "fund_qfii", "fund_rqfii", "fund_connect"])],
                "Fund Manager & Analyst": [m for m in methods if any(x in m for x in ["fund_manager", "fund_analyst", "fund_research", "fund_commentary"])],
                "Fund Comparison & Analysis": [m for m in methods if any(x in m for x in ["fund_comparison", "fund_correlation", "fund_cluster", "fund_factor", "fund_regression"])]
            },
            "timestamp": int(datetime.now().timestamp())
        }

    

# ==================== COMMAND LINE INTERFACE ====================

def main():
    """Command line interface for the Expanded Funds wrapper"""
    wrapper = ExpandedFundsWrapper()

    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python akshare_funds_expanded.py <endpoint> [args...]",
            "available_endpoints": wrapper.get_all_available_endpoints()["available_endpoints"]
        }, indent=2))
        return

    endpoint = sys.argv[1]
    args = sys.argv[2:] if len(sys.argv) > 2 else []

    # Map endpoint names to method calls
    endpoint_map = {
        "get_all_endpoints": wrapper.get_all_available_endpoints,
        # Enhanced ETF Data
        "etf_ths": wrapper.get_fund_etf_ths,
        "etf_category": wrapper.get_fund_etf_category,
        "etf_rank": wrapper.get_fund_etf_rank,
        "etf_portfolio": wrapper.get_fund_etf_portfolio,
        "etf_dividend": wrapper.get_fund_etf_dividend,
        "etf_split": wrapper.get_fund_etf_split,
        "etf_expense": wrapper.get_fund_etf_expense_ratio,
        "etf_tracking": wrapper.get_fund_etf_tracking_error,
        "etf_premium": wrapper.get_fund_etf_premium_discount,
        "etf_creation": wrapper.get_fund_etf_creation_redemption,
        # Fund Portfolio & Holdings
        "fund_portfolio": wrapper.get_fund_portfolio_em,
        "fund_holdings": wrapper.get_fund_holdings_em,
        "fund_holdings_change": wrapper.get_fund_holdings_change,
        "fund_top_holdings": wrapper.get_fund_top_holdings,
        "fund_sector": wrapper.get_fund_sector_allocation,
        "fund_asset": wrapper.get_fund_asset_allocation,
        "fund_geographic": wrapper.get_fund_geographic_allocation,
        "fund_style": wrapper.get_fund_style_box,
        # Fund Ratings & Performance
        "fund_rating": wrapper.get_fund_rating,
        "fund_rating_morningstar": wrapper.get_fund_rating_morningstar,
        "fund_rating_zhishu": wrapper.get_fund_rating_zhishu,
        "fund_performance": wrapper.get_fund_performance_attribution,
        "fund_risk": wrapper.get_fund_risk_metrics,
        "fund_sharpe": wrapper.get_fund_sharpe_ratio,
        "fund_info_ratio": wrapper.get_fund_information_ratio,
        "fund_sortino": wrapper.get_fund_sortino_ratio,
        "fund_beta": wrapper.get_fund_beta,
        "fund_alpha": wrapper.get_fund_alpha,
        "fund_stddev": wrapper.get_fund_standard_deviation,
        "fund_maxdd": wrapper.get_fund_max_drawdown,
        "fund_var": wrapper.get_fund_var,
        # Fund Scale & Flows
        "fund_scale": wrapper.get_fund_scale_em,
        "fund_scale_trend": wrapper.get_fund_scale_trend,
        "fund_flow": wrapper.get_fund_flow_em,
        "fund_flow_daily": wrapper.get_fund_flow_daily,
        "fund_flow_weekly": wrapper.get_fund_flow_weekly,
        "fund_flow_monthly": wrapper.get_fund_flow_monthly,
        "fund_subscription": wrapper.get_fund_subscription_redemption,
        "fund_new": wrapper.get_fund_new_issue,
        "fund_new_detail": wrapper.get_fund_new_issue_detail,
        # AMAC & Regulatory
        "fund_amac": wrapper.get_fund_amac,
        "fund_amac_stats": wrapper.get_fund_amac_statistics,
        "fund_amac_private": wrapper.get_fund_amac_private,
        "fund_amac_securities": wrapper.get_fund_amac_securities,
        "fund_amac_futures": wrapper.get_fund_amac_futures,
        "fund_regulatory": wrapper.get_fund_regulatory_filing,
        "fund_disclosure": wrapper.get_fund_disclosure,
        # Specialized Funds
        "fund_financial": wrapper.get_fund_financial_fund_daily_em,
        "fund_money": wrapper.get_fund_money_fund_daily_em,
        "fund_hedge": wrapper.get_fund_hedge_fund,
        "fund_private_equity": wrapper.get_fund_private_equity,
        "fund_venture": wrapper.get_fund_venture_capital,
        "fund_qfii": wrapper.get_fund_qfii,
        "fund_rqfii": wrapper.get_fund_rqfii,
        "fund_stock_connect": wrapper.get_fund_stock_connect,
        "fund_bond_connect": wrapper.get_fund_bond_connect,
        # Fund Manager & Analyst
        "fund_manager_detail": wrapper.get_fund_manager_detail,
        "fund_manager_perf": wrapper.get_fund_manager_performance,
        "fund_manager_rank": wrapper.get_fund_manager_ranking,
        "fund_analyst": wrapper.get_fund_analyst_recommendation,
        "fund_research": wrapper.get_fund_research_report,
        "fund_commentary": wrapper.get_fund_commentary,
        # Fund Comparison & Analysis
        "fund_comparison": wrapper.get_fund_comparison,
        "fund_correlation": wrapper.get_fund_correlation,
        "fund_cluster": wrapper.get_fund_cluster_analysis,
        "fund_factor": wrapper.get_fund_factor_analysis,
        "fund_regression": wrapper.get_fund_regression_analysis
    }

    method = endpoint_map.get(endpoint)
    if method:
        if args:
            try:
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