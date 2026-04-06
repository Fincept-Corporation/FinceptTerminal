#!/usr/bin/env python3
"""
CNINFO PDF Downloader
Downloads announcement PDFs with retry, checksum, and dedupe index.
"""

import hashlib
import io
import json
import re
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Tuple

import requests


DATA_ROOT = Path(__file__).resolve().parent / "data" / "cninfo" / "pdf_downloader"
PDF_DIR = DATA_ROOT / "pdfs"
INDEX_FILE = DATA_ROOT / "index.json"
ANNOUNCEMENT_RECORDS_DIR = (
    Path(__file__).resolve().parent / "data" / "cninfo" / "announcements_incremental" / "records"
)


def _now_ts() -> int:
    return int(datetime.now().timestamp())


def _to_bool(value: Any) -> bool:
    return str(value).strip().lower() in {"1", "true", "yes", "y", "on"}


def _json(data: Dict[str, Any]) -> None:
    print(json.dumps(data, ensure_ascii=True, default=str))


def _sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        while True:
            chunk = fh.read(1024 * 1024)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def _safe_name(name: str) -> str:
    return re.sub(r"[^a-zA-Z0-9_\-.]", "_", name)


class CNInfoPdfDownloader:
    def __init__(self) -> None:
        PDF_DIR.mkdir(parents=True, exist_ok=True)
        self.session = requests.Session()
        self.session.headers.update(
            {
                "User-Agent": (
                    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                    "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36"
                )
            }
        )

    def _index(self) -> Dict[str, Any]:
        if not INDEX_FILE.exists():
            return {"entries": {}, "updated_at": _now_ts()}
        try:
            return json.loads(INDEX_FILE.read_text(encoding="utf-8"))
        except Exception:
            return {"entries": {}, "updated_at": _now_ts()}

    def _save_index(self, index: Dict[str, Any]) -> None:
        index["updated_at"] = _now_ts()
        INDEX_FILE.write_text(json.dumps(index, ensure_ascii=True, indent=2), encoding="utf-8")

    @staticmethod
    def _entry_key(announcement_id: str, url: str) -> str:
        if announcement_id:
            return announcement_id
        return hashlib.sha1(url.encode("utf-8")).hexdigest()

    @staticmethod
    def _filename(announcement_id: str, url: str) -> str:
        if announcement_id:
            return f"{_safe_name(announcement_id)}.pdf"
        return f"{hashlib.md5(url.encode('utf-8')).hexdigest()}.pdf"

    def _download_once(self, url: str, out_path: Path, timeout_s: int = 45) -> Tuple[str, int]:
        tmp_path = out_path.with_suffix(out_path.suffix + ".part")
        h = hashlib.sha256()
        size = 0
        with self.session.get(url, stream=True, timeout=timeout_s) as resp:
            resp.raise_for_status()
            with tmp_path.open("wb") as fh:
                for chunk in resp.iter_content(chunk_size=1024 * 1024):
                    if not chunk:
                        continue
                    fh.write(chunk)
                    h.update(chunk)
                    size += len(chunk)
        tmp_path.replace(out_path)
        return h.hexdigest(), size

    def download_one(
        self,
        url: str,
        announcement_id: str = "",
        stock_code: str = "",
        max_retries: int = 3,
        force: bool = False,
    ) -> Dict[str, Any]:
        if not url:
            return {"success": False, "error": "Missing URL", "data": [], "timestamp": _now_ts()}

        index = self._index()
        entries = index["entries"]
        key = self._entry_key(announcement_id, url)
        filename = self._filename(announcement_id, url)
        out_path = PDF_DIR / filename

        if key in entries and out_path.exists() and not force:
            expected = entries[key].get("sha256")
            if expected and _sha256_file(out_path) == expected:
                return {
                    "success": True,
                    "data": {
                        "status": "skipped_existing",
                        "key": key,
                        "file": str(out_path),
                        "sha256": expected,
                    },
                    "timestamp": _now_ts(),
                }

        last_err = ""
        for attempt in range(max_retries):
            try:
                sha256, size = self._download_once(url, out_path)
                entries[key] = {
                    "announcementId": announcement_id,
                    "stockCode": stock_code,
                    "url": url,
                    "file": str(out_path),
                    "sha256": sha256,
                    "size": size,
                    "downloaded_at": _now_ts(),
                }
                self._save_index(index)
                return {
                    "success": True,
                    "data": {
                        "status": "downloaded",
                        "key": key,
                        "file": str(out_path),
                        "sha256": sha256,
                        "size": size,
                    },
                    "timestamp": _now_ts(),
                }
            except Exception as e:
                last_err = str(e)
                time.sleep(1 + attempt)
                continue

        return {
            "success": False,
            "error": f"Download failed after retries: {last_err}",
            "data": [],
            "timestamp": _now_ts(),
        }

    @staticmethod
    def _load_records_file(path: Path) -> List[Dict[str, Any]]:
        if not path.exists():
            return []
        text = path.read_text(encoding="utf-8").strip()
        if not text:
            return []
        if path.suffix.lower() == ".jsonl":
            rows = []
            for line in text.splitlines():
                line = line.strip()
                if not line:
                    continue
                try:
                    rows.append(json.loads(line))
                except Exception:
                    continue
            return rows
        try:
            loaded = json.loads(text)
            if isinstance(loaded, list):
                return loaded
            if isinstance(loaded, dict):
                return [loaded]
        except Exception:
            pass
        return []

    def download_from_records_file(
        self,
        records_file: str,
        max_files: int = 0,
        force: bool = False,
    ) -> Dict[str, Any]:
        path = Path(records_file)
        rows = self._load_records_file(path)
        if max_files > 0:
            rows = rows[:max_files]

        downloaded = 0
        skipped = 0
        failed = 0
        errors = []
        for row in rows:
            ann_id = str(row.get("announcementId") or "")
            url = str(row.get("pdfUrl") or row.get("adjunctUrl") or "")
            stock_code = str(row.get("secCode") or "")
            if url and not url.startswith("http"):
                url = f"http://static.cninfo.com.cn/{url.lstrip('/')}"
            res = self.download_one(url=url, announcement_id=ann_id, stock_code=stock_code, force=force)
            if res.get("success"):
                status = (res.get("data") or {}).get("status")
                if status == "skipped_existing":
                    skipped += 1
                else:
                    downloaded += 1
            else:
                failed += 1
                errors.append({"announcementId": ann_id, "url": url, "error": res.get("error")})

        return {
            "success": True,
            "data": {
                "records_file": str(path),
                "records_count": len(rows),
                "downloaded": downloaded,
                "skipped": skipped,
                "failed": failed,
                "errors": errors[:20],
                "pdf_dir": str(PDF_DIR),
                "index_file": str(INDEX_FILE),
            },
            "timestamp": _now_ts(),
        }

    def download_from_stock_records(
        self,
        stock_code: str,
        category: str = "category_ndbg_szsh",
        max_files: int = 0,
        force: bool = False,
    ) -> Dict[str, Any]:
        code = re.sub(r"[^\d]", "", stock_code or "")[:6].zfill(6)
        safe_cat = re.sub(r"[^a-zA-Z0-9_\-]", "_", category)
        path = ANNOUNCEMENT_RECORDS_DIR / f"{code}_{safe_cat}.jsonl"
        return self.download_from_records_file(str(path), max_files=max_files, force=force)

    def download_from_urls(self, urls_csv: str, ids_csv: str = "", force: bool = False) -> Dict[str, Any]:
        urls = [x.strip() for x in urls_csv.split(",") if x.strip()]
        ids = [x.strip() for x in ids_csv.split(",")] if ids_csv else []
        downloaded = 0
        skipped = 0
        failed = 0
        errors = []

        for i, url in enumerate(urls):
            ann_id = ids[i] if i < len(ids) else ""
            res = self.download_one(url=url, announcement_id=ann_id, force=force)
            if res.get("success"):
                status = (res.get("data") or {}).get("status")
                if status == "skipped_existing":
                    skipped += 1
                else:
                    downloaded += 1
            else:
                failed += 1
                errors.append({"url": url, "error": res.get("error")})

        return {
            "success": True,
            "data": {
                "urls_count": len(urls),
                "downloaded": downloaded,
                "skipped": skipped,
                "failed": failed,
                "errors": errors[:20],
            },
            "timestamp": _now_ts(),
        }

    def verify_index(self) -> Dict[str, Any]:
        index = self._index()
        entries = index.get("entries", {})
        ok = 0
        missing = 0
        mismatch = 0
        bad_entries = []

        for key, meta in entries.items():
            path = Path(meta.get("file", ""))
            if not path.exists():
                missing += 1
                bad_entries.append({"key": key, "issue": "missing_file"})
                continue
            expected = meta.get("sha256")
            if expected:
                actual = _sha256_file(path)
                if actual != expected:
                    mismatch += 1
                    bad_entries.append({"key": key, "issue": "sha256_mismatch"})
                    continue
            ok += 1

        return {
            "success": True,
            "data": {
                "total_entries": len(entries),
                "ok": ok,
                "missing": missing,
                "mismatch": mismatch,
                "bad_entries": bad_entries[:50],
            },
            "timestamp": _now_ts(),
        }

    @staticmethod
    def get_all_endpoints() -> Dict[str, Any]:
        endpoints = [
            "get_all_endpoints",
            "download_one",
            "download_from_records_file",
            "download_from_stock_records",
            "download_from_urls",
            "verify_index",
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

    wrapper = CNInfoPdfDownloader()
    endpoint_map = {
        "get_all_endpoints": wrapper.get_all_endpoints,
        "download_one": wrapper.download_one,
        "download_from_records_file": wrapper.download_from_records_file,
        "download_from_stock_records": wrapper.download_from_stock_records,
        "download_from_urls": wrapper.download_from_urls,
        "verify_index": wrapper.verify_index,
    }

    try:
        if len(sys.argv) < 2:
            _json(
                {
                    "success": False,
                    "error": "Usage: python cninfo_pdf_downloader.py <endpoint> [args...]",
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

        if endpoint == "download_one":
            if len(args) < 1:
                _json({"success": False, "error": "download_one requires: <url>"})
                return
            url = args[0]
            announcement_id = args[1] if len(args) > 1 else ""
            stock_code = args[2] if len(args) > 2 else ""
            max_retries = int(args[3]) if len(args) > 3 and args[3] else 3
            force = _to_bool(args[4]) if len(args) > 4 else False
            result = method(url, announcement_id, stock_code, max_retries, force)
        elif endpoint == "download_from_records_file":
            if len(args) < 1:
                _json({"success": False, "error": "download_from_records_file requires: <records_file>"})
                return
            records_file = args[0]
            max_files = int(args[1]) if len(args) > 1 and args[1] else 0
            force = _to_bool(args[2]) if len(args) > 2 else False
            result = method(records_file, max_files, force)
        elif endpoint == "download_from_stock_records":
            if len(args) < 1:
                _json({"success": False, "error": "download_from_stock_records requires: <stock_code>"})
                return
            stock_code = args[0]
            category = args[1] if len(args) > 1 else "category_ndbg_szsh"
            max_files = int(args[2]) if len(args) > 2 and args[2] else 0
            force = _to_bool(args[3]) if len(args) > 3 else False
            result = method(stock_code, category, max_files, force)
        elif endpoint == "download_from_urls":
            if len(args) < 1:
                _json({"success": False, "error": "download_from_urls requires: <urls_csv>"})
                return
            urls_csv = args[0]
            ids_csv = args[1] if len(args) > 1 else ""
            force = _to_bool(args[2]) if len(args) > 2 else False
            result = method(urls_csv, ids_csv, force)
        else:
            result = method()

        _json(result)
    except Exception as e:  # pragma: no cover - defensive
        _json({"success": False, "error": str(e), "data": [], "timestamp": _now_ts()})


if __name__ == "__main__":
    main()

