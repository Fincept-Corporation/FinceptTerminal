"""
AKShare Derivatives Data Wrapper
Comprehensive wrapper for options, futures and derivatives market data
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


class DerivativesWrapper:
    """Comprehensive derivatives data wrapper with 80+ endpoints"""

    def __init__(self):
        self.session = None
        self.default_timeout = 30
        self.retry_delay = 2

        # Common date parameters
        self.default_start_date = (datetime.now() - timedelta(days=365)).strftime('%Y%m%d')
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

    # ==================== OPTIONS MARKET OVERVIEW ====================

    def get_option_em(self) -> Dict[str, Any]:
        """Get options market overview"""
        return self._safe_call_with_retry(ak.option_em)

    def get_option_sse(self) -> Dict[str, Any]:
        """Get Shanghai Stock Exchange options"""
        return self._safe_call_with_retry(ak.option_sse)

    def get_option_szse(self) -> Dict[str, Any]:
        """Get Shenzhen Stock Exchange options"""
        return self._safe_call_with_retry(ak.option_szse)

    def get_option_cffex(self) -> Dict[str, Any]:
        """Get China Financial Futures Exchange options"""
        return self._safe_call_with_retry(ak.option_cffex)

    def get_option_czce(self) -> Dict[str, Any]:
        """Get Zhengzhou Commodity Exchange options"""
        return self._safe_call_with_retry(ak.option_czce)

    def get_option_dce(self) -> Dict[str, Any]:
        """Get Dalian Commodity Exchange options"""
        return self._safe_call_with_retry(ak.option_dce)

    def get_option_ine(self) -> Dict[str, Any]:
        """Get Shanghai International Energy Exchange options"""
        return self._safe_call_with_retry(ak.option_ine)

    def get_option_shfe(self) -> Dict[str, Any]:
        """Get Shanghai Futures Exchange options"""
        return self._safe_call_with_retry(ak.option_shfe)

    # ==================== REAL-TIME OPTIONS DATA ====================

    def get_option_current_sse(self) -> Dict[str, Any]:
        """Get current SSE options data"""
        return self._safe_call_with_retry(ak.option_current_sse)

    def get_option_current_szse(self) -> Dict[str, Any]:
        """Get current SZSE options data"""
        return self._safe_call_with_retry(ak.option_current_szse)

    def get_option_current_cffex(self) -> Dict[str, Any]:
        """Get current CFFEX options data"""
        return self._safe_call_with_retry(ak.option_current_cffex)

    def get_option_current_czce(self) -> Dict[str, Any]:
        """Get current CZCE options data"""
        return self._safe_call_with_retry(ak.option_current_czce)

    def get_option_current_dce(self) -> Dict[str, Any]:
        """Get current DCE options data"""
        return self._safe_call_with_retry(ak.option_current_dce)

    def get_option_current_ine(self) -> Dict[str, Any]:
        """Get current INE options data"""
        return self._safe_call_with_retry(ak.option_current_ine)

    def get_option_current_shfe(self) -> Dict[str, Any]:
        """Get current SHFE options data"""
        return self._safe_call_with_retry(ak.option_current_shfe)

    # ==================== OPTIONS ANALYSIS ====================

    def get_option_risk_analysis_em(self) -> Dict[str, Any]:
        """Get options risk analysis"""
        return self._safe_call_with_retry(ak.option_risk_analysis_em)

    def get_option_value_analysis_em(self) -> Dict[str, Any]:
        """Get options value analysis"""
        return self._safe_call_with_retry(ak.option_value_analysis_em)

    def get_option_premium_analysis_em(self) -> Dict[str, Any]:
        """Get options premium analysis"""
        return self._safe_call_with_retry(ak.option_premium_analysis_em)

    def get_option_greeks_em(self) -> Dict[str, Any]:
        """Get options greeks analysis"""
        return self._safe_call_with_retry(ak.option_greeks_em)

    def get_option_implied_volatility_em(self) -> Dict[str, Any]:
        """Get options implied volatility"""
        return self._safe_call_with_retry(ak.option_implied_volatility_em)

    def get_option_historical_volatility_em(self) -> Dict[str, Any]:
        """Get options historical volatility"""
        return self._safe_call_with_retry(ak.option_historical_volatility_em)

    def get_option_skew_em(self) -> Dict[str, Any]:
        """Get options volatility skew"""
        return self._safe_call_with_retry(ak.option_skew_em)

    def get_option_term_structure_em(self) -> Dict[str, Any]:
        """Get options term structure"""
        return self._safe_call_with_retry(ak.option_term_structure_em)

    # ==================== OPTIONS CHAIN ====================

    def get_option_chain_sse(self, symbol: str) -> Dict[str, Any]:
        """Get SSE options chain"""
        return self._safe_call_with_retry(ak.option_chain_sse, symbol=symbol)

    def get_option_chain_szse(self, symbol: str) -> Dict[str, Any]:
        """Get SZSE options chain"""
        return self._safe_call_with_retry(ak.option_chain_szse, symbol=symbol)

    def get_option_chain_cffex(self, symbol: str) -> Dict[str, Any]:
        """Get CFFEX options chain"""
        return self._safe_call_with_retry(ak.option_chain_cffex, symbol=symbol)

    def get_option_chain_czce(self, symbol: str) -> Dict[str, Any]:
        """Get CZCE options chain"""
        return self._safe_call_with_retry(ak.option_chain_czce, symbol=symbol)

    def get_option_chain_dce(self, symbol: str) -> Dict[str, Any]:
        """Get DCE options chain"""
        return self._safe_call_with_retry(ak.option_chain_dce, symbol=symbol)

    def get_option_chain_ine(self, symbol: str) -> Dict[str, Any]:
        """Get INE options chain"""
        return self._safe_call_with_retry(ak.option_chain_ine, symbol=symbol)

    def get_option_chain_shfe(self, symbol: str) -> Dict[str, Any]:
        """Get SHFE options chain"""
        return self._safe_call_with_retry(ak.option_chain_shfe, symbol=symbol)

    # ==================== OPTIONS TRADING DATA ====================

    def get_option_volume_sse(self) -> Dict[str, Any]:
        """Get SSE options volume data"""
        return self._safe_call_with_retry(ak.option_volume_sse)

    def get_option_volume_szse(self) -> Dict[str, Any]:
        """Get SZSE options volume data"""
        return self._safe_call_with_retry(ak.option_volume_szse)

    def get_option_volume_cffex(self) -> Dict[str, Any]:
        """Get CFFEX options volume data"""
        return self._safe_call_with_retry(ak.option_volume_cffex)

    def get_option_volume_czce(self) -> Dict[str, Any]:
        """Get CZCE options volume data"""
        return self._safe_call_with_retry(ak.option_volume_czce)

    def get_option_volume_dce(self) -> Dict[str, Any]:
        """Get DCE options volume data"""
        return self._safe_call_with_retry(ak.option_volume_dce)

    def get_option_volume_ine(self) -> Dict[str, Any]:
        """Get INE options volume data"""
        return self._safe_call_with_retry(ak.option_volume_ine)

    def get_option_volume_shfe(self) -> Dict[str, Any]:
        """Get SHFE options volume data"""
        return self._safe_call_with_retry(ak.option_volume_shfe)

    def get_option_open_interest_sse(self) -> Dict[str, Any]:
        """Get SSE options open interest"""
        return self._safe_call_with_retry(ak.option_open_interest_sse)

    def get_option_open_interest_szse(self) -> Dict[str, Any]:
        """Get SZSE options open interest"""
        return self._safe_call_with_retry(ak.option_open_interest_szse)

    def get_option_open_interest_cffex(self) -> Dict[str, Any]:
        """Get CFFEX options open interest"""
        return self._safe_call_with_retry(ak.option_open_interest_cffex)

    def get_option_open_interest_czce(self) -> Dict[str, Any]:
        """Get CZCE options open interest"""
        return self._safe_call_with_retry(ak.option_open_interest_czce)

    def get_option_open_interest_dce(self) -> Dict[str, Any]:
        """Get DCE options open interest"""
        return self._safe_call_with_retry(ak.option_open_interest_dce)

    def get_option_open_interest_ine(self) -> Dict[str, Any]:
        """Get INE options open interest"""
        return self._safe_call_with_retry(ak.option_open_interest_ine)

    def get_option_open_interest_shfe(self) -> Dict[str, Any]:
        """Get SHFE options open interest"""
        return self._safe_call_with_retry(ak.option_open_interest_shfe)

    # ==================== OPTIONS HISTORICAL DATA ====================

    def get_option_hist_sse(self, symbol: str, start_date: str = None, end_date: str = None) -> Dict[str, Any]:
        """Get SSE options historical data"""
        start = start_date or self.default_start_date
        end = end_date or self.default_end_date
        return self._safe_call_with_retry(ak.option_hist_sse, symbol=symbol, start_date=start, end_date=end)

    def get_option_hist_szse(self, symbol: str, start_date: str = None, end_date: str = None) -> Dict[str, Any]:
        """Get SZSE options historical data"""
        start = start_date or self.default_start_date
        end = end_date or self.default_end_date
        return self._safe_call_with_retry(ak.option_hist_szse, symbol=symbol, start_date=start, end_date=end)

    def get_option_hist_cffex(self, symbol: str, start_date: str = None, end_date: str = None) -> Dict[str, Any]:
        """Get CFFEX options historical data"""
        start = start_date or self.default_start_date
        end = end_date or self.default_end_date
        return self._safe_call_with_retry(ak.option_hist_cffex, symbol=symbol, start_date=start, end_date=end)

    def get_option_hist_czce(self, symbol: str, start_date: str = None, end_date: str = None) -> Dict[str, Any]:
        """Get CZCE options historical data"""
        start = start_date or self.default_start_date
        end = end_date or self.default_end_date
        return self._safe_call_with_retry(ak.option_hist_czce, symbol=symbol, start_date=start, end_date=end)

    def get_option_hist_dce(self, symbol: str, start_date: str = None, end_date: str = None) -> Dict[str, Any]:
        """Get DCE options historical data"""
        start = start_date or self.default_start_date
        end = end_date or self.default_end_date
        return self._safe_call_with_retry(ak.option_hist_dce, symbol=symbol, start_date=start, end_date=end)

    def get_option_hist_ine(self, symbol: str, start_date: str = None, end_date: str = None) -> Dict[str, Any]:
        """Get INE options historical data"""
        start = start_date or self.default_start_date
        end = end_date or self.default_end_date
        return self._safe_call_with_retry(ak.option_hist_ine, symbol=symbol, start_date=start, end_date=end)

    def get_option_hist_shfe(self, symbol: str, start_date: str = None, end_date: str = None) -> Dict[str, Any]:
        """Get SHFE options historical data"""
        start = start_date or self.default_start_date
        end = end_date or self.default_end_date
        return self._safe_call_with_retry(ak.option_hist_shfe, symbol=symbol, start_date=start, end_date=end)

    # ==================== OPTIONS MARGIN ====================

    def get_option_margin_sse(self) -> Dict[str, Any]:
        """Get SSE options margin data"""
        return self._safe_call_with_retry(ak.option_margin_sse)

    def get_option_margin_szse(self) -> Dict[str, Any]:
        """Get SZSE options margin data"""
        return self._safe_call_with_retry(ak.option_margin_szse)

    def get_option_margin_cffex(self) -> Dict[str, Any]:
        """Get CFFEX options margin data"""
        return self._safe_call_with_retry(ak.option_margin_cffex)

    def get_option_margin_czce(self) -> Dict[str, Any]:
        """Get CZCE options margin data"""
        return self._safe_call_with_retry(ak.option_margin_czce)

    def get_option_margin_dce(self) -> Dict[str, Any]:
        """Get DCE options margin data"""
        return self._safe_call_with_retry(ak.option_margin_dce)

    def get_option_margin_ine(self) -> Dict[str, Any]:
        """Get INE options margin data"""
        return self._safe_call_with_retry(ak.option_margin_ine)

    def get_option_margin_shfe(self) -> Dict[str, Any]:
        """Get SHFE options margin data"""
        return self._safe_call_with_retry(ak.option_margin_shfe)

    # ==================== FUTURES DERIVATIVES ====================

    def get_futures_basis(self, symbol: str) -> Dict[str, Any]:
        """Get futures basis analysis"""
        return self._safe_call_with_retry(ak.futures_basis, symbol=symbol)

    def get_futures_roll_yield(self, symbol: str) -> Dict[str, Any]:
        """Get futures roll yield analysis"""
        return self._safe_call_with_retry(ak.futures_roll_yield, symbol=symbol)

    def get_futures_spread(self, symbol1: str, symbol2: str) -> Dict[str, Any]:
        """Get futures spread analysis"""
        return self._safe_call_with_retry(ak.futures_spread, symbol1=symbol1, symbol2=symbol2)

    def get_futures_butterfly(self, symbol1: str, symbol2: str, symbol3: str) -> Dict[str, Any]:
        """Get futures butterfly spread"""
        return self._safe_call_with_retry(ak.futures_butterfly, symbol1=symbol1, symbol2=symbol2, symbol3=symbol3)

    def get_futures_calendar_spread(self, symbol: str) -> Dict[str, Any]:
        """Get futures calendar spread"""
        return self._safe_call_with_retry(ak.futures_calendar_spread, symbol=symbol)

    def get_futures_crack_spread(self) -> Dict[str, Any]:
        """Get futures crack spread"""
        return self._safe_call_with_retry(ak.futures_crack_spread)

    def get_futures_crush_spread(self) -> Dict[str, Any]:
        """Get futures crush spread"""
        return self._safe_call_with_retry(ak.futures_crush_spread)

    def get_futures_spark_spread(self) -> Dict[str, Any]:
        """Get futures spark spread"""
        return self._safe_call_with_retry(ak.futures_spark_spread)

    def get_futures_tower_spread(self) -> Dict[str, Any]:
        """Get futures tower spread"""
        return self._safe_call_with_retry(ak.futures_tower_spread)

    # ==================== FUTURES INVENTORY AND STORAGE ====================

    def get_futures_inventory_em(self) -> Dict[str, Any]:
        """Get futures inventory data"""
        return self._safe_call_with_retry(ak.futures_inventory_em)

    def get_futures_inventory_lme(self) -> Dict[str, Any]:
        """Get LME inventory data"""
        return self._safe_call_with_retry(ak.futures_inventory_lme)

    def get_futures_inventory_shfe(self) -> Dict[str, Any]:
        """Get SHFE inventory data"""
        return self._safe_call_with_retry(ak.futures_inventory_shfe)

    def get_futures_inventory_czce(self) -> Dict[str, Any]:
        """Get CZCE warehouse data"""
        return self._safe_call_with_retry(ak.futures_inventory_czce)

    def get_futures_inventory_dce(self) -> Dict[str, Any]:
        """Get DCE warehouse data"""
        return self._safe_call_with_retry(ak.futures_inventory_dce)

    # ==================== DERIVATIVES MARKET DEPTH ====================

    def get_option_depth_sse(self, symbol: str) -> Dict[str, Any]:
        """Get SSE options market depth"""
        return self._safe_call_with_retry(ak.option_depth_sse, symbol=symbol)

    def get_option_depth_szse(self, symbol: str) -> Dict[str, Any]:
        """Get SZSE options market depth"""
        return self._safe_call_with_retry(ak.option_depth_szse, symbol=symbol)

    def get_option_depth_cffex(self, symbol: str) -> Dict[str, Any]:
        """Get CFFEX options market depth"""
        return self._safe_call_with_retry(ak.option_depth_cffex, symbol=symbol)

    def get_option_depth_czce(self, symbol: str) -> Dict[str, Any]:
        """Get CZCE options market depth"""
        return self._safe_call_with_retry(ak.option_depth_czce, symbol=symbol)

    def get_option_depth_dce(self, symbol: str) -> Dict[str, Any]:
        """Get DCE options market depth"""
        return self._safe_call_with_retry(ak.option_depth_dce, symbol=symbol)

    def get_option_depth_ine(self, symbol: str) -> Dict[str, Any]:
        """Get INE options market depth"""
        return self._safe_call_with_retry(ak.option_depth_ine, symbol=symbol)

    def get_option_depth_shfe(self, symbol: str) -> Dict[str, Any]:
        """Get SHFE options market depth"""
        return self._safe_call_with_retry(ak.option_depth_shfe, symbol=symbol)

    # ==================== DERIVATIVES TRADING STATISTICS ====================

    def get_option_stats_sse(self) -> Dict[str, Any]:
        """Get SSE options trading statistics"""
        return self._safe_call_with_retry(ak.option_stats_sse)

    def get_option_stats_szse(self) -> Dict[str, Any]:
        """Get SZSE options trading statistics"""
        return self._safe_call_with_retry(ak.option_stats_szse)

    def get_option_stats_cffex(self) -> Dict[str, Any]:
        """Get CFFEX options trading statistics"""
        return self._safe_call_with_retry(ak.option_stats_cffex)

    def get_option_stats_czce(self) -> Dict[str, Any]:
        """Get CZCE options trading statistics"""
        return self._safe_call_with_retry(ak.option_stats_czce)

    def get_option_stats_dce(self) -> Dict[str, Any]:
        """Get DCE options trading statistics"""
        return self._safe_call_with_retry(ak.option_stats_dce)

    def get_option_stats_ine(self) -> Dict[str, Any]:
        """Get INE options trading statistics"""
        return self._safe_call_with_retry(ak.option_stats_ine)

    def get_option_stats_shfe(self) -> Dict[str, Any]:
        """Get SHFE options trading statistics"""
        return self._safe_call_with_retry(ak.option_stats_shfe)

    # ==================== EXOTIC DERIVATIVES ====================

    def get_variants_warrants(self) -> Dict[str, Any]:
        """Get warrants data"""
        return self._safe_call_with_retry(ak.variants_warrants)

    def get_variants_cbs(self) -> Dict[str, Any]:
        """Get convertible bonds data"""
        return self._safe_call_with_retry(ak.variants_cbs)

    def get_variants_etfs(self) -> Dict[str, Any]:
        """Get ETF derivatives data"""
        return self._safe_call_with_retry(ak.variants_etfs)

    def get_variants_reits(self) -> Dict[str, Any]:
        """Get REITs data"""
        return self._safe_call_with_retry(ak.variants_reits)

    def get_variants_dr(self) -> Dict[str, Any]:
        """Get depositary receipts data"""
        return self._safe_call_with_retry(ak.variants_dr)

    # ==================== DERIVATIVES VOLATILITY INDEXES ====================

    def get_vol_index_cffex(self) -> Dict[str, Any]:
        """Get CFFEX volatility index"""
        return self._safe_call_with_retry(ak.vol_index_cffex)

    def get_vol_index_szse(self) -> Dict[str, Any]:
        """Get SZSE volatility index"""
        return self._safe_call_with_retry(ak.vol_index_szse)

    def get_vol_index_sse(self) -> Dict[str, Any]:
        """Get SSE volatility index"""
        return self._safe_call_with_retry(ak.vol_index_sse)

    def get_vol_term_structure(self) -> Dict[str, Any]:
        """Get volatility term structure"""
        return self._safe_call_with_retry(ak.vol_term_structure)

    def get_vol_surface(self) -> Dict[str, Any]:
        """Get volatility surface"""
        return self._safe_call_with_retry(ak.vol_surface)

    # ==================== DERIVATIVES RISK METRICS ====================

    def get_risk_var(self) -> Dict[str, Any]:
        """Get Value at Risk data"""
        return self._safe_call_with_retry(ak.risk_var)

    def get_risk_expected_shortfall(self) -> Dict[str, Any]:
        """Get Expected Shortfall data"""
        return self._safe_call_with_retry(ak.risk_expected_shortfall)

    def get_risk_greeks(self) -> Dict[str, Any]:
        """Get options greeks risk metrics"""
        return self._safe_call_with_retry(ak.risk_greeks)

    def get_risk_greeks_portfolio(self) -> Dict[str, Any]:
        """Get portfolio greeks analysis"""
        return self._safe_call_with_retry(ak.risk_greeks_portfolio)

    # ==================== UTILITIES ====================

    def get_all_available_endpoints(self) -> Dict[str, Any]:
        """Get list of all available endpoints in this wrapper"""
        methods = [method for method in dir(self) if method.startswith('get_') and callable(getattr(self, method))]
        return {
            "available_endpoints": methods,
            "total_count": len(methods),
            "categories": {
                "Options Overview": [m for m in methods if any(x in m for x in ["option_em", "option_sse", "option_szse", "option_cffex", "option_czce", "option_dce", "option_ine", "option_shfe"])],
                "Real-time Options": [m for m in methods if "option_current" in m],
                "Options Analysis": [m for m in methods if any(x in m for x in ["risk_analysis", "value_analysis", "premium_analysis", "greeks_em", "implied_volatility", "historical_volatility", "skew", "term_structure"])],
                "Options Chain": [m for m in methods if "option_chain" in m],
                "Options Trading": [m for m in methods if any(x in m for x in ["option_volume", "option_open_interest"])],
                "Options Historical": [m for m in methods if "option_hist" in m],
                "Options Margin": [m for m in methods if "option_margin" in m],
                "Futures Derivatives": [m for m in methods if any(x in m for x in ["futures_basis", "futures_roll_yield", "futures_spread", "futures_butterfly", "futures_calendar_spread", "futures_crack_spread", "futures_crush_spread", "futures_spark_spread", "futures_tower_spread"])],
                "Futures Inventory": [m for m in methods if "futures_inventory" in m],
                "Market Depth": [m for m in methods if "option_depth" in m],
                "Trading Statistics": [m for m in methods if "option_stats" in m],
                "Exotic Derivatives": [m for m in methods if "variants" in m],
                "Volatility Indexes": [m for m in methods if any(x in m for x in ["vol_index", "vol_term", "vol_surface"])],
                "Risk Metrics": [m for m in methods if "risk_" in m]
            },
            "timestamp": int(datetime.now().timestamp())
        }

  

# ==================== COMMAND LINE INTERFACE ====================

def main():
    """Command line interface for the Derivatives wrapper"""
    wrapper = DerivativesWrapper()

    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python akshare_derivatives.py <endpoint> [args...]",
            "available_endpoints": wrapper.get_all_available_endpoints()["available_endpoints"]
        }, indent=2))
        return

    endpoint = sys.argv[1]
    args = sys.argv[2:] if len(sys.argv) > 2 else []

    # Map endpoint names to method calls
    endpoint_map = {
        "get_all_endpoints": wrapper.get_all_available_endpoints,
        # Options Overview
        "option_em": wrapper.get_option_em,
        "option_sse": wrapper.get_option_sse,
        "option_szse": wrapper.get_option_szse,
        "option_cffex": wrapper.get_option_cffex,
        # Real-time Options
        "option_current_sse": wrapper.get_option_current_sse,
        "option_current_szse": wrapper.get_option_current_szse,
        "option_current_cffex": wrapper.get_option_current_cffex,
        # Options Analysis
        "option_risk_analysis": wrapper.get_option_risk_analysis_em,
        "option_value_analysis": wrapper.get_option_value_analysis_em,
        "option_premium_analysis": wrapper.get_option_premium_analysis_em,
        "option_greeks": wrapper.get_option_greeks_em,
        "option_implied_volatility": wrapper.get_option_implied_volatility_em,
        "option_historical_volatility": wrapper.get_option_historical_volatility_em,
        "option_skew": wrapper.get_option_skew_em,
        "option_term_structure": wrapper.get_option_term_structure_em,
        # Options Chain
        "option_chain_sse": wrapper.get_option_chain_sse,
        "option_chain_szse": wrapper.get_option_chain_szse,
        "option_chain_cffex": wrapper.get_option_chain_cffex,
        # Options Trading
        "option_volume_sse": wrapper.get_option_volume_sse,
        "option_volume_szse": wrapper.get_option_volume_szse,
        "option_volume_cffex": wrapper.get_option_volume_cffex,
        "option_open_interest_sse": wrapper.get_option_open_interest_sse,
        "option_open_interest_szse": wrapper.get_option_open_interest_szse,
        "option_open_interest_cffex": wrapper.get_option_open_interest_cffex,
        # Options Historical
        "option_hist_sse": wrapper.get_option_hist_sse,
        "option_hist_szse": wrapper.get_option_hist_szse,
        "option_hist_cffex": wrapper.get_option_hist_cffex,
        # Options Margin
        "option_margin_sse": wrapper.get_option_margin_sse,
        "option_margin_szse": wrapper.get_option_margin_szse,
        "option_margin_cffex": wrapper.get_option_margin_cffex,
        # Futures Derivatives
        "futures_basis": wrapper.get_futures_basis,
        "futures_roll_yield": wrapper.get_futures_roll_yield,
        "futures_spread": wrapper.get_futures_spread,
        "futures_butterfly": wrapper.get_futures_butterfly,
        "futures_calendar_spread": wrapper.get_futures_calendar_spread,
        "futures_crack_spread": wrapper.get_futures_crack_spread,
        "futures_crush_spread": wrapper.get_futures_crush_spread,
        "futures_spark_spread": wrapper.get_futures_spark_spread,
        "futures_tower_spread": wrapper.get_futures_tower_spread,
        # Futures Inventory
        "futures_inventory": wrapper.get_futures_inventory_em,
        "futures_inventory_lme": wrapper.get_futures_inventory_lme,
        "futures_inventory_shfe": wrapper.get_futures_inventory_shfe,
        # Exotic Derivatives
        "variants_warrants": wrapper.get_variants_warrants,
        "variants_cbs": wrapper.get_variants_cbs,
        "variants_etfs": wrapper.get_variants_etfs,
        "variants_reits": wrapper.get_variants_reits,
        # Volatility Indexes
        "vol_index_cffex": wrapper.get_vol_index_cffex,
        "vol_index_szse": wrapper.get_vol_index_szse,
        "vol_index_sse": wrapper.get_vol_index_sse,
        "vol_term_structure": wrapper.get_vol_term_structure,
        "vol_surface": wrapper.get_vol_surface,
        # Risk Metrics
        "risk_var": wrapper.get_risk_var,
        "risk_expected_shortfall": wrapper.get_risk_expected_shortfall,
        "risk_greeks": wrapper.get_risk_greeks,
        "risk_greeks_portfolio": wrapper.get_risk_greeks_portfolio
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