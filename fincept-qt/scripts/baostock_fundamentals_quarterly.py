#!/usr/bin/env python3
"""
BaoStock Fundamentals Quarterly
Incremental quarterly fetch for profit/growth/balance/cashflow/dupont/operation.
"""

import io
import json
import sys
from datetime import datetime
from pathlib import Path
from typing import Any, Callable, Dict, List, Tuple

import pandas as pd

try:
    import baostock as bs
except ImportError as e:
    print(json.dumps({"success": False, "error": f"Missing dependency: {e}", "data": []}))
    sys.exit(1)


DATA_ROOT = Path(__file__).resolve().parent / "data" / "baostock" / "fundamentals_quarterly"
STATE_FILE = DATA_ROOT / "state.json"


def _now_ts() -> int:
    return int(datetime.now().timestamp())


def _to_bool(value: Any) -> bool:
    return str(value).strip().lower() in {"1", "true", "yes", "y", "on"}


def _json(data: Dict[str, Any]) -> None:
    print(json.dumps(data, ensure_ascii=True, default=str))


def _period_key(year: int, quarter: int) -> str:
    return f"{year}Q{quarter}"


def _next_period(year: int, quarter: int) -> Tuple[int, int]:
    if quarter >= 4:
        return year + 1, 1
    return year, quarter + 1


def _period_range(start_year: int, start_quarter: int, end_year: int, end_quarter: int) -> List[Tuple[int, int]]:
    out: List[Tuple[int, int]] = []
    y, q = start_year, start_quarter
    while (y < end_year) or (y == end_year and q <= end_quarter):
        out.append((y, q))
        y, q = _next_period(y, q)
    return out


class BaoStockFundamentalsQuarterly:
    def __init__(self) -> None:
        self._logged_in = False
        DATA_ROOT.mkdir(parents=True, exist_ok=True)
        self.query_map: Dict[str, Callable[..., Any]] = {
            "profit": bs.query_profit_data,
            "growth": bs.query_growth_data,
            "balance": bs.query_balance_data,
            "cash_flow": bs.query_cash_flow_data,
            "dupont": bs.query_dupont_data,
            "operation": bs.query_operation_data,
        }

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

    @staticmethod
    def _query_to_df(rs) -> pd.DataFrame:
        if rs.error_code != "0":
            raise RuntimeError(rs.error_msg)
        rows: List[List[str]] = []
        while rs.next():
            rows.append(rs.get_row_data())
        return pd.DataFrame(rows, columns=rs.fields)

    def _state(self) -> Dict[str, Any]:
        if not STATE_FILE.exists():
            return {"completed": {}, "updated_at": _now_ts()}
        try:
            return json.loads(STATE_FILE.read_text(encoding="utf-8"))
        except Exception:
            return {"completed": {}, "updated_at": _now_ts()}

    def _save_state(self, state: Dict[str, Any]) -> None:
        state["updated_at"] = _now_ts()
        STATE_FILE.write_text(json.dumps(state, ensure_ascii=True, indent=2), encoding="utf-8")

    def _codes(self, day: str = "") -> List[str]:
        rs = bs.query_all_stock(day=day or None)
        df = self._query_to_df(rs)
        if df.empty:
            return []
        codes = df["code"].astype(str).tolist()
        return [code for code in codes if self._is_equity_code(code)]

    @staticmethod
    def _is_equity_code(code: str) -> bool:
        return code.startswith(
            (
                "sh.600",
                "sh.601",
                "sh.603",
                "sh.605",
                "sh.688",
                "sz.000",
                "sz.001",
                "sz.002",
                "sz.003",
                "sz.300",
                "bj.",
            )
        )

    def _fetch_one(self, name: str, code: str, year: int, quarter: int) -> pd.DataFrame:
        rs = self.query_map[name](code=code, year=str(year), quarter=str(quarter))
        df = self._query_to_df(rs)
        if not df.empty:
            df["source_code"] = code
            df["fiscal_year"] = year
            df["fiscal_quarter"] = quarter
        return df

    @staticmethod
    def _normalize_code(code: str) -> str:
        c = str(code).strip()
        if "." in c:
            return c
        digits = "".join(ch for ch in c if ch.isdigit())[:6].zfill(6)
        if digits.startswith(("5", "6", "9")):
            return f"sh.{digits}"
        return f"sz.{digits}"

    def run_quarter(
        self,
        year: int,
        quarter: int,
        trade_date: str = "",
        symbol_limit: int = 0,
        force: bool = False,
        codes_csv: str = "",
    ) -> Dict[str, Any]:
        self._login()
        state = self._state()
        period = _period_key(year, quarter)
        state["completed"].setdefault(period, {})

        if codes_csv.strip():
            codes = [self._normalize_code(x) for x in codes_csv.split(",") if x.strip()]
        else:
            codes = self._codes(trade_date)
        if symbol_limit > 0:
            codes = codes[:symbol_limit]

        result_summary: Dict[str, Any] = {"period": period, "symbols": len(codes), "datasets": {}}
        for dataset in self.query_map:
            out_file = DATA_ROOT / f"{dataset}_{period}.csv"
            if out_file.exists() and state["completed"][period].get(dataset) and not force:
                result_summary["datasets"][dataset] = {
                    "status": "skipped_existing",
                    "file": str(out_file),
                }
                continue

            parts: List[pd.DataFrame] = []
            failures = 0
            for code in codes:
                try:
                    df = self._fetch_one(dataset, code, year, quarter)
                    if not df.empty:
                        parts.append(df)
                except Exception:
                    failures += 1

            if parts:
                out_df = pd.concat(parts, ignore_index=True)
                subset = [col for col in ["source_code", "statDate", "pubDate"] if col in out_df.columns]
                if subset:
                    out_df = out_df.drop_duplicates(subset=subset)
                out_df.to_csv(out_file, index=False, encoding="utf-8")
                rows_saved = len(out_df)
            else:
                pd.DataFrame().to_csv(out_file, index=False, encoding="utf-8")
                rows_saved = 0

            state["completed"][period][dataset] = {
                "file": str(out_file),
                "rows_saved": rows_saved,
                "failures": failures,
                "updated_at": _now_ts(),
            }
            result_summary["datasets"][dataset] = {
                "status": "done",
                "rows_saved": rows_saved,
                "failures": failures,
                "file": str(out_file),
            }

        self._save_state(state)
        return {"success": True, "data": result_summary, "timestamp": _now_ts()}

    def run_range(
        self,
        start_year: int,
        start_quarter: int,
        end_year: int,
        end_quarter: int,
        trade_date: str = "",
        symbol_limit: int = 0,
        force: bool = False,
        codes_csv: str = "",
    ) -> Dict[str, Any]:
        periods = _period_range(start_year, start_quarter, end_year, end_quarter)
        summaries: List[Dict[str, Any]] = []
        for year, quarter in periods:
            summaries.append(
                self.run_quarter(year, quarter, trade_date, symbol_limit, force, codes_csv)["data"]
            )
        return {
            "success": True,
            "data": {
                "periods": [f"{y}Q{q}" for y, q in periods],
                "count": len(periods),
                "summaries": summaries,
            },
            "timestamp": _now_ts(),
        }

    def get_state(self) -> Dict[str, Any]:
        return {"success": True, "data": self._state(), "timestamp": _now_ts()}

    def reset_state(self) -> Dict[str, Any]:
        if STATE_FILE.exists():
            STATE_FILE.unlink()
        return {"success": True, "data": {"status": "state_reset"}, "timestamp": _now_ts()}

    def get_all_endpoints(self) -> Dict[str, Any]:
        endpoints = [
            "get_all_endpoints",
            "run_quarter",
            "run_range",
            "get_state",
            "reset_state",
        ]
        return {
            "success": True,
            "data": {
                "available_endpoints": endpoints,
                "datasets": list(self.query_map.keys()),
                "total_count": len(endpoints),
            },
            "count": len(endpoints),
            "timestamp": _now_ts(),
        }


def main() -> None:
    if sys.platform == "win32":
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")

    wrapper = BaoStockFundamentalsQuarterly()
    endpoint_map = {
        "get_all_endpoints": wrapper.get_all_endpoints,
        "run_quarter": wrapper.run_quarter,
        "run_range": wrapper.run_range,
        "get_state": wrapper.get_state,
        "reset_state": wrapper.reset_state,
    }

    try:
        if len(sys.argv) < 2:
            _json(
                {
                    "success": False,
                    "error": "Usage: python baostock_fundamentals_quarterly.py <endpoint> [args...]",
                    "available_endpoints": list(endpoint_map.keys()),
                }
            )
            return

        endpoint = sys.argv[1]
        args = sys.argv[2:]
        method = endpoint_map.get(endpoint)
        if method is None:
            _json(
                {
                    "success": False,
                    "error": f"Unknown endpoint: {endpoint}",
                    "available_endpoints": list(endpoint_map.keys()),
                }
            )
            return

        if endpoint == "run_quarter":
            if len(args) < 2:
                _json({"success": False, "error": "run_quarter requires: <year> <quarter>"})
                return
            year = int(args[0])
            quarter = int(args[1])
            trade_date = args[2] if len(args) > 2 else ""
            symbol_limit = int(args[3]) if len(args) > 3 and args[3] else 0
            force = _to_bool(args[4]) if len(args) > 4 else False
            codes_csv = args[5] if len(args) > 5 else ""
            result = method(year, quarter, trade_date, symbol_limit, force, codes_csv)
        elif endpoint == "run_range":
            if len(args) < 4:
                _json(
                    {
                        "success": False,
                        "error": "run_range requires: <start_year> <start_quarter> <end_year> <end_quarter>",
                    }
                )
                return
            start_year = int(args[0])
            start_quarter = int(args[1])
            end_year = int(args[2])
            end_quarter = int(args[3])
            trade_date = args[4] if len(args) > 4 else ""
            symbol_limit = int(args[5]) if len(args) > 5 and args[5] else 0
            force = _to_bool(args[6]) if len(args) > 6 else False
            codes_csv = args[7] if len(args) > 7 else ""
            result = method(
                start_year,
                start_quarter,
                end_year,
                end_quarter,
                trade_date,
                symbol_limit,
                force,
                codes_csv,
            )
        else:
            result = method()

        _json(result)
    except Exception as e:  # pragma: no cover - defensive
        _json({"success": False, "error": str(e), "data": [], "timestamp": _now_ts()})
    finally:
        wrapper._logout()


if __name__ == "__main__":
    main()
