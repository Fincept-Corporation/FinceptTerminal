#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AKShare Macro Economic Data Wrapper
Covers 96 macroeconomic functions from akshare library (version 1.18.20)
Organized by region: Australia, Brazil, India, Russia, New Zealand, Canada, Euro, Germany, Japan, China, Switzerland, UK, USA
"""

import akshare as ak
import json
import sys
from typing import Dict, Any, Optional
from datetime import datetime, date
import pandas as pd


class DateTimeEncoder(json.JSONEncoder):
    """Custom JSON encoder for datetime objects"""
    def default(self, obj):
        if isinstance(obj, (datetime, date)):
            return obj.isoformat()
        if isinstance(obj, pd.Timestamp):
            return obj.isoformat()
        return super().default(obj)


class AKShareError(Exception):
    """Custom exception for AKShare errors"""
    pass


class MacroEconomicWrapper:
    """Wrapper for AKShare macroeconomic data functions"""

    def __init__(self):
        self.version = "1.18.20"

    def _safe_call_with_retry(self, func, *args, max_retries: int = 3, **kwargs):
        """
        Safely call akshare function with retry logic

        Args:
            func: The akshare function to call
            max_retries: Maximum number of retry attempts
            *args, **kwargs: Arguments to pass to the function

        Returns:
            Dictionary with success status and data/error message
        """
        for attempt in range(max_retries):
            try:
                df = func(*args, **kwargs)
                if df is None or (isinstance(df, pd.DataFrame) and df.empty):
                    return {
                        "success": False,
                        "error": "No data returned",
                        "data": None
                    }

                # Convert DataFrame to JSON-serializable format
                if isinstance(df, pd.DataFrame):
                    # Convert date columns to strings
                    for col in df.columns:
                        if pd.api.types.is_datetime64_any_dtype(df[col]):
                            df[col] = df[col].astype(str)

                    data = df.to_dict(orient='records')
                else:
                    data = df

                return {
                    "success": True,
                    "data": data,
                    "error": None
                }

            except Exception as e:
                if attempt == max_retries - 1:
                    return {
                        "success": False,
                        "error": str(e),
                        "data": None
                    }
                continue

        return {
            "success": False,
            "error": "Max retries exceeded",
            "data": None
        }

    # ==================== AUSTRALIA (5 functions) ====================

    def macro_australia_bank_rate(self) -> Dict[str, Any]:
        """Australia bank interest rate data"""
        return self._safe_call_with_retry(ak.macro_australia_bank_rate)

    def macro_australia_cpi_yearly(self) -> Dict[str, Any]:
        """Australia yearly CPI data"""
        return self._safe_call_with_retry(ak.macro_australia_cpi_yearly)

    def macro_australia_ppi_quarterly(self) -> Dict[str, Any]:
        """Australia quarterly PPI data"""
        return self._safe_call_with_retry(ak.macro_australia_ppi_quarterly)

    def macro_australia_retail_rate_monthly(self) -> Dict[str, Any]:
        """Australia monthly retail rate"""
        return self._safe_call_with_retry(ak.macro_australia_retail_rate_monthly)

    # ==================== BRAZIL, INDIA, RUSSIA, NEW ZEALAND (4 functions) ====================

    def macro_bank_brazil_interest_rate(self) -> Dict[str, Any]:
        """Brazil bank interest rate"""
        return self._safe_call_with_retry(ak.macro_bank_brazil_interest_rate)

    def macro_bank_india_interest_rate(self) -> Dict[str, Any]:
        """India bank interest rate"""
        return self._safe_call_with_retry(ak.macro_bank_india_interest_rate)

    def macro_bank_newzealand_interest_rate(self) -> Dict[str, Any]:
        """New Zealand bank interest rate"""
        return self._safe_call_with_retry(ak.macro_bank_newzealand_interest_rate)

    def macro_bank_russia_interest_rate(self) -> Dict[str, Any]:
        """Russia bank interest rate"""
        return self._safe_call_with_retry(ak.macro_bank_russia_interest_rate)

    # ==================== CANADA (6 functions) ====================

    def macro_canada_bank_rate(self) -> Dict[str, Any]:
        """Canada bank interest rate"""
        return self._safe_call_with_retry(ak.macro_canada_bank_rate)

    def macro_canada_core_cpi_monthly(self) -> Dict[str, Any]:
        """Canada monthly core CPI"""
        return self._safe_call_with_retry(ak.macro_canada_core_cpi_monthly)

    def macro_canada_core_cpi_yearly(self) -> Dict[str, Any]:
        """Canada yearly core CPI"""
        return self._safe_call_with_retry(ak.macro_canada_core_cpi_yearly)

    def macro_canada_cpi_yearly(self) -> Dict[str, Any]:
        """Canada yearly CPI"""
        return self._safe_call_with_retry(ak.macro_canada_cpi_yearly)

    def macro_canada_new_house_rate(self) -> Dict[str, Any]:
        """Canada new house rate"""
        return self._safe_call_with_retry(ak.macro_canada_new_house_rate)

    def macro_canada_retail_rate_monthly(self) -> Dict[str, Any]:
        """Canada monthly retail rate"""
        return self._safe_call_with_retry(ak.macro_canada_retail_rate_monthly)

    # ==================== CHINA (2 functions) ====================

    def macro_bank_china_interest_rate(self) -> Dict[str, Any]:
        """China bank interest rate"""
        return self._safe_call_with_retry(ak.macro_bank_china_interest_rate)

    def macro_cnbs(self) -> Dict[str, Any]:
        """China National Bureau of Statistics data"""
        return self._safe_call_with_retry(ak.macro_cnbs)

    # ==================== COMMODITIES (4 functions) ====================

    def macro_cons_gold(self) -> Dict[str, Any]:
        """Gold consumption data"""
        return self._safe_call_with_retry(ak.macro_cons_gold)

    def macro_cons_opec_month(self) -> Dict[str, Any]:
        """OPEC monthly consumption data"""
        return self._safe_call_with_retry(ak.macro_cons_opec_month)

    def macro_cons_silver(self) -> Dict[str, Any]:
        """Silver consumption data"""
        return self._safe_call_with_retry(ak.macro_cons_silver)

    # ==================== EURO ZONE (10 functions) ====================

    def macro_euro_cpi_mom(self) -> Dict[str, Any]:
        """Euro zone CPI month-over-month"""
        return self._safe_call_with_retry(ak.macro_euro_cpi_mom)

    def macro_euro_current_account_mom(self) -> Dict[str, Any]:
        """Euro zone current account month-over-month"""
        return self._safe_call_with_retry(ak.macro_euro_current_account_mom)

    def macro_euro_employment_change_qoq(self) -> Dict[str, Any]:
        """Euro zone employment change quarter-over-quarter"""
        return self._safe_call_with_retry(ak.macro_euro_employment_change_qoq)

    def macro_euro_industrial_production_mom(self) -> Dict[str, Any]:
        """Euro zone industrial production month-over-month"""
        return self._safe_call_with_retry(ak.macro_euro_industrial_production_mom)

    def macro_euro_lme_holding(self) -> Dict[str, Any]:
        """Euro zone LME holdings data"""
        return self._safe_call_with_retry(ak.macro_euro_lme_holding)

    def macro_euro_lme_stock(self) -> Dict[str, Any]:
        """Euro zone LME stock data"""
        return self._safe_call_with_retry(ak.macro_euro_lme_stock)

    def macro_euro_ppi_mom(self) -> Dict[str, Any]:
        """Euro zone PPI month-over-month"""
        return self._safe_call_with_retry(ak.macro_euro_ppi_mom)

    def macro_euro_retail_sales_mom(self) -> Dict[str, Any]:
        """Euro zone retail sales month-over-month"""
        return self._safe_call_with_retry(ak.macro_euro_retail_sales_mom)

    def macro_euro_sentix_investor_confidence(self) -> Dict[str, Any]:
        """Euro zone Sentix investor confidence"""
        return self._safe_call_with_retry(ak.macro_euro_sentix_investor_confidence)

    def macro_euro_zew_economic_sentiment(self) -> Dict[str, Any]:
        """Euro zone ZEW economic sentiment"""
        return self._safe_call_with_retry(ak.macro_euro_zew_economic_sentiment)

    # ==================== GERMANY (6 functions) ====================

    def macro_germany_cpi_monthly(self) -> Dict[str, Any]:
        """Germany monthly CPI"""
        return self._safe_call_with_retry(ak.macro_germany_cpi_monthly)

    def macro_germany_ifo(self) -> Dict[str, Any]:
        """Germany IFO business climate index"""
        return self._safe_call_with_retry(ak.macro_germany_ifo)

    def macro_germany_retail_sale_monthly(self) -> Dict[str, Any]:
        """Germany monthly retail sales"""
        return self._safe_call_with_retry(ak.macro_germany_retail_sale_monthly)

    def macro_germany_retail_sale_yearly(self) -> Dict[str, Any]:
        """Germany yearly retail sales"""
        return self._safe_call_with_retry(ak.macro_germany_retail_sale_yearly)

    def macro_germany_trade_adjusted(self) -> Dict[str, Any]:
        """Germany adjusted trade balance"""
        return self._safe_call_with_retry(ak.macro_germany_trade_adjusted)

    # ==================== GLOBAL & MISC (4 functions) ====================

    def macro_global_sox_index(self) -> Dict[str, Any]:
        """Global SOX semiconductor index"""
        return self._safe_call_with_retry(ak.macro_global_sox_index)

    def macro_fx_sentiment(self) -> Dict[str, Any]:
        """FX sentiment data"""
        return self._safe_call_with_retry(ak.macro_fx_sentiment)

    def macro_info_ws(self) -> Dict[str, Any]:
        """World Steel information data"""
        return self._safe_call_with_retry(ak.macro_info_ws)

    # ==================== JAPAN (3 functions) ====================

    def macro_japan_bank_rate(self) -> Dict[str, Any]:
        """Japan bank interest rate"""
        return self._safe_call_with_retry(ak.macro_japan_bank_rate)

    def macro_japan_core_cpi_yearly(self) -> Dict[str, Any]:
        """Japan yearly core CPI"""
        return self._safe_call_with_retry(ak.macro_japan_core_cpi_yearly)

    def macro_japan_head_indicator(self) -> Dict[str, Any]:
        """Japan leading indicator"""
        return self._safe_call_with_retry(ak.macro_japan_head_indicator)

    # ==================== CHINA FINANCIAL (6 functions) ====================

    def macro_rmb_deposit(self) -> Dict[str, Any]:
        """China RMB deposit data"""
        return self._safe_call_with_retry(ak.macro_rmb_deposit)

    def macro_rmb_loan(self) -> Dict[str, Any]:
        """China RMB loan data"""
        return self._safe_call_with_retry(ak.macro_rmb_loan)

    def macro_stock_finance(self) -> Dict[str, Any]:
        """China stock financing data"""
        return self._safe_call_with_retry(ak.macro_stock_finance)

    # ==================== SHIPPING (4 functions) ====================

    def macro_shipping_bci(self) -> Dict[str, Any]:
        """Baltic Capesize Index (BCI)"""
        return self._safe_call_with_retry(ak.macro_shipping_bci)

    def macro_shipping_bcti(self) -> Dict[str, Any]:
        """Baltic Clean Tanker Index (BCTI)"""
        return self._safe_call_with_retry(ak.macro_shipping_bcti)

    def macro_shipping_bdi(self) -> Dict[str, Any]:
        """Baltic Dry Index (BDI)"""
        return self._safe_call_with_retry(ak.macro_shipping_bdi)

    def macro_shipping_bpi(self) -> Dict[str, Any]:
        """Baltic Panamax Index (BPI)"""
        return self._safe_call_with_retry(ak.macro_shipping_bpi)

    # ==================== SWITZERLAND (4 functions) ====================

    def macro_swiss_gbd_bank_rate(self) -> Dict[str, Any]:
        """Switzerland bank interest rate"""
        return self._safe_call_with_retry(ak.macro_swiss_gbd_bank_rate)

    def macro_swiss_gbd_yearly(self) -> Dict[str, Any]:
        """Switzerland yearly GDP data"""
        return self._safe_call_with_retry(ak.macro_swiss_gbd_yearly)

    def macro_swiss_svme(self) -> Dict[str, Any]:
        """Switzerland SVME purchasing managers index"""
        return self._safe_call_with_retry(ak.macro_swiss_svme)

    def macro_swiss_trade(self) -> Dict[str, Any]:
        """Switzerland trade balance"""
        return self._safe_call_with_retry(ak.macro_swiss_trade)

    # ==================== UNITED KINGDOM (11 functions) ====================

    def macro_uk_bank_rate(self) -> Dict[str, Any]:
        """UK bank interest rate"""
        return self._safe_call_with_retry(ak.macro_uk_bank_rate)

    def macro_uk_core_cpi_monthly(self) -> Dict[str, Any]:
        """UK monthly core CPI"""
        return self._safe_call_with_retry(ak.macro_uk_core_cpi_monthly)

    def macro_uk_core_cpi_yearly(self) -> Dict[str, Any]:
        """UK yearly core CPI"""
        return self._safe_call_with_retry(ak.macro_uk_core_cpi_yearly)

    def macro_uk_cpi_monthly(self) -> Dict[str, Any]:
        """UK monthly CPI"""
        return self._safe_call_with_retry(ak.macro_uk_cpi_monthly)

    def macro_uk_gdp_quarterly(self) -> Dict[str, Any]:
        """UK quarterly GDP"""
        return self._safe_call_with_retry(ak.macro_uk_gdp_quarterly)

    def macro_uk_halifax_monthly(self) -> Dict[str, Any]:
        """UK Halifax monthly house price index"""
        return self._safe_call_with_retry(ak.macro_uk_halifax_monthly)

    def macro_uk_halifax_yearly(self) -> Dict[str, Any]:
        """UK Halifax yearly house price index"""
        return self._safe_call_with_retry(ak.macro_uk_halifax_yearly)

    def macro_uk_retail_monthly(self) -> Dict[str, Any]:
        """UK monthly retail sales"""
        return self._safe_call_with_retry(ak.macro_uk_retail_monthly)

    def macro_uk_retail_yearly(self) -> Dict[str, Any]:
        """UK yearly retail sales"""
        return self._safe_call_with_retry(ak.macro_uk_retail_yearly)

    def macro_uk_rightmove_monthly(self) -> Dict[str, Any]:
        """UK Rightmove monthly house price index"""
        return self._safe_call_with_retry(ak.macro_uk_rightmove_monthly)

    def macro_uk_rightmove_yearly(self) -> Dict[str, Any]:
        """UK Rightmove yearly house price index"""
        return self._safe_call_with_retry(ak.macro_uk_rightmove_yearly)

    # ==================== UNITED STATES (31 functions) ====================

    def macro_usa_api_crude_stock(self) -> Dict[str, Any]:
        """USA API crude oil stock"""
        return self._safe_call_with_retry(ak.macro_usa_api_crude_stock)

    def macro_usa_business_inventories(self) -> Dict[str, Any]:
        """USA business inventories"""
        return self._safe_call_with_retry(ak.macro_usa_business_inventories)

    def macro_usa_cb_consumer_confidence(self) -> Dict[str, Any]:
        """USA CB consumer confidence index"""
        return self._safe_call_with_retry(ak.macro_usa_cb_consumer_confidence)

    def macro_usa_cftc_c_holding(self) -> Dict[str, Any]:
        """USA CFTC commodity holdings"""
        return self._safe_call_with_retry(ak.macro_usa_cftc_c_holding)

    def macro_usa_cftc_merchant_currency_holding(self) -> Dict[str, Any]:
        """USA CFTC merchant currency holdings"""
        return self._safe_call_with_retry(ak.macro_usa_cftc_merchant_currency_holding)

    def macro_usa_cftc_merchant_goods_holding(self) -> Dict[str, Any]:
        """USA CFTC merchant goods holdings"""
        return self._safe_call_with_retry(ak.macro_usa_cftc_merchant_goods_holding)

    def macro_usa_cftc_nc_holding(self) -> Dict[str, Any]:
        """USA CFTC non-commercial holdings"""
        return self._safe_call_with_retry(ak.macro_usa_cftc_nc_holding)

    def macro_usa_cme_merchant_goods_holding(self) -> Dict[str, Any]:
        """USA CME merchant goods holdings"""
        return self._safe_call_with_retry(ak.macro_usa_cme_merchant_goods_holding)

    def macro_usa_core_pce_price(self) -> Dict[str, Any]:
        """USA core PCE price index"""
        return self._safe_call_with_retry(ak.macro_usa_core_pce_price)

    def macro_usa_core_ppi(self) -> Dict[str, Any]:
        """USA core PPI"""
        return self._safe_call_with_retry(ak.macro_usa_core_ppi)

    def macro_usa_cpi_yoy(self) -> Dict[str, Any]:
        """USA CPI year-over-year"""
        return self._safe_call_with_retry(ak.macro_usa_cpi_yoy)

    def macro_usa_crude_inner(self) -> Dict[str, Any]:
        """USA crude oil domestic production"""
        return self._safe_call_with_retry(ak.macro_usa_crude_inner)

    def macro_usa_eia_crude_rate(self) -> Dict[str, Any]:
        """USA EIA crude oil utilization rate"""
        return self._safe_call_with_retry(ak.macro_usa_eia_crude_rate)

    def macro_usa_exist_home_sales(self) -> Dict[str, Any]:
        """USA existing home sales"""
        return self._safe_call_with_retry(ak.macro_usa_exist_home_sales)

    def macro_usa_export_price(self) -> Dict[str, Any]:
        """USA export price index"""
        return self._safe_call_with_retry(ak.macro_usa_export_price)

    def macro_usa_factory_orders(self) -> Dict[str, Any]:
        """USA factory orders"""
        return self._safe_call_with_retry(ak.macro_usa_factory_orders)

    def macro_usa_house_price_index(self) -> Dict[str, Any]:
        """USA house price index"""
        return self._safe_call_with_retry(ak.macro_usa_house_price_index)

    def macro_usa_import_price(self) -> Dict[str, Any]:
        """USA import price index"""
        return self._safe_call_with_retry(ak.macro_usa_import_price)

    def macro_usa_initial_jobless(self) -> Dict[str, Any]:
        """USA initial jobless claims"""
        return self._safe_call_with_retry(ak.macro_usa_initial_jobless)

    def macro_usa_ism_non_pmi(self) -> Dict[str, Any]:
        """USA ISM non-manufacturing PMI"""
        return self._safe_call_with_retry(ak.macro_usa_ism_non_pmi)

    def macro_usa_job_cuts(self) -> Dict[str, Any]:
        """USA job cuts data"""
        return self._safe_call_with_retry(ak.macro_usa_job_cuts)

    def macro_usa_lmci(self) -> Dict[str, Any]:
        """USA Labor Market Conditions Index"""
        return self._safe_call_with_retry(ak.macro_usa_lmci)

    def macro_usa_michigan_consumer_sentiment(self) -> Dict[str, Any]:
        """USA Michigan consumer sentiment"""
        return self._safe_call_with_retry(ak.macro_usa_michigan_consumer_sentiment)

    def macro_usa_nahb_house_market_index(self) -> Dict[str, Any]:
        """USA NAHB housing market index"""
        return self._safe_call_with_retry(ak.macro_usa_nahb_house_market_index)

    def macro_usa_new_home_sales(self) -> Dict[str, Any]:
        """USA new home sales"""
        return self._safe_call_with_retry(ak.macro_usa_new_home_sales)

    def macro_usa_nfib_small_business(self) -> Dict[str, Any]:
        """USA NFIB small business optimism index"""
        return self._safe_call_with_retry(ak.macro_usa_nfib_small_business)

    def macro_usa_pending_home_sales(self) -> Dict[str, Any]:
        """USA pending home sales"""
        return self._safe_call_with_retry(ak.macro_usa_pending_home_sales)

    def macro_usa_personal_spending(self) -> Dict[str, Any]:
        """USA personal spending"""
        return self._safe_call_with_retry(ak.macro_usa_personal_spending)

    def macro_usa_phs(self) -> Dict[str, Any]:
        """USA PHS index"""
        return self._safe_call_with_retry(ak.macro_usa_phs)

    def macro_usa_pmi(self) -> Dict[str, Any]:
        """USA PMI"""
        return self._safe_call_with_retry(ak.macro_usa_pmi)

    def macro_usa_real_consumer_spending(self) -> Dict[str, Any]:
        """USA real consumer spending"""
        return self._safe_call_with_retry(ak.macro_usa_real_consumer_spending)

    def macro_usa_rig_count(self) -> Dict[str, Any]:
        """USA oil rig count"""
        return self._safe_call_with_retry(ak.macro_usa_rig_count)

    def macro_usa_services_pmi(self) -> Dict[str, Any]:
        """USA services PMI"""
        return self._safe_call_with_retry(ak.macro_usa_services_pmi)

    def macro_usa_spcs20(self) -> Dict[str, Any]:
        """USA S&P Case-Shiller 20-city home price index"""
        return self._safe_call_with_retry(ak.macro_usa_spcs20)


def main():
    """CLI interface for macro economic data wrapper"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python akshare_macro.py <function_name> [args]",
            "available_regions": [
                "Australia (5 functions)",
                "Brazil, India, Russia, New Zealand (4 functions)",
                "Canada (6 functions)",
                "China (2 functions)",
                "Commodities (3 functions)",
                "Euro Zone (10 functions)",
                "Germany (6 functions)",
                "Global (3 functions)",
                "Japan (3 functions)",
                "China Financial (3 functions)",
                "Shipping (4 functions)",
                "Switzerland (4 functions)",
                "UK (11 functions)",
                "USA (31 functions)"
            ],
            "total_functions": 96,
            "example": "python akshare_macro.py macro_usa_pmi"
        }, cls=DateTimeEncoder, ensure_ascii=False, indent=2))
        sys.exit(1)

    wrapper = MacroEconomicWrapper()
    function_name = sys.argv[1]

    if hasattr(wrapper, function_name):
        func = getattr(wrapper, function_name)
        result = func()
        print(json.dumps(result, cls=DateTimeEncoder, ensure_ascii=False, indent=2))
    else:
        print(json.dumps({
            "error": f"Function '{function_name}' not found",
            "hint": "Use 'python akshare_macro.py' to see available functions"
        }, cls=DateTimeEncoder, ensure_ascii=False, indent=2))
        sys.exit(1)


if __name__ == "__main__":
    main()
