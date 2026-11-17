"""SEC Filing Scanner for M&A Deal Detection

Real data sources (no mocks, no demo data):
  1. SEC EDGAR Full-Text Search API  (efts.sec.gov)  - primary, no key needed
  2. SEC EDGAR submissions endpoint   (data.sec.gov/submissions/) - per-CIK filing history
  3. SEC company ticker list          (www.sec.gov/files/company_tickers.json) - name / ticker -> CIK

All HTTP calls use the standard library (urllib): no edgartools, no extra dependencies.

SEC fair-access policy (https://www.sec.gov/os/accessing-edgar-data): every request declares a User-Agent naming the
requester, and the whole process stays under 10 requests per second - _SEC_LIMITER spaces request starts at least
1/8 s apart across all threads. On HTTP 403/429 (SEC's "request rate threshold exceeded") the scanner stops sending
requests for the rest of the run instead of retrying into a longer block, and says so.

CLI: python deal_scanner.py scan '{"days_back": 30}' | scan_company '{"ticker": "MSFT", "years_back": 5}'
     | high_confidence '{"min_confidence": 0.75}'   (native: positional scalars)
"""
import sys
import json
import re
import threading
import time
from pathlib import Path
from typing import List, Dict, Any, Optional, Callable
from datetime import datetime, timedelta
from urllib.request import urlopen, Request
from urllib.error import URLError, HTTPError
from urllib.parse import quote

scripts_path = Path(__file__).parent.parent.parent.parent
sys.path.append(str(scripts_path))

analytics_path = Path(__file__).parent.parent.parent
sys.path.insert(0, str(analytics_path))

from corporateFinance.config import MATERIAL_EVENT_ITEMS
from corporateFinance.deal_database.database_schema import MADatabase

# ---------------------------------------------------------------------------
# HTTP helpers (shared with deal_parser)
# ---------------------------------------------------------------------------

# The declared requester. Unchanged: this is the contact every scan has always sent.
_EDGAR_HEADERS = {
    "User-Agent": "Fincept Terminal support@fincept.in",   # SEC requires a valid UA
    "Accept": "application/json",
}

SEC_MAX_REQUESTS_PER_SECOND = 8   # SEC allows 10; keep a margin for clock jitter


class _RateLimiter:
    """Spaces request start times at least 1/rate seconds apart, across threads."""

    def __init__(self, per_second: float):
        self._interval = 1.0 / per_second
        self._lock = threading.Lock()
        self._next = 0.0
        self.requests = 0

    def wait(self):
        with self._lock:
            now = time.monotonic()
            slot = max(now, self._next)
            self._next = slot + self._interval
            self.requests += 1
        delay = slot - time.monotonic()
        if delay > 0:
            time.sleep(delay)


_SEC_LIMITER = _RateLimiter(SEC_MAX_REQUESTS_PER_SECOND)
# Set once SEC answers 403/429: no further requests are sent in this process.
_SEC_STATE = {"throttled": False, "throttle_code": None}


def sec_throttled() -> bool:
    return bool(_SEC_STATE["throttled"])


def sec_request_count() -> int:
    return _SEC_LIMITER.requests


def _open(url: str, accept: str, timeout: int):
    if _SEC_STATE["throttled"]:
        raise ConnectionRefusedError("SEC rate limit reached earlier in this run")
    _SEC_LIMITER.wait()
    req = Request(url, headers={**_EDGAR_HEADERS, "Accept": accept})
    try:
        return urlopen(req, timeout=timeout)
    except HTTPError as exc:
        if exc.code in (403, 429):
            _SEC_STATE["throttled"] = True
            _SEC_STATE["throttle_code"] = exc.code
            print(f"[Scanner] SEC answered HTTP {exc.code}: rate limit - no further requests this run",
                  file=sys.stderr, flush=True)
        raise


def _get(url: str, timeout: int = 15) -> Optional[dict]:
    """GET JSON from a URL, return parsed dict or None on error."""
    try:
        with _open(url, "application/json", timeout) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except HTTPError as exc:
        if exc.code not in (404, 403, 429):  # 404s are expected for optional index files
            print(f"[Scanner] HTTP {exc.code} {url}", file=sys.stderr, flush=True)
        return None
    except Exception:
        return None


def http_get_text(url: str, timeout: int = 30, max_bytes: int = 1_500_000) -> str:
    """GET raw text (at most `max_bytes`). '' on any error."""
    try:
        with _open(url, "text/plain,text/html", timeout) as resp:
            raw = resp.read(max_bytes)
    except Exception as exc:
        if not isinstance(exc, (HTTPError, ConnectionRefusedError)):
            print(f"[Scanner] EDGAR fetch failed {url}: {exc}", file=sys.stderr, flush=True)
        return ""
    try:
        return raw.decode("utf-8")
    except UnicodeDecodeError:
        return raw.decode("latin-1")


def filing_index_url(cik: str, accession_number: str) -> str:
    """The filing's own index page on sec.gov (lists the form and every exhibit)."""
    cik_path = str(cik or "").strip().lstrip("0")
    acc = (accession_number or "").strip()
    if not cik_path or not acc:
        return ""
    return f"https://www.sec.gov/Archives/edgar/data/{cik_path}/{acc.replace('-', '')}/{acc}-index.htm"


# ---------------------------------------------------------------------------
# Company names -> CIK / ticker (SEC company_tickers.json, fetched once per run)
# ---------------------------------------------------------------------------

_TICKERS: Dict[str, Any] = {"loaded": False, "by_name": {}, "by_ticker": {}}
_SUFFIX_RE = re.compile(r"\b(incorporated|inc|corporation|corp|company|co|limited|ltd|llc|l\.?p|plc|n\.?v|s\.?a|ag|se|"
                        r"holdings?|group|the)\b\.?", re.I)


def normalize_company_name(name: str) -> str:
    n = _SUFFIX_RE.sub(" ", (name or "").lower())
    n = re.sub(r"[^a-z0-9]+", " ", n)
    return " ".join(n.split())


def _load_tickers():
    if _TICKERS["loaded"]:
        return
    _TICKERS["loaded"] = True
    data = _get("https://www.sec.gov/files/company_tickers.json", timeout=30)
    if not data:
        return
    for entry in data.values():
        cik = str(entry.get("cik_str", ""))
        ticker = str(entry.get("ticker", "")).upper()
        title = entry.get("title", "")
        if ticker:
            _TICKERS["by_ticker"].setdefault(ticker, (cik, title))
        key = normalize_company_name(title)
        if key:
            _TICKERS["by_name"].setdefault(key, (cik, ticker))


def lookup_company(name: str) -> Optional[Dict[str, str]]:
    """Exact (suffix-insensitive) name match against SEC's ticker list: {cik, ticker} or None. No fuzzy matching:
    a wrong CIK is worse than none."""
    key = normalize_company_name(name)
    if not key:
        return None
    _load_tickers()
    hit = _TICKERS["by_name"].get(key)
    return {"cik": hit[0], "ticker": hit[1]} if hit else None


# ---------------------------------------------------------------------------
# Scoring and industry
# ---------------------------------------------------------------------------

_HIGH_CONFIDENCE_FORMS = {'S-4', 'DEFM14A', 'PREM14A', 'SC TO-T', 'SC 13E-3', 'SC 14D9'}
_DEAL_ITEMS = {'1.01', '2.01'}
DEFAULT_SCAN_FORMS = ["S-4", "DEFM14A", "PREM14A", "8-K", "425", "SC TO-T", "SC 14D9", "SC 13E-3"]


def _score_hit(form_type: str, items: List[str]) -> float:
    """Confidence that a full-text hit is an M&A filing, from its form and 8-K items (no extra request)."""
    if form_type in _HIGH_CONFIDENCE_FORMS:
        return 0.90
    if form_type == '8-K':
        if any(i in _DEAL_ITEMS for i in items):
            return 0.80
        if any(i in MATERIAL_EVENT_ITEMS for i in items):
            return 0.60
        return 0.50
    if form_type == '425':   # business-combination communication, M&A by definition
        return 0.70
    if form_type == 'SC 13D':
        return 0.55
    return 0.50


def industry_from_sic(sic: str) -> Optional[str]:
    """Broad industry for a 4-digit SIC code (the filer's own classification on EDGAR)."""
    try:
        s = int(str(sic).strip()[:4])
    except (TypeError, ValueError):
        return None
    ranges = [
        (100, 999, 'Consumer Staples'), (1000, 1099, 'Materials'), (1200, 1399, 'Energy'),
        (1400, 1499, 'Materials'), (1500, 1799, 'Industrials'), (2000, 2199, 'Consumer Staples'),
        (2200, 2399, 'Consumer Discretionary'), (2400, 2499, 'Materials'), (2500, 2599, 'Consumer Discretionary'),
        (2600, 2699, 'Materials'), (2700, 2799, 'Media'), (2800, 2829, 'Materials'), (2830, 2839, 'Healthcare'),
        (2840, 2849, 'Consumer Staples'), (2850, 2899, 'Materials'), (2900, 2999, 'Energy'),
        (3000, 3399, 'Materials'), (3400, 3569, 'Industrials'), (3570, 3579, 'Technology'),
        (3580, 3599, 'Industrials'), (3630, 3639, 'Consumer Discretionary'), (3600, 3699, 'Technology'),
        (3710, 3716, 'Consumer Discretionary'), (3700, 3799, 'Industrials'), (3840, 3851, 'Healthcare'),
        (3800, 3899, 'Technology'), (3900, 3999, 'Consumer Discretionary'), (4000, 4799, 'Industrials'),
        (4830, 4841, 'Media'), (4800, 4899, 'Telecommunications'), (4900, 4999, 'Utilities'),
        (5122, 5122, 'Healthcare'), (5000, 5199, 'Industrials'), (5400, 5499, 'Consumer Staples'),
        (5910, 5912, 'Consumer Staples'), (5200, 5999, 'Consumer Discretionary'),
        (6798, 6798, 'Real Estate'), (6500, 6599, 'Real Estate'), (6000, 6799, 'Financial Services'),
        (7370, 7379, 'Technology'), (7300, 7399, 'Industrials'), (7800, 7899, 'Media'), (7000, 7999, 'Consumer Discretionary'),
        (8000, 8099, 'Healthcare'), (8731, 8731, 'Healthcare'), (8100, 8999, 'Industrials'),
    ]
    for lo, hi, name in ranges:   # specific ranges are listed before the broad ones that contain them
        if lo <= s <= hi:
            return name
    return None


# ---------------------------------------------------------------------------
# EDGAR Full-Text Search (efts.sec.gov)
# ---------------------------------------------------------------------------

def _efts_search(query_term: str, form_types: List[str], start_date: str, end_date: str) -> List[Dict[str, Any]]:
    """One full-text search request (up to 100 hits): hit dicts with accession_number, entity_name, ..."""
    forms_str = ",".join(quote(f, safe="") for f in form_types)
    url = (f"https://efts.sec.gov/LATEST/search-index?q=%22{quote(query_term, safe='')}%22&forms={forms_str}"
           f"&dateRange=custom&startdt={start_date}&enddt={end_date}")
    data = _get(url)
    if not data:
        return []
    results = []
    for hit in data.get("hits", {}).get("hits", []):
        src = hit.get("_source", {})
        acc = src.get("adsh", "")
        if not acc:
            raw_id = hit.get("_id", "")
            acc = raw_id.split(":")[0] if ":" in raw_id else raw_id
        if not acc:
            continue
        # display_names: "Company Name  (TICK)  (CIK 0000XXXXXX)"
        display = (src.get("display_names") or [""])[0]
        entity_name = display.split("(")[0].strip()
        tick = re.search(r"\(([A-Z0-9.\-]{1,10}(?:,\s*[A-Z0-9.\-]{1,10})*)\)\s*\(CIK", display)
        ciks = src.get("ciks", [])
        form = src.get("form") or ((src.get("root_forms") or [""])[0])
        results.append({
            "accession_number": acc,
            "entity_name": entity_name,
            "ticker": tick.group(1).split(",")[0].strip() if tick else "",
            "form_type": form,
            "file_date": src.get("file_date", ""),
            "cik": ciks[0].lstrip("0") if ciks else "",
            "items": src.get("items") or [],
            "sic": (src.get("sics") or [""])[0],
            "query": query_term,
        })
    return results


def _get_company_recent_filings(cik_str: str, form_types: List[str],
                                start_date: str, end_date: str) -> List[Dict[str, Any]]:
    """Recent filings for one CIK via data.sec.gov/submissions/."""
    data = _get(f"https://data.sec.gov/submissions/CIK{cik_str.zfill(10)}.json")
    if not data:
        return []
    entity_name = data.get("name", "")
    sic = str(data.get("sic", ""))
    tickers = data.get("tickers") or []
    recent = data.get("filings", {}).get("recent", {})
    results = []
    for form, date_str, acc, items in zip(recent.get("form", []), recent.get("filingDate", []),
                                          recent.get("accessionNumber", []),
                                          recent.get("items", [""] * len(recent.get("form", [])))):
        if form not in form_types or date_str < start_date or date_str > end_date:
            continue
        results.append({
            "accession_number": acc, "entity_name": entity_name, "ticker": tickers[0] if tickers else "",
            "form_type": form, "file_date": date_str, "cik": cik_str,
            "items": [i.strip() for i in str(items or "").split(",") if i.strip()], "sic": sic, "query": "",
        })
    return results


# ---------------------------------------------------------------------------
# Main scanner class
# ---------------------------------------------------------------------------

ProgressFn = Callable[[str, int, int, str], None]


class MADealScanner:
    """
    Find M&A filings on SEC EDGAR (all free, no API key, no third-party library):
      1. Full-text search (efts.sec.gov) with M&A trigger phrases across the M&A form types - one request per
         phrase, scored from the form type and 8-K items EDGAR returns with each hit.
      2. Persist the filings in the local SQLite database (sec_filings).
    Reading a filing and extracting the deal terms is deal_parser's job.
    """

    # M&A trigger phrases sent to EFTS - each returns REAL matching filings
    _SEARCH_QUERIES = [
        "agreement and plan of merger",
        "definitive merger agreement",
        "definitive agreement to acquire",
        "tender offer",
        "merger of equals",
        "going private transaction",
        "acquire all of the outstanding shares",
        "stock purchase agreement",
        "asset purchase agreement",
        "business combination agreement",
    ]

    _SCAN_FORMS = DEFAULT_SCAN_FORMS

    def __init__(self, db: Optional[MADatabase] = None):
        self.db = db or MADatabase()

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def scan_recent_filings(self, days_back: int = 30, max_workers: int = 1,
                            forms: Optional[List[str]] = None,
                            progress: Optional[ProgressFn] = None) -> List[Dict[str, Any]]:
        """
        Search EDGAR for M&A filings in the last `days_back` days; store and return them (newest first).
        `max_workers` is kept for compatibility: the requests are sequential and rate-limited.
        """
        end_date = datetime.now()
        start_date = end_date - timedelta(days=days_back)
        start_str, end_str = start_date.strftime("%Y-%m-%d"), end_date.strftime("%Y-%m-%d")
        form_types = [f for f in (forms or self._SCAN_FORMS) if f]
        print(f"[Scanner] Scanning EDGAR {start_str} -> {end_str} forms={','.join(form_types)}",
              file=sys.stderr, flush=True)

        all_filings: Dict[str, Dict[str, Any]] = {}
        total = len(self._SEARCH_QUERIES)
        for i, query in enumerate(self._SEARCH_QUERIES, start=1):
            if sec_throttled():
                break
            for hit in _efts_search(query, form_types, start_str, end_str):
                acc = hit["accession_number"]
                if acc in all_filings:
                    all_filings[acc]["queries"].add(query)
                    continue
                scored = self._score_and_enrich(hit)
                if scored:
                    scored["queries"] = {query}
                    all_filings[acc] = scored
            if progress:
                progress("search", i, total, f"Searching SEC full-text index ({i} of {total} phrases) - "
                                             f"{len(all_filings)} filings found")

        results = []
        for f in all_filings.values():
            queries = sorted(f.pop("queries"))
            f["deal_indicators"] = ", ".join([f["deal_indicators"]] + [f'"{q}"' for q in queries[:3]])
            results.append(f)
        results.sort(key=lambda f: (f.get("filing_date", ""), f.get("confidence_score", 0)), reverse=True)
        print(f"[Scanner] Found {len(results)} unique filings", file=sys.stderr, flush=True)

        for filing in results:
            self._store_filing(filing)
        return results

    def scan_company_ma_history(self, ticker_or_cik: str, years_back: int = 5) -> List[Dict[str, Any]]:
        """Scan a specific company's M&A filing history via data.sec.gov."""
        end_date = datetime.now()
        start_date = end_date - timedelta(days=years_back * 365)
        cik = self._resolve_cik(ticker_or_cik)
        if not cik:
            print(f"[Scanner] Could not resolve CIK for {ticker_or_cik}", file=sys.stderr, flush=True)
            return []
        results = []
        for hit in _get_company_recent_filings(cik, self._SCAN_FORMS, start_date.strftime("%Y-%m-%d"),
                                               end_date.strftime("%Y-%m-%d")):
            scored = self._score_and_enrich(hit)
            if scored:
                results.append(scored)
                self._store_filing(scored)
        return results

    def get_high_confidence_deals(self, min_confidence: float = 0.75) -> List[Dict[str, Any]]:
        """Return high-confidence M&A filings already stored in the DB and not parsed yet."""
        cursor = self.db._get_connection().cursor()
        cursor.execute("""
            SELECT * FROM sec_filings
            WHERE contains_ma_flag = 1 AND confidence_score >= ?
              AND parsed_flag = 0
            ORDER BY filing_date DESC
        """, (min_confidence,))
        return [dict(row) for row in cursor.fetchall()]

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _score_and_enrich(self, hit: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """Standard filing dict for a raw EFTS / submissions hit, or None below the 0.5 confidence floor."""
        form_type = hit.get("form_type", "")
        items = [str(i) for i in (hit.get("items") or [])]
        score = _score_hit(form_type, items)
        if score < 0.50:
            return None
        cik = hit.get("cik", "")
        acc = hit.get("accession_number", "")
        indicators = [f"form:{form_type}"] + ([f"items:{','.join(items)}"] if items else [])
        return {
            "accession_number": acc,
            "filing_type": form_type,
            "filing_date": hit.get("file_date", ""),
            "company_name": hit.get("entity_name", ""),
            "ticker": hit.get("ticker", ""),
            "cik": cik,
            "sic": hit.get("sic", ""),
            "industry": industry_from_sic(hit.get("sic", "")) or "",
            "items": ",".join(items),
            "filing_url": filing_index_url(cik, acc),
            "confidence_score": round(score, 3),
            "contains_ma_flag": 1,
            "deal_indicators": ", ".join(indicators),
        }

    def _resolve_cik(self, ticker_or_cik: str) -> Optional[str]:
        """Resolve a ticker symbol or CIK string to a CIK."""
        if ticker_or_cik.isdigit():
            return ticker_or_cik
        _load_tickers()
        hit = _TICKERS["by_ticker"].get(ticker_or_cik.upper())
        return hit[0] if hit else None

    def _store_filing(self, filing: Dict[str, Any]):
        """Persist filing to sec_filings (duplicates are skipped)."""
        try:
            self.db.insert_sec_filing({
                "accession_number": filing["accession_number"],
                "filing_type": filing["filing_type"],
                "filing_date": filing["filing_date"],
                "company_name": filing["company_name"],
                "cik": filing.get("cik", ""),
                "items": filing.get("items", ""),
                "filing_url": filing.get("filing_url", ""),
                "confidence_score": filing.get("confidence_score", 0.0),
                "contains_ma_flag": filing.get("contains_ma_flag", 1),
                "parsed_flag": 0,
                "extracted_data": json.dumps({k: filing.get(k, "") for k in
                                              ("ticker", "sic", "industry", "deal_indicators")}),
            })
        except Exception as exc:
            print(f"[Scanner] Store error: {exc}", file=sys.stderr, flush=True)


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------

def main():
    import json as _json

    if len(sys.argv) < 2:
        print(_json.dumps({"success": False, "error": "No command. Usage: deal_scanner.py <scan|scan_company|high_confidence> [args]"}))
        sys.exit(1)

    command = sys.argv[1]
    # Qt bridge: accept (command, {json}) and expand into the native positional
    # scalar argv the handlers below expect. No effect on the legacy CLI form
    # (a bare scalar in argv[2] isn't a JSON dict, so this falls through).
    _QT_ARGMAP = {"scan": ["days_back"], "scan_company": ["ticker", "years_back"], "high_confidence": ["min_confidence"]}
    if len(sys.argv) == 3 and command in _QT_ARGMAP:
        try:
            _qp = _json.loads(sys.argv[2])
            if isinstance(_qp, dict):
                _qav = [sys.argv[0], command]
                for _qk in _QT_ARGMAP[command]:
                    if _qk in _qp and _qp[_qk] is not None:
                        _qav.append(str(_qp[_qk]))
                sys.argv = _qav
        except Exception:
            pass
    scanner = MADealScanner()

    try:
        if command == "scan":
            days_back = int(sys.argv[2]) if len(sys.argv) > 2 else 30
            deals = scanner.scan_recent_filings(days_back=days_back)
            print(_json.dumps({"success": True, "data": deals, "count": len(deals), "days_scanned": days_back}))

        elif command == "scan_company":
            if len(sys.argv) < 3:
                raise ValueError("Ticker or CIK required")
            ticker = sys.argv[2]
            years_back = int(sys.argv[3]) if len(sys.argv) > 3 else 3
            deals = scanner.scan_company_ma_history(ticker, years_back)
            print(_json.dumps({"success": True, "data": deals, "ticker": ticker, "count": len(deals)}))

        elif command == "high_confidence":
            min_conf = float(sys.argv[2]) if len(sys.argv) > 2 else 0.75
            deals = scanner.get_high_confidence_deals(min_conf)
            print(_json.dumps({"success": True, "data": deals, "count": len(deals), "min_confidence": min_conf}))

        else:
            print(_json.dumps({"success": False, "error": f"Unknown command: {command}. Available: scan, scan_company, high_confidence"}))
            sys.exit(1)

    except Exception as exc:
        print(_json.dumps({"success": False, "error": str(exc), "command": command}))
        sys.exit(1)


if __name__ == "__main__":
    main()
