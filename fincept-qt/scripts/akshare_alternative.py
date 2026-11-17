"""
AKShare Alternative Data Wrapper
Wrapper for alternative and specialized data sources
Returns JSON output for Qt/C++ integration

NOTE: This file has been cleaned to contain only VALID akshare functions.
Many placeholder functions were removed as they don't exist in akshare 1.18.20.
"""

import sys
import json
import time
import pandas as pd
import akshare as ak

# safe_call: one akshare call -> {success, data, columns, count} or a plain-English error;
# run_wrapper_cli: get_all_endpoints / <endpoint> [field=value ...] -> one JSON line on stdout.
from akshare_screen_args import run_wrapper_cli, safe_call
from typing import Dict, Any, List
from datetime import datetime, timedelta, date


class AlternativeDataWrapper:
    """Alternative data wrapper with VALIDATED akshare functions"""

    def __init__(self):
        self.default_timeout = 15
        self.retry_delay = 2
        self.default_start_date = (datetime.now() - timedelta(days=365)).strftime('%Y%m%d')
        self.default_end_date = datetime.now().strftime('%Y%m%d')

    def _safe_call_with_retry(self, func, *args, max_retries: int = 2, **kwargs) -> Dict[str, Any]:
        """One akshare call -> {success, data, columns, count} or a plain-English error (shared
        akshare_screen_args.safe_call: only a dropped connection or a timeout is retried, never a
        rate limit or a refusal)."""
        return safe_call(func, *args, _retries=max_retries, **kwargs)

    # ==================== ENERGY ====================


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
        # Gasgoo's ranking left akshare; CPCA's manufacturer sales ranking (passenger cars, year to date,
        # wholesale) is the same kind of table.
        return self._safe_call_with_retry(ak.car_market_man_rank_cpca, symbol="狭义乘用车-累计", indicator="批发")

    # ==================== UTILITY FUNCTIONS ====================

    def get_all_available_endpoints(self) -> Dict[str, Any]:
        """Get list of all available endpoints"""
        endpoints = [
            "energy_oil_hist", "energy_oil_detail",
            "movie_boxoffice_daily", "movie_boxoffice_monthly",
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

def main():
    wrapper = AlternativeDataWrapper()
    # One JSON line on stdout (NaN -> null, dates as ISO text); akshare's own prints go to stderr.
    return run_wrapper_cli(wrapper, wrapper.get_all_available_endpoints, ascii=True, iso_dates=True)


if __name__ == "__main__":
    sys.exit(main())
