#!/usr/bin/env python3
"""
BaoStock Corporate Actions
Collects dividend and adjust-factor datasets incrementally.
"""

import io
import json
import sys
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List

import pandas as pd

try:
    import baostock as bs
except ImportError as e:
    print(json.dumps({"success": False, "error": f"Missing dependency: {e}", "data": []}))
    sys.exit(1)


DATA_ROOT = Path(__file__).resolve().parent / "data" / "baostock" / "corporate_actions"
STATE_FILE = DATA_ROOT / "state.json"
ADJUST_DIR = DATA_ROOT / "adjust_factor"
DIVIDEND_DIR = DATA_ROOT / "dividend"


def _now_ts() -> int:
    return int(datetime.now().timestamp())


def _to_bool(value: Any) -> bool:
    return str(value).strip().lower() in {"1", "true", "yes", "y", "on"}


def _json(data: Dict[str, Any]) -> None:
    print(json.dumps(data, ensure_ascii=True, default=str))


def _safe_code_for_file(code: str) -> str:
    return code.replace(".", "_")


class BaoStockCorporateActions:
    def __init__(self) -> None:
        self._logged_in = False
        ADJUST_DIR.mkdir(parents=True, exist_ok=True)
        DIVIDEND_DIR.mkdir(parents=True, exist_ok=True)

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
            return {"runs": [], "updated_at": _now_ts()}
        try:
            return json.loads(STATE_FILE.read_text(encoding="utf-8"))
        except Exception:
            return {"runs": [], "updated_at": _now_ts()}

    def _save_state(self, state: Dict[str, Any]) -> None:
        state["updated_at"] = _now_ts()
        STATE_FILE.write_text(json.dumps(state, ensure_ascii=True, indent=2), encoding="utf-8")

    def _codes(self, trade_date: str = "", codes_csv: str = "", symbol_limit: int = 0) -> List[str]:
        if codes_csv:
            codes = [x.strip() for x in codes_csv.split(",") if x.strip()]
        else:
            rs = bs.query_all_stock(day=trade_date or None)
            df = self._query_to_df(rs)
            raw_codes = [] if df.empty else df["code"].astype(str).tolist()
            codes = [code for code in raw_codes if self._is_equity_code(code)]
        if symbol_limit > 0:
            codes = codes[:symbol_limit]
        return codes

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

    @staticmethod
    def _merge_write_csv(path: Path, new_df: pd.DataFrame, subset: List[str]) -> int:
        if path.exists():
            old_df = pd.read_csv(path, dtype=str)
            merged = pd.concat([old_df, new_df], ignore_index=True)
        else:
            merged = new_df
        subset_existing = [c for c in subset if c in merged.columns]
        if subset_existing:
            merged = merged.drop_duplicates(subset=subset_existing)
        else:
            merged = merged.drop_duplicates()
        merged.to_csv(path, index=False, encoding="utf-8")
        return len(merged)

    def run(
        self,
        start_date: str,
        end_date: str,
        trade_date: str = "",
        codes_csv: str = "",
        symbol_limit: int = 0,
        year_type: str = "report",
        force: bool = False,
    ) -> Dict[str, Any]:
        self._login()

        start_year = int(start_date[:4])
        end_year = int(end_date[:4])
        years = list(range(start_year, end_year + 1))
        codes = self._codes(trade_date=trade_date, codes_csv=codes_csv, symbol_limit=symbol_limit)

        adjust_rows_saved = 0
        dividend_rows_saved = 0
        failures = 0

        for code in codes:
            safe_code = _safe_code_for_file(code)

            try:
                rs_adjust = bs.query_adjust_factor(code=code, start_date=start_date, end_date=end_date)
                adjust_df = self._query_to_df(rs_adjust)
                if not adjust_df.empty:
                    adjust_df["source_code"] = code
                    adjust_file = ADJUST_DIR / f"{safe_code}.csv"
                    if force and adjust_file.exists():
                        adjust_file.unlink()
                    adjust_rows_saved += self._merge_write_csv(
                        adjust_file,
                        adjust_df,
                        ["source_code", "dividOperateDate"],
                    )
            except Exception:
                failures += 1

            for year in years:
                try:
                    rs_div = bs.query_dividend_data(code=code, year=str(year), yearType=year_type)
                    div_df = self._query_to_df(rs_div)
                    if div_df.empty:
                        continue
                    div_df["source_code"] = code
                    div_df["dividend_year"] = year
                    div_file = DIVIDEND_DIR / f"{safe_code}_{year}.csv"
                    if force and div_file.exists():
                        div_file.unlink()
                    dividend_rows_saved += self._merge_write_csv(
                        div_file,
                        div_df,
                        ["source_code", "dividend_year", "dividOperateDate"],
                    )
                except Exception:
                    failures += 1

        state = self._state()
        state["runs"].append(
            {
                "start_date": start_date,
                "end_date": end_date,
                "year_type": year_type,
                "symbols": len(codes),
                "adjust_rows_saved": adjust_rows_saved,
                "dividend_rows_saved": dividend_rows_saved,
                "failures": failures,
                "ran_at": _now_ts(),
            }
        )
        self._save_state(state)

        return {
            "success": True,
            "data": {
                "start_date": start_date,
                "end_date": end_date,
                "year_type": year_type,
                "symbols_processed": len(codes),
                "adjust_rows_saved": adjust_rows_saved,
                "dividend_rows_saved": dividend_rows_saved,
                "failures": failures,
                "adjust_dir": str(ADJUST_DIR),
                "dividend_dir": str(DIVIDEND_DIR),
            },
            "timestamp": _now_ts(),
        }

    def get_state(self) -> Dict[str, Any]:
        return {"success": True, "data": self._state(), "timestamp": _now_ts()}

    def reset_state(self) -> Dict[str, Any]:
        if STATE_FILE.exists():
            STATE_FILE.unlink()
        return {"success": True, "data": {"status": "state_reset"}, "timestamp": _now_ts()}

    @staticmethod
    def get_all_endpoints() -> Dict[str, Any]:
        endpoints = ["get_all_endpoints", "run", "get_state", "reset_state"]
        return {
            "success": True,
            "data": {"available_endpoints": endpoints, "total_count": len(endpoints)},
            "count": len(endpoints),
            "timestamp": _now_ts(),
        }


def main() -> None:
    if sys.platform == "win32":
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")

    wrapper = BaoStockCorporateActions()
    endpoint_map = {
        "get_all_endpoints": wrapper.get_all_endpoints,
        "run": wrapper.run,
        "get_state": wrapper.get_state,
        "reset_state": wrapper.reset_state,
    }

    try:
        if len(sys.argv) < 2:
            _json(
                {
                    "success": False,
                    "error": "Usage: python baostock_corporate_actions.py <endpoint> [args...]",
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

        if endpoint == "run":
            if len(args) < 2:
                _json({"success": False, "error": "run requires: <start_date> <end_date>"})
                return
            start_date = args[0]
            end_date = args[1]
            trade_date = args[2] if len(args) > 2 else ""
            codes_csv = args[3] if len(args) > 3 else ""
            symbol_limit = int(args[4]) if len(args) > 4 and args[4] else 0
            year_type = args[5] if len(args) > 5 else "report"
            force = _to_bool(args[6]) if len(args) > 6 else False
            result = method(start_date, end_date, trade_date, codes_csv, symbol_limit, year_type, force)
        else:
            result = method()

        _json(result)
    except Exception as e:  # pragma: no cover - defensive
        _json({"success": False, "error": str(e), "data": [], "timestamp": _now_ts()})
    finally:
        wrapper._logout()


if __name__ == "__main__":
    main()
