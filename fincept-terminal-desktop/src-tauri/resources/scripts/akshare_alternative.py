"""
AKShare Alternative Data Wrapper
Wrapper for alternative and specialized data sources
Returns JSON output for Rust integration

NOTE: This file has been cleaned to contain only VALID akshare functions.
Many placeholder functions were removed as they don't exist in akshare 1.18.20.
"""

import sys
import json
import pandas as pd
import akshare as ak
from typing import Dict, Any, List
from datetime import datetime, timedelta


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


class AlternativeDataWrapper:
    """Alternative data wrapper with VALIDATED akshare functions"""

    def __init__(self):
        self.default_timeout = 15
        self.retry_delay = 2
        self.default_start_date = (datetime.now() - timedelta(days=365)).strftime('%Y%m%d')
        self.default_end_date = datetime.now().strftime('%Y%m%d')

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

                if result is not None:
                    if hasattr(result, 'empty') and not result.empty:
                        return {
                            "success": True,
                            "data": self._convert_dataframe_to_json_safe(result),
                            "count": len(result),
                            "timestamp": int(datetime.now().timestamp()),
                            "source": f"akshare.{func.__name__}"
                        }
                    elif hasattr(result, '__len__') and len(result) > 0:
                        return {
                            "success": True,
                            "data": result if isinstance(result, list) else [result],
                            "count": len(result) if isinstance(result, list) else 1,
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

    # ==================== AIR QUALITY ====================

    def get_air_quality_hebei(self) -> Dict[str, Any]:
        """Get Hebei province air quality data"""
        return self._safe_call_with_retry(ak.air_quality_hebei)

    # ==================== ENERGY ====================

    def get_energy_carbon(self) -> Dict[str, Any]:
        """Get carbon emission trading data (domestic)"""
        return self._safe_call_with_retry(ak.energy_carbon_domestic)

    def get_energy_oil_hist(self) -> Dict[str, Any]:
        """Get historical oil price data"""
        return self._safe_call_with_retry(ak.energy_oil_hist)

    def get_energy_oil_detail(self) -> Dict[str, Any]:
        """Get detailed oil price data"""
        return self._safe_call_with_retry(ak.energy_oil_detail)

    # ==================== MOVIES ====================

    def get_movie_boxoffice_daily(self) -> Dict[str, Any]:
        """Get daily movie box office data"""
        return self._safe_call_with_retry(ak.movie_boxoffice_daily)

    def get_movie_boxoffice_weekly(self) -> Dict[str, Any]:
        """Get weekly movie box office data"""
        return self._safe_call_with_retry(ak.movie_boxoffice_weekly)

    def get_movie_boxoffice_monthly(self) -> Dict[str, Any]:
        """Get monthly movie box office data"""
        return self._safe_call_with_retry(ak.movie_boxoffice_monthly)

    def get_movie_boxoffice_yearly(self) -> Dict[str, Any]:
        """Get yearly movie box office data"""
        return self._safe_call_with_retry(ak.movie_boxoffice_yearly)

    def get_movie_boxoffice_realtime(self) -> Dict[str, Any]:
        """Get real-time movie box office data"""
        return self._safe_call_with_retry(ak.movie_boxoffice_realtime)

    # ==================== AUTOMOBILES ====================

    def get_car_market_total(self) -> Dict[str, Any]:
        """Get total car market data from CPCA"""
        return self._safe_call_with_retry(ak.car_market_total_cpca)

    def get_car_market_segment(self) -> Dict[str, Any]:
        """Get car market segment data from CPCA"""
        return self._safe_call_with_retry(ak.car_market_segment_cpca)

    def get_car_sale_rank(self) -> Dict[str, Any]:
        """Get car sales ranking from Gasgoo"""
        return self._safe_call_with_retry(ak.car_sale_rank_gasgoo)

    # ==================== BILLIONAIRES ====================

    def get_billionaires_index(self) -> Dict[str, Any]:
        """Get Bloomberg Billionaires Index"""
        return self._safe_call_with_retry(ak.index_bloomberg_billionaires)

    def get_billionaires_hist(self) -> Dict[str, Any]:
        """Get Bloomberg Billionaires historical data"""
        return self._safe_call_with_retry(ak.index_bloomberg_billionaires_hist)

    # ==================== UTILITY FUNCTIONS ====================

    def get_all_available_endpoints(self) -> Dict[str, Any]:
        """Get list of all available endpoints"""
        endpoints = [
            "air_quality_hebei", "energy_carbon", "energy_oil_hist", "energy_oil_detail",
            "movie_boxoffice_daily", "movie_boxoffice_weekly", "movie_boxoffice_monthly",
            "movie_boxoffice_yearly", "movie_boxoffice_realtime",
            "car_market_total", "car_market_segment", "car_sale_rank",
            "billionaires_index", "billionaires_hist"
        ]

        return {
            "success": True,
            "endpoints": endpoints,
            "count": len(endpoints),
            "timestamp": int(datetime.now().timestamp())
        }


# Export wrapper instance
alternative_wrapper = AlternativeDataWrapper()


if __name__ == "__main__":
    """Test the wrapper"""
    import time

    print("Testing akshare_alternative.py wrapper...")

    # Test energy carbon
    result = alternative_wrapper.get_energy_carbon()
    print(f"\nEnergy Carbon: {result['success']}, count: {result.get('count', 0)}")

    # Test available endpoints
    endpoints = alternative_wrapper.get_all_available_endpoints()
    print(f"\nTotal endpoints: {endpoints['count']}")
    print("Available endpoints:", endpoints['endpoints'])
