"""
AKShare Alternative Data Wrapper
Wrapper for alternative and specialized data sources
Returns JSON output for Rust integration

NOTE: This file has been cleaned to contain only VALID akshare functions.
Many placeholder functions were removed as they don't exist in akshare 1.18.20.
"""

import sys
import json
import time
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

            except ValueError as e:
                error_msg = str(e)
                if "Length mismatch" in error_msg or "Expected axis" in error_msg:
                    return {
                        "success": False,
                        "error": "AKShare API structure changed (column mismatch). Endpoint temporarily unavailable.",
                        "data": [],
                        "error_type": "api_mismatch",
                        "timestamp": int(datetime.now().timestamp())
                    }
                last_error = error_msg
                if attempt < max_retries - 1:
                    time.sleep(self.retry_delay)
                continue
            except AttributeError as e:
                error_msg = str(e)
                if "NoneType" in error_msg and "find_all" in error_msg:
                    return {
                        "success": False,
                        "error": "Data source returned empty response. Website may be unavailable.",
                        "data": [],
                        "error_type": "empty_response",
                        "timestamp": int(datetime.now().timestamp())
                    }
                last_error = error_msg
                if attempt < max_retries - 1:
                    time.sleep(self.retry_delay)
                continue
            except KeyError as e:
                return {
                    "success": False,
                    "error": f"Missing data field: {str(e)}. API format may have changed.",
                    "data": [],
                    "error_type": "missing_field",
                    "timestamp": int(datetime.now().timestamp())
                }
            except (ConnectionError, TimeoutError) as e:
                last_error = str(e)
                if attempt < max_retries - 1:
                    time.sleep(self.retry_delay * 2)
                continue
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

    # ==================== UTILITY FUNCTIONS ====================

    def get_all_available_endpoints(self) -> Dict[str, Any]:
        """Get list of all available endpoints"""
        endpoints = [
            "energy_carbon", "energy_oil_hist", "energy_oil_detail",
            "movie_boxoffice_daily", "movie_boxoffice_weekly", "movie_boxoffice_monthly",
            "movie_boxoffice_yearly", "movie_boxoffice_realtime",
            "car_market_total", "car_market_segment", "car_sale_rank"
        ]

        return {
            "success": True,
            "endpoints": endpoints,
            "count": len(endpoints),
            "timestamp": int(datetime.now().timestamp())
        }


# Export wrapper instance
alternative_wrapper = AlternativeDataWrapper()

# ==================== CLI ====================
if __name__ == "__main__":
    import sys
    import json

    # Get wrapper instance
    wrapper = AlternativeDataWrapper()

    if len(sys.argv) < 2:
        print(json.dumps({"error": "Usage: python akshare_alternative.py <endpoint> [args...]"}))
        sys.exit(1)

    endpoint = sys.argv[1]
    args = sys.argv[2:] if len(sys.argv) > 2 else []

    # Handle get_all_endpoints
    if endpoint == "get_all_endpoints":
        if hasattr(wrapper, 'get_all_available_endpoints'):
            result = wrapper.get_all_available_endpoints()
        elif hasattr(wrapper, 'get_all_endpoints'):
            result = wrapper.get_all_endpoints()
        else:
            result = {"success": False, "error": "Endpoint list not available"}
        print(json.dumps(result, ensure_ascii=True))
        sys.exit(0)

    # Dynamic method resolution
    method_name = f"get_{endpoint}" if not endpoint.startswith("get_") else endpoint

    if hasattr(wrapper, method_name):
        method = getattr(wrapper, method_name)
        try:
            result = method(*args) if args else method()
            print(json.dumps(result, ensure_ascii=True, cls=DateTimeEncoder))
        except Exception as e:
            print(json.dumps({"success": False, "error": str(e), "endpoint": endpoint}))
    else:
        print(json.dumps({"success": False, "error": f"Unknown endpoint: {endpoint}. Method '{method_name}' not found."}))

