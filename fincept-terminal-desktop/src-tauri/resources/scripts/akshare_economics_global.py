"""
AKShare Global Economics Data Wrapper
Wrapper for international economic indicators and macro data
Returns JSON output for Rust integration

NOTE: Cleaned to contain only VALID akshare macro functions (35 total).
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


class GlobalEconomicsWrapper:
    """Global economic data wrapper with VALIDATED akshare functions (35 total)"""

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

    # ==================== USA ECONOMIC INDICATORS ====================

    def get_macro_usa_cpi_monthly(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_usa_cpi_monthly)

    def get_macro_usa_core_cpi_monthly(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_usa_core_cpi_monthly)

    def get_macro_usa_ppi(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_usa_ppi)

    def get_macro_usa_ism_pmi(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_usa_ism_pmi)

    def get_macro_usa_unemployment_rate(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_usa_unemployment_rate)

    def get_macro_usa_non_farm(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_usa_non_farm)

    def get_macro_usa_adp_employment(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_usa_adp_employment)

    def get_macro_usa_retail_sales(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_usa_retail_sales)

    def get_macro_usa_industrial_production(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_usa_industrial_production)

    def get_macro_usa_durable_goods_orders(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_usa_durable_goods_orders)

    def get_macro_usa_house_starts(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_usa_house_starts)

    def get_macro_usa_building_permits(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_usa_building_permits)

    def get_macro_usa_gdp_monthly(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_usa_gdp_monthly)

    def get_macro_usa_trade_balance(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_usa_trade_balance)

    def get_macro_usa_current_account(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_usa_current_account)

    def get_macro_bank_usa_interest_rate(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_bank_usa_interest_rate)

    # ==================== EUROZONE ECONOMIC INDICATORS ====================

    def get_macro_euro_gdp_yoy(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_euro_gdp_yoy)

    def get_macro_euro_cpi_yoy(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_euro_cpi_yoy)

    def get_macro_euro_manufacturing_pmi(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_euro_manufacturing_pmi)

    def get_macro_euro_services_pmi(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_euro_services_pmi)

    def get_macro_euro_unemployment_rate_mom(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_euro_unemployment_rate_mom)

    def get_macro_euro_trade_balance(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_euro_trade_balance)

    def get_macro_bank_euro_interest_rate(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_bank_euro_interest_rate)

    # ==================== UK ECONOMIC INDICATORS ====================

    def get_macro_uk_gdp_yearly(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_uk_gdp_yearly)

    def get_macro_uk_cpi_yearly(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_uk_cpi_yearly)

    def get_macro_uk_unemployment_rate(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_uk_unemployment_rate)

    def get_macro_uk_trade(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_uk_trade)

    def get_macro_bank_english_interest_rate(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_bank_english_interest_rate)

    # ==================== JAPAN ECONOMIC INDICATORS ====================

    def get_macro_japan_cpi_yearly(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_japan_cpi_yearly)

    def get_macro_japan_unemployment_rate(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_japan_unemployment_rate)

    def get_macro_bank_japan_interest_rate(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_bank_japan_interest_rate)

    # ==================== GERMANY ECONOMIC INDICATORS ====================

    def get_macro_germany_gdp(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_germany_gdp)

    def get_macro_germany_cpi_yearly(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_germany_cpi_yearly)

    def get_macro_germany_zew(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_germany_zew)

    # ==================== CANADA ECONOMIC INDICATORS ====================

    def get_macro_canada_gdp_monthly(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_canada_gdp_monthly)

    def get_macro_canada_cpi_monthly(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_canada_cpi_monthly)

    def get_macro_canada_unemployment_rate(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_canada_unemployment_rate)

    def get_macro_canada_trade(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_canada_trade)

    # ==================== AUSTRALIA ECONOMIC INDICATORS ====================

    def get_macro_australia_cpi_quarterly(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_australia_cpi_quarterly)

    def get_macro_australia_unemployment_rate(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_australia_unemployment_rate)

    def get_macro_australia_trade(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_australia_trade)

    def get_macro_bank_australia_interest_rate(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_bank_australia_interest_rate)

    # ==================== SWITZERLAND ECONOMIC INDICATORS ====================

    def get_macro_swiss_cpi_yearly(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_swiss_cpi_yearly)

    def get_macro_swiss_gdp_quarterly(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_swiss_gdp_quarterly)

    def get_macro_bank_switzerland_interest_rate(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_bank_switzerland_interest_rate)

    # ==================== UTILITY ====================

    def get_all_available_endpoints(self) -> Dict[str, Any]:
        """Get list of all available endpoints"""
        endpoints = [
            "macro_usa_cpi_monthly", "macro_usa_core_cpi_monthly", "macro_usa_ppi",
            "macro_usa_ism_pmi", "macro_usa_unemployment_rate", "macro_usa_non_farm",
            "macro_usa_adp_employment", "macro_usa_retail_sales", "macro_usa_industrial_production",
            "macro_usa_durable_goods_orders", "macro_usa_house_starts", "macro_usa_building_permits",
            "macro_usa_gdp_monthly", "macro_usa_trade_balance", "macro_usa_current_account",
            "macro_bank_usa_interest_rate", "macro_euro_gdp_yoy", "macro_euro_cpi_yoy",
            "macro_euro_manufacturing_pmi", "macro_euro_services_pmi", "macro_euro_unemployment_rate_mom",
            "macro_euro_trade_balance", "macro_bank_euro_interest_rate", "macro_uk_gdp_yearly",
            "macro_uk_cpi_yearly", "macro_uk_unemployment_rate", "macro_uk_trade",
            "macro_bank_english_interest_rate", "macro_japan_cpi_yearly", "macro_japan_unemployment_rate",
            "macro_bank_japan_interest_rate", "macro_germany_gdp", "macro_germany_cpi_yearly",
            "macro_germany_zew", "macro_canada_gdp_monthly", "macro_canada_cpi_monthly",
            "macro_canada_unemployment_rate", "macro_canada_trade", "macro_australia_cpi_quarterly",
            "macro_australia_unemployment_rate", "macro_australia_trade", "macro_bank_australia_interest_rate",
            "macro_swiss_cpi_yearly", "macro_swiss_gdp_quarterly", "macro_bank_switzerland_interest_rate"
        ]

        return {
            "success": True,
            "endpoints": endpoints,
            "count": len(endpoints),
            "timestamp": int(datetime.now().timestamp())
        }


# Export wrapper instance
global_economics_wrapper = GlobalEconomicsWrapper()
