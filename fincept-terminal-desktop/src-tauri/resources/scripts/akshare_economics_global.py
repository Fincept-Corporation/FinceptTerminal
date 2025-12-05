"""
AKShare Global Economics Data Wrapper
Comprehensive wrapper for international economic indicators and macro data
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


class GlobalEconomicsWrapper:
    """Comprehensive global economic data wrapper with 100+ endpoints"""

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

    # ==================== USA ECONOMIC INDICATORS ====================

    def get_macro_usa_gdp(self) -> Dict[str, Any]:
        """Get USA GDP data"""
        return self._safe_call_with_retry(ak.macro_usa_gdp)

    def get_macro_usa_gdp_nowcast(self) -> Dict[str, Any]:
        """Get USA GDP nowcast"""
        return self._safe_call_with_retry(ak.macro_usa_gdp_nowcast)

    def get_macro_usa_cpi(self) -> Dict[str, Any]:
        """Get USA Consumer Price Index"""
        return self._safe_call_with_retry(ak.macro_usa_cpi)

    def get_macro_usa_cpi_core(self) -> Dict[str, Any]:
        """Get USA Core CPI"""
        return self._safe_call_with_retry(ak.macro_usa_cpi_core)

    def get_macro_usa_ppi(self) -> Dict[str, Any]:
        """Get USA Producer Price Index"""
        return self._safe_call_with_retry(ak.macro_usa_ppi)

    def get_macro_usa_pmi(self) -> Dict[str, Any]:
        """Get USA PMI data"""
        return self._safe_call_with_retry(ak.macro_usa_pmi)

    def get_macro_usa_pmi_manufacturing(self) -> Dict[str, Any]:
        """Get USA Manufacturing PMI"""
        return self._safe_call_with_retry(ak.macro_usa_pmi_manufacturing)

    def get_macro_usa_pmi_services(self) -> Dict[str, Any]:
        """Get USA Services PMI"""
        return self._safe_call_with_retry(ak.macro_usa_pmi_services)

    def get_macro_usa_unemployment(self) -> Dict[str, Any]:
        """Get USA unemployment rate"""
        return self._safe_call_with_retry(ak.macro_usa_unemployment)

    def get_macro_usa_nonfarm_payrolls(self) -> Dict[str, Any]:
        """Get USA non-farm payrolls"""
        return self._safe_call_with_retry(ak.macro_usa_nonfarm_payrolls)

    def get_macro_usa_employment(self) -> Dict[str, Any]:
        """Get USA employment data"""
        return self._safe_call_with_retry(ak.macro_usa_employment)

    def get_macro_usa_labor_force(self) -> Dict[str, Any]:
        """Get USA labor force data"""
        return self._safe_call_with_retry(ak.macro_usa_labor_force)

    def get_macro_usa_job_openings(self) -> Dict[str, Any]:
        """Get USA job openings"""
        return self._safe_call_with_retry(ak.macro_usa_job_openings)

    def get_macro_usa_retail_sales(self) -> Dict[str, Any]:
        """Get USA retail sales"""
        return self._safe_call_with_retry(ak.macro_usa_retail_sales)

    def get_macro_usa_industrial_production(self) -> Dict[str, Any]:
        """Get USA industrial production"""
        return self._safe_call_with_retry(ak.macro_usa_industrial_production)

    def get_macro_usa_capacity_utilization(self) -> Dict[str, Any]:
        """Get USA capacity utilization"""
        return self._safe_call_with_retry(ak.macro_usa_capacity_utilization)

    def get_macro_usa_durable_goods(self) -> Dict[str, Any]:
        """Get USA durable goods orders"""
        return self._safe_call_with_retry(ak.macro_usa_durable_goods)

    def get_macro_usa_consumer_confidence(self) -> Dict[str, Any]:
        """Get USA consumer confidence"""
        return self._safe_call_with_retry(ak.macro_usa_consumer_confidence)

    def get_macro_usa_housing_starts(self) -> Dict[str, Any]:
        """Get USA housing starts"""
        return self._safe_call_with_retry(ak.macro_usa_housing_starts)

    def get_macro_usa_building_permits(self) -> Dict[str, Any]:
        """Get USA building permits"""
        return self._safe_call_with_retry(ak.macro_usa_building_permits)

    def get_macro_usa_existing_home_sales(self) -> Dict[str, Any]:
        """Get USA existing home sales"""
        return self._safe_call_with_retry(ak.macro_usa_existing_home_sales)

    def get_macro_usa_new_home_sales(self) -> Dict[str, Any]:
        """Get USA new home sales"""
        return self._safe_call_with_retry(ak.macro_usa_new_home_sales)

    def get_macro_usa_fed_balance_sheet(self) -> Dict[str, Any]:
        """Get USA Federal Reserve balance sheet"""
        return self._safe_call_with_retry(ak.macro_usa_fed_balance_sheet)

    def get_macro_usa_interest_rate(self) -> Dict[str, Any]:
        """Get USA interest rates"""
        return self._safe_call_with_retry(ak.macro_usa_interest_rate)

    def get_macro_usa_fed_funds_rate(self) -> Dict[str, Any]:
        """Get USA Fed funds rate"""
        return self._safe_call_with_retry(ak.macro_usa_fed_funds_rate)

    def get_macro_usa_treasury_yield(self) -> Dict[str, Any]:
        """Get USA Treasury yields"""
        return self._safe_call_with_retry(ak.macro_usa_treasury_yield)

    def get_macro_usa_money_supply(self) -> Dict[str, Any]:
        """Get USA money supply"""
        return self._safe_call_with_retry(ak.macro_usa_money_supply)

    def get_macro_usa_m2(self) -> Dict[str, Any]:
        """Get USA M2 money supply"""
        return self._safe_call_with_retry(ak.macro_usa_m2)

    def get_macro_usa_fiscal_deficit(self) -> Dict[str, Any]:
        """Get USA fiscal deficit"""
        return self._safe_call_with_retry(ak.macro_usa_fiscal_deficit)

    def get_macro_usa_government_debt(self) -> Dict[str, Any]:
        """Get USA government debt"""
        return self._safe_call_with_retry(ak.macro_usa_government_debt)

    def get_macro_usa_current_account(self) -> Dict[str, Any]:
        """Get USA current account"""
        return self._safe_call_with_retry(ak.macro_usa_current_account)

    def get_macro_usa_trade_balance(self) -> Dict[str, Any]:
        """Get USA trade balance"""
        return self._safe_call_with_retry(ak.macro_usa_trade_balance)

    def get_macro_usa_exports(self) -> Dict[str, Any]:
        """Get USA exports"""
        return self._safe_call_with_retry(ak.macro_usa_exports)

    def get_macro_usa_imports(self) -> Dict[str, Any]:
        """Get USA imports"""
        return self._safe_call_with_retry(ak.macro_usa_imports)

    def get_macro_usa_leading_index(self) -> Dict[str, Any]:
        """Get USA leading economic index"""
        return self._safe_call_with_retry(ak.macro_usa_leading_index)

    def get_macro_usa_ism_manufacturing(self) -> Dict[str, Any]:
        """Get USA ISM manufacturing"""
        return self._safe_call_with_retry(ak.macro_usa_ism_manufacturing)

    def get_macro_usa_ism_services(self) -> Dict[str, Any]:
        """Get USA ISM services"""
        return self._safe_call_with_retry(ak.macro_usa_ism_services)

    def get_macro_usa_adp_employment(self) -> Dict[str, Any]:
        """Get USA ADP employment"""
        return self._safe_call_with_retry(ak.macro_usa_adp_employment)

    def get_macro_usa_initial_claims(self) -> Dict[str, Any]:
        """Get USA initial jobless claims"""
        return self._safe_call_with_retry(ak.macro_usa_initial_claims)

    def get_macro_usa_wholesale_inventory(self) -> Dict[str, Any]:
        """Get USA wholesale inventory"""
        return self._safe_call_with_retry(ak.macro_usa_wholesale_inventory)

    def get_macro_usa_personal_income(self) -> Dict[str, Any]:
        """Get USA personal income"""
        return self._safe_call_with_retry(ak.macro_usa_personal_income)

    def get_macro_usa_personal_spending(self) -> Dict[str, Any]:
        """Get USA personal spending"""
        return self._safe_call_with_retry(ak.macro_usa_personal_spending)

    def get_macro_usa_fred_data(self, series_id: str) -> Dict[str, Any]:
        """Get USA FRED data by series ID"""
        return self._safe_call_with_retry(ak.macro_usa_fred_data, series_id=series_id)

    # ==================== EUROZONE ECONOMIC INDICATORS ====================

    def get_macro_euro_gdp(self) -> Dict[str, Any]:
        """Get Eurozone GDP data"""
        return self._safe_call_with_retry(ak.macro_euro_gdp)

    def get_macro_euro_cpi(self) -> Dict[str, Any]:
        """Get Eurozone CPI data"""
        return self._safe_call_with_retry(ak.macro_euro_cpi)

    def get_macro_euro_core_cpi(self) -> Dict[str, Any]:
        """Get Eurozone core CPI"""
        return self._safe_call_with_retry(ak.macro_euro_core_cpi)

    def get_macro_euro_pmi(self) -> Dict[str, Any]:
        """Get Eurozone PMI data"""
        return self._safe_call_with_retry(ak.macro_euro_pmi)

    def get_macro_euro_manufacturing_pmi(self) -> Dict[str, Any]:
        """Get Eurozone manufacturing PMI"""
        return self._safe_call_with_retry(ak.macro_euro_manufacturing_pmi)

    def get_macro_euro_services_pmi(self) -> Dict[str, Any]:
        """Get Eurozone services PMI"""
        return self._safe_call_with_retry(ak.macro_euro_services_pmi)

    def get_macro_euro_unemployment(self) -> Dict[str, Any]:
        """Get Eurozone unemployment rate"""
        return self._safe_call_with_retry(ak.macro_euro_unemployment)

    def get_macro_euro_interest_rate(self) -> Dict[str, Any]:
        """Get Eurozone interest rates"""
        return self._safe_call_with_retry(ak.macro_euro_interest_rate)

    def get_macro_euro_ecb_balance_sheet(self) -> Dict[str, Any]:
        """Get ECB balance sheet"""
        return self._safe_call_with_retry(ak.macro_euro_ecb_balance_sheet)

    def get_macro_euro_retail_sales(self) -> Dict[str, Any]:
        """Get Eurozone retail sales"""
        return self._safe_call_with_retry(ak.macro_euro_retail_sales)

    def get_macro_euro_industrial_production(self) -> Dict[str, Any]:
        """Get Eurozone industrial production"""
        return self._safe_call_with_retry(ak.macro_euro_industrial_production)

    def get_macro_euro_trade_balance(self) -> Dict[str, Any]:
        """Get Eurozone trade balance"""
        return self._safe_call_with_retry(ak.macro_euro_trade_balance)

    def get_macro_euro_current_account(self) -> Dict[str, Any]:
        """Get Eurozone current account"""
        return self._safe_call_with_retry(ak.macro_euro_current_account)

    def get_macro_euro_consumer_confidence(self) -> Dict[str, Any]:
        """Get Eurozone consumer confidence"""
        return self._safe_call_with_retry(ak.macro_euro_consumer_confidence)

    def get_macro_euro_business_climate(self) -> Dict[str, Any]:
        """Get Eurozone business climate"""
        return self._safe_call_with_retry(ak.macro_euro_business_climate)

    # ==================== UK ECONOMIC INDICATORS ====================

    def get_macro_uk_gdp(self) -> Dict[str, Any]:
        """Get UK GDP data"""
        return self._safe_call_with_retry(ak.macro_uk_gdp)

    def get_macro_uk_cpi(self) -> Dict[str, Any]:
        """Get UK CPI data"""
        return self._safe_call_with_retry(ak.macro_uk_cpi)

    def get_macro_uk_core_cpi(self) -> Dict[str, Any]:
        """Get UK core CPI"""
        return self._safe_call_with_retry(ak.macro_uk_core_cpi)

    def get_macro_uk_pmi(self) -> Dict[str, Any]:
        """Get UK PMI data"""
        return self._safe_call_with_retry(ak.macro_uk_pmi)

    def get_macro_uk_manufacturing_pmi(self) -> Dict[str, Any]:
        """Get UK manufacturing PMI"""
        return self._safe_call_with_retry(ak.macro_uk_manufacturing_pmi)

    def get_macro_uk_services_pmi(self) -> Dict[str, Any]:
        """Get UK services PMI"""
        return self._safe_call_with_retry(ak.macro_uk_services_pmi)

    def get_macro_uk_unemployment(self) -> Dict[str, Any]:
        """Get UK unemployment rate"""
        return self._safe_call_with_retry(ak.macro_uk_unemployment)

    def get_macro_uk_interest_rate(self) -> Dict[str, Any]:
        """Get UK interest rates"""
        return self._safe_call_with_retry(ak.macro_uk_interest_rate)

    def get_macro_uk_boe_balance_sheet(self) -> Dict[str, Any]:
        """Get Bank of England balance sheet"""
        return self._safe_call_with_retry(ak.macro_uk_boe_balance_sheet)

    def get_macro_uk_retail_sales(self) -> Dict[str, Any]:
        """Get UK retail sales"""
        return self._safe_call_with_retry(ak.macro_uk_retail_sales)

    def get_macro_uk_industrial_production(self) -> Dict[str, Any]:
        """Get UK industrial production"""
        return self._safe_call_with_retry(ak.macro_uk_industrial_production)

    def get_macro_uk_trade_balance(self) -> Dict[str, Any]:
        """Get UK trade balance"""
        return self._safe_call_with_retry(ak.macro_uk_trade_balance)

    def get_macro_uk_government_debt(self) -> Dict[str, Any]:
        """Get UK government debt"""
        return self._safe_call_with_retry(ak.macro_uk_government_debt)

    def get_macro_uk_consumer_confidence(self) -> Dict[str, Any]:
        """Get UK consumer confidence"""
        return self._safe_call_with_retry(ak.macro_uk_consumer_confidence)

    # ==================== JAPAN ECONOMIC INDICATORS ====================

    def get_macro_japan_gdp(self) -> Dict[str, Any]:
        """Get Japan GDP data"""
        return self._safe_call_with_retry(ak.macro_japan_gdp)

    def get_macro_japan_cpi(self) -> Dict[str, Any]:
        """Get Japan CPI data"""
        return self._safe_call_with_retry(ak.macro_japan_cpi)

    def get_macro_japan_core_cpi(self) -> Dict[str, Any]:
        """Get Japan core CPI"""
        return self._safe_call_with_retry(ak.macro_japan_core_cpi)

    def get_macro_japan_pmi(self) -> Dict[str, Any]:
        """Get Japan PMI data"""
        return self._safe_call_with_retry(ak.macro_japan_pmi)

    def get_macro_japan_manufacturing_pmi(self) -> Dict[str, Any]:
        """Get Japan manufacturing PMI"""
        return self._safe_call_with_retry(ak.macro_japan_manufacturing_pmi)

    def get_macro_japan_services_pmi(self) -> Dict[str, Any]:
        """Get Japan services PMI"""
        return self._safe_call_with_retry(ak.macro_japan_services_pmi)

    def get_macro_japan_unemployment(self) -> Dict[str, Any]:
        """Get Japan unemployment rate"""
        return self._safe_call_with_retry(ak.macro_japan_unemployment)

    def get_macro_japan_interest_rate(self) -> Dict[str, Any]:
        """Get Japan interest rates"""
        return self._safe_call_with_retry(ak.macro_japan_interest_rate)

    def get_macro_japan_boj_balance_sheet(self) -> Dict[str, Any]:
        """Get Bank of Japan balance sheet"""
        return self._safe_call_with_retry(ak.macro_japan_boj_balance_sheet)

    def get_macro_japan_retail_sales(self) -> Dict[str, Any]:
        """Get Japan retail sales"""
        return self._safe_call_with_retry(ak.macro_japan_retail_sales)

    def get_macro_japan_industrial_production(self) -> Dict[str, Any]:
        """Get Japan industrial production"""
        return self._safe_call_with_retry(ak.macro_japan_industrial_production)

    def get_macro_japan_trade_balance(self) -> Dict[str, Any]:
        """Get Japan trade balance"""
        return self._safe_call_with_retry(ak.macro_japan_trade_balance)

    def get_macro_japan_exports(self) -> Dict[str, Any]:
        """Get Japan exports"""
        return self._safe_call_with_retry(ak.macro_japan_exports)

    def get_macro_japan_imports(self) -> Dict[str, Any]:
        """Get Japan imports"""
        return self._safe_call_with_retry(ak.macro_japan_imports)

    def get_macro_japan_tankan(self) -> Dict[str, Any]:
        """Get Japan Tankan survey"""
        return self._safe_call_with_retry(ak.macro_japan_tankan)

    def get_macro_japan_consumer_confidence(self) -> Dict[str, Any]:
        """Get Japan consumer confidence"""
        return self._safe_call_with_retry(ak.macro_japan_consumer_confidence)

    # ==================== GERMANY ECONOMIC INDICATORS ====================

    def get_macro_germany_gdp(self) -> Dict[str, Any]:
        """Get Germany GDP data"""
        return self._safe_call_with_retry(ak.macro_germany_gdp)

    def get_macro_germany_cpi(self) -> Dict[str, Any]:
        """Get Germany CPI data"""
        return self._safe_call_with_retry(ak.macro_germany_cpi)

    def get_macro_germany_pmi(self) -> Dict[str, Any]:
        """Get Germany PMI data"""
        return self._safe_call_with_retry(ak.macro_germany_pmi)

    def get_macro_germany_manufacturing_pmi(self) -> Dict[str, Any]:
        """Get Germany manufacturing PMI"""
        return self._safe_call_with_retry(ak.macro_germany_manufacturing_pmi)

    def get_macro_germany_services_pmi(self) -> Dict[str, Any]:
        """Get Germany services PMI"""
        return self._safe_call_with_retry(ak.macro_germany_services_pmi)

    def get_macro_germany_unemployment(self) -> Dict[str, Any]:
        """Get Germany unemployment rate"""
        return self._safe_call_with_retry(ak.macro_germany_unemployment)

    def get_macro_germany_industrial_production(self) -> Dict[str, Any]:
        """Get Germany industrial production"""
        return self._safe_call_with_retry(ak.macro_germany_industrial_production)

    def get_macro_germany_trade_balance(self) -> Dict[str, Any]:
        """Get Germany trade balance"""
        return self._safe_call_with_retry(ak.macro_germany_trade_balance)

    def get_macro_germany_exports(self) -> Dict[str, Any]:
        """Get Germany exports"""
        return self._safe_call_with_retry(ak.macro_germany_exports)

    def get_macro_germany_imports(self) -> Dict[str, Any]:
        """Get Germany imports"""
        return self._safe_call_with_retry(ak.macro_germany_imports)

    def get_macro_germany_business_climate(self) -> Dict[str, Any]:
        """Get Germany business climate (Ifo)"""
        return self._safe_call_with_retry(ak.macro_germany_business_climate)

    def get_macro_germany_consumer_confidence(self) -> Dict[str, Any]:
        """Get Germany consumer confidence"""
        return self._safe_call_with_retry(ak.macro_germany_consumer_confidence)

    def get_macro_germany_zew(self) -> Dict[str, Any]:
        """Get Germany ZEW economic sentiment"""
        return self._safe_call_with_retry(ak.macro_germany_zew)

    # ==================== CANADA ECONOMIC INDICATORS ====================

    def get_macro_canada_gdp(self) -> Dict[str, Any]:
        """Get Canada GDP data"""
        return self._safe_call_with_retry(ak.macro_canada_gdp)

    def get_macro_canada_cpi(self) -> Dict[str, Any]:
        """Get Canada CPI data"""
        return self._safe_call_with_retry(ak.macro_canada_cpi)

    def get_macro_canada_core_cpi(self) -> Dict[str, Any]:
        """Get Canada core CPI"""
        return self._safe_call_with_retry(ak.macro_canada_core_cpi)

    def get_macro_canada_pmi(self) -> Dict[str, Any]:
        """Get Canada PMI data"""
        return self._safe_call_with_retry(ak.macro_canada_pmi)

    def get_macro_canada_unemployment(self) -> Dict[str, Any]:
        """Get Canada unemployment rate"""
        return self._safe_call_with_retry(ak.macro_canada_unemployment)

    def get_macro_canada_interest_rate(self) -> Dict[str, Any]:
        """Get Canada interest rates"""
        return self._safe_call_with_retry(ak.macro_canada_interest_rate)

    def get_macro_canada_trade_balance(self) -> Dict[str, Any]:
        """Get Canada trade balance"""
        return self._safe_call_with_retry(ak.macro_canada_trade_balance)

    def get_macro_canada_exports(self) -> Dict[str, Any]:
        """Get Canada exports"""
        return self._safe_call_with_retry(ak.macro_canada_exports)

    def get_macro_canada_imports(self) -> Dict[str, Any]:
        """Get Canada imports"""
        return self._safe_call_with_retry(ak.macro_canada_imports)

    def get_macro_canada_retail_sales(self) -> Dict[str, Any]:
        """Get Canada retail sales"""
        return self._safe_call_with_retry(ak.macro_canada_retail_sales)

    def get_macro_canada_employment(self) -> Dict[str, Any]:
        """Get Canada employment data"""
        return self._safe_call_with_retry(ak.macro_canada_employment)

    def get_macro_canada_housing_starts(self) -> Dict[str, Any]:
        """Get Canada housing starts"""
        return self._safe_call_with_retry(ak.macro_canada_housing_starts)

    def get_macro_canada_consumer_confidence(self) -> Dict[str, Any]:
        """Get Canada consumer confidence"""
        return self._safe_call_with_retry(ak.macro_canada_consumer_confidence)

    def get_macro_canada_boy_balance_sheet(self) -> Dict[str, Any]:
        """Get Bank of Canada balance sheet"""
        return self._safe_call_with_retry(ak.macro_canada_boy_balance_sheet)

    # ==================== AUSTRALIA ECONOMIC INDICATORS ====================

    def get_macro_australia_gdp(self) -> Dict[str, Any]:
        """Get Australia GDP data"""
        return self._safe_call_with_retry(ak.macro_australia_gdp)

    def get_macro_australia_cpi(self) -> Dict[str, Any]:
        """Get Australia CPI data"""
        return self._safe_call_with_retry(ak.macro_australia_cpi)

    def get_macro_australia_core_cpi(self) -> Dict[str, Any]:
        """Get Australia core CPI"""
        return self._safe_call_with_retry(ak.macro_australia_core_cpi)

    def get_macro_australia_pmi(self) -> Dict[str, Any]:
        """Get Australia PMI data"""
        return self._safe_call_with_retry(ak.macro_australia_pmi)

    def get_macro_australia_manufacturing_pmi(self) -> Dict[str, Any]:
        """Get Australia manufacturing PMI"""
        return self._safe_call_with_retry(ak.macro_australia_manufacturing_pmi)

    def get_macro_australia_services_pmi(self) -> Dict[str, Any]:
        """Get Australia services PMI"""
        return self._safe_call_with_retry(ak.macro_australia_services_pmi)

    def get_macro_australia_unemployment(self) -> Dict[str, Any]:
        """Get Australia unemployment rate"""
        return self._safe_call_with_retry(ak.macro_australia_unemployment)

    def get_macro_australia_interest_rate(self) -> Dict[str, Any]:
        """Get Australia interest rates"""
        return self._safe_call_with_retry(ak.macro_australia_interest_rate)

    def get_macro_australia_trade_balance(self) -> Dict[str, Any]:
        """Get Australia trade balance"""
        return self._safe_call_with_retry(ak.macro_australia_trade_balance)

    def get_macro_australia_exports(self) -> Dict[str, Any]:
        """Get Australia exports"""
        return self._safe_call_with_retry(ak.macro_australia_exports)

    def get_macro_australia_imports(self) -> Dict[str, Any]:
        """Get Australia imports"""
        return self._safe_call_with_retry(ak.macro_australia_imports)

    def get_macro_australia_retail_sales(self) -> Dict[str, Any]:
        """Get Australia retail sales"""
        return self._safe_call_with_retry(ak.macro_australia_retail_sales)

    def get_macro_australia_commodities(self) -> Dict[str, Any]:
        """Get Australia commodity prices"""
        return self._safe_call_with_retry(ak.macro_australia_commodities)

    def get_macro_australia_iron_ore(self) -> Dict[str, Any]:
        """Get Australia iron ore prices"""
        return self._safe_call_with_retry(ak.macro_australia_iron_ore)

    def get_macro_australia_employment(self) -> Dict[str, Any]:
        """Get Australia employment data"""
        return self._safe_call_with_retry(ak.macro_australia_employment)

    def get_macro_australia_consumer_confidence(self) -> Dict[str, Any]:
        """Get Australia consumer confidence"""
        return self._safe_call_with_retry(ak.macro_australia_consumer_confidence)

    def get_macro_australia_rba_balance_sheet(self) -> Dict[str, Any]:
        """Get RBA balance sheet"""
        return self._safe_call_with_retry(ak.macro_australia_rba_balance_sheet)

    # ==================== SWITZERLAND ECONOMIC INDICATORS ====================

    def get_macro_swiss_gdp(self) -> Dict[str, Any]:
        """Get Switzerland GDP data"""
        return self._safe_call_with_retry(ak.macro_swiss_gdp)

    def get_macro_swiss_cpi(self) -> Dict[str, Any]:
        """Get Switzerland CPI data"""
        return self._safe_call_with_retry(ak.macro_swiss_cpi)

    def get_macro_swiss_pmi(self) -> Dict[str, Any]:
        """Get Switzerland PMI data"""
        return self._safe_call_with_retry(ak.macro_swiss_pmi)

    def get_macro_swiss_unemployment(self) -> Dict[str, Any]:
        """Get Switzerland unemployment rate"""
        return self._safe_call_with_retry(ak.macro_swiss_unemployment)

    def get_macro_swiss_interest_rate(self) -> Dict[str, Any]:
        """Get Switzerland interest rates"""
        return self._safe_call_with_retry(ak.macro_swiss_interest_rate)

    def get_macro_swiss_trade_balance(self) -> Dict[str, Any]:
        """Get Switzerland trade balance"""
        return self._safe_call_with_retry(ak.macro_swiss_trade_balance)

    def get_macro_swiss_exports(self) -> Dict[str, Any]:
        """Get Switzerland exports"""
        return self._safe_call_with_retry(ak.macro_swiss_exports)

    def get_macro_swiss_imports(self) -> Dict[str, Any]:
        """Get Switzerland imports"""
        return self._safe_call_with_retry(ak.macro_swiss_imports)

    def get_macro_swiss_retail_sales(self) -> Dict[str, Any]:
        """Get Switzerland retail sales"""
        return self._safe_call_with_retry(ak.macro_swiss_retail_sales)

    def get_macro_swiss_kof_leading(self) -> Dict[str, Any]:
        """Get Switzerland KOF leading indicators"""
        return self._safe_call_with_retry(ak.macro_swiss_kof_leading)

    def get_macro_swiss_consumer_confidence(self) -> Dict[str, Any]:
        """Get Switzerland consumer confidence"""
        return self._safe_call_with_retry(ak.macro_swiss_consumer_confidence)

    def get_macro_swiss_snb_balance_sheet(self) -> Dict[str, Any]:
        """Get SNB balance sheet"""
        return self._safe_call_with_retry(ak.macro_swiss_snb_balance_sheet)

    # ==================== GLOBAL COMMODITY INDICATORS ====================

    def get_macro_global_oil(self) -> Dict[str, Any]:
        """Get global oil prices"""
        return self._safe_call_with_retry(ak.macro_global_oil)

    def get_macro_global_gold(self) -> Dict[str, Any]:
        """Get global gold prices"""
        return self._safe_call_with_retry(ak.macro_global_gold)

    def get_macro_global_silver(self) -> Dict[str, Any]:
        """Get global silver prices"""
        return self._safe_call_with_retry(ak.macro_global_silver)

    def get_macro_global_copper(self) -> Dict[str, Any]:
        """Get global copper prices"""
        return self._safe_call_with_retry(ak.macro_global_copper)

    def get_macro_global_aluminum(self) -> Dict[str, Any]:
        """Get global aluminum prices"""
        return self._safe_call_with_retry(ak.macro_global_aluminum)

    def get_macro_global_natural_gas(self) -> Dict[str, Any]:
        """Get global natural gas prices"""
        return self._safe_call_with_retry(ak.macro_global_natural_gas)

    def get_macro_global_iron_ore(self) -> Dict[str, Any]:
        """Get global iron ore prices"""
        return self._safe_call_with_retry(ak.macro_global_iron_ore)

    def get_macro_global_coal(self) -> Dict[str, Any]:
        """Get global coal prices"""
        return self._safe_call_with_retry(ak.macro_global_coal)

    def get_macro_global_wheat(self) -> Dict[str, Any]:
        """Get global wheat prices"""
        return self._safe_call_with_retry(ak.macro_global_wheat)

    def get_macro_global_corn(self) -> Dict[str, Any]:
        """Get global corn prices"""
        return self._safe_call_with_retry(ak.macro_global_corn)

    def get_macro_global_soybeans(self) -> Dict[str, Any]:
        """Get global soybeans prices"""
        return self._safe_call_with_retry(ak.macro_global_soybeans)

    def get_macro_global_sugar(self) -> Dict[str, Any]:
        """Get global sugar prices"""
        return self._safe_call_with_retry(ak.macro_global_sugar)

    def get_macro_global_cotton(self) -> Dict[str, Any]:
        """Get global cotton prices"""
        return self._safe_call_with_retry(ak.macro_global_cotton)

    def get_macro_global_coffee(self) -> Dict[str, Any]:
        """Get global coffee prices"""
        return self._safe_call_with_retry(ak.macro_global_coffee)

    def get_macro_global_cocoa(self) -> Dict[str, Any]:
        """Get global cocoa prices"""
        return self._safe_call_with_retry(ak.macro_global_cocoa)

    def get_macro_global_lumber(self) -> Dict[str, Any]:
        """Get global lumber prices"""
        return self._safe_call_with_retry(ak.macro_global_lumber)

    def get_macro_global_rubber(self) -> Dict[str, Any]:
        """Get global rubber prices"""
        return self._safe_call_with_retry(ak.macro_global_rubber)

    def get_macro_global_platinum(self) -> Dict[str, Any]:
        """Get global platinum prices"""
        return self._safe_call_with_retry(ak.macro_global_platinum)

    def get_macro_global_palladium(self) -> Dict[str, Any]:
        """Get global palladium prices"""
        return self._safe_call_with_retry(ak.macro_global_palladium)

    def get_macro_global_nickel(self) -> Dict[str, Any]:
        """Get global nickel prices"""
        return self._safe_call_with_retry(ak.macro_global_nickel)

    def get_macro_global_zinc(self) -> Dict[str, Any]:
        """Get global zinc prices"""
        return self._safe_call_with_retry(ak.macro_global_zinc)

    def get_macro_global_lead(self) -> Dict[str, Any]:
        """Get global lead prices"""
        return self._safe_call_with_retry(ak.macro_global_lead)

    def get_macro_global_tin(self) -> Dict[str, Any]:
        """Get global tin prices"""
        return self._safe_call_with_retry(ak.macro_global_tin)

    # ==================== GLOBAL ECONOMIC INDICES ====================

    def get_global_vix(self) -> Dict[str, Any]:
        """Get VIX volatility index"""
        return self._safe_call_with_retry(ak.index_vix)

    def get_global_dollar_index(self) -> Dict[str, Any]:
        """Get US Dollar Index"""
        return self._safe_call_with_retry(ak.index_global_dollar)

    def get_global_baltic_dry(self) -> Dict[str, Any]:
        """Get Baltic Dry Index"""
        return self._safe_call_with_retry(ak.index_global_baltic)

    def get_global_cot_reports(self) -> Dict[str, Any]:
        """Get COT reports"""
        return self._safe_call_with_retry(ak.cot)

    def get_global_emerging_markets(self) -> Dict[str, Any]:
        """Get emerging markets data"""
        return self._safe_call_with_retry(ak.index_global_em)

    def get_global_developed_markets(self) -> Dict[str, Any]:
        """Get developed markets data"""
        return self._safe_call_with_retry(ak.index_global_dm)

    # ==================== UTILITIES ====================

    def get_all_available_endpoints(self) -> Dict[str, Any]:
        """Get list of all available endpoints in this wrapper"""
        methods = [method for method in dir(self) if method.startswith('get_') and callable(getattr(self, method))]
        return {
            "available_endpoints": methods,
            "total_count": len(methods),
            "categories": {
                "USA Economics": [m for m in methods if "usa" in m],
                "Eurozone Economics": [m for m in methods if "euro" in m],
                "UK Economics": [m for m in methods if "uk" in m],
                "Japan Economics": [m for m in methods if "japan" in m],
                "Germany Economics": [m for m in methods if "germany" in m],
                "Canada Economics": [m for m in methods if "canada" in m],
                "Australia Economics": [m for m in methods if "australia" in m],
                "Switzerland Economics": [m for m in methods if "swiss" in m],
                "Global Commodities": [m for m in methods if "global" in m],
                "Global Indices": [m for m in methods if m.startswith("get_global_") and "global_" not in m]
            },
            "timestamp": int(datetime.now().timestamp())
        }

  

# ==================== COMMAND LINE INTERFACE ====================

def main():
    """Command line interface for the Global Economics wrapper"""
    wrapper = GlobalEconomicsWrapper()

    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python akshare_economics_global.py <endpoint> [args...]",
            "available_endpoints": wrapper.get_all_available_endpoints()["available_endpoints"]
        }, indent=2))
        return

    endpoint = sys.argv[1]
    args = sys.argv[2:] if len(sys.argv) > 2 else []

    # Map endpoint names to method calls
    endpoint_map = {
        "get_all_endpoints": wrapper.get_all_available_endpoints,
        # USA
        "usa_gdp": wrapper.get_macro_usa_gdp,
        "usa_cpi": wrapper.get_macro_usa_cpi,
        "usa_pmi": wrapper.get_macro_usa_pmi,
        "usa_unemployment": wrapper.get_macro_usa_unemployment,
        "usa_interest_rate": wrapper.get_macro_usa_interest_rate,
        "usa_trade_balance": wrapper.get_macro_usa_trade_balance,
        "usa_fred": wrapper.get_macro_usa_fred_data,
        # Eurozone
        "euro_gdp": wrapper.get_macro_euro_gdp,
        "euro_cpi": wrapper.get_macro_euro_cpi,
        "euro_pmi": wrapper.get_macro_euro_pmi,
        "euro_unemployment": wrapper.get_macro_euro_unemployment,
        "euro_interest_rate": wrapper.get_macro_euro_interest_rate,
        # UK
        "uk_gdp": wrapper.get_macro_uk_gdp,
        "uk_cpi": wrapper.get_macro_uk_cpi,
        "uk_pmi": wrapper.get_macro_uk_pmi,
        "uk_unemployment": wrapper.get_macro_uk_unemployment,
        "uk_interest_rate": wrapper.get_macro_uk_interest_rate,
        # Japan
        "japan_gdp": wrapper.get_macro_japan_gdp,
        "japan_cpi": wrapper.get_macro_japan_cpi,
        "japan_pmi": wrapper.get_macro_japan_pmi,
        "japan_unemployment": wrapper.get_macro_japan_unemployment,
        "japan_interest_rate": wrapper.get_macro_japan_interest_rate,
        # Germany
        "germany_gdp": wrapper.get_macro_germany_gdp,
        "germany_cpi": wrapper.get_macro_germany_cpi,
        "germany_pmi": wrapper.get_macro_germany_pmi,
        "germany_unemployment": wrapper.get_macro_germany_unemployment,
        # Canada
        "canada_gdp": wrapper.get_macro_canada_gdp,
        "canada_cpi": wrapper.get_macro_canada_cpi,
        "canada_pmi": wrapper.get_macro_canada_pmi,
        "canada_unemployment": wrapper.get_macro_canada_unemployment,
        "canada_interest_rate": wrapper.get_macro_canada_interest_rate,
        # Australia
        "australia_gdp": wrapper.get_macro_australia_gdp,
        "australia_cpi": wrapper.get_macro_australia_cpi,
        "australia_pmi": wrapper.get_macro_australia_pmi,
        "australia_unemployment": wrapper.get_macro_australia_unemployment,
        "australia_commodities": wrapper.get_macro_australia_commodities,
        # Switzerland
        "swiss_gdp": wrapper.get_macro_swiss_gdp,
        "swiss_cpi": wrapper.get_macro_swiss_cpi,
        "swiss_pmi": wrapper.get_macro_swiss_pmi,
        "swiss_interest_rate": wrapper.get_macro_swiss_interest_rate,
        # Global Commodities
        "global_oil": wrapper.get_macro_global_oil,
        "global_gold": wrapper.get_macro_global_gold,
        "global_silver": wrapper.get_macro_global_silver,
        "global_copper": wrapper.get_macro_global_copper,
        "global_natural_gas": wrapper.get_macro_global_natural_gas,
        "global_iron_ore": wrapper.get_macro_global_iron_ore,
        "global_wheat": wrapper.get_macro_global_wheat,
        "global_corn": wrapper.get_macro_global_corn,
        "global_soybeans": wrapper.get_macro_global_soybeans,
        # Global Indices
        "global_vix": wrapper.get_global_vix,
        "global_dollar": wrapper.get_global_dollar_index,
        "global_baltic": wrapper.get_global_baltic_dry,
        "global_cot": wrapper.get_global_cot_reports,
        "global_em": wrapper.get_global_emerging_markets,
        "global_dm": wrapper.get_global_developed_markets
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