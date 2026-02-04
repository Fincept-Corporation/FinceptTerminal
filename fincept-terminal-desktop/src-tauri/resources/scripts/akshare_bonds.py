"""
AKShare Bonds Data Wrapper
Wrapper for bond market data (treasury, corporate, convertible bonds)
Returns JSON output for Rust integration

NOTE: Cleaned to contain only VALID akshare bond functions (33 total).
"""

import sys
import json
import pandas as pd
import akshare as ak
from typing import Dict, Any, List
from datetime import datetime, timedelta
from datetime import date


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


class BondsWrapper:
    """Bonds data wrapper with VALIDATED akshare functions"""

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

            except ValueError as e:
                error_msg = str(e)
                # Handle AKShare API bugs gracefully
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
                    import time
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
                    import time
                    time.sleep(self.retry_delay * 2)
                continue
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

    # ==================== CONVERTIBLE BONDS ====================

    def get_bond_cb_jsl(self) -> Dict[str, Any]:
        """Get convertible bonds list from JSL"""
        return self._safe_call_with_retry(ak.bond_cb_jsl)

    def get_bond_cb_index_jsl(self) -> Dict[str, Any]:
        """Get convertible bond index from JSL"""
        return self._safe_call_with_retry(ak.bond_cb_index_jsl)

    def get_bond_cb_redeem_jsl(self) -> Dict[str, Any]:
        """Get convertible bond redemption data from JSL"""
        return self._safe_call_with_retry(ak.bond_cb_redeem_jsl)

    def get_bond_cb_adj_logs_jsl(self) -> Dict[str, Any]:
        """Get convertible bond adjustment logs from JSL"""
        return self._safe_call_with_retry(ak.bond_cb_adj_logs_jsl)

    def get_bond_cb_profile_sina(self) -> Dict[str, Any]:
        """Get convertible bond profile from Sina"""
        return self._safe_call_with_retry(ak.bond_cb_profile_sina)

    def get_bond_cb_summary_sina(self) -> Dict[str, Any]:
        """Get convertible bond summary from Sina"""
        return self._safe_call_with_retry(ak.bond_cb_summary_sina)

    def get_bond_zh_cov(self) -> Dict[str, Any]:
        """Get convertible bond list (China)"""
        return self._safe_call_with_retry(ak.bond_zh_cov)

    def get_bond_zh_cov_info(self) -> Dict[str, Any]:
        """Get convertible bond info (China)"""
        return self._safe_call_with_retry(ak.bond_zh_cov_info)

    def get_bond_zh_cov_info_ths(self) -> Dict[str, Any]:
        """Get convertible bond info from THS"""
        return self._safe_call_with_retry(ak.bond_zh_cov_info_ths)

    def get_bond_zh_cov_spot(self) -> Dict[str, Any]:
        """Get convertible bond spot prices (China)"""
        return self._safe_call_with_retry(ak.bond_zh_hs_cov_spot)

    def get_bond_zh_cov_daily(self) -> Dict[str, Any]:
        """Get convertible bond daily data (China)"""
        return self._safe_call_with_retry(ak.bond_zh_hs_cov_daily)

    def get_bond_zh_cov_min(self) -> Dict[str, Any]:
        """Get convertible bond minute data"""
        return self._safe_call_with_retry(ak.bond_zh_hs_cov_min)

    def get_bond_zh_cov_pre_min(self) -> Dict[str, Any]:
        """Get convertible bond pre-market minute data"""
        return self._safe_call_with_retry(ak.bond_zh_hs_cov_pre_min)

    def get_bond_zh_cov_value_analysis(self) -> Dict[str, Any]:
        """Get convertible bond value analysis"""
        return self._safe_call_with_retry(ak.bond_zh_cov_value_analysis)

    # ==================== GOVERNMENT BONDS ====================

    def get_bond_treasure_issue_cninfo(self) -> Dict[str, Any]:
        """Get treasury bond issuance data from CNInfo"""
        return self._safe_call_with_retry(ak.bond_treasure_issue_cninfo)

    def get_bond_local_government_issue_cninfo(self) -> Dict[str, Any]:
        """Get local government bond issuance from CNInfo"""
        return self._safe_call_with_retry(ak.bond_local_government_issue_cninfo)

    # ==================== CORPORATE BONDS ====================

    def get_bond_corporate_issue_cninfo(self) -> Dict[str, Any]:
        """Get corporate bond issuance from CNInfo"""
        return self._safe_call_with_retry(ak.bond_corporate_issue_cninfo)

    def get_bond_cov_issue_cninfo(self) -> Dict[str, Any]:
        """Get convertible bond issuance from CNInfo"""
        return self._safe_call_with_retry(ak.bond_cov_issue_cninfo)

    def get_bond_cov_stock_issue_cninfo(self) -> Dict[str, Any]:
        """Get convertible bond stock issuance from CNInfo"""
        return self._safe_call_with_retry(ak.bond_cov_stock_issue_cninfo)

    # ==================== BOND MARKET DATA ====================

    def get_bond_zh_hs_daily(self) -> Dict[str, Any]:
        """Get bond daily data (Shanghai/Shenzhen)"""
        return self._safe_call_with_retry(ak.bond_zh_hs_daily)

    def get_bond_spot_deal(self) -> Dict[str, Any]:
        """Get bond spot deal data"""
        return self._safe_call_with_retry(ak.bond_spot_deal)

    def get_bond_spot_quote(self) -> Dict[str, Any]:
        """Get bond spot quotes"""
        return self._safe_call_with_retry(ak.bond_spot_quote)

    # ==================== BOND BUYBACK ====================

    def get_bond_buy_back_hist_em(self) -> Dict[str, Any]:
        """Get bond buyback history from EM"""
        return self._safe_call_with_retry(ak.bond_buy_back_hist_em)

    def get_bond_sh_buy_back_em(self) -> Dict[str, Any]:
        """Get Shanghai bond buyback from EM"""
        return self._safe_call_with_retry(ak.bond_sh_buy_back_em)

    def get_bond_sz_buy_back_em(self) -> Dict[str, Any]:
        """Get Shenzhen bond buyback from EM"""
        return self._safe_call_with_retry(ak.bond_sz_buy_back_em)

    # ==================== EXCHANGE DATA ====================

    def get_bond_cash_summary_sse(self) -> Dict[str, Any]:
        """Get bond cash summary from SSE"""
        return self._safe_call_with_retry(ak.bond_cash_summary_sse)

    def get_bond_deal_summary_sse(self) -> Dict[str, Any]:
        """Get bond deal summary from SSE"""
        return self._safe_call_with_retry(ak.bond_deal_summary_sse)

    # ==================== BOND INFO ====================

    def get_bond_debt_nafmii(self) -> Dict[str, Any]:
        """Get bond debt data from NAFMII"""
        return self._safe_call_with_retry(ak.bond_debt_nafmii)

    def get_amac_person_bond_org_list(self) -> Dict[str, Any]:
        """Get AMAC person bond organization list"""
        return self._safe_call_with_retry(ak.amac_person_bond_org_list)

    def get_fund_portfolio_bond_hold_em(self) -> Dict[str, Any]:
        """Get fund portfolio bond holdings from EM"""
        return self._safe_call_with_retry(ak.fund_portfolio_bond_hold_em)

    def get_macro_china_bond_public(self) -> Dict[str, Any]:
        """Get China macro bond public data"""
        return self._safe_call_with_retry(ak.macro_china_bond_public)

    # ==================== UTILITY ====================

    def get_all_available_endpoints(self) -> Dict[str, Any]:
        """Get list of all available endpoints"""
        endpoints = [
            "bond_cb_jsl", "bond_cb_index_jsl", "bond_cb_redeem_jsl",
            "bond_cb_profile_sina", "bond_cb_summary_sina", "bond_zh_cov_spot",
            "bond_treasure_issue_cninfo", "bond_local_government_issue_cninfo",
            "bond_corporate_issue_cninfo", "bond_cov_issue_cninfo",
            "bond_zh_hs_daily", "bond_spot_deal", "bond_spot_quote",
            "bond_buy_back_hist_em", "bond_sh_buy_back_em", "bond_sz_buy_back_em",
            "bond_cash_summary_sse", "bond_deal_summary_sse",
            "bond_debt_nafmii"
        ]

        return {
            "success": True,
            "data": {
                "available_endpoints": endpoints,
                "total_count": len(endpoints),
                "categories": {
                    "Chinese Bond Market": ["bond_cb_jsl", "bond_cb_index_jsl", "bond_zh_cov_spot", "bond_zh_hs_daily"],
                    "Government Bonds": ["bond_treasure_issue_cninfo", "bond_local_government_issue_cninfo"],
                    "Corporate Bonds": ["bond_corporate_issue_cninfo", "bond_cov_issue_cninfo"],
                    "Convertible Bonds": ["bond_cb_profile_sina", "bond_cb_summary_sina"],
                },
                "timestamp": int(datetime.now().timestamp())
            },
            "count": len(endpoints),
            "timestamp": int(datetime.now().timestamp())
        }


# ==================== CLI ====================
def main():
    import sys
    wrapper = BondsWrapper()

    if len(sys.argv) < 2:
        print(json.dumps({"error": "Usage: python akshare_bonds.py <endpoint>"}))
        return

    endpoint = sys.argv[1]

    # Dynamic endpoint resolution - check if method exists
    if endpoint == "get_all_endpoints":
        result = wrapper.get_all_available_endpoints()
        print(json.dumps(result, ensure_ascii=True, cls=DateTimeEncoder))
        return

    # Try to find the method on the wrapper
    method_name = f"get_{endpoint}" if not endpoint.startswith("get_") else endpoint

    if hasattr(wrapper, method_name):
        method = getattr(wrapper, method_name)
        result = method()
        print(json.dumps(result, ensure_ascii=True, cls=DateTimeEncoder))
    else:
        print(json.dumps({"error": f"Unknown endpoint: {endpoint}. Method '{method_name}' not found."}))

if __name__ == "__main__":
    main()
