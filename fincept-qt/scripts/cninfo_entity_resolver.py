#!/usr/bin/env python3
"""
CNINFO Entity Resolver
Builds/maintains stock_code -> org_id/sec_name/market master mapping.
"""

import io
import json
import re
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional

import requests

try:
    import baostock as bs
except Exception:
    bs = None


CNINFO_BASE = "http://www.cninfo.com.cn"
TOP_SEARCH_URL = f"{CNINFO_BASE}/new/information/topSearch/query"
DATA_ROOT = Path(__file__).resolve().parent / "data" / "cninfo" / "entity_resolver"
ENTITIES_JSONL = DATA_ROOT / "entities.jsonl"
ENTITIES_CSV = DATA_ROOT / "entities.csv"
STATE_FILE = DATA_ROOT / "state.json"


def _now_ts() -> int:
    return int(datetime.now().timestamp())


def _to_bool(value: Any) -> bool:
    return str(value).strip().lower() in {"1", "true", "yes", "y", "on"}


def _norm_stock_code(code: str) -> str:
    digits = re.sub(r"[^\d]", "", code or "")
    return digits[:6].zfill(6)


def _guess_market(code: str, raw_type: Optional[str]) -> str:
    t = (raw_type or "").lower()
    if code.startswith(("0", "2", "3")):
        return "szse"
    if code.startswith(("5", "6", "9")):
        return "sse"
    if "sz" in t:
        return "szse"
    if "sh" in t:
        return "sse"
    return "szse"


def _json(data: Dict[str, Any]) -> None:
    print(json.dumps(data, ensure_ascii=True, default=str))


class CNInfoEntityResolver:
    def __init__(self) -> None:
        DATA_ROOT.mkdir(parents=True, exist_ok=True)
        self.session = requests.Session()
        self.session.headers.update(
            {
                "User-Agent": (
                    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                    "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"
                ),
                "Accept": "application/json, text/plain, */*",
                "Content-Type": "application/x-www-form-urlencoded; charset=UTF-8",
                "Origin": CNINFO_BASE,
                "Referer": (
                    f"{CNINFO_BASE}/new/commonUrl/pageOfSearch?"
                    "url=disclosure/list/search&lastPage=index"
                ),
            }
        )
        self._logged_in = False

    def _login_bs(self) -> None:
        if bs is None:
            raise RuntimeError("baostock not installed in current environment")
        if self._logged_in:
            return
        lg = bs.login()
        if lg.error_code != "0":
            raise RuntimeError(f"BaoStock login failed: {lg.error_msg}")
        self._logged_in = True

    def _logout_bs(self) -> None:
        if self._logged_in and bs is not None:
            try:
                bs.logout()
            finally:
                self._logged_in = False

    def _state(self) -> Dict[str, Any]:
        if not STATE_FILE.exists():
            return {"updated_at": _now_ts(), "last_run": None}
        try:
            return json.loads(STATE_FILE.read_text(encoding="utf-8"))
        except Exception:
            return {"updated_at": _now_ts(), "last_run": None}

    def _save_state(self, state: Dict[str, Any]) -> None:
        state["updated_at"] = _now_ts()
        STATE_FILE.write_text(json.dumps(state, ensure_ascii=True, indent=2), encoding="utf-8")

    @staticmethod
    def _extract_search_items(raw: Any) -> List[Dict[str, Any]]:
        if isinstance(raw, list):
            return raw
        if isinstance(raw, dict):
            items = raw.get("keyBoardList")
            if isinstance(items, list):
                return items
        return []

    def _post(self, data: Dict[str, Any], max_retries: int = 3) -> Dict[str, Any]:
        for attempt in range(max_retries):
            try:
                resp = self.session.post(TOP_SEARCH_URL, data=data, timeout=20)
                resp.raise_for_status()
                return {"success": True, "data": resp.json(), "timestamp": _now_ts()}
            except Exception as e:
                if attempt < max_retries - 1:
                    time.sleep(1 + attempt)
                    continue
                return {"success": False, "error": str(e), "data": [], "timestamp": _now_ts()}
        return {"success": False, "error": "Max retries exceeded", "data": [], "timestamp": _now_ts()}

    def _load_entities(self) -> Dict[str, Dict[str, Any]]:
        entities: Dict[str, Dict[str, Any]] = {}
        if not ENTITIES_JSONL.exists():
            return entities
        for line in ENTITIES_JSONL.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if not line:
                continue
            try:
                row = json.loads(line)
                code = str(row.get("stock_code") or "")
                if code:
                    entities[code] = row
            except Exception:
                continue
        return entities

    @staticmethod
    def _save_entities_csv(entities: Dict[str, Dict[str, Any]]) -> None:
        import csv

        fields = [
            "stock_code",
            "org_id",
            "sec_code",
            "sec_name",
            "market",
            "raw_type",
            "resolved_at",
        ]
        with ENTITIES_CSV.open("w", encoding="utf-8", newline="") as fh:
            writer = csv.DictWriter(fh, fieldnames=fields)
            writer.writeheader()
            for code in sorted(entities.keys()):
                row = entities[code]
                writer.writerow({k: row.get(k, "") for k in fields})

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

    def _codes_from_baostock(self, trade_date: str = "", max_codes: int = 0) -> List[str]:
        self._login_bs()
        rs = bs.query_all_stock(day=trade_date or None)
        if rs.error_code != "0":
            raise RuntimeError(rs.error_msg)
        rows: List[List[str]] = []
        while rs.next():
            rows.append(rs.get_row_data())
        if not rows:
            return []
        import pandas as pd

        df = pd.DataFrame(rows, columns=rs.fields)
        candidates = df["code"].astype(str).tolist()
        out = []
        for code in candidates:
            if not self._is_equity_code(code):
                continue
            if "." in code:
                out.append(code.split(".")[1])
            else:
                out.append(code)
        if max_codes > 0:
            out = out[:max_codes]
        return out

    def resolve_one(self, stock_code: str) -> Dict[str, Any]:
        code = _norm_stock_code(stock_code)
        res = self._post({"keyWord": code, "maxSecNum": "50"})
        if not res.get("success"):
            return res
        items = self._extract_search_items(res.get("data"))
        if not items:
            return {
                "success": False,
                "error": f"No CNINFO result for: {code}",
                "data": [],
                "timestamp": _now_ts(),
            }

        exact = [it for it in items if str(it.get("seccode", "")).zfill(6) == code]
        picked = exact[0] if exact else items[0]
        sec_code = (picked.get("seccode") or picked.get("code") or code).zfill(6)
        entity = {
            "stock_code": code,
            "org_id": picked.get("orgId"),
            "sec_code": sec_code,
            "sec_name": picked.get("secname") or picked.get("zwjc") or picked.get("name"),
            "market": picked.get("market") or _guess_market(sec_code, picked.get("type")),
            "raw_type": picked.get("type"),
            "resolved_at": _now_ts(),
        }
        return {"success": True, "data": entity, "timestamp": _now_ts()}

    def resolve_batch(
        self, stock_codes_csv: str, max_codes: int = 0, force_update: bool = False
    ) -> Dict[str, Any]:
        codes = [_norm_stock_code(x) for x in stock_codes_csv.split(",") if x.strip()]
        if max_codes > 0:
            codes = codes[:max_codes]

        entities = self._load_entities()
        resolved = 0
        skipped = 0
        failed = 0
        errors = []

        for code in codes:
            if code in entities and not force_update:
                skipped += 1
                continue
            res = self.resolve_one(code)
            if res.get("success"):
                entities[code] = res["data"]
                resolved += 1
            else:
                failed += 1
                errors.append({"stock_code": code, "error": res.get("error")})

        with ENTITIES_JSONL.open("w", encoding="utf-8") as fh:
            for code in sorted(entities.keys()):
                fh.write(json.dumps(entities[code], ensure_ascii=True))
                fh.write("\n")
        self._save_entities_csv(entities)
        state = self._state()
        state["last_run"] = {
            "count_input": len(codes),
            "resolved": resolved,
            "skipped": skipped,
            "failed": failed,
            "ran_at": _now_ts(),
        }
        self._save_state(state)

        return {
            "success": True,
            "data": {
                "count_input": len(codes),
                "resolved": resolved,
                "skipped": skipped,
                "failed": failed,
                "errors": errors[:20],
                "entities_jsonl": str(ENTITIES_JSONL),
                "entities_csv": str(ENTITIES_CSV),
            },
            "timestamp": _now_ts(),
        }

    def build_from_baostock(
        self, trade_date: str = "", max_codes: int = 100, force_update: bool = False
    ) -> Dict[str, Any]:
        codes = self._codes_from_baostock(trade_date=trade_date, max_codes=max_codes)
        csv_codes = ",".join(codes)
        return self.resolve_batch(csv_codes, max_codes=0, force_update=force_update)

    def get_entities(self, limit: int = 0) -> Dict[str, Any]:
        entities = self._load_entities()
        rows = [entities[k] for k in sorted(entities.keys())]
        if limit > 0:
            rows = rows[:limit]
        return {
            "success": True,
            "data": {"count": len(rows), "entities": rows},
            "timestamp": _now_ts(),
        }

    def reset_entities(self) -> Dict[str, Any]:
        for p in [ENTITIES_JSONL, ENTITIES_CSV, STATE_FILE]:
            if p.exists():
                p.unlink()
        return {"success": True, "data": {"status": "reset_done"}, "timestamp": _now_ts()}

    @staticmethod
    def get_all_endpoints() -> Dict[str, Any]:
        endpoints = [
            "get_all_endpoints",
            "resolve_one",
            "resolve_batch",
            "build_from_baostock",
            "get_entities",
            "reset_entities",
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

    wrapper = CNInfoEntityResolver()
    endpoint_map = {
        "get_all_endpoints": wrapper.get_all_endpoints,
        "resolve_one": wrapper.resolve_one,
        "resolve_batch": wrapper.resolve_batch,
        "build_from_baostock": wrapper.build_from_baostock,
        "get_entities": wrapper.get_entities,
        "reset_entities": wrapper.reset_entities,
    }

    try:
        if len(sys.argv) < 2:
            _json(
                {
                    "success": False,
                    "error": "Usage: python cninfo_entity_resolver.py <endpoint> [args...]",
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

        if endpoint == "resolve_one":
            if len(args) < 1:
                _json({"success": False, "error": "resolve_one requires: <stock_code>"})
                return
            result = method(args[0])
        elif endpoint == "resolve_batch":
            if len(args) < 1:
                _json({"success": False, "error": "resolve_batch requires: <stock_codes_csv>"})
                return
            stock_codes_csv = args[0]
            max_codes = int(args[1]) if len(args) > 1 and args[1] else 0
            force_update = _to_bool(args[2]) if len(args) > 2 else False
            result = method(stock_codes_csv, max_codes, force_update)
        elif endpoint == "build_from_baostock":
            trade_date = args[0] if len(args) > 0 else ""
            max_codes = int(args[1]) if len(args) > 1 and args[1] else 100
            force_update = _to_bool(args[2]) if len(args) > 2 else False
            result = method(trade_date, max_codes, force_update)
        elif endpoint == "get_entities":
            limit = int(args[0]) if len(args) > 0 and args[0] else 0
            result = method(limit)
        else:
            result = method()

        _json(result)
    except Exception as e:  # pragma: no cover - defensive
        _json({"success": False, "error": str(e), "data": [], "timestamp": _now_ts()})
    finally:
        wrapper._logout_bs()


if __name__ == "__main__":
    main()

