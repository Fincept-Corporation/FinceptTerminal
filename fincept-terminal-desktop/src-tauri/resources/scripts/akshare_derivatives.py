"""
AKShare Derivatives Data Wrapper
Wrapper for options and derivatives market data
Returns JSON output for Rust integration

NOTE: Cleaned to contain only VALID akshare option functions (46 total).
"""

import sys
import json
import pandas as pd
import akshare as ak
from typing import Dict, Any, List
from datetime import datetime, timedelta, date


class DateTimeEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, (datetime, date)):
            return obj.isoformat()
        return super().default(obj)


class AKShareError:
    """Custom error class for AKShare API errors"""
    def __init__(self, endpoint: str, error: str, data_source: str = None):
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
    """Derivatives data wrapper with VALIDATED akshare functions (46 total)"""

    def __init__(self):
        self.default_timeout = 30
        self.retry_delay = 2

    def _convert_dataframe_to_json_safe(self, df: pd.DataFrame) -> List[Dict[str, Any]]:
        """Convert DataFrame to JSON-safe format"""
        df_copy = df.copy()
        for col in df_copy.columns:
            if pd.api.types.is_datetime64_any_dtype(df_copy[col]):
                df_copy[col] = df_copy[col].astype(str)
        return df_copy.to_dict('records')

    def _safe_call_with_retry(self, func, *args, max_retries: int = 3, **kwargs) -> Dict[str, Any]:
        """Safely call AKShare function with retry logic"""
        last_error = None

        for attempt in range(max_retries):
            try:
                result = func(*args, **kwargs)

                if result is not None and hasattr(result, 'empty') and not result.empty:
                    return {
                        "success": True,
                        "data": self._convert_dataframe_to_json_safe(result),
                        "count": len(result),
                        "timestamp": int(datetime.now().timestamp()),
                        "source": f"akshare.{func.__name__}"
                    }

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
                    import time
                    time.sleep(self.retry_delay)
                continue

        error_obj = AKShareError(
            endpoint=func.__name__,
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

    # ==================== CFFEX OPTIONS ====================

    def get_option_cffex_hs300_daily_sina(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_cffex_hs300_daily_sina)

    def get_option_cffex_hs300_list_sina(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_cffex_hs300_list_sina)

    def get_option_cffex_hs300_spot_sina(self, symbol: str = "io2503") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_cffex_hs300_spot_sina, symbol=symbol)

    def get_option_cffex_sz50_daily_sina(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_cffex_sz50_daily_sina)

    def get_option_cffex_sz50_list_sina(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_cffex_sz50_list_sina)

    def get_option_cffex_sz50_spot_sina(self, symbol: str = "ho2503") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_cffex_sz50_spot_sina, symbol=symbol)

    def get_option_cffex_zz1000_daily_sina(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_cffex_zz1000_daily_sina)

    def get_option_cffex_zz1000_list_sina(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_cffex_zz1000_list_sina)

    def get_option_cffex_zz1000_spot_sina(self, symbol: str = "mo2503") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_cffex_zz1000_spot_sina, symbol=symbol)

    # ==================== COMMODITY OPTIONS ====================

    def get_option_comm_info(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_comm_info)

    def get_option_comm_symbol(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_comm_symbol)

    def get_option_commodity_contract_sina(self, symbol: str = "M") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_commodity_contract_sina, symbol=symbol)

    def get_option_commodity_contract_table_sina(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_commodity_contract_table_sina)

    def get_option_commodity_hist_sina(self, symbol: str = "M2505-C-3100") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_commodity_hist_sina, symbol=symbol)

    def get_option_contract_info_ctp(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_contract_info_ctp)

    # ==================== SSE/SZSE OPTIONS ====================

    def get_option_current_day_sse(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_current_day_sse)

    def get_option_current_day_szse(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_current_day_szse)

    def get_option_current_em(self, symbol: str = "沪深300ETF期权") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_current_em, symbol=symbol)

    def get_option_daily_stats_sse(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_daily_stats_sse)

    def get_option_daily_stats_szse(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_daily_stats_szse)

    # ==================== FINANCE OPTIONS ====================

    def get_option_finance_board(self, symbol: str = "510300") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_finance_board, symbol=symbol)

    def get_option_finance_minute_sina(self, symbol: str = "10004354") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_finance_minute_sina, symbol=symbol)

    def get_option_finance_sse_underlying(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_finance_sse_underlying)

    # ==================== HISTORICAL DATA ====================

    def get_option_hist_czce(self, symbol: str = "CF", date: str = "20241030") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_hist_czce, symbol=symbol, date=date)

    def get_option_hist_dce(self, symbol: str = "m", date: str = "20241030") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_hist_dce, symbol=symbol, date=date)

    def get_option_hist_gfex(self, symbol: str = "si", date: str = "20241030") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_hist_gfex, symbol=symbol, date=date)

    def get_option_hist_shfe(self, symbol: str = "cu", date: str = "20241030") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_hist_shfe, symbol=symbol, date=date)

    def get_option_hist_yearly_czce(self, symbol: str = "CF", year: str = "2024") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_hist_yearly_czce, symbol=symbol, year=year)

    # ==================== LHB & MARGIN ====================

    def get_option_lhb_em(self, symbol: str = "沪") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_lhb_em, symbol=symbol)

    def get_option_margin(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_margin)

    def get_option_margin_symbol(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_margin_symbol)

    # ==================== MINUTE & ANALYSIS ====================

    def get_option_minute_em(self, symbol: str = "10004354") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_minute_em, symbol=symbol)

    def get_option_premium_analysis_em(self, symbol: str = "沪深300ETF期权") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_premium_analysis_em, symbol=symbol)

    def get_option_risk_analysis_em(self, symbol: str = "沪深300ETF期权") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_risk_analysis_em, symbol=symbol)

    def get_option_risk_indicator_sse(self, symbol: str = "10004354", date: str = "20241030") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_risk_indicator_sse, symbol=symbol, date=date)

    def get_option_value_analysis_em(self, symbol: str = "沪深300ETF期权") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_value_analysis_em, symbol=symbol)

    # ==================== SSE SINA OPTIONS ====================

    def get_option_sse_codes_sina(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_sse_codes_sina)

    def get_option_sse_daily_sina(self, symbol: str = "10004354") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_sse_daily_sina, symbol=symbol)

    def get_option_sse_expire_day_sina(self, symbol: str = "510300") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_sse_expire_day_sina, symbol=symbol)

    def get_option_sse_greeks_sina(self, symbol: str = "510300") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_sse_greeks_sina, symbol=symbol)

    def get_option_sse_list_sina(self, symbol: str = "510300", date: str = "202412") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_sse_list_sina, symbol=symbol, date=date)

    def get_option_sse_minute_sina(self, symbol: str = "10004354") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_sse_minute_sina, symbol=symbol)

    def get_option_sse_spot_price_sina(self, symbol: str = "510300") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_sse_spot_price_sina, symbol=symbol)

    def get_option_sse_underlying_spot_price_sina(self, symbol: str = "510300") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_sse_underlying_spot_price_sina, symbol=symbol)

    # ==================== VOLATILITY ====================

    def get_option_vol_gfex(self, symbol: str = "si") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_vol_gfex, symbol=symbol)

    def get_option_vol_shfe(self, symbol: str = "cu") -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.option_vol_shfe, symbol=symbol)


# Export wrapper instance
derivatives_wrapper = DerivativesWrapper()
