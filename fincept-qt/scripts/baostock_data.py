#!/usr/bin/env python3
"""
BaoStock Data Wrapper
Provides a CLI-style endpoint interface with JSON output for Fincept Terminal.
"""

import io
import json
import sys
import time
from datetime import datetime
from typing import Any, Dict, List

try:
    import baostock as bs
    import pandas as pd
except ImportError as e:
    print(
        json.dumps(
            {
                "success": False,
                "error": f"Missing dependency: {e}",
                "data": [],
            },
            ensure_ascii=True,
        )
    )
    sys.exit(1)


def _now_ts() -> int:
    return int(datetime.now().timestamp())


class BaoStockWrapper:
    """BaoStock endpoint wrapper with safe execution and retries."""

    def __init__(self) -> None:
        self._logged_in = False

    def _login(self) -> None:
        if self._logged_in:
            return
        lg = bs.login()
        if lg.error_code != "0":
            raise RuntimeError(f"BaoStock login failed: {lg.error_msg}")
        self._logged_in = True

    def _logout(self) -> None:
        if self._logged_in:
            try:
                bs.logout()
            finally:
                self._logged_in = False

    def _normalize_df(self, df: pd.DataFrame) -> List[Dict[str, Any]]:
        if df.empty:
            return []
        for col in df.columns:
            if pd.api.types.is_datetime64_any_dtype(df[col]):
                df[col] = df[col].astype(str)
        df = df.replace([float("inf"), float("-inf")], None)
        df = df.where(pd.notna(df), None)
        return df.to_dict(orient="records")

    def _query_to_df(self, rs) -> pd.DataFrame:
        if rs.error_code != "0":
            raise RuntimeError(rs.error_msg)
        rows: List[List[str]] = []
        while rs.next():
            rows.append(rs.get_row_data())
        return pd.DataFrame(rows, columns=rs.fields)

    def _safe_call(self, func, *args, **kwargs) -> Dict[str, Any]:
        max_retries = 2
        for attempt in range(max_retries):
            try:
                self._login()
                result = func(*args, **kwargs)
                if isinstance(result, pd.DataFrame):
                    data = self._normalize_df(result)
                    return {
                        "success": True,
                        "data": data,
                        "count": len(data),
                        "timestamp": _now_ts(),
                    }
                if isinstance(result, (list, dict)):
                    count = len(result) if isinstance(result, list) else 1
                    return {
                        "success": True,
                        "data": result,
                        "count": count,
                        "timestamp": _now_ts(),
                    }
                return {
                    "success": True,
                    "data": str(result),
                    "count": 1,
                    "timestamp": _now_ts(),
                }
            except Exception as e:  # pragma: no cover - defensive
                if attempt < max_retries - 1:
                    self._logout()
                    time.sleep(1)
                    continue
                return {
                    "success": False,
                    "error": str(e),
                    "data": [],
                    "timestamp": _now_ts(),
                }
        return {
            "success": False,
            "error": "Max retries exceeded",
            "data": [],
            "timestamp": _now_ts(),
        }

    def _query_safe(self, query_func, *args, **kwargs) -> Dict[str, Any]:
        def _inner():
            rs = query_func(*args, **kwargs)
            return self._query_to_df(rs)

        return self._safe_call(_inner)

    # -------- Endpoints --------

    def get_stock_basic(self, code: str = "", code_name: str = "") -> Dict[str, Any]:
        return self._query_safe(bs.query_stock_basic, code=code, code_name=code_name)

    def get_trade_dates(
        self, start_date: str = "2024-01-01", end_date: str = "2024-12-31"
    ) -> Dict[str, Any]:
        return self._query_safe(
            bs.query_trade_dates, start_date=start_date, end_date=end_date
        )

    def get_hs300_stocks(self, date: str = "") -> Dict[str, Any]:
        return self._query_safe(bs.query_hs300_stocks, date=date)

    def get_sz50_stocks(self, date: str = "") -> Dict[str, Any]:
        return self._query_safe(bs.query_sz50_stocks, date=date)

    def get_zz500_stocks(self, date: str = "") -> Dict[str, Any]:
        return self._query_safe(bs.query_zz500_stocks, date=date)

    def get_all_stock(self, day: str = "") -> Dict[str, Any]:
        return self._query_safe(bs.query_all_stock, day=day or None)

    def get_stock_industry(self, date: str = "") -> Dict[str, Any]:
        return self._query_safe(bs.query_stock_industry, code="", date=date)

    def get_stock_profit_data(
        self, code: str, year: str = "2024", quarter: str = "4"
    ) -> Dict[str, Any]:
        return self._query_safe(
            bs.query_profit_data, code=code, year=year, quarter=quarter
        )

    def get_stock_growth_data(
        self, code: str, year: str = "2024", quarter: str = "4"
    ) -> Dict[str, Any]:
        return self._query_safe(
            bs.query_growth_data, code=code, year=year, quarter=quarter
        )

    def get_stock_balance_data(
        self, code: str, year: str = "2024", quarter: str = "4"
    ) -> Dict[str, Any]:
        return self._query_safe(
            bs.query_balance_data, code=code, year=year, quarter=quarter
        )

    def get_stock_cash_flow_data(
        self, code: str, year: str = "2024", quarter: str = "4"
    ) -> Dict[str, Any]:
        return self._query_safe(
            bs.query_cash_flow_data, code=code, year=year, quarter=quarter
        )

    def get_stock_dupont_data(
        self, code: str, year: str = "2024", quarter: str = "4"
    ) -> Dict[str, Any]:
        return self._query_safe(
            bs.query_dupont_data, code=code, year=year, quarter=quarter
        )

    def get_stock_operation_data(
        self, code: str, year: str = "2024", quarter: str = "4"
    ) -> Dict[str, Any]:
        return self._query_safe(
            bs.query_operation_data, code=code, year=year, quarter=quarter
        )

    def get_stock_dividend_data(
        self, code: str, year: str = "2024", year_type: str = "report"
    ) -> Dict[str, Any]:
        return self._query_safe(
            bs.query_dividend_data, code=code, year=year, yearType=year_type
        )

    def get_stock_adjust_factor(
        self, code: str, start_date: str = "", end_date: str = ""
    ) -> Dict[str, Any]:
        return self._query_safe(
            bs.query_adjust_factor,
            code=code,
            start_date=start_date or None,
            end_date=end_date or None,
        )

    def get_stock_performance_express_report(
        self, code: str, start_date: str = "", end_date: str = ""
    ) -> Dict[str, Any]:
        return self._query_safe(
            bs.query_performance_express_report,
            code=code,
            start_date=start_date or None,
            end_date=end_date or None,
        )

    def get_stock_forecast_report(
        self, code: str, start_date: str = "", end_date: str = ""
    ) -> Dict[str, Any]:
        return self._query_safe(
            bs.query_forecast_report,
            code=code,
            start_date=start_date or None,
            end_date=end_date or None,
        )

    def get_stock_k_data(
        self,
        code: str,
        start_date: str = "2024-01-01",
        end_date: str = "2024-12-31",
        frequency: str = "d",
        adjustflag: str = "3",
    ) -> Dict[str, Any]:
        fields = (
            "date,code,open,high,low,close,preclose,volume,amount,adjustflag,"
            "turn,tradestatus,pctChg,isST"
        )
        return self._query_safe(
            bs.query_history_k_data_plus,
            code=code,
            fields=fields,
            start_date=start_date,
            end_date=end_date,
            frequency=frequency,
            adjustflag=adjustflag,
        )

    def get_deposit_rate_data(
        self, start_date: str = "", end_date: str = ""
    ) -> Dict[str, Any]:
        return self._query_safe(
            bs.query_deposit_rate_data, start_date=start_date, end_date=end_date
        )

    def get_loan_rate_data(self, start_date: str = "", end_date: str = "") -> Dict[str, Any]:
        return self._query_safe(
            bs.query_loan_rate_data, start_date=start_date, end_date=end_date
        )

    def get_required_reserve_ratio_data(
        self, start_date: str = "", end_date: str = "", year_type: str = "0"
    ) -> Dict[str, Any]:
        return self._query_safe(
            bs.query_required_reserve_ratio_data,
            start_date=start_date,
            end_date=end_date,
            yearType=year_type,
        )

    def get_money_supply_data_month(
        self, start_date: str = "", end_date: str = ""
    ) -> Dict[str, Any]:
        return self._query_safe(
            bs.query_money_supply_data_month, start_date=start_date, end_date=end_date
        )

    def get_money_supply_data_year(
        self, start_date: str = "", end_date: str = ""
    ) -> Dict[str, Any]:
        return self._query_safe(
            bs.query_money_supply_data_year, start_date=start_date, end_date=end_date
        )

    def get_all_endpoints(self) -> Dict[str, Any]:
        categories = {
            "Meta": ["get_all_endpoints"],
            "Market": [
                "stock_basic",
                "trade_dates",
                "hs300_stocks",
                "sz50_stocks",
                "zz500_stocks",
                "all_stock",
                "stock_industry",
                "stock_k_data",
                "stock_adjust_factor",
            ],
            "Fundamental": [
                "stock_profit_data",
                "stock_growth_data",
                "stock_balance_data",
                "stock_cash_flow_data",
                "stock_dupont_data",
                "stock_operation_data",
                "stock_dividend_data",
                "stock_performance_express_report",
                "stock_forecast_report",
            ],
            "Macro": [
                "deposit_rate_data",
                "loan_rate_data",
                "required_reserve_ratio_data",
                "money_supply_data_month",
                "money_supply_data_year",
            ],
        }
        endpoints = [ep for items in categories.values() for ep in items]
        return {
            "success": True,
            "data": {
                "available_endpoints": endpoints,
                "total_count": len(endpoints),
                "categories": categories,
            },
            "count": len(endpoints),
            "timestamp": _now_ts(),
        }


def main() -> None:
    if sys.platform == "win32":
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")

    wrapper = BaoStockWrapper()

    endpoint_map = {
        "get_all_endpoints": wrapper.get_all_endpoints,
        "stock_basic": wrapper.get_stock_basic,
        "trade_dates": wrapper.get_trade_dates,
        "hs300_stocks": wrapper.get_hs300_stocks,
        "sz50_stocks": wrapper.get_sz50_stocks,
        "zz500_stocks": wrapper.get_zz500_stocks,
        "all_stock": wrapper.get_all_stock,
        "stock_industry": wrapper.get_stock_industry,
        "stock_profit_data": wrapper.get_stock_profit_data,
        "stock_growth_data": wrapper.get_stock_growth_data,
        "stock_balance_data": wrapper.get_stock_balance_data,
        "stock_cash_flow_data": wrapper.get_stock_cash_flow_data,
        "stock_dupont_data": wrapper.get_stock_dupont_data,
        "stock_operation_data": wrapper.get_stock_operation_data,
        "stock_dividend_data": wrapper.get_stock_dividend_data,
        "stock_adjust_factor": wrapper.get_stock_adjust_factor,
        "stock_performance_express_report": wrapper.get_stock_performance_express_report,
        "stock_forecast_report": wrapper.get_stock_forecast_report,
        "stock_k_data": wrapper.get_stock_k_data,
        "deposit_rate_data": wrapper.get_deposit_rate_data,
        "loan_rate_data": wrapper.get_loan_rate_data,
        "required_reserve_ratio_data": wrapper.get_required_reserve_ratio_data,
        "money_supply_data_month": wrapper.get_money_supply_data_month,
        "money_supply_data_year": wrapper.get_money_supply_data_year,
    }

    try:
        if len(sys.argv) < 2:
            print(
                json.dumps(
                    {
                        "success": False,
                        "error": "Usage: python baostock_data.py <endpoint> [args...]",
                        "available_endpoints": list(endpoint_map.keys()),
                    },
                    ensure_ascii=True,
                )
            )
            return

        endpoint = sys.argv[1]
        args = sys.argv[2:]

        method = endpoint_map.get(endpoint)
        if method is None:
            print(
                json.dumps(
                    {
                        "success": False,
                        "error": f"Unknown endpoint: {endpoint}",
                        "available_endpoints": list(endpoint_map.keys()),
                    },
                    ensure_ascii=True,
                )
            )
            return

        result = method(*args) if args else method()
        print(json.dumps(result, ensure_ascii=True, default=str))
    finally:
        wrapper._logout()


if __name__ == "__main__":
    main()
