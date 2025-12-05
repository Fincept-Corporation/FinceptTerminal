"""
AKShare Bonds Data Wrapper
Comprehensive wrapper for Chinese and international bond market data
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


class BondsWrapper:
    """Comprehensive bonds data wrapper with 50+ endpoints"""

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

    # ==================== CHINESE BOND MARKET OVERVIEW ====================

    def get_bond_zh_hs_spot(self) -> Dict[str, Any]:
        """Get Chinese bond spot quotes"""
        return self._safe_call_with_retry(ak.bond_zh_hs_spot)

    def get_bond_zh_hs_spot_cb(self) -> Dict[str, Any]:
        """Get Chinese convertible bond spot quotes"""
        return self._safe_call_with_retry(ak.bond_zh_hs_spot_cb)

    def get_bond_zh_sina(self) -> Dict[str, Any]:
        """Get Chinese bond market data from Sina"""
        return self._safe_call_with_retry(ak.bond_zh_sina)

    def get_bond_zh_sina_cb(self) -> Dict[str, Any]:
        """Get Chinese convertible bond data from Sina"""
        return self._safe_call_with_retry(ak.bond_zh_sina_cb)

    def get_bond_em(self) -> Dict[str, Any]:
        """Get bond market overview from Eastmoney"""
        return self._safe_call_with_retry(ak.bond_em)

    def get_bond_summary(self) -> Dict[str, Any]:
        """Get bond market summary"""
        return self._safe_call_with_retry(ak.bond_summary)

    def get_bond_zh_cov(self) -> Dict[str, Any]:
        """Get Chinese bond market overview"""
        return self._safe_call_with_retry(ak.bond_zh_cov)

    # ==================== CHINESE BOND YIELD CURVE ====================

    def get_bond_china_yield(self) -> Dict[str, Any]:
        """Get China bond yield curve"""
        return self._safe_call_with_retry(ak.bond_china_yield)

    def get_bond_china_yield_cy(self) -> Dict[str, Any]:
        """Get China corporate bond yield curve"""
        return self._safe_call_with_retry(ak.bond_china_yield_cy)

    def get_bond_china_yield_gov(self) -> Dict[str, Any]:
        """Get China government bond yield curve"""
        return self._safe_call_with_retry(ak.bond_china_yield_gov)

    def get_bond_china_yield_shibor(self) -> Dict[str, Any]:
        """Get China SHIBOR yield curve"""
        return self._safe_call_with_retry(ak.bond_china_yield_shibor)

    def get_bond_china_yield_lpr(self) -> Dict[str, Any]:
        """Get China LPR yield curve"""
        return self._safe_call_with_retry(ak.bond_china_yield_lpr)

    def get_bond_china_yield_curve(self) -> Dict[str, Any]:
        """Get complete China yield curve data"""
        return self._safe_call_with_retry(ak.bond_china_yield_curve)

    def get_bond_china_yield_curve_spot(self) -> Dict[str, Any]:
        """Get China spot yield curve"""
        return self._safe_call_with_retry(ak.bond_china_yield_curve_spot)

    def get_bond_china_yield_curve_forward(self) -> Dict[str, Any]:
        """Get China forward yield curve"""
        return self._safe_call_with_retry(ak.bond_china_yield_curve_forward)

    def get_bond_china_yield_curve_historical(self, start_date: str = None, end_date: str = None) -> Dict[str, Any]:
        """Get China historical yield curve"""
        start = start_date or self.default_start_date
        end = end_date or self.default_end_date
        return self._safe_call_with_retry(ak.bond_china_yield_curve_historical, start_date=start, end_date=end)

    # ==================== CHINESE BOND MARKET DATA ====================

    def get_bond_zh_hist(self, symbol: str, start_date: str = None, end_date: str = None) -> Dict[str, Any]:
        """Get Chinese bond historical data"""
        start = start_date or self.default_start_date
        end = end_date or self.default_end_date
        return self._safe_call_with_retry(ak.bond_zh_hist, symbol=symbol, start_date=start, end_date=end)

    def get_bond_zh_hist_cb(self, symbol: str, start_date: str = None, end_date: str = None) -> Dict[str, Any]:
        """Get Chinese convertible bond historical data"""
        start = start_date or self.default_start_date
        end = end_date or self.default_end_date
        return self._safe_call_with_retry(ak.bond_zh_hist_cb, symbol=symbol, start_date=start, end_date=end)

    def get_bond_zh_daily(self) -> Dict[str, Any]:
        """Get Chinese bond daily data"""
        return self._safe_call_with_retry(ak.bond_zh_daily)

    def get_bond_zh_daily_cb(self) -> Dict[str, Any]:
        """Get Chinese convertible bond daily data"""
        return self._safe_call_with_retry(ak.bond_zh_daily_cb)

    def get_bond_zh_quote(self) -> Dict[str, Any]:
        """Get Chinese bond quotes"""
        return self._safe_call_with_retry(ak.bond_zh_quote)

    def get_bond_zh_quote_cb(self) -> Dict[str, Any]:
        """Get Chinese convertible bond quotes"""
        return self._safe_call_with_retry(ak.bond_zh_quote_cb)

    # ==================== CHINESE GOVERNMENT BONDS ====================

    def get_bond_gov_cn(self) -> Dict[str, Any]:
        """Get Chinese government bonds"""
        return self._safe_call_with_retry(ak.bond_gov_cn)

    def get_bond_gov_hist(self, symbol: str, start_date: str = None, end_date: str = None) -> Dict[str, Any]:
        """Get Chinese government bond historical data"""
        start = start_date or self.default_start_date
        end = end_date or self.default_end_date
        return self._safe_call_with_retry(ak.bond_gov_hist, symbol=symbol, start_date=start, end_date=end)

    def get_bond_gov_spot(self) -> Dict[str, Any]:
        """Get Chinese government bond spot prices"""
        return self._safe_call_with_retry(ak.bond_gov_spot)

    def get_bond_gov_yield(self) -> Dict[str, Any]:
        """Get Chinese government bond yields"""
        return self._safe_call_with_retry(ak.bond_gov_yield)

    def get_bond_gov_10y(self) -> Dict[str, Any]:
        """Get Chinese 10-year government bond yield"""
        return self._safe_call_with_retry(ak.bond_gov_10y)

    def get_bond_gov_1y(self) -> Dict[str, Any]:
        """Get Chinese 1-year government bond yield"""
        return self._safe_call_with_retry(ak.bond_gov_1y)

    def get_bond_gov_2y(self) -> Dict[str, Any]:
        """Get Chinese 2-year government bond yield"""
        return self._safe_call_with_retry(ak.bond_gov_2y)

    def get_bond_gov_5y(self) -> Dict[str, Any]:
        """Get Chinese 5-year government bond yield"""
        return self._safe_call_with_retry(ak.bond_gov_5y)

    def get_bond_gov_30y(self) -> Dict[str, Any]:
        """Get Chinese 30-year government bond yield"""
        return self._safe_call_with_retry(ak.bond_gov_30y)

    # ==================== CHINESE CORPORATE BONDS ====================

    def get_bond_corp_cn(self) -> Dict[str, Any]:
        """Get Chinese corporate bonds"""
        return self._safe_call_with_retry(ak.bond_corp_cn)

    def get_bond_corp_hist(self, symbol: str, start_date: str = None, end_date: str = None) -> Dict[str, Any]:
        """Get Chinese corporate bond historical data"""
        start = start_date or self.default_start_date
        end = end_date or self.default_end_date
        return self._safe_call_with_retry(ak.bond_corp_hist, symbol=symbol, start_date=start, end_date=end)

    def get_bond_corp_spot(self) -> Dict[str, Any]:
        """Get Chinese corporate bond spot prices"""
        return self._safe_call_with_retry(ak.bond_corp_spot)

    def get_bond_corp_yield(self) -> Dict[str, Any]:
        """Get Chinese corporate bond yields"""
        return self._safe_call_with_retry(ak.bond_corp_yield)

    def get_bond_corp_spread(self) -> Dict[str, Any]:
        """Get Chinese corporate bond spreads"""
        return self._safe_call_with_retry(ak.bond_corp_spread)

    def get_bond_corp_rating(self) -> Dict[str, Any]:
        """Get Chinese corporate bond ratings"""
        return self._safe_call_with_retry(ak.bond_corp_rating)

    # ==================== CONVERTIBLE BONDS ====================

    def get_bond_convert(self) -> Dict[str, Any]:
        """Get convertible bonds overview"""
        return self._safe_call_with_retry(ak.bond_convert)

    def get_bond_convert_spot(self) -> Dict[str, Any]:
        """Get convertible bonds spot prices"""
        return self._safe_call_with_retry(ak.bond_convert_spot)

    def get_bond_convert_hist(self, symbol: str, start_date: str = None, end_date: str = None) -> Dict[str, Any]:
        """Get convertible bond historical data"""
        start = start_date or self.default_start_date
        end = end_date or self.default_end_date
        return self._safe_call_with_retry(ak.bond_convert_hist, symbol=symbol, start_date=start, end_date=end)

    def get_bond_convert_quote(self) -> Dict[str, Any]:
        """Get convertible bond quotes"""
        return self._safe_call_with_retry(ak.bond_convert_quote)

    def get_bond_convert_premium(self) -> Dict[str, Any]:
        """Get convertible bond premiums"""
        return self._safe_call_with_retry(ak.bond_convert_premium)

    def get_bond_convert_analysis(self) -> Dict[str, Any]:
        """Get convertible bond analysis"""
        return self._safe_call_with_retry(ak.bond_convert_analysis)

    def get_bond_cb_index_jsl(self) -> Dict[str, Any]:
        """Get convertible bond index from JSL"""
        return self._safe_call_with_retry(ak.bond_cb_index_jsl)

    def get_bond_cb_index_ths(self) -> Dict[str, Any]:
        """Get convertible bond index from THS"""
        return self._safe_call_with_retry(ak.bond_cb_index_ths)

    # ==================== BOND ISSUANCE DATA ====================

    def get_bond_issue_cninfo(self) -> Dict[str, Any]:
        """Get bond issuance data from CNInfo"""
        return self._safe_call_with_retry(ak.bond_issue_cninfo)

    def get_bond_issue_detail(self, bond_code: str) -> Dict[str, Any]:
        """Get bond issuance details"""
        return self._safe_call_with_retry(ak.bond_issue_detail, bond_code=bond_code)

    def get_bond_issue_gov(self) -> Dict[str, Any]:
        """Get government bond issuance"""
        return self._safe_call_with_retry(ak.bond_issue_gov)

    def get_bond_issue_corp(self) -> Dict[str, Any]:
        """Get corporate bond issuance"""
        return self._safe_call_with_retry(ak.bond_issue_corp)

    def get_bond_issue_convert(self) -> Dict[str, Any]:
        """Get convertible bond issuance"""
        return self._safe_call_with_retry(ak.bond_issue_convert)

    def get_bond_issue_financial(self) -> Dict[str, Any]:
        """Get financial bond issuance"""
        return self._safe_call_with_retry(ak.bond_issue_financial)

    def get_bond_issue_local_gov(self) -> Dict[str, Any]:
        """Get local government bond issuance"""
        return self._safe_call_with_retry(ak.bond_issue_local_gov)

    # ==================== MONEY MARKET BONDS ====================

    def get_bond_china_money(self) -> Dict[str, Any]:
        """Get China money market bonds"""
        return self._safe_call_with_retry(ak.bond_china_money)

    def get_bond_money_market(self) -> Dict[str, Any]:
        """Get money market bonds"""
        return self._safe_call_with_retry(ak.bond_money_market)

    def get_bond_money_repo(self) -> Dict[str, Any]:
        """Get repo market data"""
        return self._safe_call_with_retry(ak.bond_money_repo)

    def get_bond_money_shibor(self) -> Dict[str, Any]:
        """Get SHIBOR money market rates"""
        return self._safe_call_with_retry(ak.bond_money_shibor)

    def get_bond_money_lpr(self) -> Dict[str, Any]:
        """Get LPR money market rates"""
        return self._safe_call_with_retry(ak.bond_money_lpr)

    # ==================== NAFMII BOND DATA ====================

    def get_bond_nafmii(self) -> Dict[str, Any]:
        """Get NAFMII bond data"""
        return self._safe_call_with_retry(ak.bond_nafmii)

    def get_bond_nafmii_abn(self) -> Dict[str, Any]:
        """Get NAFMII ABN data"""
        return self._safe_call_with_retry(ak.bond_nafmii_abn)

    def get_bond_nafmii_mtn(self) -> Dict[str, Any]:
        """Get NAFMII MTN data"""
        return self._safe_call_with_retry(ak.bond_nafmii_mtn)

    def get_bond_nafmii_cp(self) -> Dict[str, Any]:
        """Get NAFMII CP data"""
        return self._safe_call_with_retry(ak.bond_nafmii_cp)

    def get_bond_nafmii_pn(self) -> Dict[str, Any]:
        """Get NAFMII PN data"""
        return self._safe_call_with_retry(ak.bond_nafmii_pn)

    def get_bond_nafmii_registrations(self) -> Dict[str, Any]:
        """Get NAFMII bond registrations"""
        return self._safe_call_with_retry(ak.bond_nafmii_registrations)

    # ==================== BOND MARKET STATISTICS ====================

    def get_bond_stats_daily(self) -> Dict[str, Any]:
        """Get daily bond market statistics"""
        return self._safe_call_with_retry(ak.bond_stats_daily)

    def get_bond_stats_weekly(self) -> Dict[str, Any]:
        """Get weekly bond market statistics"""
        return self._safe_call_with_retry(ak.bond_stats_weekly)

    def get_bond_stats_monthly(self) -> Dict[str, Any]:
        """Get monthly bond market statistics"""
        return self._safe_call_with_retry(ak.bond_stats_monthly)

    def get_bond_stats_yearly(self) -> Dict[str, Any]:
        """Get yearly bond market statistics"""
        return self._safe_call_with_retry(ak.bond_stats_yearly)

    def get_bond_turnover(self) -> Dict[str, Any]:
        """Get bond market turnover"""
        return self._safe_call_with_retry(ak.bond_turnover)

    def get_bond_liquidity(self) -> Dict[str, Any]:
        """Get bond market liquidity"""
        return self._safe_call_with_retry(ak.bond_liquidity)

    # ==================== INTERNATIONAL BONDS ====================

    def get_bond_us_treasury(self) -> Dict[str, Any]:
        """Get US Treasury bond data"""
        return self._safe_call_with_retry(ak.bond_us_treasury)

    def get_bond_us_treasury_yield(self) -> Dict[str, Any]:
        """Get US Treasury yields"""
        return self._safe_call_with_retry(ak.bond_us_treasury_yield)

    def get_bond_us_corporate(self) -> Dict[str, Any]:
        """Get US corporate bond data"""
        return self._safe_call_with_retry(ak.bond_us_corporate)

    def get_bond_us_municipal(self) -> Dict[str, Any]:
        """Get US municipal bond data"""
        return self._safe_call_with_retry(ak.bond_us_municipal)

    def get_bond_europe(self) -> Dict[str, Any]:
        """Get European bond data"""
        return self._safe_call_with_retry(ak.bond_europe)

    def get_bond_japan(self) -> Dict[str, Any]:
        """Get Japanese bond data"""
        return self._safe_call_with_retry(ak.bond_japan)

    def get_bond_uk_gilt(self) -> Dict[str, Any]:
        """Get UK Gilt data"""
        return self._safe_call_with_retry(ak.bond_uk_gilt)

    def get_bond_germany_bund(self) -> Dict[str, Any]:
        """Get German Bund data"""
        return self._safe_call_with_retry(ak.bond_germany_bund)

    # ==================== BOND INDICES ====================

    def get_bond_index_china(self) -> Dict[str, Any]:
        """Get Chinese bond indices"""
        return self._safe_call_with_retry(ak.bond_index_china)

    def get_bond_index_citic(self) -> Dict[str, Any]:
        """Get CITIC bond indices"""
        return self._safe_call_with_retry(ak.bond_index_citic)

    def get_bond_index_cyb(self) -> Dict[str, Any]:
        """Get CYB bond indices"""
        return self._safe_call_with_retry(ak.bond_index_cyb)

    def get_bond_index_ths(self) -> Dict[str, Any]:
        """Get THS bond indices"""
        return self._safe_call_with_retry(ak.bond_index_ths)

    def get_bond_index_sw(self) -> Dict[str, Any]:
        """Get SW bond indices"""
        return self._safe_call_with_retry(ak.bond_index_sw)

    def get_bond_index_global(self) -> Dict[str, Any]:
        """Get global bond indices"""
        return self._safe_call_with_retry(ak.bond_index_global)

    # ==================== BOND RATINGS ====================

    def get_bond_rating_domestic(self) -> Dict[str, Any]:
        """Get domestic bond ratings"""
        return self._safe_call_with_retry(ak.bond_rating_domestic)

    def get_bond_rating_international(self) -> Dict[str, Any]:
        """Get international bond ratings"""
        return self._safe_call_with_retry(ak.bond_rating_international)

    def get_bond_rating_agencies(self) -> Dict[str, Any]:
        """Get bond rating agencies data"""
        return self._safe_call_with_retry(ak.bond_rating_agencies)

    def get_bond_rating_changes(self) -> Dict[str, Any]:
        """Get bond rating changes"""
        return self._safe_call_with_retry(ak.bond_rating_changes)

    # ==================== UTILITIES ====================

    def get_all_available_endpoints(self) -> Dict[str, Any]:
        """Get list of all available endpoints in this wrapper"""
        methods = [method for method in dir(self) if method.startswith('get_') and callable(getattr(self, method))]
        return {
            "available_endpoints": methods,
            "total_count": len(methods),
            "categories": {
                "Chinese Bond Market": [m for m in methods if any(x in m for x in ["bond_zh", "bond_em", "bond_summary", "bond_cov"])],
                "Chinese Yield Curve": [m for m in methods if "bond_china_yield" in m],
                "Chinese Market Data": [m for m in methods if any(x in m for x in ["bond_zh_hist", "bond_zh_daily", "bond_zh_quote"])],
                "Chinese Government Bonds": [m for m in methods if "bond_gov" in m],
                "Chinese Corporate Bonds": [m for m in methods if "bond_corp" in m],
                "Convertible Bonds": [m for m in methods if any(x in m for x in ["bond_convert", "bond_cb_index"])],
                "Bond Issuance": [m for m in methods if "bond_issue" in m],
                "Money Market": [m for m in methods if any(x in m for x in ["bond_china_money", "bond_money"])],
                "NAFMII": [m for m in methods if "bond_nafmii" in m],
                "Market Statistics": [m for m in methods if any(x in m for x in ["bond_stats", "bond_turnover", "bond_liquidity"])],
                "International Bonds": [m for m in methods if any(x in m for x in ["bond_us", "bond_europe", "bond_japan", "bond_uk", "bond_germany"])],
                "Bond Indices": [m for m in methods if "bond_index" in m],
                "Bond Ratings": [m for m in methods if "bond_rating" in m]
            },
            "timestamp": int(datetime.now().timestamp())
        }

    

# ==================== COMMAND LINE INTERFACE ====================

def main():
    """Command line interface for the Bonds wrapper"""
    wrapper = BondsWrapper()

    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python akshare_bonds.py <endpoint> [args...]",
            "available_endpoints": wrapper.get_all_available_endpoints()["available_endpoints"]
        }, indent=2))
        return

    endpoint = sys.argv[1]
    args = sys.argv[2:] if len(sys.argv) > 2 else []

    # Map endpoint names to method calls
    endpoint_map = {
        "get_all_endpoints": wrapper.get_all_available_endpoints,
        # Chinese Bond Market
        "bond_spot": wrapper.get_bond_zh_hs_spot,
        "bond_spot_cb": wrapper.get_bond_zh_hs_spot_cb,
        "bond_sina": wrapper.get_bond_zh_sina,
        "bond_sina_cb": wrapper.get_bond_zh_sina_cb,
        "bond_em": wrapper.get_bond_em,
        "bond_summary": wrapper.get_bond_summary,
        "bond_cov": wrapper.get_bond_zh_cov,
        # Chinese Yield Curve
        "bond_yield": wrapper.get_bond_china_yield,
        "bond_yield_cy": wrapper.get_bond_china_yield_cy,
        "bond_yield_gov": wrapper.get_bond_china_yield_gov,
        "bond_yield_shibor": wrapper.get_bond_china_yield_shibor,
        "bond_yield_lpr": wrapper.get_bond_china_yield_lpr,
        "bond_yield_curve": wrapper.get_bond_china_yield_curve,
        "bond_yield_spot": wrapper.get_bond_china_yield_curve_spot,
        "bond_yield_forward": wrapper.get_bond_china_yield_curve_forward,
        "bond_yield_historical": wrapper.get_bond_china_yield_curve_historical,
        # Chinese Market Data
        "bond_hist": wrapper.get_bond_zh_hist,
        "bond_hist_cb": wrapper.get_bond_zh_hist_cb,
        "bond_daily": wrapper.get_bond_zh_daily,
        "bond_daily_cb": wrapper.get_bond_zh_daily_cb,
        "bond_quote": wrapper.get_bond_zh_quote,
        "bond_quote_cb": wrapper.get_bond_zh_quote_cb,
        # Chinese Government Bonds
        "bond_gov": wrapper.get_bond_gov_cn,
        "bond_gov_hist": wrapper.get_bond_gov_hist,
        "bond_gov_spot": wrapper.get_bond_gov_spot,
        "bond_gov_yield": wrapper.get_bond_gov_yield,
        "bond_gov_10y": wrapper.get_bond_gov_10y,
        "bond_gov_1y": wrapper.get_bond_gov_1y,
        "bond_gov_2y": wrapper.get_bond_gov_2y,
        "bond_gov_5y": wrapper.get_bond_gov_5y,
        "bond_gov_30y": wrapper.get_bond_gov_30y,
        # Chinese Corporate Bonds
        "bond_corp": wrapper.get_bond_corp_cn,
        "bond_corp_hist": wrapper.get_bond_corp_hist,
        "bond_corp_spot": wrapper.get_bond_corp_spot,
        "bond_corp_yield": wrapper.get_bond_corp_yield,
        "bond_corp_spread": wrapper.get_bond_corp_spread,
        "bond_corp_rating": wrapper.get_bond_corp_rating,
        # Convertible Bonds
        "bond_convert": wrapper.get_bond_convert,
        "bond_convert_spot": wrapper.get_bond_convert_spot,
        "bond_convert_hist": wrapper.get_bond_convert_hist,
        "bond_convert_quote": wrapper.get_bond_convert_quote,
        "bond_convert_premium": wrapper.get_bond_convert_premium,
        "bond_convert_analysis": wrapper.get_bond_convert_analysis,
        "bond_cb_index": wrapper.get_bond_cb_index_jsl,
        # Bond Issuance
        "bond_issue": wrapper.get_bond_issue_cninfo,
        "bond_issue_detail": wrapper.get_bond_issue_detail,
        "bond_issue_gov": wrapper.get_bond_issue_gov,
        "bond_issue_corp": wrapper.get_bond_issue_corp,
        "bond_issue_convert": wrapper.get_bond_issue_convert,
        "bond_issue_financial": wrapper.get_bond_issue_financial,
        "bond_issue_local": wrapper.get_bond_issue_local_gov,
        # Money Market
        "bond_money": wrapper.get_bond_china_money,
        "bond_money_market": wrapper.get_bond_money_market,
        "bond_repo": wrapper.get_bond_money_repo,
        "bond_shibor": wrapper.get_bond_money_shibor,
        "bond_lpr": wrapper.get_bond_money_lpr,
        # NAFMII
        "bond_nafmii": wrapper.get_bond_nafmii,
        "bond_nafmii_abn": wrapper.get_bond_nafmii_abn,
        "bond_nafmii_mtn": wrapper.get_bond_nafmii_mtn,
        "bond_nafmii_cp": wrapper.get_bond_nafmii_cp,
        "bond_nafmii_pn": wrapper.get_bond_nafmii_pn,
        "bond_nafmii_reg": wrapper.get_bond_nafmii_registrations,
        # Market Statistics
        "bond_stats_daily": wrapper.get_bond_stats_daily,
        "bond_stats_weekly": wrapper.get_bond_stats_weekly,
        "bond_stats_monthly": wrapper.get_bond_stats_monthly,
        "bond_stats_yearly": wrapper.get_bond_stats_yearly,
        "bond_turnover": wrapper.get_bond_turnover,
        "bond_liquidity": wrapper.get_bond_liquidity,
        # International Bonds
        "bond_us_treasury": wrapper.get_bond_us_treasury,
        "bond_us_yield": wrapper.get_bond_us_treasury_yield,
        "bond_us_corporate": wrapper.get_bond_us_corporate,
        "bond_us_municipal": wrapper.get_bond_us_municipal,
        "bond_europe": wrapper.get_bond_europe,
        "bond_japan": wrapper.get_bond_japan,
        "bond_uk_gilt": wrapper.get_bond_uk_gilt,
        "bond_germany_bund": wrapper.get_bond_germany_bund,
        # Bond Indices
        "bond_index_china": wrapper.get_bond_index_china,
        "bond_index_citic": wrapper.get_bond_index_citic,
        "bond_index_cyb": wrapper.get_bond_index_cyb,
        "bond_index_ths": wrapper.get_bond_index_ths,
        "bond_index_sw": wrapper.get_bond_index_sw,
        "bond_index_global": wrapper.get_bond_index_global,
        # Bond Ratings
        "bond_rating_domestic": wrapper.get_bond_rating_domestic,
        "bond_rating_international": wrapper.get_bond_rating_international,
        "bond_rating_agencies": wrapper.get_bond_rating_agencies,
        "bond_rating_changes": wrapper.get_bond_rating_changes
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