#!/usr/bin/env python3
"""
CNINFO Announcements Incremental
Incremental filings sync by stock and category with watermark-based pagination.
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


CNINFO_BASE = "http://www.cninfo.com.cn"
TOP_SEARCH_URL = f"{CNINFO_BASE}/new/information/topSearch/query"
ANNOUNCE_URL = f"{CNINFO_BASE}/new/hisAnnouncement/query"
STATIC_BASE = "http://static.cninfo.com.cn"

DATA_ROOT = Path(__file__).resolve().parent / "data" / "cninfo" / "announcements_incremental"
RECORDS_DIR = DATA_ROOT / "records"
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
    # Prefer code-based exchange mapping for A-share symbols.
    if code.startswith(("0", "2", "3")):
        return "szse"
    if code.startswith(("5", "6", "9")):
        return "sse"
    if "sz" in t:
        return "szse"
    if "sh" in t:
        return "sse"
    return "szse"


def _headers() -> Dict[str, str]:
    return {
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


def _json(data: Dict[str, Any]) -> None:
    print(json.dumps(data, ensure_ascii=True, default=str))


class CNInfoAnnouncementsIncremental:
    def __init__(self) -> None:
        DATA_ROOT.mkdir(parents=True, exist_ok=True)
        RECORDS_DIR.mkdir(parents=True, exist_ok=True)
        self.session = requests.Session()
        self.session.headers.update(_headers())

    def _state(self) -> Dict[str, Any]:
        if not STATE_FILE.exists():
            return {"watermarks": {}, "updated_at": _now_ts()}
        try:
            return json.loads(STATE_FILE.read_text(encoding="utf-8"))
        except Exception:
            return {"watermarks": {}, "updated_at": _now_ts()}

    def _save_state(self, state: Dict[str, Any]) -> None:
        state["updated_at"] = _now_ts()
        STATE_FILE.write_text(json.dumps(state, ensure_ascii=True, indent=2), encoding="utf-8")

    def _post(self, url: str, data: Dict[str, Any], max_retries: int = 3) -> Dict[str, Any]:
        for attempt in range(max_retries):
            try:
                resp = self.session.post(url, data=data, timeout=25)
                resp.raise_for_status()
                return {"success": True, "data": resp.json(), "timestamp": _now_ts()}
            except Exception as e:
                if attempt < max_retries - 1:
                    time.sleep(1 + attempt)
                    continue
                return {"success": False, "error": str(e), "data": [], "timestamp": _now_ts()}
        return {"success": False, "error": "Max retries exceeded", "data": [], "timestamp": _now_ts()}

    @staticmethod
    def _extract_search_items(raw: Any) -> List[Dict[str, Any]]:
        if isinstance(raw, list):
            return raw
        if isinstance(raw, dict):
            items = raw.get("keyBoardList")
            if isinstance(items, list):
                return items
        return []

    def resolve_orgid(self, stock_code: str) -> Dict[str, Any]:
        code = _norm_stock_code(stock_code)
        res = self._post(TOP_SEARCH_URL, {"keyWord": code, "maxSecNum": "50"})
        if not res.get("success"):
            return res

        items = self._extract_search_items(res.get("data"))
        if not items:
            return {
                "success": False,
                "error": f"No CNINFO search result for stock code: {code}",
                "data": [],
                "timestamp": _now_ts(),
            }

        exact = [it for it in items if str(it.get("seccode", "")).zfill(6) == code]
        picked = exact[0] if exact else items[0]
        sec_code = (picked.get("seccode") or picked.get("code") or code).zfill(6)
        sec_name = picked.get("secname") or picked.get("zwjc") or picked.get("name")
        market = picked.get("market") or _guess_market(sec_code, picked.get("type"))
        return {
            "success": True,
            "data": {
                "stock_code": code,
                "org_id": picked.get("orgId"),
                "sec_code": sec_code,
                "sec_name": sec_name,
                "market": market,
            },
            "timestamp": _now_ts(),
        }

    @staticmethod
    def _category_file_name(stock_code: str, category: str) -> str:
        safe_cat = re.sub(r"[^a-zA-Z0-9_\-]", "_", category)
        return f"{stock_code}_{safe_cat}.jsonl"

    def _record_path(self, stock_code: str, category: str) -> Path:
        return RECORDS_DIR / self._category_file_name(stock_code, category)

    @staticmethod
    def _normalize_ann(ann: Dict[str, Any], stock_info: Dict[str, Any], category: str) -> Dict[str, Any]:
        adjunct_url = ann.get("adjunctUrl") or ""
        if adjunct_url and not adjunct_url.startswith("http"):
            pdf_url = f"{STATIC_BASE}/{adjunct_url}"
        else:
            pdf_url = adjunct_url
        return {
            "announcementId": ann.get("announcementId"),
            "announcementTitle": ann.get("announcementTitle"),
            "announcementTime": ann.get("announcementTime"),
            "adjunctUrl": ann.get("adjunctUrl"),
            "pdfUrl": pdf_url,
            "secCode": ann.get("secCode") or stock_info.get("sec_code"),
            "secName": ann.get("secName") or stock_info.get("sec_name"),
            "orgId": ann.get("orgId") or stock_info.get("org_id"),
            "category": category,
            "syncedAt": _now_ts(),
            "raw": ann,
        }

    def sync_stock(
        self,
        stock_code: str,
        category: str = "category_ndbg_szsh",
        page_size: int = 30,
        max_pages: int = 200,
        start_date: str = "",
        end_date: str = "",
        force_full: bool = False,
    ) -> Dict[str, Any]:
        state = self._state()
        resolved = self.resolve_orgid(stock_code)
        if not resolved.get("success"):
            return resolved
        info = resolved["data"]
        code = info["stock_code"]
        watermark_key = f"{code}|{category}"
        watermark = int(state["watermarks"].get(watermark_key, 0) or 0)
        if force_full:
            watermark = 0

        output_path = self._record_path(code, category)
        new_records: List[Dict[str, Any]] = []
        seen_ids = set()
        page_num = 1
        scanned = 0

        while page_num <= max_pages:
            payload = {
                "stock": f"{info['sec_code']},{info['org_id']}",
                "tabName": "fulltext",
                "pageSize": str(page_size),
                "pageNum": str(page_num),
                "column": str(info.get("market") or ""),
                "plate": "",
                "searchkey": "",
                "secid": "",
                "category": category,
                "trade": "",
                "seDate": f"{start_date}~{end_date}" if start_date or end_date else "",
                "sortName": "",
                "sortType": "",
                "isHLtitle": "true",
            }
            res = self._post(ANNOUNCE_URL, payload)
            if not res.get("success"):
                return res

            raw = res.get("data") or {}
            anns = raw.get("announcements") or []
            if not anns:
                break

            scanned += len(anns)
            page_times = []
            for ann in anns:
                ann_id = str(ann.get("announcementId") or "")
                if ann_id in seen_ids:
                    continue
                seen_ids.add(ann_id)
                ann_time = int(ann.get("announcementTime") or 0)
                page_times.append(ann_time)
                if ann_time > watermark or force_full:
                    new_records.append(self._normalize_ann(ann, info, category))

            if len(anns) < page_size:
                break
            if not force_full and page_times and min(page_times) <= watermark:
                break
            page_num += 1

        if new_records:
            new_records.sort(key=lambda x: int(x.get("announcementTime") or 0))
            with output_path.open("a", encoding="utf-8") as fh:
                for rec in new_records:
                    fh.write(json.dumps(rec, ensure_ascii=True))
                    fh.write("\n")

        new_watermark = max([watermark] + [int(r.get("announcementTime") or 0) for r in new_records])
        state["watermarks"][watermark_key] = new_watermark
        self._save_state(state)

        return {
            "success": True,
            "data": {
                "stock_code": code,
                "category": category,
                "watermark_before": watermark,
                "watermark_after": new_watermark,
                "records_scanned": scanned,
                "new_records": len(new_records),
                "pages_scanned": page_num,
                "output_file": str(output_path),
            },
            "timestamp": _now_ts(),
        }

    def sync_batch(
        self,
        stock_codes_csv: str,
        category: str = "category_ndbg_szsh",
        page_size: int = 30,
        max_pages: int = 200,
        start_date: str = "",
        end_date: str = "",
        force_full: bool = False,
    ) -> Dict[str, Any]:
        stocks = [_norm_stock_code(x) for x in stock_codes_csv.split(",") if x.strip()]
        summaries: List[Dict[str, Any]] = []
        total_new = 0
        for stock in stocks:
            res = self.sync_stock(
                stock_code=stock,
                category=category,
                page_size=page_size,
                max_pages=max_pages,
                start_date=start_date,
                end_date=end_date,
                force_full=force_full,
            )
            summaries.append(
                {
                    "stock_code": stock,
                    "success": res.get("success", False),
                    "new_records": (res.get("data") or {}).get("new_records", 0),
                    "error": res.get("error"),
                }
            )
            total_new += int((res.get("data") or {}).get("new_records", 0) or 0)
        return {
            "success": True,
            "data": {
                "stocks": stocks,
                "count": len(stocks),
                "total_new_records": total_new,
                "summaries": summaries,
            },
            "timestamp": _now_ts(),
        }

    def get_state(self) -> Dict[str, Any]:
        return {"success": True, "data": self._state(), "timestamp": _now_ts()}

    def reset_state(self, stock_code: str = "", category: str = "") -> Dict[str, Any]:
        if not stock_code and not category:
            if STATE_FILE.exists():
                STATE_FILE.unlink()
            return {"success": True, "data": {"status": "state_reset_all"}, "timestamp": _now_ts()}

        state = self._state()
        stock_code = _norm_stock_code(stock_code)
        keys = list(state.get("watermarks", {}).keys())
        removed = []
        for key in keys:
            code, cat = key.split("|", 1)
            if code == stock_code and (not category or category == cat):
                removed.append(key)
                state["watermarks"].pop(key, None)
        self._save_state(state)
        return {
            "success": True,
            "data": {"status": "state_reset_partial", "removed": removed},
            "timestamp": _now_ts(),
        }

    @staticmethod
    def get_all_endpoints() -> Dict[str, Any]:
        endpoints = [
            "get_all_endpoints",
            "sync_stock",
            "sync_batch",
            "get_state",
            "reset_state",
            "resolve_orgid",
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

    wrapper = CNInfoAnnouncementsIncremental()
    endpoint_map = {
        "get_all_endpoints": wrapper.get_all_endpoints,
        "sync_stock": wrapper.sync_stock,
        "sync_batch": wrapper.sync_batch,
        "get_state": wrapper.get_state,
        "reset_state": wrapper.reset_state,
        "resolve_orgid": wrapper.resolve_orgid,
    }

    try:
        if len(sys.argv) < 2:
            _json(
                {
                    "success": False,
                    "error": "Usage: python cninfo_announcements_incremental.py <endpoint> [args...]",
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

        if endpoint == "sync_stock":
            if len(args) < 1:
                _json({"success": False, "error": "sync_stock requires: <stock_code>"})
                return
            stock_code = args[0]
            category = args[1] if len(args) > 1 else "category_ndbg_szsh"
            page_size = int(args[2]) if len(args) > 2 and args[2] else 30
            max_pages = int(args[3]) if len(args) > 3 and args[3] else 200
            start_date = args[4] if len(args) > 4 else ""
            end_date = args[5] if len(args) > 5 else ""
            force_full = _to_bool(args[6]) if len(args) > 6 else False
            result = method(stock_code, category, page_size, max_pages, start_date, end_date, force_full)
        elif endpoint == "sync_batch":
            if len(args) < 1:
                _json({"success": False, "error": "sync_batch requires: <stock_codes_csv>"})
                return
            stock_codes_csv = args[0]
            category = args[1] if len(args) > 1 else "category_ndbg_szsh"
            page_size = int(args[2]) if len(args) > 2 and args[2] else 30
            max_pages = int(args[3]) if len(args) > 3 and args[3] else 200
            start_date = args[4] if len(args) > 4 else ""
            end_date = args[5] if len(args) > 5 else ""
            force_full = _to_bool(args[6]) if len(args) > 6 else False
            result = method(stock_codes_csv, category, page_size, max_pages, start_date, end_date, force_full)
        elif endpoint == "reset_state":
            stock_code = args[0] if len(args) > 0 else ""
            category = args[1] if len(args) > 1 else ""
            result = method(stock_code, category)
        elif endpoint == "resolve_orgid":
            if len(args) < 1:
                _json({"success": False, "error": "resolve_orgid requires: <stock_code>"})
                return
            result = method(args[0])
        else:
            result = method()

        _json(result)
    except Exception as e:  # pragma: no cover - defensive
        _json({"success": False, "error": str(e), "data": [], "timestamp": _now_ts()})


if __name__ == "__main__":
    main()
