#!/usr/bin/env python3
"""
CNINFO PDF Text Extractor
Extracts text from downloaded CNINFO PDFs with dedupe/checkpoint state.
"""

import hashlib
import io
import json
import sys
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Tuple


DATA_ROOT = Path(__file__).resolve().parent / "data" / "cninfo" / "pdf_text_extractor"
TEXT_DIR = DATA_ROOT / "texts"
STATE_FILE = DATA_ROOT / "state.json"
PDF_INDEX_FILE = Path(__file__).resolve().parent / "data" / "cninfo" / "pdf_downloader" / "index.json"


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


def _load_reader() -> Tuple[str, Any]:
    try:
        from pypdf import PdfReader  # type: ignore

        return "pypdf", PdfReader
    except Exception:
        pass
    try:
        from PyPDF2 import PdfReader  # type: ignore

        return "PyPDF2", PdfReader
    except Exception:
        pass
    raise RuntimeError(
        "Missing PDF parser. Install one: pip install pypdf (recommended) or pip install PyPDF2"
    )


class CNInfoPdfTextExtractor:
    def __init__(self) -> None:
        DATA_ROOT.mkdir(parents=True, exist_ok=True)
        TEXT_DIR.mkdir(parents=True, exist_ok=True)

    def _state(self) -> Dict[str, Any]:
        if not STATE_FILE.exists():
            return {"extracted": {}, "updated_at": _now_ts()}
        try:
            return json.loads(STATE_FILE.read_text(encoding="utf-8"))
        except Exception:
            return {"extracted": {}, "updated_at": _now_ts()}

    def _save_state(self, state: Dict[str, Any]) -> None:
        state["updated_at"] = _now_ts()
        STATE_FILE.write_text(json.dumps(state, ensure_ascii=True, indent=2), encoding="utf-8")

    @staticmethod
    def _text_out_path(pdf_path: Path) -> Path:
        return TEXT_DIR / f"{pdf_path.stem}.json"

    def extract_file(self, pdf_path: str, force: bool = False) -> Dict[str, Any]:
        path = Path(pdf_path)
        if not path.exists():
            return {"success": False, "error": f"PDF not found: {pdf_path}", "data": [], "timestamp": _now_ts()}

        state = self._state()
        extracted = state["extracted"]
        key = str(path.resolve())
        file_sha = _sha256_file(path)
        out_path = self._text_out_path(path)

        if key in extracted and extracted[key].get("sha256") == file_sha and out_path.exists() and not force:
            return {
                "success": True,
                "data": {
                    "status": "skipped_existing",
                    "pdf_path": str(path),
                    "text_file": str(out_path),
                    "sha256": file_sha,
                },
                "timestamp": _now_ts(),
            }

        parser_name, PdfReader = _load_reader()
        reader = PdfReader(str(path))
        pages: List[Dict[str, Any]] = []
        all_text_parts: List[str] = []
        for i, page in enumerate(reader.pages):
            try:
                text = page.extract_text() or ""
            except Exception:
                text = ""
            pages.append({"page": i + 1, "chars": len(text), "text": text})
            all_text_parts.append(text)

        full_text = "\n".join(all_text_parts)
        payload = {
            "pdf_path": str(path.resolve()),
            "pdf_name": path.name,
            "sha256": file_sha,
            "parser": parser_name,
            "page_count": len(pages),
            "char_count": len(full_text),
            "pages": pages,
            "full_text": full_text,
            "extracted_at": _now_ts(),
        }
        out_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")

        extracted[key] = {
            "sha256": file_sha,
            "text_file": str(out_path.resolve()),
            "page_count": len(pages),
            "char_count": len(full_text),
            "parser": parser_name,
            "extracted_at": _now_ts(),
        }
        self._save_state(state)

        return {
            "success": True,
            "data": {
                "status": "extracted",
                "pdf_path": str(path),
                "text_file": str(out_path),
                "page_count": len(pages),
                "char_count": len(full_text),
                "parser": parser_name,
            },
            "timestamp": _now_ts(),
        }

    def extract_from_index(self, max_files: int = 0, force: bool = False) -> Dict[str, Any]:
        if not PDF_INDEX_FILE.exists():
            return {
                "success": False,
                "error": f"PDF index not found: {PDF_INDEX_FILE}",
                "data": [],
                "timestamp": _now_ts(),
            }
        raw = json.loads(PDF_INDEX_FILE.read_text(encoding="utf-8"))
        entries = list((raw.get("entries") or {}).values())
        if max_files > 0:
            entries = entries[:max_files]

        extracted = 0
        skipped = 0
        failed = 0
        errors: List[Dict[str, Any]] = []
        for item in entries:
            pdf_path = item.get("file") or ""
            res = self.extract_file(pdf_path, force=force)
            if res.get("success"):
                status = (res.get("data") or {}).get("status")
                if status == "skipped_existing":
                    skipped += 1
                else:
                    extracted += 1
            else:
                failed += 1
                errors.append({"pdf_path": pdf_path, "error": res.get("error")})

        return {
            "success": True,
            "data": {
                "files_seen": len(entries),
                "extracted": extracted,
                "skipped": skipped,
                "failed": failed,
                "errors": errors[:20],
                "text_dir": str(TEXT_DIR),
                "state_file": str(STATE_FILE),
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
        endpoints = [
            "get_all_endpoints",
            "extract_file",
            "extract_from_index",
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

    wrapper = CNInfoPdfTextExtractor()
    endpoint_map = {
        "get_all_endpoints": wrapper.get_all_endpoints,
        "extract_file": wrapper.extract_file,
        "extract_from_index": wrapper.extract_from_index,
        "get_state": wrapper.get_state,
        "reset_state": wrapper.reset_state,
    }

    try:
        if len(sys.argv) < 2:
            _json(
                {
                    "success": False,
                    "error": "Usage: python cninfo_pdf_text_extractor.py <endpoint> [args...]",
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

        if endpoint == "extract_file":
            if len(args) < 1:
                _json({"success": False, "error": "extract_file requires: <pdf_path>"})
                return
            pdf_path = args[0]
            force = _to_bool(args[1]) if len(args) > 1 else False
            result = method(pdf_path, force)
        elif endpoint == "extract_from_index":
            max_files = int(args[0]) if len(args) > 0 and args[0] else 0
            force = _to_bool(args[1]) if len(args) > 1 else False
            result = method(max_files, force)
        else:
            result = method()

        _json(result)
    except Exception as e:  # pragma: no cover - defensive
        _json({"success": False, "error": str(e), "data": [], "timestamp": _now_ts()})


if __name__ == "__main__":
    main()

