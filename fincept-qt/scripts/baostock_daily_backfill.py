#!/usr/bin/env python3
"""
BaoStock Daily Backfill
Incremental daily OHLCV backfill for all BaoStock symbols.
"""

import io
import json
import sys
import time
from datetime import datetime, timedelta
from pathlib import Path
from typing import Any, Dict, List

import pandas as pd

try:
    import baostock as bs
except ImportError as e:
    print(json.dumps({"success": False, "error": f"Missing dependency: {e}", "data": []}))
    sys.exit(1)


FIELDS = (
    "date,code,open,high,low,close,preclose,volume,amount,adjustflag,"
    "turn,tradestatus,pctChg,isST"
)
DATA_ROOT = Path(__file__).resolve().parent / "data" / "baostock" / "daily_backfill"
STATE_FILE = DATA_ROOT / "state.json"


def _now_ts() -> int:
    return int(datetime.now().timestamp())


def _to_bool(value: Any) -> bool:
    return str(value).strip().lower() in {"1", "true", "yes", "y", "on"}


def _json(data: Dict[str, Any]) -> None:
    print(json.dumps(data, ensure_ascii=True, default=str))


class BaoStockDailyBackfill:
    def __init__(self) -> None:
        self._logged_in = False
        DATA_ROOT.mkdir(parents=True, exist_ok=True)

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
            return {
                "last_trade_date": "",
                "completed_dates": [],
                "updated_at": _now_ts(),
            }
        try:
            return json.loads(STATE_FILE.read_text(encoding="utf-8"))
        except Exception:
            return {
                "last_trade_date": "",
                "completed_dates": [],
                "updated_at": _now_ts(),
            }

    def _save_state(self, state: Dict[str, Any]) -> None:
        state["updated_at"] = _now_ts()
        STATE_FILE.write_text(json.dumps(state, ensure_ascii=True, indent=2), encoding="utf-8")

    def _trade_dates(self, start_date: str, end_date: str) -> List[str]:
        rs = bs.query_trade_dates(start_date=start_date, end_date=end_date)
        df = self._query_to_df(rs)
        if df.empty:
            return []
        return df[df["is_trading_day"] == "1"]["calendar_date"].tolist()

    def _codes(self, day: str) -> List[str]:
        rs = bs.query_all_stock(day=day)
        df = self._query_to_df(rs)
        if df.empty:
            return []
        return df["code"].astype(str).tolist()

    def fetch_day(
        self,
        day: str,
        frequency: str = "d",
        adjustflag: str = "3",
        symbol_limit: int = 0,
        force: bool = False,
    ) -> Dict[str, Any]:
        self._login()
        out_file = DATA_ROOT / f"{day}.csv"
        if out_file.exists() and not force:
            return {
                "success": True,
                "data": {
                    "date": day,
                    "status": "skipped_existing",
                    "file": str(out_file),
                },
                "timestamp": _now_ts(),
            }

        codes = self._codes(day)
        if symbol_limit > 0:
            codes = codes[:symbol_limit]

        rows: List[pd.DataFrame] = []
        failed = 0
        for code in codes:
            try:
                rs = bs.query_history_k_data_plus(
                    code=code,
                    fields=FIELDS,
                    start_date=day,
                    end_date=day,
                    frequency=frequency,
                    adjustflag=adjustflag,
                )
                df = self._query_to_df(rs)
                if not df.empty:
                    rows.append(df)
            except Exception:
                failed += 1
                continue

        if rows:
            out_df = pd.concat(rows, ignore_index=True)
            out_df = out_df.drop_duplicates(subset=["date", "code", "adjustflag"])
            out_df.to_csv(out_file, index=False, encoding="utf-8")
            saved = len(out_df)
        else:
            pd.DataFrame(columns=FIELDS.split(",")).to_csv(out_file, index=False, encoding="utf-8")
            saved = 0

        return {
            "success": True,
            "data": {
                "date": day,
                "symbols": len(codes),
                "rows_saved": saved,
                "failed_symbols": failed,
                "file": str(out_file),
            },
            "timestamp": _now_ts(),
        }

    def run_backfill(
        self,
        start_date: str = "",
        end_date: str = "",
        frequency: str = "d",
        adjustflag: str = "3",
        symbol_limit: int = 0,
        force: bool = False,
    ) -> Dict[str, Any]:
        self._login()
        state = self._state()

        today = datetime.now().strftime("%Y-%m-%d")
        if not end_date:
            end_date = today
        if not start_date:
            if state.get("last_trade_date"):
                start_date = (
                    datetime.strptime(state["last_trade_date"], "%Y-%m-%d") + timedelta(days=1)
                ).strftime("%Y-%m-%d")
            else:
                start_date = (datetime.now() - timedelta(days=30)).strftime("%Y-%m-%d")

        if start_date > end_date:
            return {
                "success": True,
                "data": {
                    "status": "nothing_to_do",
                    "start_date": start_date,
                    "end_date": end_date,
                },
                "timestamp": _now_ts(),
            }

        trade_days = self._trade_dates(start_date, end_date)
        completed = set(state.get("completed_dates", []))
        processed = 0
        total_rows = 0
        total_failed_symbols = 0

        for day in trade_days:
            if day in completed and not force:
                continue
            day_res = self.fetch_day(
                day=day,
                frequency=frequency,
                adjustflag=adjustflag,
                symbol_limit=symbol_limit,
                force=force,
            )
            payload = day_res.get("data", {})
            processed += 1
            total_rows += int(payload.get("rows_saved", 0) or 0)
            total_failed_symbols += int(payload.get("failed_symbols", 0) or 0)
            completed.add(day)
            state["last_trade_date"] = day

        state["completed_dates"] = sorted(completed)
        self._save_state(state)

        return {
            "success": True,
            "data": {
                "start_date": start_date,
                "end_date": end_date,
                "trade_days_in_range": len(trade_days),
                "days_processed": processed,
                "rows_saved": total_rows,
                "failed_symbols": total_failed_symbols,
                "data_dir": str(DATA_ROOT),
                "state_file": str(STATE_FILE),
            },
            "timestamp": _now_ts(),
        }

    def get_state(self) -> Dict[str, Any]:
        return {"success": True, "data": self._state(), "timestamp": _now_ts()}

    def reset_state(self) -> Dict[str, Any]:
        if STATE_FILE.exists():
            STATE_FILE.unlink()
        return {
            "success": True,
            "data": {"status": "state_reset", "state_file": str(STATE_FILE)},
            "timestamp": _now_ts(),
        }

    @staticmethod
    def get_all_endpoints() -> Dict[str, Any]:
        endpoints = [
            "get_all_endpoints",
            "run_backfill",
            "fetch_day",
            "get_state",
            "reset_state",
        ]
        return {
            "success": True,
            "data": {"available_endpoints": endpoints, "total_count": len(endpoints)},
            "count": len(endpoints),
            "timestamp": _now_ts(),
        }


def main() -> None:
    if sys.platform == "win32":
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")

    wrapper = BaoStockDailyBackfill()
    endpoint_map = {
        "get_all_endpoints": wrapper.get_all_endpoints,
        "run_backfill": wrapper.run_backfill,
        "fetch_day": wrapper.fetch_day,
        "get_state": wrapper.get_state,
        "reset_state": wrapper.reset_state,
    }

    try:
        if len(sys.argv) < 2:
            _json(
                {
                    "success": False,
                    "error": "Usage: python baostock_daily_backfill.py <endpoint> [args...]",
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

        if endpoint == "run_backfill":
            start_date = args[0] if len(args) > 0 else ""
            end_date = args[1] if len(args) > 1 else ""
            frequency = args[2] if len(args) > 2 else "d"
            adjustflag = args[3] if len(args) > 3 else "3"
            symbol_limit = int(args[4]) if len(args) > 4 and args[4] else 0
            force = _to_bool(args[5]) if len(args) > 5 else False
            result = method(start_date, end_date, frequency, adjustflag, symbol_limit, force)
        elif endpoint == "fetch_day":
            if len(args) < 1:
                _json({"success": False, "error": "fetch_day requires: <YYYY-MM-DD>"})
                return
            day = args[0]
            frequency = args[1] if len(args) > 1 else "d"
            adjustflag = args[2] if len(args) > 2 else "3"
            symbol_limit = int(args[3]) if len(args) > 3 and args[3] else 0
            force = _to_bool(args[4]) if len(args) > 4 else False
            result = method(day, frequency, adjustflag, symbol_limit, force)
        else:
            result = method()

        _json(result)
    except Exception as e:  # pragma: no cover - defensive
        _json({"success": False, "error": str(e), "data": [], "timestamp": _now_ts()})
    finally:
        wrapper._logout()


if __name__ == "__main__":
    main()

