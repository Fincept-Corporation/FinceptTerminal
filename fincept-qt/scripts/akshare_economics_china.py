"""
AKShare China Economics Data Wrapper
Wrapper for Chinese economic indicators and macro data
Returns JSON output for Qt/C++ integration

NOTE: Contains 85 endpoints total.

FAST endpoints (54): Response time < 20 seconds
gdp, gdp_yearly, cpi, ppi, pmi, non_man_pmi, money_supply, shibor_all, lpr, reserve_requirement_ratio,
new_financial_credit, bank_financing, central_bank_balance, fdi, fx_reserves_yearly, fx_gold, rmb,
construction_index, enterprise_boom_index, cx_pmi_yearly, cx_services_pmi_yearly, real_estate, new_house_price,
consumer_goods_retail, daily_energy, commodity_price_index, au_report, urban_unemployment, national_tax_receipts,
czsr, gdzctz, hgjck, gyzjz, qyspjg, wbck, whxd, xfzxx, stock_market_cap, market_margin_sh, market_margin_sz,
insurance_income, freight_index, lpi_index, mobile_number, hk_cpi, hk_cpi_ratio, hk_ppi, hk_gdp, hk_gdp_ratio,
hk_rate_of_unemployment, hk_trade_diff_ratio, hk_building_amount, hk_building_volume, hk_market_info

SLOW endpoints (25): Response time 60-120 seconds - These work but take 1-2 minutes
cpi_monthly, cpi_yearly, ppi_yearly, pmi_yearly, m2_yearly, trade_balance, exports_yoy, imports_yoy,
industrial_production_yoy, construction_price_index, agricultural_index, agricultural_product, vegetable_basket,
energy_index, bond_public, bdti_index, bsi_index, yw_electronic_index

UNRELIABLE endpoints (4): Connection issues or server unavailable
supply_of_money, foreign_exchange_gold, retail_price_index, society_electricity, insurance, society_traffic_volume,
passenger_load_factor, postal_telecommunicational, international_tourism_fx, shrzgm

REQUIRES PARAMETERS (2): Cannot be auto-called without specific arguments
nbs_nation (needs: kind, path), nbs_region (needs: kind, path, region)

DEPRECATED (1): API structure changed in AkShare library
swap_rate
"""

import sys
import json
import pandas as pd
import akshare as ak

# safe_call: one akshare call -> {success, data, columns, count} or a plain-English error;
# run_wrapper_cli: get_all_endpoints / <endpoint> [field=value ...] -> one JSON line on stdout.
from akshare_screen_args import run_wrapper_cli, safe_call
from typing import Dict, Any, List
from datetime import datetime, timedelta, date
import time


class ChinaEconomicsWrapper:
    """China economics wrapper with VALIDATED akshare functions (85 total)"""

    def __init__(self):
        self.default_timeout = 30
        self.retry_delay = 2

    def _safe_call_with_retry(self, func, *args, max_retries: int = 2, **kwargs) -> Dict[str, Any]:
        """One akshare call -> {success, data, columns, count} or a plain-English error (shared
        akshare_screen_args.safe_call: only a dropped connection or a timeout is retried, never a
        rate limit or a refusal)."""
        return safe_call(func, *args, _retries=max_retries, **kwargs)

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

    # The NBS functions take a table of data.stats.gov.cn by kind and path (catalogue levels joined
    # with " > "). The screen has no box for those, so each endpoint opens a useful table by default;
    # a manual run can name another: nbs_nation 月度数据 "能源 > ..." LAST13.

    def _nbs_table(self, func, first_column: str, **kwargs) -> Dict[str, Any]:
        # The row labels (indicators or regions) are the frame's index: keep them as a column.
        return self._safe_call_with_retry(
            lambda: func(**kwargs).reset_index().rename(columns={"index": first_column}))

    def get_nbs_nation(self, kind: str = "季度数据", path: str = "国民经济核算 > 国内生产总值 (现价)",
                       period: str = "LAST10") -> Dict[str, Any]:
        """NBS national table, default quarterly GDP (current prices), last 10 quarters"""
        return self._nbs_table(ak.macro_china_nbs_nation, "指标", kind=kind, path=path, period=period)

    def get_nbs_region(self, kind: str = "分省季度数据", path: str = "国民经济核算 > 地区生产总值",
                       indicator: str = "地区生产总值_累计值(亿元)", region: str = "",
                       period: str = "LAST5") -> Dict[str, Any]:
        """NBS provincial table, default year-to-date GDP by province, last 5 quarters"""
        return self._nbs_table(ak.macro_china_nbs_region, "指标" if region else "地区", kind=kind, path=path,
                               indicator=indicator or None, region=region or None, period=period)

    # ==================== HONG KONG ====================

    def get_hk_cpi(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_hk_cpi)

    def get_hk_cpi_ratio(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_hk_cpi_ratio)

    def get_hk_ppi(self) -> Dict[str, Any]:
        return self._safe_call_with_retry(ak.macro_china_hk_ppi)

    def get_hk_gdp(self) -> Dict[str, Any]:
        # akshare spells its Hong Kong GDP functions macro_china_hk_gbp*
        return self._safe_call_with_retry(ak.macro_china_hk_gbp)

    def get_hk_gdp_ratio(self) -> Dict[str, Any]:
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
            "hk_cpi", "hk_cpi_ratio", "hk_ppi", "hk_gdp", "hk_gdp_ratio",
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
    # One JSON line on stdout (NaN -> null, dates as ISO text); akshare's own prints go to stderr.
    return run_wrapper_cli(wrapper, wrapper.get_all_available_endpoints, ascii=True, iso_dates=True)


if __name__ == "__main__":
    sys.exit(main())
