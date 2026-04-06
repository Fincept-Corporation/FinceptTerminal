#!/usr/bin/env python3
"""
CNINFO Data Wrapper
Provides disclosure search endpoints for Chinese listed companies.
"""

import io
import json
import re
import sys
import time
import warnings
from datetime import datetime
from typing import Any, Dict, Optional

warnings.filterwarnings(
    "ignore",
    message=r"urllib3 .* doesn't match a supported version!",
)

try:
    import pandas as pd
    import requests
    from cninfo import hq as cninfo_hq
    from cninfo import hq_info as cninfo_hq_info
    from cninfo import stock_structure as cninfo_stock_structure
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


CNINFO_BASE = "http://www.cninfo.com.cn"
TOP_SEARCH_URL = f"{CNINFO_BASE}/new/information/topSearch/query"
ANNOUNCE_URL = f"{CNINFO_BASE}/new/hisAnnouncement/query"
DETAIL_URL = f"{CNINFO_BASE}/new/announcement/bulletin_detail"
STATIC_BASE = "http://static.cninfo.com.cn"


def _now_ts() -> int:
    return int(datetime.now().timestamp())


def _norm_stock_code(code: str) -> str:
    digits = re.sub(r"[^\d]", "", code or "")
    return digits[:6].zfill(6)


def _guess_market(code: str, raw_type: Optional[str]) -> str:
    t = (raw_type or "").lower()
    if "sz" in t:
        return "szse"
    if "sh" in t:
        return "sse"
    if code.startswith("6") or code.startswith("9"):
        return "sse"
    return "szse"


def _to_bool(value: Any) -> bool:
    return str(value).strip().lower() in {"1", "true", "yes", "y", "on"}


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


class CNInfoWrapper:
    def __init__(self) -> None:
        self.session = requests.Session()
        self.session.headers.update(_headers())

    @staticmethod
    def _normalize_df(df: pd.DataFrame) -> list:
        if df.empty:
            return []
        for col in df.columns:
            if pd.api.types.is_datetime64_any_dtype(df[col]):
                df[col] = df[col].astype(str)
        df = df.replace([float("inf"), float("-inf")], None)
        df = df.where(pd.notna(df), None)
        return df.to_dict(orient="records")

    def _post(
        self, url: str, data: Dict[str, Any], max_retries: int = 2
    ) -> Dict[str, Any]:
        for attempt in range(max_retries):
            try:
                resp = self.session.post(url, data=data, timeout=20)
                resp.raise_for_status()
                parsed = resp.json()
                return {
                    "success": True,
                    "data": parsed,
                    "timestamp": _now_ts(),
                }
            except Exception as e:  # pragma: no cover - defensive
                if attempt < max_retries - 1:
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

    def _safe_lib_call(self, func, *args, **kwargs) -> Dict[str, Any]:
        try:
            result = func(*args, **kwargs)
            if isinstance(result, pd.DataFrame):
                data = self._normalize_df(result)
                return {
                    "success": True,
                    "data": data,
                    "count": len(data),
                    "timestamp": _now_ts(),
                }
            if isinstance(result, list):
                return {
                    "success": True,
                    "data": result,
                    "count": len(result),
                    "timestamp": _now_ts(),
                }
            if isinstance(result, dict):
                return {
                    "success": True,
                    "data": result,
                    "count": 1,
                    "timestamp": _now_ts(),
                }
            return {
                "success": True,
                "data": result,
                "count": 1,
                "timestamp": _now_ts(),
            }
        except Exception as e:  # pragma: no cover - defensive
            return {
                "success": False,
                "error": str(e),
                "data": [],
                "timestamp": _now_ts(),
            }

    @staticmethod
    def _extract_search_items(raw: Any) -> list:
        if isinstance(raw, list):
            return raw
        if isinstance(raw, dict):
            maybe = raw.get("keyBoardList")
            if isinstance(maybe, list):
                return maybe
        return []

    def top_search(self, keyword: str) -> Dict[str, Any]:
        result = self._post(TOP_SEARCH_URL, data={"keyWord": keyword, "maxSecNum": "50"})
        if result.get("success"):
            items = self._extract_search_items(result.get("data"))
            result["count"] = len(items)
        return result

    def resolve_orgid(self, stock_code: str) -> Dict[str, Any]:
        code = _norm_stock_code(stock_code)
        searched = self.top_search(code)
        if not searched.get("success"):
            return searched

        items = self._extract_search_items(searched.get("data"))
        if not items:
            return {
                "success": False,
                "error": f"No CNINFO search result for stock code: {code}",
                "data": [],
                "timestamp": _now_ts(),
            }

        # Prioritize exact code matches
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
                "raw": picked,
            },
            "timestamp": _now_ts(),
        }

    def announcements(
        self,
        stock_code: str,
        page_num: str = "1",
        page_size: str = "30",
        start_date: str = "",
        end_date: str = "",
        category: str = "category_ndbg_szsh",
    ) -> Dict[str, Any]:
        resolved = self.resolve_orgid(stock_code)
        if not resolved.get("success"):
            return resolved

        data = resolved["data"]
        payload = {
            "stock": f"{data['sec_code']},{data['org_id']}",
            "tabName": "fulltext",
            "pageSize": str(page_size),
            "pageNum": str(page_num),
            "column": str(data.get("market") or ""),
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
        result = self._post(ANNOUNCE_URL, data=payload)
        if not result.get("success"):
            return result

        raw = result.get("data") or {}
        anns = raw.get("announcements") or []
        for ann in anns:
            adjunct_url = ann.get("adjunctUrl") or ""
            if adjunct_url and not adjunct_url.startswith("http"):
                ann["pdfUrl"] = f"{STATIC_BASE}/{adjunct_url}"
            else:
                ann["pdfUrl"] = adjunct_url
        return {
            "success": True,
            "data": {
                "stock_code": data["stock_code"],
                "org_id": data["org_id"],
                "sec_name": data["sec_name"],
                "total_records": raw.get("totalRecordNum", len(anns)),
                "page_num": int(page_num),
                "page_size": int(page_size),
                "announcements": anns,
            },
            "count": len(anns),
            "timestamp": _now_ts(),
        }

    def announcement_detail(self, announcement_id: str) -> Dict[str, Any]:
        return self._post(DETAIL_URL, data={"announcementId": announcement_id})

    def _resolve_package_stock(self, stock_code: str) -> Dict[str, Any]:
        code = _norm_stock_code(stock_code)
        info = cninfo_hq_info.HQInfo(code)
        if getattr(info, "s", None):
            return info.s
        resolved = self.resolve_orgid(code)
        if not resolved.get("success"):
            raise RuntimeError(f"Cannot resolve stock code for cninfo package: {code}")
        data = resolved["data"]
        return {
            "code": data.get("sec_code", code),
            "market": "szmb" if data.get("market") == "szse" else "shmb",
            "orgId": data.get("org_id", ""),
            "startTime": "",
        }

    def pkg_hq_info_init(self, stock_code: str) -> Dict[str, Any]:
        def _inner() -> Dict[str, Any]:
            info = cninfo_hq_info.HQInfo(_norm_stock_code(stock_code))
            payload = {
                "stock": getattr(info, "s", None),
                "available_types": ["hq", "lrb", "fzb", "llb"],
            }
            if payload["stock"] is None:
                raise RuntimeError(f"CNINFO package cannot find stock: {stock_code}")
            return payload

        return self._safe_lib_call(_inner)

    def pkg_hq_load(self, stock_code: str, data_type: str = "hq") -> Dict[str, Any]:
        def _inner() -> pd.DataFrame:
            s = self._resolve_package_stock(stock_code)
            return cninfo_hq.load(s, data_type)

        return self._safe_lib_call(_inner)

    def pkg_hq_download(self, stock_code: str, data_type: str = "hq") -> Dict[str, Any]:
        def _inner() -> list:
            s = self._resolve_package_stock(stock_code)
            return cninfo_hq.download(s, data_type)

        return self._safe_lib_call(_inner)

    def pkg_hq_info_load(self, stock_code: str, data_type: str = "hq") -> Dict[str, Any]:
        def _inner() -> pd.DataFrame:
            s = self._resolve_package_stock(stock_code)
            return cninfo_hq_info.load(s, data_type)

        return self._safe_lib_call(_inner)

    def pkg_stock_structure_main(
        self, code: str, enable_log: str = "false"
    ) -> Dict[str, Any]:
        def _inner() -> Dict[str, Any]:
            stock_code = code.strip()
            if "." not in stock_code:
                normalized = _norm_stock_code(stock_code)
                suffix = "sh" if normalized.startswith(("6", "9")) else "sz"
                stock_code = f"{normalized}.{suffix}"
            structure = cninfo_stock_structure.main(
                stock_code, enable_log=_to_bool(enable_log)
            )
            if structure is None:
                raise RuntimeError(f"No stock structure found: {stock_code}")
            return {
                "name": structure.name,
                "code": structure.code,
                "dates": structure.dates,
                "negotiable_shares": structure.negotiable_shares,
                "restricted_shares": structure.restricted_shares,
                "outstanding_shares": structure.outstanding_shares,
                "general_capitals": structure.general_capitals,
            }

        return self._safe_lib_call(_inner)

    def pkg_stock_structure_ctor(
        self,
        name: str,
        code: str,
        dates_json: str,
        negotiable_shares_json: str,
        restricted_shares_json: str,
        outstanding_shares_json: str,
        general_capitals_json: str,
    ) -> Dict[str, Any]:
        def _inner() -> Dict[str, Any]:
            structure = cninfo_stock_structure.StockStructure(
                name=name,
                code=code,
                dates=json.loads(dates_json),
                negotiable_shares=json.loads(negotiable_shares_json),
                restricted_shares=json.loads(restricted_shares_json),
                outstanding_shares=json.loads(outstanding_shares_json),
                general_capitals=json.loads(general_capitals_json),
            )
            return {
                "name": structure.name,
                "code": structure.code,
                "dates": structure.dates,
                "negotiable_shares": structure.negotiable_shares,
                "restricted_shares": structure.restricted_shares,
                "outstanding_shares": structure.outstanding_shares,
                "general_capitals": structure.general_capitals,
            }

        return self._safe_lib_call(_inner)

    def get_all_endpoints(self) -> Dict[str, Any]:
        categories = {
            "Meta": ["get_all_endpoints"],
            "Search": ["top_search", "resolve_orgid"],
            "Disclosure": ["announcements", "announcement_detail"],
            "CNINFO_Package": [
                "pkg_hq_info_init",
                "pkg_hq_load",
                "pkg_hq_download",
                "pkg_hq_info_load",
                "pkg_stock_structure_main",
                "pkg_stock_structure_ctor",
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

    wrapper = CNInfoWrapper()
    endpoint_map = {
        "get_all_endpoints": wrapper.get_all_endpoints,
        "top_search": wrapper.top_search,
        "resolve_orgid": wrapper.resolve_orgid,
        "announcements": wrapper.announcements,
        "announcement_detail": wrapper.announcement_detail,
        "pkg_hq_info_init": wrapper.pkg_hq_info_init,
        "pkg_hq_load": wrapper.pkg_hq_load,
        "pkg_hq_download": wrapper.pkg_hq_download,
        "pkg_hq_info_load": wrapper.pkg_hq_info_load,
        "pkg_stock_structure_main": wrapper.pkg_stock_structure_main,
        "pkg_stock_structure_ctor": wrapper.pkg_stock_structure_ctor,
    }

    if len(sys.argv) < 2:
        print(
            json.dumps(
                {
                    "success": False,
                    "error": "Usage: python cninfo_data.py <endpoint> [args...]",
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

    try:
        result = method(*args) if args else method()
    except TypeError as e:
        result = {
            "success": False,
            "error": f"Invalid arguments for endpoint '{endpoint}': {e}",
            "data": [],
            "timestamp": _now_ts(),
        }
    except Exception as e:  # pragma: no cover - defensive
        result = {
            "success": False,
            "error": str(e),
            "data": [],
            "timestamp": _now_ts(),
        }

    print(json.dumps(result, ensure_ascii=True, default=str))


if __name__ == "__main__":
    main()
