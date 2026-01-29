"""
AKShare China Economics Data Wrapper
Wrapper for Chinese economic indicators and macro data
Returns JSON output for Rust integration

NOTE: Cleaned to contain only VALID akshare macro_china functions (85 total).
"""

import sys
import json
import pandas as pd
import akshare as ak
from typing import Dict, Any, List
from datetime import datetime, timedelta, date
import time


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


class ChinaEconomicsWrapper:
    """China economics wrapper with VALIDATED akshare functions (85 total)"""

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

    # ==================== CORE INDICATORS ====================

    def get_gdp(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_gdp)

    def get_gdp_yearly(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_gdp_yearly)

    def get_cpi(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_cpi)

    def get_cpi_monthly(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_cpi_monthly)

    def get_cpi_yearly(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_cpi_yearly)

    def get_ppi(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_ppi)

    def get_ppi_yearly(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_ppi_yearly)

    def get_pmi(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_pmi)

    def get_pmi_yearly(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_pmi_yearly)

    def get_non_man_pmi(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_non_man_pmi)

    # ==================== MONETARY & FINANCIAL ====================

    def get_money_supply(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_money_supply)

    def get_supply_of_money(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_supply_of_money)

    def get_m2_yearly(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_m2_yearly)

    def get_shibor_all(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_shibor_all)

    def get_lpr(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_lpr)

    def get_swap_rate(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_swap_rate)

    def get_reserve_requirement_ratio(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_reserve_requirement_ratio)

    def get_new_financial_credit(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_new_financial_credit)

    def get_bank_financing(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_bank_financing)

    def get_central_bank_balance(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_central_bank_balance)

    # ==================== TRADE & FX ====================

    def get_trade_balance(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_trade_balance)

    def get_exports_yoy(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_exports_yoy)

    def get_imports_yoy(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_imports_yoy)

    def get_fdi(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_fdi)

    def get_fx_reserves_yearly(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_fx_reserves_yearly)

    def get_fx_gold(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_fx_gold)

    def get_foreign_exchange_gold(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_foreign_exchange_gold)

    def get_rmb(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_rmb)

    # ==================== INDUSTRY & PRODUCTION ====================

    def get_industrial_production_yoy(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_industrial_production_yoy)

    def get_construction_index(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_construction_index)

    def get_construction_price_index(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_construction_price_index)

    def get_enterprise_boom_index(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_enterprise_boom_index)

    def get_cx_pmi_yearly(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_cx_pmi_yearly)

    def get_cx_services_pmi_yearly(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_cx_services_pmi_yearly)

    # ==================== REAL ESTATE ====================

    def get_real_estate(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_real_estate)

    def get_new_house_price(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_new_house_price)

    # ==================== CONSUMPTION & RETAIL ====================

    def get_consumer_goods_retail(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_consumer_goods_retail)

    def get_retail_price_index(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_retail_price_index)

    # ==================== AGRICULTURE ====================

    def get_agricultural_index(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_agricultural_index)

    def get_agricultural_product(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_agricultural_product)

    def get_vegetable_basket(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_vegetable_basket)

    # ==================== ENERGY & COMMODITIES ====================

    def get_daily_energy(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_daily_energy)

    def get_energy_index(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_energy_index)

    def get_society_electricity(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_society_electricity)

    def get_commodity_price_index(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_commodity_price_index)

    def get_au_report(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_au_report)

    # ==================== EMPLOYMENT ====================

    def get_urban_unemployment(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_urban_unemployment)

    # ==================== FISCAL ====================

    def get_national_tax_receipts(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_national_tax_receipts)

    def get_czsr(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_czsr)

    def get_gdzctz(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_gdzctz)

    def get_hgjck(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_hgjck)

    def get_gyzjz(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_gyzjz)

    def get_qyspjg(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_qyspjg)

    def get_shrzgm(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_shrzgm)

    def get_wbck(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_wbck)

    def get_whxd(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_whxd)

    def get_xfzxx(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_xfzxx)

    # ==================== MARKETS ====================

    def get_stock_market_cap(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_stock_market_cap)

    def get_market_margin_sh(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_market_margin_sh)

    def get_market_margin_sz(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_market_margin_sz)

    def get_bond_public(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_bond_public)

    # ==================== INSURANCE & FINANCE ====================

    def get_insurance(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_insurance)

    def get_insurance_income(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_insurance_income)

    # ==================== TRANSPORTATION & LOGISTICS ====================

    def get_freight_index(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_freight_index)

    def get_bdti_index(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_bdti_index)

    def get_bsi_index(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_bsi_index)

    def get_lpi_index(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_lpi_index)

    def get_society_traffic_volume(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_society_traffic_volume)

    def get_passenger_load_factor(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_passenger_load_factor)

    # ==================== TELECOM & SERVICES ====================

    def get_postal_telecommunicational(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_postal_telecommunicational)

    def get_mobile_number(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_mobile_number)

    def get_international_tourism_fx(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_international_tourism_fx)

    def get_yw_electronic_index(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_yw_electronic_index)

    # ==================== NBS DATA ====================

    def get_nbs_nation(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_nbs_nation)

    def get_nbs_region(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_nbs_region)

    # ==================== HONG KONG ====================

    def get_hk_cpi(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_hk_cpi)

    def get_hk_cpi_ratio(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_hk_cpi_ratio)

    def get_hk_ppi(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_hk_ppi)

    def get_hk_gbp(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_hk_gbp)

    def get_hk_gbp_ratio(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_hk_gbp_ratio)

    def get_hk_rate_of_unemployment(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_hk_rate_of_unemployment)

    def get_hk_trade_diff_ratio(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_hk_trade_diff_ratio)

    def get_hk_building_amount(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_hk_building_amount)

    def get_hk_building_volume(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_hk_building_volume)

    def get_hk_market_info(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_hk_market_info)

    # ==================== UTILITY ====================

    def get_all_available_endpoints(self) -> Dict[str, Any]:
        """Get list of all available endpoints"""
        endpoints = [
            "gdp", "gdp_yearly", "cpi", "cpi_monthly", "cpi_yearly", "ppi", "ppi_yearly",
            "pmi", "pmi_yearly", "non_man_pmi", "money_supply", "supply_of_money", "m2_yearly",
            "shibor_all", "lpr", "swap_rate", "reserve_requirement_ratio", "new_financial_credit",
            "bank_financing", "central_bank_balance", "trade_balance", "exports_yoy", "imports_yoy",
            "fdi", "fx_reserves_yearly", "fx_gold", "foreign_exchange_gold", "rmb",
            "industrial_production_yoy", "construction_index", "construction_price_index",
            "enterprise_boom_index", "cx_pmi_yearly", "cx_services_pmi_yearly",
            "real_estate", "new_house_price", "consumer_goods_retail", "retail_price_index",
            "agricultural_index", "agricultural_product", "vegetable_basket",
            "daily_energy", "energy_index", "society_electricity", "commodity_price_index", "au_report",
            "urban_unemployment", "national_tax_receipts", "czsr", "gdzctz", "hgjck",
            "gyzjz", "qyspjg", "shrzgm", "wbck", "whxd", "xfzxx",
            "stock_market_cap", "market_margin_sh", "market_margin_sz", "bond_public",
            "insurance", "insurance_income", "freight_index", "bdti_index", "bsi_index",
            "lpi_index", "society_traffic_volume", "passenger_load_factor",
            "postal_telecommunicational", "mobile_number", "international_tourism_fx", "yw_electronic_index",
            "nbs_nation", "nbs_region",
            "hk_cpi", "hk_cpi_ratio", "hk_ppi", "hk_gbp", "hk_gbp_ratio",
            "hk_rate_of_unemployment", "hk_trade_diff_ratio", "hk_building_amount",
            "hk_building_volume", "hk_market_info"
        ]

        return {
            "success": True,
            "data": {
                "available_endpoints": endpoints,
                "total_count": len(endpoints),
                "categories": {
                    "Core Indicators": ["gdp", "cpi", "ppi", "pmi", "gdp_yearly", "cpi_yearly", "ppi_yearly"],
                    "Monetary & Financial": ["money_supply", "m2_yearly", "shibor_all", "lpr", "swap_rate"],
                    "Trade & FX": ["trade_balance", "exports_yoy", "imports_yoy", "fdi", "fx_reserves_yearly"],
                    "Industry & Production": ["industrial_production_yoy", "construction_index", "enterprise_boom_index"],
                    "Real Estate": ["real_estate", "new_house_price"],
                    "Consumption": ["consumer_goods_retail", "retail_price_index"],
                    "Agriculture": ["agricultural_index", "agricultural_product", "vegetable_basket"],
                    "Energy": ["daily_energy", "energy_index", "society_electricity"],
                    "Employment": ["urban_unemployment"],
                    "Markets": ["stock_market_cap", "market_margin_sh", "market_margin_sz"],
                    "Hong Kong": ["hk_cpi", "hk_gdp", "hk_ppi", "hk_market_info"],
                },
                "timestamp": int(datetime.now().timestamp())
            },
            "count": len(endpoints),
            "timestamp": int(datetime.now().timestamp())
        }


# ==================== CLI ====================
def main():
    wrapper = ChinaEconomicsWrapper()

    if len(sys.argv) < 2:
        print(json.dumps({"error": "Usage: python akshare_economics_china.py <endpoint>"}))
        return

    endpoint = sys.argv[1]

    endpoint_map = {
        "get_all_endpoints": wrapper.get_all_available_endpoints,
        "gdp": wrapper.get_gdp,
        "cpi": wrapper.get_cpi,
        "pmi": wrapper.get_pmi,
        "money_supply": wrapper.get_money_supply,
        "trade_balance": wrapper.get_trade_balance,
    }

    method = endpoint_map.get(endpoint)
    if method:
        result = method()
        print(json.dumps(result, ensure_ascii=True, cls=DateTimeEncoder))
    else:
        print(json.dumps({"error": f"Unknown endpoint: {endpoint}"}))

if __name__ == "__main__":
    main()
