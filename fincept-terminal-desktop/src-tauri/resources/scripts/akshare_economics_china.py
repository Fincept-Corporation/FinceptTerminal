"""
AKShare China Economics Data Wrapper
Comprehensive wrapper for Chinese economic indicators and macro data
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


class ChinaEconomicsWrapper:
    """Comprehensive Chinese economic data wrapper with 80+ endpoints"""

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

    # ==================== GDP AND NATIONAL ACCOUNTS ====================

    def get_macro_china_gdp(self) -> Dict[str, Any]:
        """Get China GDP data"""
        return self._safe_call_with_retry(ak.macro_china_gdp)

    def get_macro_china_gdp_monthly(self) -> Dict[str, Any]:
        """Get China monthly GDP data"""
        return self._safe_call_with_retry(ak.macro_china_gdp_monthly)

    def get_macro_china_gdp_area(self) -> Dict[str, Any]:
        """Get China GDP by region"""
        return self._safe_call_with_retry(ak.macro_china_gdp_area)

    def get_macro_china_gdp_industry(self) -> Dict[str, Any]:
        """Get China GDP by industry"""
        return self._safe_call_with_retry(ak.macro_china_gdp_industry)

    def get_macro_china_gdp_demand(self) -> Dict[str, Any]:
        """Get China GDP by demand structure"""
        return self._safe_call_with_retry(ak.macro_china_gdp_demand)

    # ==================== INFLATION INDICATORS ====================

    def get_macro_china_cpi(self) -> Dict[str, Any]:
        """Get China Consumer Price Index"""
        return self._safe_call_with_retry(ak.macro_china_cpi)

    def get_macro_china_cpi_monthly(self) -> Dict[str, Any]:
        """Get China monthly CPI data"""
        return self._safe_call_with_retry(ak.macro_china_cpi_monthly)

    def get_macro_china_cpi_area(self) -> Dict[str, Any]:
        """Get China CPI by region"""
        return self._safe_call_with_retry(ak.macro_china_cpi_area)

    def get_macro_china_cpi_city(self) -> Dict[str, Any]:
        """Get China CPI by city/rural"""
        return self._safe_call_with_retry(ak.macro_china_cpi_city)

    def get_macro_china_cpi_category(self) -> Dict[str, Any]:
        """Get China CPI by category"""
        return self._safe_call_with_retry(ak.macro_china_cpi_category)

    def get_macro_china_ppi(self) -> Dict[str, Any]:
        """Get China Producer Price Index"""
        return self._safe_call_with_retry(ak.macro_china_ppi)

    def get_macro_china_ppi_monthly(self) -> Dict[str, Any]:
        """Get China monthly PPI data"""
        return self._safe_call_with_retry(ak.macro_china_ppi_monthly)

    def get_macro_china_ppi_industry(self) -> Dict[str, Any]:
        """Get China PPI by industry"""
        return self._safe_call_with_retry(ak.macro_china_ppi_industry)

    # ==================== PMI AND MANUFACTURING ====================

    def get_macro_china_pmi(self) -> Dict[str, Any]:
        """Get China Purchasing Managers Index"""
        return self._safe_call_with_retry(ak.macro_china_pmi)

    def get_macro_china_pmi_manufacturing(self) -> Dict[str, Any]:
        """Get China manufacturing PMI"""
        return self._safe_call_with_retry(ak.macro_china_pmi_manufacturing)

    def get_macro_china_pmi_non_manufacturing(self) -> Dict[str, Any]:
        """Get China non-manufacturing PMI"""
        return self._safe_call_with_retry(ak.macro_china_pmi_non_manufacturing)

    def get_macro_china_pmi_caixin(self) -> Dict[str, Any]:
        """Get China Caixin PMI"""
        return self._safe_call_with_retry(ak.macro_china_pmi_caixin)

    # ==================== MONEY SUPPLY AND FINANCIAL ====================

    def get_macro_china_m2(self) -> Dict[str, Any]:
        """Get China M2 money supply"""
        return self._safe_call_with_retry(ak.macro_china_m2)

    def get_macro_china_m1(self) -> Dict[str, Any]:
        """Get China M1 money supply"""
        return self._safe_call_with_retry(ak.macro_china_m1)

    def get_macro_china_m0(self) -> Dict[str, Any]:
        """Get China M0 money supply"""
        return self._safe_call_with_retry(ak.macro_china_m0)

    def get_macro_china_money_supply(self) -> Dict[str, Any]:
        """Get China comprehensive money supply data"""
        return self._safe_call_with_retry(ak.macro_china_money_supply)

    def get_macro_china_shibor(self) -> Dict[str, Any]:
        """Get China SHIBOR rates"""
        return self._safe_call_with_retry(ak.macro_china_shibor)

    def get_macro_china_shibor_all(self) -> Dict[str, Any]:
        """Get China SHIBOR all tenors"""
        return self._safe_call_with_retry(ak.macro_china_shibor_all)

    def get_macro_china_lpr(self) -> Dict[str, Any]:
        """Get China Loan Prime Rate"""
        return self._safe_call_with_retry(ak.macro_china_lpr)

    def get_macro_china_rrr(self) -> Dict[str, Any]:
        """Get China Reserve Requirement Ratio"""
        return self._safe_call_with_retry(ak.macro_china_rrr)

    # ==================== FOREIGN EXCHANGE AND RESERVES ====================

    def get_macro_china_fx_reserves(self) -> Dict[str, Any]:
        """Get China foreign exchange reserves"""
        return self._safe_call_with_retry(ak.macro_china_fx_reserves)

    def get_macro_china_gold_reserves(self) -> Dict[str, Any]:
        """Get China gold reserves"""
        return self._safe_call_with_retry(ak.macro_china_gold_reserves)

    def get_macro_china_bop(self) -> Dict[str, Any]:
        """Get China balance of payments"""
        return self._safe_call_with_retry(ak.macro_china_bop)

    # ==================== TRADE AND EXTERNAL SECTOR ====================

    def get_macro_china_trade(self) -> Dict[str, Any]:
        """Get China trade data"""
        return self._safe_call_with_retry(ak.macro_china_trade)

    def get_macro_china_trade_balance(self) -> Dict[str, Any]:
        """Get China trade balance"""
        return self._safe_call_with_retry(ak.macro_china_trade_balance)

    def get_macro_china_export(self) -> Dict[str, Any]:
        """Get China export data"""
        return self._safe_call_with_retry(ak.macro_china_export)

    def get_macro_china_import(self) -> Dict[str, Any]:
        """Get China import data"""
        return self._safe_call_with_retry(ak.macro_china_import)

    def get_macro_china_trade_partner(self) -> Dict[str, Any]:
        """Get China trade by partner"""
        return self._safe_call_with_retry(ak.macro_china_trade_partner)

    def get_macro_china_trade_commodity(self) -> Dict[str, Any]:
        """Get China trade by commodity"""
        return self._safe_call_with_retry(ak.macro_china_trade_commodity)

    # ==================== INDUSTRIAL PRODUCTION ====================

    def get_macro_china_industrial_production(self) -> Dict[str, Any]:
        """Get China industrial production"""
        return self._safe_call_with_retry(ak.macro_china_industrial_production)

    def get_macro_china_industrial_production_monthly(self) -> Dict[str, Any]:
        """Get China monthly industrial production"""
        return self._safe_call_with_retry(ak.macro_china_industrial_production_monthly)

    def get_macro_china_industrial_value_added(self) -> Dict[str, Any]:
        """Get China industrial value added"""
        return self._safe_call_with_retry(ak.macro_china_industrial_value_added)

    def get_macro_china_manufacturing_pmi(self) -> Dict[str, Any]:
        """Get China manufacturing PMI (alternative)"""
        return self._safe_call_with_retry(ak.macro_china_manufacturing_pmi)

    # ==================== INVESTMENT AND CONSTRUCTION ====================

    def get_macro_china_fixed_asset(self) -> Dict[str, Any]:
        """Get China fixed asset investment"""
        return self._safe_call_with_retry(ak.macro_china_fixed_asset)

    def get_macro_china_fixed_asset_monthly(self) -> Dict[str, Any]:
        """Get China monthly fixed asset investment"""
        return self._safe_call_with_retry(ak.macro_china_fixed_asset_monthly)

    def get_macro_china_real_estate(self) -> Dict[str, Any]:
        """Get China real estate investment"""
        return self._safe_call_with_retry(ak.macro_china_real_estate)

    def get_macro_china_construction(self) -> Dict[str, Any]:
        """Get China construction data"""
        return self._safe_call_with_retry(ak.macro_china_construction)

    # ==================== CONSUMER AND RETAIL ====================

    def get_macro_china_retail_sales(self) -> Dict[str, Any]:
        """Get China retail sales"""
        return self._safe_call_with_retry(ak.macro_china_retail_sales)

    def get_macro_china_retail_sales_monthly(self) -> Dict[str, Any]:
        """Get China monthly retail sales"""
        return self._safe_call_with_retry(ak.macro_china_retail_sales_monthly)

    def get_macro_china_consumer_confidence(self) -> Dict[str, Any]:
        """Get China consumer confidence index"""
        return self._safe_call_with_retry(ak.macro_china_consumer_confidence)

    def get_macro_china_urban_income(self) -> Dict[str, Any]:
        """Get China urban income data"""
        return self._safe_call_with_retry(ak.macro_china_urban_income)

    def get_macro_china_rural_income(self) -> Dict[str, Any]:
        """Get China rural income data"""
        return self._safe_call_with_retry(ak.macro_china_rural_income)

    def get_macro_china_employment(self) -> Dict[str, Any]:
        """Get China employment data"""
        return self._safe_call_with_retry(ak.macro_china_employment)

    def get_macro_china_unemployment(self) -> Dict[str, Any]:
        """Get China unemployment rate"""
        return self._safe_call_with_retry(ak.macro_china_unemployment)

    # ==================== FISCAL AND GOVERNMENT FINANCE ====================

    def get_macro_china_fiscal_revenue(self) -> Dict[str, Any]:
        """Get China fiscal revenue"""
        return self._safe_call_with_retry(ak.macro_china_fiscal_revenue)

    def get_macro_china_fiscal_expenditure(self) -> Dict[str, Any]:
        """Get China fiscal expenditure"""
        return self._safe_call_with_retry(ak.macro_china_fiscal_expenditure)

    def get_macro_china_government_debt(self) -> Dict[str, Any]:
        """Get China government debt"""
        return self._safe_call_with_retry(ak.macro_china_government_debt)

    def get_macro_china_tax_revenue(self) -> Dict[str, Any]:
        """Get China tax revenue"""
        return self._safe_call_with_retry(ak.macro_china_tax_revenue)

    # ==================== BANKING AND FINANCIAL INDICATORS ====================

    def get_macro_china_bank_loan(self) -> Dict[str, Any]:
        """Get China bank loan data"""
        return self._safe_call_with_retry(ak.macro_china_bank_loan)

    def get_macro_china_bank_deposit(self) -> Dict[str, Any]:
        """Get China bank deposit data"""
        return self._safe_call_with_retry(ak.macro_china_bank_deposit)

    def get_macro_china_social_financing(self) -> Dict[str, Any]:
        """Get China social financing data"""
        return self._safe_call_with_retry(ak.macro_china_social_financing)

    def get_macro_china_credit_growth(self) -> Dict[str, Any]:
        """Get China credit growth data"""
        return self._safe_call_with_retry(ak.macro_china_credit_growth)

    # ==================== COMMODITIES AND ENERGY ====================

    def get_macro_china_electricity(self) -> Dict[str, Any]:
        """Get China electricity production/consumption"""
        return self._safe_call_with_retry(ak.macro_china_electricity)

    def get_macro_china_coal(self) -> Dict[str, Any]:
        """Get China coal production/data"""
        return self._safe_call_with_retry(ak.macro_china_coal)

    def get_macro_china_oil(self) -> Dict[str, Any]:
        """Get China oil data"""
        return self._safe_call_with_retry(ak.macro_china_oil)

    def get_macro_china_steel(self) -> Dict[str, Any]:
        """Get China steel production/data"""
        return self._safe_call_with_retry(ak.macro_china_steel)

    def get_macro_china_cement(self) -> Dict[str, Any]:
        """Get China cement production"""
        return self._safe_call_with_retry(ak.macro_china_cement)

    # ==================== AGRICULTURE AND RURAL DATA ====================

    def get_macro_china_agriculture(self) -> Dict[str, Any]:
        """Get China agricultural data"""
        return self._safe_call_with_retry(ak.macro_china_agriculture)

    def get_macro_china_grain(self) -> Dict[str, Any]:
        """Get China grain production"""
        return self._safe_call_with_retry(ak.macro_china_grain)

    def get_macro_china_livestock(self) -> Dict[str, Any]:
        """Get China livestock data"""
        return self._safe_call_with_retry(ak.macro_china_livestock)

    # ==================== TRANSPORTATION AND LOGISTICS ====================

    def get_macro_china_freight(self) -> Dict[str, Any]:
        """Get China freight transport data"""
        return self._safe_call_with_retry(ak.macro_china_freight)

    def get_macro_china_railway(self) -> Dict[str, Any]:
        """Get China railway data"""
        return self._safe_call_with_retry(ak.macro_china_railway)

    def get_macro_china_shipping(self) -> Dict[str, Any]:
        """Get China shipping data"""
        return self._safe_call_with_retry(ak.macro_china_shipping)

    # ==================== REAL ESTATE MARKET ====================

    def get_macro_china_house_price(self) -> Dict[str, Any]:
        """Get China house price index"""
        return self._safe_call_with_retry(ak.macro_china_house_price)

    def get_macro_china_house_price_city(self) -> Dict[str, Any]:
        """Get China house price by city"""
        return self._safe_call_with_retry(ak.macro_china_house_price_city)

    def get_macro_china_land_price(self) -> Dict[str, Any]:
        """Get China land price data"""
        return self._safe_call_with_retry(ak.macro_china_land_price)

    # ==================== STOCK MARKET INDICATORS ====================

    def get_macro_china_market_cap(self) -> Dict[str, Any]:
        """Get China stock market capitalization"""
        return self._safe_call_with_retry(ak.macro_china_market_cap)

    def get_macro_china_market_value(self) -> Dict[str, Any]:
        """Get China market value data"""
        return self._safe_call_with_retry(ak.macro_china_market_value)

    # ==================== ECONOMIC LEADING INDICATORS ====================

    def get_macro_china_leading_index(self) -> Dict[str, Any]:
        """Get China economic leading indicators"""
        return self._safe_call_with_retry(ak.macro_china_leading_index)

    def get_macro_china_coincident_index(self) -> Dict[str, Any]:
        """Get China economic coincident indicators"""
        return self._safe_call_with_retry(ak.macro_china_coincident_index)

    def get_macro_china_lagging_index(self) -> Dict[str, Any]:
        """Get China economic lagging indicators"""
        return self._safe_call_with_retry(ak.macro_china_lagging_index)

    # ==================== BUSINESS SURVEYS ====================

    def get_macro_china_business_climate(self) -> Dict[str, Any]:
        """Get China business climate index"""
        return self._safe_call_with_retry(ak.macro_china_business_climate)

    def get_macro_china_entrepreneur_confidence(self) -> Dict[str, Any]:
        """Get China entrepreneur confidence index"""
        return self._safe_call_with_retry(ak.macro_china_entrepreneur_confidence)

    def get_macro_china_enterprise_profits(self) -> Dict[str, Any]:
        """Get China enterprise profits"""
        return self._safe_call_with_retry(ak.macro_china_enterprise_profits)

    # ==================== UTILITIES ====================

    def get_all_available_endpoints(self) -> Dict[str, Any]:
        """Get list of all available endpoints in this wrapper"""
        methods = [method for method in dir(self) if method.startswith('get_') and callable(getattr(self, method))]
        return {
            "available_endpoints": methods,
            "total_count": len(methods),
            "categories": {
                "GDP & National Accounts": [m for m in methods if "gdp" in m],
                "Inflation": [m for m in methods if any(x in m for x in ["cpi", "ppi"])],
                "PMI & Manufacturing": [m for m in methods if "pmi" in m or "manufacturing" in m],
                "Money Supply": [m for m in methods if any(x in m for x in ["m0", "m1", "m2", "money", "shibor", "lpr", "rrr"])],
                "FX & Reserves": [m for m in methods if any(x in m for x in ["fx", "reserves", "gold", "bop"])],
                "Trade": [m for m in methods if "trade" in m or any(x in m for x in ["export", "import"])],
                "Industrial": [m for m in methods if any(x in m for x in ["industrial", "production"])],
                "Investment": [m for m in methods if any(x in m for x in ["fixed", "asset", "real_estate", "construction"])],
                "Consumer": [m for m in methods if any(x in m for x in ["retail", "consumer", "income", "employment"])],
                "Fiscal": [m for m in methods if any(x in m for x in ["fiscal", "government", "tax"])],
                "Banking": [m for m in methods if any(x in m for x in ["bank", "loan", "deposit", "financing", "credit"])],
                "Commodities": [m for m in methods if any(x in m for x in ["electricity", "coal", "oil", "steel", "cement"])],
                "Agriculture": [m for m in methods if any(x in m for x in ["agriculture", "grain", "livestock"])],
                "Transportation": [m for m in methods if any(x in m for x in ["freight", "railway", "shipping"])],
                "Real Estate": [m for m in methods if any(x in m for x in ["house", "land"])],
                "Stock Market": [m for m in methods if "market" in m],
                "Leading Indicators": [m for m in methods if any(x in m for x in ["leading", "coincident", "lagging"])],
                "Business Surveys": [m for m in methods if any(x in m for x in ["business", "entrepreneur", "enterprise"])]
            },
            "timestamp": int(datetime.now().timestamp())
        }

    

# ==================== COMMAND LINE INTERFACE ====================

def main():
    """Command line interface for the China Economics wrapper"""
    wrapper = ChinaEconomicsWrapper()

    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python akshare_economics_china.py <endpoint> [args...]",
            "available_endpoints": wrapper.get_all_available_endpoints()["available_endpoints"]
        }, indent=2))
        return

    endpoint = sys.argv[1]
    args = sys.argv[2:] if len(sys.argv) > 2 else []

    # Map endpoint names to method calls
    endpoint_map = {
                "get_all_endpoints": wrapper.get_all_available_endpoints,
        "gdp": wrapper.get_macro_china_gdp,
        "gdp_monthly": wrapper.get_macro_china_gdp_monthly,
        "gdp_area": wrapper.get_macro_china_gdp_area,
        "gdp_industry": wrapper.get_macro_china_gdp_industry,
        "gdp_demand": wrapper.get_macro_china_gdp_demand,
        "cpi": wrapper.get_macro_china_cpi,
        "cpi_monthly": wrapper.get_macro_china_cpi_monthly,
        "cpi_area": wrapper.get_macro_china_cpi_area,
        "cpi_city": wrapper.get_macro_china_cpi_city,
        "cpi_category": wrapper.get_macro_china_cpi_category,
        "ppi": wrapper.get_macro_china_ppi,
        "ppi_monthly": wrapper.get_macro_china_ppi_monthly,
        "ppi_industry": wrapper.get_macro_china_ppi_industry,
        "pmi": wrapper.get_macro_china_pmi,
        "pmi_manufacturing": wrapper.get_macro_china_pmi_manufacturing,
        "pmi_non_manufacturing": wrapper.get_macro_china_pmi_non_manufacturing,
        "pmi_caixin": wrapper.get_macro_china_pmi_caixin,
        "m2": wrapper.get_macro_china_m2,
        "m1": wrapper.get_macro_china_m1,
        "m0": wrapper.get_macro_china_m0,
        "money_supply": wrapper.get_macro_china_money_supply,
        "shibor": wrapper.get_macro_china_shibor,
        "shibor_all": wrapper.get_macro_china_shibor_all,
        "lpr": wrapper.get_macro_china_lpr,
        "rrr": wrapper.get_macro_china_rrr,
        "fx_reserves": wrapper.get_macro_china_fx_reserves,
        "gold_reserves": wrapper.get_macro_china_gold_reserves,
        "bop": wrapper.get_macro_china_bop,
        "trade": wrapper.get_macro_china_trade,
        "trade_balance": wrapper.get_macro_china_trade_balance,
        "export": wrapper.get_macro_china_export,
        "import": wrapper.get_macro_china_import,
        "trade_partner": wrapper.get_macro_china_trade_partner,
        "trade_commodity": wrapper.get_macro_china_trade_commodity,
        "industrial_production": wrapper.get_macro_china_industrial_production,
        "industrial_production_monthly": wrapper.get_macro_china_industrial_production_monthly,
        "industrial_value_added": wrapper.get_macro_china_industrial_value_added,
        "manufacturing_pmi": wrapper.get_macro_china_manufacturing_pmi,
        "fixed_asset": wrapper.get_macro_china_fixed_asset,
        "fixed_asset_monthly": wrapper.get_macro_china_fixed_asset_monthly,
        "real_estate": wrapper.get_macro_china_real_estate,
        "construction": wrapper.get_macro_china_construction,
        "retail_sales": wrapper.get_macro_china_retail_sales,
        "retail_sales_monthly": wrapper.get_macro_china_retail_sales_monthly,
        "consumer_confidence": wrapper.get_macro_china_consumer_confidence,
        "urban_income": wrapper.get_macro_china_urban_income,
        "rural_income": wrapper.get_macro_china_rural_income,
        "employment": wrapper.get_macro_china_employment,
        "unemployment": wrapper.get_macro_china_unemployment,
        "fiscal_revenue": wrapper.get_macro_china_fiscal_revenue,
        "fiscal_expenditure": wrapper.get_macro_china_fiscal_expenditure,
        "government_debt": wrapper.get_macro_china_government_debt,
        "tax_revenue": wrapper.get_macro_china_tax_revenue,
        "bank_loan": wrapper.get_macro_china_bank_loan,
        "bank_deposit": wrapper.get_macro_china_bank_deposit,
        "social_financing": wrapper.get_macro_china_social_financing,
        "credit_growth": wrapper.get_macro_china_credit_growth,
        "electricity": wrapper.get_macro_china_electricity,
        "coal": wrapper.get_macro_china_coal,
        "oil": wrapper.get_macro_china_oil,
        "steel": wrapper.get_macro_china_steel,
        "cement": wrapper.get_macro_china_cement,
        "agriculture": wrapper.get_macro_china_agriculture,
        "grain": wrapper.get_macro_china_grain,
        "livestock": wrapper.get_macro_china_livestock,
        "freight": wrapper.get_macro_china_freight,
        "railway": wrapper.get_macro_china_railway,
        "shipping": wrapper.get_macro_china_shipping,
        "house_price": wrapper.get_macro_china_house_price,
        "house_price_city": wrapper.get_macro_china_house_price_city,
        "land_price": wrapper.get_macro_china_land_price,
        "market_cap": wrapper.get_macro_china_market_cap,
        "market_value": wrapper.get_macro_china_market_value,
        "leading_index": wrapper.get_macro_china_leading_index,
        "coincident_index": wrapper.get_macro_china_coincident_index,
        "lagging_index": wrapper.get_macro_china_lagging_index,
        "business_climate": wrapper.get_macro_china_business_climate,
        "entrepreneur_confidence": wrapper.get_macro_china_entrepreneur_confidence,
        "enterprise_profits": wrapper.get_macro_china_enterprise_profits
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