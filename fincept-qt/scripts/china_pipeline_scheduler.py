#!/usr/bin/env python3
"""
China Pipeline Scheduler
Orchestrates BaoStock + CNINFO incremental pipelines end-to-end.
"""

import io
import json
import sys
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Tuple

from baostock_corporate_actions import BaoStockCorporateActions
from baostock_daily_backfill import BaoStockDailyBackfill
from baostock_fundamentals_quarterly import BaoStockFundamentalsQuarterly
from china_data_quality_checks import ChinaDataQualityChecks
from china_data_unified_store import ChinaDataUnifiedStore
from cninfo_announcements_incremental import CNInfoAnnouncementsIncremental
from cninfo_entity_resolver import CNInfoEntityResolver
from cninfo_pdf_downloader import CNInfoPdfDownloader
from cninfo_pdf_text_extractor import CNInfoPdfTextExtractor


DATA_ROOT = Path(__file__).resolve().parent / "data" / "china" / "pipeline_scheduler"
RUN_LOG = DATA_ROOT / "runs.jsonl"
LAST_RUN = DATA_ROOT / "last_run.json"


def _now_ts() -> int:
    return int(datetime.now().timestamp())


def _to_bool(value: Any) -> bool:
    return str(value).strip().lower() in {"1", "true", "yes", "y", "on"}


def _json(data: Dict[str, Any]) -> None:
    print(json.dumps(data, ensure_ascii=True, default=str))


def _prev_quarter(today: datetime) -> Tuple[int, int]:
    q = (today.month - 1) // 3 + 1
    y = today.year
    if q == 1:
        return y - 1, 4
    return y, q - 1


class ChinaPipelineScheduler:
    def __init__(self) -> None:
        DATA_ROOT.mkdir(parents=True, exist_ok=True)

    @staticmethod
    def _safe_step(step_name: str, fn, *args, **kwargs) -> Dict[str, Any]:
        try:
            res = fn(*args, **kwargs)
            return {"step": step_name, "success": bool(res.get("success", False)), "result": res}
        except Exception as e:
            return {"step": step_name, "success": False, "result": {"success": False, "error": str(e)}}

    @staticmethod
    def _extract_codes_from_entities(payload: Dict[str, Any], max_codes: int) -> List[str]:
        entities = (payload.get("data") or {}).get("entities") or []
        codes = [str(item.get("stock_code") or "") for item in entities if item.get("stock_code")]
        if max_codes > 0:
            codes = codes[:max_codes]
        return codes

    @staticmethod
    def _to_baostock_code(code: str) -> str:
        c = str(code).strip()
        if "." in c:
            return c
        digits = "".join(ch for ch in c if ch.isdigit())[:6].zfill(6)
        if digits.startswith(("5", "6", "9")):
            return f"sh.{digits}"
        return f"sz.{digits}"

    def run_for_codes(
        self,
        stock_codes_csv: str,
        max_pdfs_per_stock: int = 3,
        force: bool = False,
        max_codes: int = 20,
    ) -> Dict[str, Any]:
        now = datetime.now()
        today = now.strftime("%Y-%m-%d")
        year_start = f"{now.year}-01-01"
        q_year, q_num = _prev_quarter(now)

        codes = [c.strip() for c in stock_codes_csv.split(",") if c.strip()]
        if max_codes > 0:
            codes = codes[:max_codes]
        if not codes:
            return {"success": False, "error": "No stock codes provided", "data": [], "timestamp": _now_ts()}
        codes_csv = ",".join(codes)
        bs_codes = [self._to_baostock_code(c) for c in codes]
        bs_codes_csv = ",".join(bs_codes)

        daily = BaoStockDailyBackfill()
        fundamentals = BaoStockFundamentalsQuarterly()
        corp = BaoStockCorporateActions()
        resolver = CNInfoEntityResolver()
        ann = CNInfoAnnouncementsIncremental()
        downloader = CNInfoPdfDownloader()
        extractor = CNInfoPdfTextExtractor()
        unified = ChinaDataUnifiedStore()
        quality = ChinaDataQualityChecks()

        steps: List[Dict[str, Any]] = []
        try:
            steps.append(
                self._safe_step(
                    "baostock_daily_backfill",
                    daily.run_backfill,
                    today,
                    today,
                    "d",
                    "3",
                    len(codes),
                    force,
                )
            )
            steps.append(
                self._safe_step(
                    "baostock_fundamentals_quarterly",
                    fundamentals.run_quarter,
                    q_year,
                    q_num,
                    today,
                    len(codes),
                    force,
                    bs_codes_csv,
                )
            )
            steps.append(
                self._safe_step(
                    "baostock_corporate_actions",
                    corp.run,
                    year_start,
                    today,
                    today,
                    bs_codes_csv,
                    len(codes),
                    "report",
                    force,
                )
            )
            steps.append(
                self._safe_step(
                    "cninfo_entity_resolver",
                    resolver.resolve_batch,
                    codes_csv,
                    len(codes),
                    force,
                )
            )
            steps.append(
                self._safe_step(
                    "cninfo_announcements_incremental",
                    ann.sync_batch,
                    codes_csv,
                    "category_ndbg_szsh",
                    30,
                    200,
                    "",
                    "",
                    force,
                )
            )
            if max_pdfs_per_stock > 0:
                download_summaries = []
                for code in codes:
                    dl = self._safe_step(
                        f"cninfo_pdf_downloader_{code}",
                        downloader.download_from_stock_records,
                        code,
                        "category_ndbg_szsh",
                        max_pdfs_per_stock,
                        force,
                    )
                    download_summaries.append(dl)
                steps.append(
                    {
                        "step": "cninfo_pdf_downloader",
                        "success": all(x.get("success", False) for x in download_summaries),
                        "result": {"success": True, "details": download_summaries},
                    }
                )
                max_extract = max(1, len(codes) * max(1, max_pdfs_per_stock))
                extract_step = self._safe_step(
                    "cninfo_pdf_text_extractor",
                    extractor.extract_from_index,
                    max_extract,
                    force,
                )
                extract_error = str((extract_step.get("result") or {}).get("error") or "")
                if (not extract_step.get("success")) and "Missing PDF parser" in extract_error:
                    extract_step["success"] = True
                    extract_step["result"] = {
                        "success": True,
                        "warning": extract_error,
                        "status": "skipped_missing_pdf_parser",
                    }
                steps.append(extract_step)
            else:
                steps.append(
                    {
                        "step": "cninfo_pdf_downloader",
                        "success": True,
                        "result": {"success": True, "status": "skipped_max_pdfs_per_stock_zero"},
                    }
                )
                steps.append(
                    {
                        "step": "cninfo_pdf_text_extractor",
                        "success": True,
                        "result": {"success": True, "status": "skipped_max_pdfs_per_stock_zero"},
                    }
                )
            steps.append(self._safe_step("china_data_unified_store", unified.run_all, 0))
            steps.append(self._safe_step("china_data_quality_checks", quality.run_checks))
            steps.append(self._safe_step("china_data_unified_store_stats", unified.stats))
        finally:
            daily._logout()
            fundamentals._logout()
            corp._logout()
            resolver._logout_bs()

        overall_success = all(step.get("success", False) for step in steps)
        run_payload = {
            "run_id": f"run_{_now_ts()}",
            "ran_at": _now_ts(),
            "codes": codes,
            "max_pdfs_per_stock": max_pdfs_per_stock,
            "force": force,
            "overall_success": overall_success,
            "steps": steps,
        }
        with RUN_LOG.open("a", encoding="utf-8") as fh:
            fh.write(json.dumps(run_payload, ensure_ascii=True))
            fh.write("\n")
        LAST_RUN.write_text(json.dumps(run_payload, ensure_ascii=True, indent=2), encoding="utf-8")

        return {"success": overall_success, "data": run_payload, "timestamp": _now_ts()}

    def run_daily(
        self,
        stock_codes_csv: str = "",
        max_codes: int = 20,
        max_pdfs_per_stock: int = 3,
        force: bool = False,
    ) -> Dict[str, Any]:
        if stock_codes_csv.strip():
            return self.run_for_codes(stock_codes_csv, max_pdfs_per_stock, force, max_codes)

        resolver = CNInfoEntityResolver()
        try:
            build_res = resolver.build_from_baostock("", max_codes, force)
            if not build_res.get("success"):
                return build_res
            entities_res = resolver.get_entities(limit=max_codes)
            codes = self._extract_codes_from_entities(entities_res, max_codes)
        finally:
            resolver._logout_bs()

        if not codes:
            return {
                "success": False,
                "error": "No codes resolved from baostock/entity resolver",
                "data": [],
                "timestamp": _now_ts(),
            }
        return self.run_for_codes(",".join(codes), max_pdfs_per_stock, force, max_codes)

    def get_last_run(self) -> Dict[str, Any]:
        if not LAST_RUN.exists():
            return {"success": False, "error": "No run found", "data": [], "timestamp": _now_ts()}
        return {
            "success": True,
            "data": json.loads(LAST_RUN.read_text(encoding="utf-8")),
            "timestamp": _now_ts(),
        }

    @staticmethod
    def get_all_endpoints() -> Dict[str, Any]:
        endpoints = ["get_all_endpoints", "run_daily", "run_for_codes", "get_last_run"]
        return {
            "success": True,
            "data": {"available_endpoints": endpoints, "total_count": len(endpoints)},
            "count": len(endpoints),
            "timestamp": _now_ts(),
        }


def main() -> None:
    if sys.platform == "win32":
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")

    wrapper = ChinaPipelineScheduler()
    endpoint_map = {
        "get_all_endpoints": wrapper.get_all_endpoints,
        "run_daily": wrapper.run_daily,
        "run_for_codes": wrapper.run_for_codes,
        "get_last_run": wrapper.get_last_run,
    }

    try:
        if len(sys.argv) < 2:
            _json(
                {
                    "success": False,
                    "error": "Usage: python china_pipeline_scheduler.py <endpoint> [args...]",
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

        if endpoint == "run_daily":
            stock_codes_csv = args[0] if len(args) > 0 else ""
            max_codes = int(args[1]) if len(args) > 1 and args[1] else 20
            max_pdfs_per_stock = int(args[2]) if len(args) > 2 and args[2] else 3
            force = _to_bool(args[3]) if len(args) > 3 else False
            result = method(stock_codes_csv, max_codes, max_pdfs_per_stock, force)
        elif endpoint == "run_for_codes":
            if len(args) < 1:
                _json({"success": False, "error": "run_for_codes requires: <stock_codes_csv>"})
                return
            stock_codes_csv = args[0]
            max_pdfs_per_stock = int(args[1]) if len(args) > 1 and args[1] else 3
            force = _to_bool(args[2]) if len(args) > 2 else False
            max_codes = int(args[3]) if len(args) > 3 and args[3] else 20
            result = method(stock_codes_csv, max_pdfs_per_stock, force, max_codes)
        else:
            result = method()

        _json(result)
    except Exception as e:  # pragma: no cover - defensive
        _json({"success": False, "error": str(e), "data": [], "timestamp": _now_ts()})


if __name__ == "__main__":
    main()
