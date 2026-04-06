#!/usr/bin/env python3
"""
China Data Unified Store
Normalizes BaoStock + CNINFO outputs into one SQLite store.
"""

import csv
import io
import json
import sqlite3
import sys
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List


DATA_ROOT = Path(__file__).resolve().parent / "data" / "china" / "unified_store"
DB_FILE = DATA_ROOT / "china_data.db"

BAO_DAILY_DIR = Path(__file__).resolve().parent / "data" / "baostock" / "daily_backfill"
BAO_FUND_DIR = Path(__file__).resolve().parent / "data" / "baostock" / "fundamentals_quarterly"
BAO_CA_ADJUST_DIR = (
    Path(__file__).resolve().parent / "data" / "baostock" / "corporate_actions" / "adjust_factor"
)
BAO_CA_DIV_DIR = (
    Path(__file__).resolve().parent / "data" / "baostock" / "corporate_actions" / "dividend"
)

CN_ANN_DIR = (
    Path(__file__).resolve().parent / "data" / "cninfo" / "announcements_incremental" / "records"
)
CN_ENTITIES_JSONL = (
    Path(__file__).resolve().parent / "data" / "cninfo" / "entity_resolver" / "entities.jsonl"
)
CN_PDF_INDEX = Path(__file__).resolve().parent / "data" / "cninfo" / "pdf_downloader" / "index.json"
CN_PDF_TEXT_STATE = (
    Path(__file__).resolve().parent / "data" / "cninfo" / "pdf_text_extractor" / "state.json"
)


def _now_ts() -> int:
    return int(datetime.now().timestamp())


def _json(data: Dict[str, Any]) -> None:
    print(json.dumps(data, ensure_ascii=True, default=str))


class ChinaDataUnifiedStore:
    def __init__(self) -> None:
        DATA_ROOT.mkdir(parents=True, exist_ok=True)
        self._init_db()

    def _connect(self) -> sqlite3.Connection:
        conn = sqlite3.connect(DB_FILE)
        conn.row_factory = sqlite3.Row
        return conn

    def _init_db(self) -> None:
        with self._connect() as conn:
            conn.execute(
                """
                CREATE TABLE IF NOT EXISTS market_daily (
                    date TEXT NOT NULL,
                    code TEXT NOT NULL,
                    adjustflag TEXT NOT NULL DEFAULT '',
                    open TEXT,
                    high TEXT,
                    low TEXT,
                    close TEXT,
                    volume TEXT,
                    amount TEXT,
                    turn TEXT,
                    pctChg TEXT,
                    raw_json TEXT NOT NULL,
                    PRIMARY KEY (date, code, adjustflag)
                )
                """
            )
            conn.execute(
                """
                CREATE TABLE IF NOT EXISTS fundamentals_quarterly (
                    dataset TEXT NOT NULL,
                    source_code TEXT NOT NULL,
                    fiscal_year INTEGER NOT NULL,
                    fiscal_quarter INTEGER NOT NULL,
                    raw_json TEXT NOT NULL,
                    PRIMARY KEY (dataset, source_code, fiscal_year, fiscal_quarter)
                )
                """
            )
            conn.execute(
                """
                CREATE TABLE IF NOT EXISTS corporate_actions_adjust (
                    source_code TEXT NOT NULL,
                    dividOperateDate TEXT NOT NULL,
                    raw_json TEXT NOT NULL,
                    PRIMARY KEY (source_code, dividOperateDate)
                )
                """
            )
            conn.execute(
                """
                CREATE TABLE IF NOT EXISTS corporate_actions_dividend (
                    source_code TEXT NOT NULL,
                    dividend_year INTEGER NOT NULL,
                    dividOperateDate TEXT NOT NULL,
                    raw_json TEXT NOT NULL,
                    PRIMARY KEY (source_code, dividend_year, dividOperateDate)
                )
                """
            )
            conn.execute(
                """
                CREATE TABLE IF NOT EXISTS announcements (
                    announcement_id TEXT PRIMARY KEY,
                    sec_code TEXT,
                    sec_name TEXT,
                    org_id TEXT,
                    category TEXT,
                    announcement_time INTEGER,
                    title TEXT,
                    pdf_url TEXT,
                    raw_json TEXT NOT NULL
                )
                """
            )
            conn.execute(
                """
                CREATE TABLE IF NOT EXISTS entities (
                    stock_code TEXT PRIMARY KEY,
                    org_id TEXT,
                    sec_code TEXT,
                    sec_name TEXT,
                    market TEXT,
                    raw_json TEXT NOT NULL
                )
                """
            )
            conn.execute(
                """
                CREATE TABLE IF NOT EXISTS pdf_index (
                    entry_key TEXT PRIMARY KEY,
                    announcement_id TEXT,
                    stock_code TEXT,
                    url TEXT,
                    file_path TEXT,
                    sha256 TEXT,
                    size INTEGER,
                    downloaded_at INTEGER,
                    raw_json TEXT NOT NULL
                )
                """
            )
            conn.execute(
                """
                CREATE TABLE IF NOT EXISTS pdf_texts (
                    pdf_path TEXT PRIMARY KEY,
                    sha256 TEXT,
                    text_file TEXT,
                    page_count INTEGER,
                    char_count INTEGER,
                    parser TEXT,
                    extracted_at INTEGER,
                    raw_json TEXT NOT NULL
                )
                """
            )
            conn.commit()

    @staticmethod
    def _list_files(directory: Path, pattern: str, max_files: int = 0) -> List[Path]:
        if not directory.exists():
            return []
        files = sorted(directory.glob(pattern))
        if max_files > 0:
            return files[:max_files]
        return files

    def ingest_baostock_daily(self, max_files: int = 0) -> Dict[str, Any]:
        files = self._list_files(BAO_DAILY_DIR, "*.csv", max_files=max_files)
        inserted = 0
        with self._connect() as conn:
            for path in files:
                with path.open("r", encoding="utf-8") as fh:
                    reader = csv.DictReader(fh)
                    for row in reader:
                        if not row.get("date") or not row.get("code"):
                            continue
                        conn.execute(
                            """
                            INSERT OR REPLACE INTO market_daily
                            (date, code, adjustflag, open, high, low, close, volume, amount, turn, pctChg, raw_json)
                            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                            """,
                            (
                                row.get("date"),
                                row.get("code"),
                                row.get("adjustflag", ""),
                                row.get("open"),
                                row.get("high"),
                                row.get("low"),
                                row.get("close"),
                                row.get("volume"),
                                row.get("amount"),
                                row.get("turn"),
                                row.get("pctChg"),
                                json.dumps(row, ensure_ascii=True),
                            ),
                        )
                        inserted += 1
            conn.commit()
        return {"files": len(files), "rows_upserted": inserted}

    def ingest_fundamentals(self, max_files: int = 0) -> Dict[str, Any]:
        files = self._list_files(BAO_FUND_DIR, "*.csv", max_files=max_files)
        inserted = 0
        with self._connect() as conn:
            for path in files:
                stem = path.stem
                if "_" not in stem:
                    continue
                dataset, period = stem.rsplit("_", 1)
                if "Q" not in period:
                    continue
                year = int(period.split("Q")[0])
                quarter = int(period.split("Q")[1])
                with path.open("r", encoding="utf-8") as fh:
                    reader = csv.DictReader(fh)
                    for row in reader:
                        source_code = row.get("source_code") or row.get("code") or ""
                        if not source_code:
                            continue
                        conn.execute(
                            """
                            INSERT OR REPLACE INTO fundamentals_quarterly
                            (dataset, source_code, fiscal_year, fiscal_quarter, raw_json)
                            VALUES (?, ?, ?, ?, ?)
                            """,
                            (
                                dataset,
                                source_code,
                                year,
                                quarter,
                                json.dumps(row, ensure_ascii=True),
                            ),
                        )
                        inserted += 1
            conn.commit()
        return {"files": len(files), "rows_upserted": inserted}

    def ingest_corporate_actions(self, max_files: int = 0) -> Dict[str, Any]:
        adjust_files = self._list_files(BAO_CA_ADJUST_DIR, "*.csv", max_files=max_files)
        div_files = self._list_files(BAO_CA_DIV_DIR, "*.csv", max_files=max_files)
        adjust_rows = 0
        div_rows = 0

        with self._connect() as conn:
            for path in adjust_files:
                with path.open("r", encoding="utf-8") as fh:
                    reader = csv.DictReader(fh)
                    for row in reader:
                        source_code = row.get("source_code") or row.get("code") or ""
                        date = row.get("dividOperateDate") or ""
                        if not source_code or not date:
                            continue
                        conn.execute(
                            """
                            INSERT OR REPLACE INTO corporate_actions_adjust
                            (source_code, dividOperateDate, raw_json)
                            VALUES (?, ?, ?)
                            """,
                            (source_code, date, json.dumps(row, ensure_ascii=True)),
                        )
                        adjust_rows += 1

            for path in div_files:
                with path.open("r", encoding="utf-8") as fh:
                    reader = csv.DictReader(fh)
                    for row in reader:
                        source_code = row.get("source_code") or row.get("code") or ""
                        date = row.get("dividOperateDate") or ""
                        year = int(row.get("dividend_year") or path.stem.split("_")[-1])
                        if not source_code or not date:
                            continue
                        conn.execute(
                            """
                            INSERT OR REPLACE INTO corporate_actions_dividend
                            (source_code, dividend_year, dividOperateDate, raw_json)
                            VALUES (?, ?, ?, ?)
                            """,
                            (source_code, year, date, json.dumps(row, ensure_ascii=True)),
                        )
                        div_rows += 1
            conn.commit()

        return {
            "adjust_files": len(adjust_files),
            "dividend_files": len(div_files),
            "adjust_rows_upserted": adjust_rows,
            "dividend_rows_upserted": div_rows,
        }

    def ingest_announcements(self, max_files: int = 0) -> Dict[str, Any]:
        files = self._list_files(CN_ANN_DIR, "*.jsonl", max_files=max_files)
        inserted = 0
        with self._connect() as conn:
            for path in files:
                for line in path.read_text(encoding="utf-8").splitlines():
                    line = line.strip()
                    if not line:
                        continue
                    try:
                        row = json.loads(line)
                    except Exception:
                        continue
                    ann_id = str(row.get("announcementId") or "")
                    if not ann_id:
                        continue
                    conn.execute(
                        """
                        INSERT OR REPLACE INTO announcements
                        (announcement_id, sec_code, sec_name, org_id, category, announcement_time, title, pdf_url, raw_json)
                        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
                        """,
                        (
                            ann_id,
                            row.get("secCode"),
                            row.get("secName"),
                            row.get("orgId"),
                            row.get("category"),
                            int(row.get("announcementTime") or 0),
                            row.get("announcementTitle"),
                            row.get("pdfUrl"),
                            json.dumps(row, ensure_ascii=True),
                        ),
                    )
                    inserted += 1
            conn.commit()
        return {"files": len(files), "rows_upserted": inserted}

    def ingest_entities(self) -> Dict[str, Any]:
        if not CN_ENTITIES_JSONL.exists():
            return {"files": 0, "rows_upserted": 0}
        inserted = 0
        with self._connect() as conn:
            for line in CN_ENTITIES_JSONL.read_text(encoding="utf-8").splitlines():
                line = line.strip()
                if not line:
                    continue
                try:
                    row = json.loads(line)
                except Exception:
                    continue
                stock_code = str(row.get("stock_code") or "")
                if not stock_code:
                    continue
                conn.execute(
                    """
                    INSERT OR REPLACE INTO entities
                    (stock_code, org_id, sec_code, sec_name, market, raw_json)
                    VALUES (?, ?, ?, ?, ?, ?)
                    """,
                    (
                        stock_code,
                        row.get("org_id"),
                        row.get("sec_code"),
                        row.get("sec_name"),
                        row.get("market"),
                        json.dumps(row, ensure_ascii=True),
                    ),
                )
                inserted += 1
            conn.commit()
        return {"files": 1, "rows_upserted": inserted}

    def ingest_pdf_index(self) -> Dict[str, Any]:
        if not CN_PDF_INDEX.exists():
            return {"files": 0, "rows_upserted": 0}
        raw = json.loads(CN_PDF_INDEX.read_text(encoding="utf-8"))
        entries = raw.get("entries") or {}
        inserted = 0
        with self._connect() as conn:
            for key, row in entries.items():
                conn.execute(
                    """
                    INSERT OR REPLACE INTO pdf_index
                    (entry_key, announcement_id, stock_code, url, file_path, sha256, size, downloaded_at, raw_json)
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
                    """,
                    (
                        key,
                        row.get("announcementId"),
                        row.get("stockCode"),
                        row.get("url"),
                        row.get("file"),
                        row.get("sha256"),
                        int(row.get("size") or 0),
                        int(row.get("downloaded_at") or 0),
                        json.dumps(row, ensure_ascii=True),
                    ),
                )
                inserted += 1
            conn.commit()
        return {"files": 1, "rows_upserted": inserted}

    def ingest_pdf_texts(self) -> Dict[str, Any]:
        if not CN_PDF_TEXT_STATE.exists():
            return {"files": 0, "rows_upserted": 0}
        raw = json.loads(CN_PDF_TEXT_STATE.read_text(encoding="utf-8"))
        entries = raw.get("extracted") or {}
        inserted = 0
        with self._connect() as conn:
            for pdf_path, row in entries.items():
                conn.execute(
                    """
                    INSERT OR REPLACE INTO pdf_texts
                    (pdf_path, sha256, text_file, page_count, char_count, parser, extracted_at, raw_json)
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?)
                    """,
                    (
                        pdf_path,
                        row.get("sha256"),
                        row.get("text_file"),
                        int(row.get("page_count") or 0),
                        int(row.get("char_count") or 0),
                        row.get("parser"),
                        int(row.get("extracted_at") or 0),
                        json.dumps(row, ensure_ascii=True),
                    ),
                )
                inserted += 1
            conn.commit()
        return {"files": 1, "rows_upserted": inserted}

    def run_all(self, max_files: int = 0) -> Dict[str, Any]:
        self._init_db()
        res = {
            "baostock_daily": self.ingest_baostock_daily(max_files=max_files),
            "fundamentals": self.ingest_fundamentals(max_files=max_files),
            "corporate_actions": self.ingest_corporate_actions(max_files=max_files),
            "announcements": self.ingest_announcements(max_files=max_files),
            "entities": self.ingest_entities(),
            "pdf_index": self.ingest_pdf_index(),
            "pdf_texts": self.ingest_pdf_texts(),
        }
        return {"success": True, "data": res, "timestamp": _now_ts()}

    def stats(self) -> Dict[str, Any]:
        tables = [
            "market_daily",
            "fundamentals_quarterly",
            "corporate_actions_adjust",
            "corporate_actions_dividend",
            "announcements",
            "entities",
            "pdf_index",
            "pdf_texts",
        ]
        out = {}
        with self._connect() as conn:
            for table in tables:
                cnt = conn.execute(f"SELECT COUNT(1) AS c FROM {table}").fetchone()["c"]
                out[table] = int(cnt)
        return {
            "success": True,
            "data": {"db_file": str(DB_FILE), "table_counts": out},
            "timestamp": _now_ts(),
        }

    @staticmethod
    def get_all_endpoints() -> Dict[str, Any]:
        endpoints = [
            "get_all_endpoints",
            "init_db",
            "run_all",
            "stats",
            "ingest_baostock_daily",
            "ingest_fundamentals",
            "ingest_corporate_actions",
            "ingest_announcements",
            "ingest_entities",
            "ingest_pdf_index",
            "ingest_pdf_texts",
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

    wrapper = ChinaDataUnifiedStore()
    endpoint_map = {
        "get_all_endpoints": wrapper.get_all_endpoints,
        "init_db": lambda: {"success": True, "data": {"db_file": str(DB_FILE)}, "timestamp": _now_ts()},
        "run_all": wrapper.run_all,
        "stats": wrapper.stats,
        "ingest_baostock_daily": wrapper.ingest_baostock_daily,
        "ingest_fundamentals": wrapper.ingest_fundamentals,
        "ingest_corporate_actions": wrapper.ingest_corporate_actions,
        "ingest_announcements": wrapper.ingest_announcements,
        "ingest_entities": wrapper.ingest_entities,
        "ingest_pdf_index": wrapper.ingest_pdf_index,
        "ingest_pdf_texts": wrapper.ingest_pdf_texts,
    }

    try:
        if len(sys.argv) < 2:
            _json(
                {
                    "success": False,
                    "error": "Usage: python china_data_unified_store.py <endpoint> [args...]",
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

        if endpoint in {
            "run_all",
            "ingest_baostock_daily",
            "ingest_fundamentals",
            "ingest_corporate_actions",
            "ingest_announcements",
            "ingest_entities",
            "ingest_pdf_index",
            "ingest_pdf_texts",
        }:
            if endpoint in {"ingest_entities", "ingest_pdf_index", "ingest_pdf_texts"}:
                result = {"success": True, "data": method(), "timestamp": _now_ts()}
            else:
                max_files = int(args[0]) if len(args) > 0 and args[0] else 0
                result = method(max_files)
                if endpoint != "run_all":
                    result = {"success": True, "data": result, "timestamp": _now_ts()}
        else:
            result = method()

        _json(result)
    except Exception as e:  # pragma: no cover - defensive
        _json({"success": False, "error": str(e), "data": [], "timestamp": _now_ts()})


if __name__ == "__main__":
    main()
