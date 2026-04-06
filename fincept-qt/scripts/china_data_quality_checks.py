#!/usr/bin/env python3
"""
China Data Quality Checks
Runs integrity/coverage checks on unified China data store.
"""

import io
import json
import sqlite3
import sys
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List


UNIFIED_DB = Path(__file__).resolve().parent / "data" / "china" / "unified_store" / "china_data.db"
QUALITY_DIR = Path(__file__).resolve().parent / "data" / "china" / "quality_checks"
REPORT_LATEST = QUALITY_DIR / "report_latest.json"


def _now_ts() -> int:
    return int(datetime.now().timestamp())


def _json(data: Dict[str, Any]) -> None:
    print(json.dumps(data, ensure_ascii=True, default=str))


class ChinaDataQualityChecks:
    def __init__(self) -> None:
        QUALITY_DIR.mkdir(parents=True, exist_ok=True)

    @staticmethod
    def _connect() -> sqlite3.Connection:
        conn = sqlite3.connect(UNIFIED_DB)
        conn.row_factory = sqlite3.Row
        return conn

    def _table_count(self, conn: sqlite3.Connection, table: str) -> int:
        return int(conn.execute(f"SELECT COUNT(1) AS c FROM {table}").fetchone()["c"])

    def _check_market_nulls(self, conn: sqlite3.Connection) -> Dict[str, Any]:
        row = conn.execute(
            """
            SELECT
                SUM(CASE WHEN close IS NULL OR TRIM(close) = '' THEN 1 ELSE 0 END) AS null_close,
                SUM(CASE WHEN volume IS NULL OR TRIM(volume) = '' THEN 1 ELSE 0 END) AS null_volume,
                COUNT(1) AS total
            FROM market_daily
            """
        ).fetchone()
        return {
            "total": int(row["total"] or 0),
            "null_close": int(row["null_close"] or 0),
            "null_volume": int(row["null_volume"] or 0),
        }

    def _check_announcement_fields(self, conn: sqlite3.Connection) -> Dict[str, Any]:
        row = conn.execute(
            """
            SELECT
                SUM(CASE WHEN pdf_url IS NULL OR TRIM(pdf_url) = '' THEN 1 ELSE 0 END) AS missing_pdf_url,
                SUM(CASE WHEN title IS NULL OR TRIM(title) = '' THEN 1 ELSE 0 END) AS missing_title,
                COUNT(1) AS total
            FROM announcements
            """
        ).fetchone()
        return {
            "total": int(row["total"] or 0),
            "missing_pdf_url": int(row["missing_pdf_url"] or 0),
            "missing_title": int(row["missing_title"] or 0),
        }

    def _check_entity_market(self, conn: sqlite3.Connection) -> Dict[str, Any]:
        rows = conn.execute(
            """
            SELECT stock_code, market
            FROM entities
            """
        ).fetchall()
        mismatches: List[Dict[str, str]] = []
        for row in rows:
            code = str(row["stock_code"] or "")
            market = str(row["market"] or "")
            expected = "sse" if code.startswith(("5", "6", "9")) else "szse"
            if market and market != expected:
                mismatches.append({"stock_code": code, "market": market, "expected": expected})
        return {
            "total_entities": len(rows),
            "market_mismatches": len(mismatches),
            "examples": mismatches[:20],
        }

    def _check_pdf_files(self, conn: sqlite3.Connection) -> Dict[str, Any]:
        rows = conn.execute("SELECT entry_key, file_path, sha256 FROM pdf_index").fetchall()
        missing = 0
        bad_hash = 0
        checked = 0
        examples = []
        import hashlib

        for row in rows:
            checked += 1
            file_path = Path(str(row["file_path"] or ""))
            expected_sha = str(row["sha256"] or "")
            if not file_path.exists():
                missing += 1
                if len(examples) < 20:
                    examples.append({"entry_key": row["entry_key"], "issue": "missing_file"})
                continue
            if expected_sha:
                h = hashlib.sha256()
                with file_path.open("rb") as fh:
                    while True:
                        chunk = fh.read(1024 * 1024)
                        if not chunk:
                            break
                        h.update(chunk)
                actual = h.hexdigest()
                if actual != expected_sha:
                    bad_hash += 1
                    if len(examples) < 20:
                        examples.append({"entry_key": row["entry_key"], "issue": "sha256_mismatch"})
        return {
            "checked": checked,
            "missing_files": missing,
            "sha256_mismatch": bad_hash,
            "examples": examples,
        }

    def _check_pdf_text_coverage(self, conn: sqlite3.Connection) -> Dict[str, Any]:
        pdf_count = self._table_count(conn, "pdf_index")
        txt_count = self._table_count(conn, "pdf_texts")
        coverage = 0.0 if pdf_count == 0 else round((txt_count * 100.0) / pdf_count, 2)
        return {"pdf_index_count": pdf_count, "pdf_text_count": txt_count, "coverage_pct": coverage}

    def _check_fundamentals_coverage(self, conn: sqlite3.Connection) -> Dict[str, Any]:
        rows = conn.execute(
            """
            SELECT dataset, fiscal_year, fiscal_quarter, COUNT(1) AS n
            FROM fundamentals_quarterly
            GROUP BY dataset, fiscal_year, fiscal_quarter
            ORDER BY fiscal_year DESC, fiscal_quarter DESC, dataset
            """
        ).fetchall()
        details = [
            {
                "dataset": row["dataset"],
                "fiscal_year": int(row["fiscal_year"]),
                "fiscal_quarter": int(row["fiscal_quarter"]),
                "rows": int(row["n"]),
            }
            for row in rows[:60]
        ]
        return {"group_count": len(rows), "latest_groups": details}

    def run_checks(self) -> Dict[str, Any]:
        if not UNIFIED_DB.exists():
            return {
                "success": False,
                "error": f"Unified DB not found: {UNIFIED_DB}",
                "data": [],
                "timestamp": _now_ts(),
            }

        with self._connect() as conn:
            table_counts = {
                "market_daily": self._table_count(conn, "market_daily"),
                "fundamentals_quarterly": self._table_count(conn, "fundamentals_quarterly"),
                "corporate_actions_adjust": self._table_count(conn, "corporate_actions_adjust"),
                "corporate_actions_dividend": self._table_count(conn, "corporate_actions_dividend"),
                "announcements": self._table_count(conn, "announcements"),
                "entities": self._table_count(conn, "entities"),
                "pdf_index": self._table_count(conn, "pdf_index"),
                "pdf_texts": self._table_count(conn, "pdf_texts"),
            }
            checks = {
                "market_nulls": self._check_market_nulls(conn),
                "announcement_fields": self._check_announcement_fields(conn),
                "entity_market": self._check_entity_market(conn),
                "pdf_files": self._check_pdf_files(conn),
                "pdf_text_coverage": self._check_pdf_text_coverage(conn),
                "fundamentals_coverage": self._check_fundamentals_coverage(conn),
            }

        score = 100.0
        score -= min(checks["market_nulls"]["null_close"], 20)
        score -= min(checks["announcement_fields"]["missing_pdf_url"], 20)
        score -= min(checks["pdf_files"]["missing_files"], 20)
        score -= min(checks["pdf_files"]["sha256_mismatch"], 20)
        score -= min(checks["entity_market"]["market_mismatches"], 20)
        score = max(0.0, round(score, 2))

        report = {
            "generated_at": _now_ts(),
            "db_file": str(UNIFIED_DB),
            "table_counts": table_counts,
            "checks": checks,
            "quality_score": score,
        }

        ts = datetime.now().strftime("%Y%m%d_%H%M%S")
        report_file = QUALITY_DIR / f"report_{ts}.json"
        report_file.write_text(json.dumps(report, ensure_ascii=True, indent=2), encoding="utf-8")
        REPORT_LATEST.write_text(json.dumps(report, ensure_ascii=True, indent=2), encoding="utf-8")

        return {
            "success": True,
            "data": {
                "quality_score": score,
                "report_file": str(report_file),
                "report_latest": str(REPORT_LATEST),
                "summary": {
                    "market_rows": table_counts["market_daily"],
                    "announcement_rows": table_counts["announcements"],
                    "pdf_rows": table_counts["pdf_index"],
                },
            },
            "timestamp": _now_ts(),
        }

    def get_latest_report(self) -> Dict[str, Any]:
        if not REPORT_LATEST.exists():
            return {"success": False, "error": "No latest report found", "data": [], "timestamp": _now_ts()}
        data = json.loads(REPORT_LATEST.read_text(encoding="utf-8"))
        return {"success": True, "data": data, "timestamp": _now_ts()}

    @staticmethod
    def get_all_endpoints() -> Dict[str, Any]:
        endpoints = ["get_all_endpoints", "run_checks", "get_latest_report"]
        return {
            "success": True,
            "data": {"available_endpoints": endpoints, "total_count": len(endpoints)},
            "count": len(endpoints),
            "timestamp": _now_ts(),
        }


def main() -> None:
    if sys.platform == "win32":
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8")

    wrapper = ChinaDataQualityChecks()
    endpoint_map = {
        "get_all_endpoints": wrapper.get_all_endpoints,
        "run_checks": wrapper.run_checks,
        "get_latest_report": wrapper.get_latest_report,
    }

    try:
        if len(sys.argv) < 2:
            _json(
                {
                    "success": False,
                    "error": "Usage: python china_data_quality_checks.py <endpoint>",
                    "available_endpoints": list(endpoint_map.keys()),
                }
            )
            return
        endpoint = sys.argv[1]
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

        result = method()
        _json(result)
    except Exception as e:  # pragma: no cover - defensive
        _json({"success": False, "error": str(e), "data": [], "timestamp": _now_ts()})


if __name__ == "__main__":
    main()

