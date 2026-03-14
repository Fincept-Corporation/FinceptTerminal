"""SEC Filing Scanner for M&A Deal Detection

Real data sources (no mocks, no demo data):
  1. SEC EDGAR Full-Text Search API  (efts.sec.gov)  — primary, no key needed
  2. SEC EDGAR XBRL/REST API         (data.sec.gov)   — company-level filing index
  3. SEC EDGAR submissions endpoint   (data.sec.gov/submissions/) — per-CIK filing history

The scanner works without any third-party library such as edgartools.
All HTTP calls use the standard library (urllib) so there are zero extra deps.
"""
import sys
import json
import re
import time
from pathlib import Path
from typing import List, Dict, Any, Optional
from datetime import datetime, timedelta
from urllib.request import urlopen, Request
from urllib.error import URLError, HTTPError
from urllib.parse import urlencode, quote
from concurrent.futures import ThreadPoolExecutor, as_completed

scripts_path = Path(__file__).parent.parent.parent.parent
sys.path.append(str(scripts_path))

analytics_path = Path(__file__).parent.parent.parent
sys.path.insert(0, str(analytics_path))

from corporateFinance.config import MA_FILING_FORMS, MATERIAL_EVENT_ITEMS
from corporateFinance.deal_database.database_schema import MADatabase

# ---------------------------------------------------------------------------
# HTTP helpers
# ---------------------------------------------------------------------------

_EDGAR_HEADERS = {
    "User-Agent": "Fincept Terminal contact@fincept.in",   # SEC requires a valid UA
    "Accept": "application/json",
}

def _get(url: str, timeout: int = 15) -> Optional[dict]:
    """GET JSON from a URL, return parsed dict or None on error."""
    try:
        req = Request(url, headers=_EDGAR_HEADERS)
        with urlopen(req, timeout=timeout) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except HTTPError as exc:
        if exc.code != 404:  # 404s are expected for optional index files — suppress
            print(f"[Scanner] HTTP {exc.code} {url}", file=sys.stderr)
        return None
    except (URLError, json.JSONDecodeError, Exception):
        return None

def _get_text(url: str, timeout: int = 20) -> str:
    """GET raw text from a URL."""
    try:
        req = Request(url, headers={**_EDGAR_HEADERS, "Accept": "text/html,text/plain"})
        with urlopen(req, timeout=timeout) as resp:
            raw = resp.read()
            try:
                return raw.decode("utf-8")[:80000]
            except UnicodeDecodeError:
                return raw.decode("latin-1")[:80000]
    except Exception:
        return ""


# ---------------------------------------------------------------------------
# M&A signal detection
# ---------------------------------------------------------------------------

_MA_KEYWORDS = [
    'merger', 'acquisition', 'acquire', 'acquiree', 'takeover',
    'definitive agreement', 'merger agreement', 'tender offer',
    'stock purchase agreement', 'asset purchase agreement',
    'combination', 'amalgamation', 'consolidation',
]

_MA_PATTERNS = [
    r'definitive\s+(?:merger|acquisition|purchase)\s+agreement',
    r'tender\s+offer',
    r'acquire[ds]?\s+(?:all|substantially\s+all)',
    r'purchase\s+price\s+of\s+\$[\d,\.]+\s*(?:million|billion)',
    r'aggregate\s+(?:purchase|merger)\s+consideration',
    r'merger\s+of\s+equals',
    r'going[\s-]private\s+transaction',
    r'leveraged\s+buy[-\s]?out',
]

_HIGH_CONFIDENCE_FORMS = {'S-4', 'DEFM14A', 'PREM14A', 'SC TO-T', 'SC TO-I', 'SC 13E-3'}
_MEDIUM_CONFIDENCE_FORMS = {'8-K', '425', 'SC 13D'}

def _score_text(text: str, form_type: str) -> tuple[float, list]:
    """Return (confidence_score 0-1, indicators list) for a piece of filing text."""
    text_lower = text.lower()
    score = 0.0
    indicators = []

    # Form-type bonus
    if form_type in _HIGH_CONFIDENCE_FORMS:
        score += 0.45
    elif form_type in _MEDIUM_CONFIDENCE_FORMS:
        score += 0.15

    # Keyword hits (cap contribution)
    kw_hits = 0
    for kw in _MA_KEYWORDS:
        if kw in text_lower:
            kw_hits += 1
            indicators.append(kw)
    score += min(kw_hits * 0.07, 0.35)

    # Regex pattern hits
    for pat in _MA_PATTERNS:
        if re.search(pat, text_lower):
            score += 0.10
            indicators.append(pat[:40])

    # 8-K item 1.01 / 2.01 boost
    if form_type == '8-K':
        items_found = re.findall(r'Item\s+(\d+\.\d+)', text)
        if any(i in MATERIAL_EVENT_ITEMS for i in items_found):
            score += 0.20

    return min(score, 1.0), indicators[:8]


# ---------------------------------------------------------------------------
# EDGAR Full-Text Search (efts.sec.gov) — primary real-data source
# ---------------------------------------------------------------------------

_EFTS_BASE = "https://efts.sec.gov/LATEST/search-index"
_EFTS_SEARCH = "https://efts.sec.gov/LATEST/search-index?q={query}&dateRange=custom&startdt={start}&enddt={end}&forms={forms}&hits.hits._source=file_date,period_of_report,entity_name,file_num,form_type,biz_location,inc_states&hits.hits.total.value=true&hits.hits.highlight=false&hits.hits.highlight.number_of_fragments=0&_source=false"

_EFTS_API  = "https://efts.sec.gov/LATEST/search-index?q={query}&dateRange=custom&startdt={start}&enddt={end}&forms={forms}&hits.hits.total.value=true"

# Simpler, more reliable EFTS endpoint
_EFTS_SIMPLE = "https://efts.sec.gov/LATEST/search-index?q=%22{query}%22&forms={forms}&dateRange=custom&startdt={start}&enddt={end}"


def _efts_search(query_term: str, form_types: List[str],
                 start_date: str, end_date: str,
                 max_hits: int = 50) -> List[Dict[str, Any]]:
    """
    Search SEC EDGAR Full-Text Search API for filings matching query_term.
    Returns list of hit dicts with accession_number, entity_name, form_type, etc.
    """
    # URL-encode each form name individually (handles spaces in "SC TO-T", "SC 13E-3", etc.)
    forms_str = ",".join(quote(f, safe="") for f in form_types)
    # EDGAR EFTS uses %22 for quotes in the q param
    encoded_query = quote(query_term, safe="")
    url = (
        f"https://efts.sec.gov/LATEST/search-index"
        f"?q=%22{encoded_query}%22"
        f"&forms={forms_str}"
        f"&dateRange=custom&startdt={start_date}&enddt={end_date}"
        f"&hits.hits.total.value=true"
    )
    data = _get(url)
    if not data:
        return []

    hits = data.get("hits", {}).get("hits", [])
    results = []
    seen_acc = set()
    for hit in hits[:max_hits]:
        src = hit.get("_source", {})
        # `adsh` is the clean accession number (e.g. "0001193125-26-056047")
        acc = src.get("adsh", "")
        if not acc:
            # fallback: _id is "accession:filename", strip the filename part
            raw_id = hit.get("_id", "")
            acc = raw_id.split(":")[0] if ":" in raw_id else raw_id
        if not acc or acc in seen_acc:
            continue
        seen_acc.add(acc)

        # display_names format: "Company Name  (TICK)  (CIK 0000XXXXXX)"
        display_names = src.get("display_names", [])
        entity_name = display_names[0].split("(")[0].strip() if display_names else ""

        # ciks is a list of zero-padded CIK strings
        ciks = src.get("ciks", [])
        cik = ciks[0].lstrip("0") if ciks else ""

        # root_forms is a list; form is the specific filing form
        form_type = src.get("form", src.get("root_forms", [""])[0] if src.get("root_forms") else "")

        results.append({
            "accession_number": acc,
            "entity_name": entity_name,
            "form_type": form_type,
            "file_date": src.get("file_date", ""),
            "cik": cik,
            "items": src.get("items", []),
        })
    return results


# ---------------------------------------------------------------------------
# EDGAR submissions endpoint — get filing index per CIK
# ---------------------------------------------------------------------------

def _get_company_recent_filings(cik_str: str, form_types: List[str],
                                 start_date: str, end_date: str) -> List[Dict[str, Any]]:
    """Fetch recent filings for a given CIK via data.sec.gov/submissions/."""
    cik_padded = cik_str.zfill(10)
    url = f"https://data.sec.gov/submissions/CIK{cik_padded}.json"
    data = _get(url)
    if not data:
        return []

    entity_name = data.get("name", "")
    recent = data.get("filings", {}).get("recent", {})
    forms = recent.get("form", [])
    dates = recent.get("filingDate", [])
    acc_nums = recent.get("accessionNumber", [])

    results = []
    for form, date_str, acc in zip(forms, dates, acc_nums):
        if form not in form_types:
            continue
        if date_str < start_date or date_str > end_date:
            continue
        results.append({
            "accession_number": acc,
            "entity_name": entity_name,
            "form_type": form,
            "file_date": date_str,
            "cik": cik_str,
        })
    return results


# ---------------------------------------------------------------------------
# Filing text fetcher — EDGAR viewer / raw document
# ---------------------------------------------------------------------------

def _fetch_filing_text(accession_number: str, cik: str) -> str:
    """Fetch the primary document text for an accession number."""
    # acc_clean removes dashes: "0001193125-26-056047" → "000119312526056047"
    acc_clean = accession_number.replace("-", "")
    cik_int = cik.lstrip("0") or "0"   # numeric CIK without leading zeros for URL path

    # 1. Try the JSON filing index: /Archives/edgar/data/{cik}/{acc_clean}/{acc}-index.json
    index_url = (
        f"https://www.sec.gov/Archives/edgar/data/{cik_int}"
        f"/{acc_clean}/{accession_number}-index.json"
    )
    index_data = _get(index_url)
    if index_data:
        documents = index_data.get("directory", {}).get("item", [])
        for doc in documents:
            name = doc.get("name", "")
            if name.endswith((".htm", ".txt", ".html")):
                doc_url = f"https://www.sec.gov/Archives/edgar/data/{cik_int}/{acc_clean}/{name}"
                text = _get_text(doc_url)
                if text:
                    return re.sub(r'<[^>]+>', ' ', text)[:60000]

    # 2. Fallback: directory listing page
    if cik_int:
        viewer_url = f"https://www.sec.gov/Archives/edgar/data/{cik_int}/{acc_clean}/"
        text = _get_text(viewer_url)
        if text:
            return re.sub(r'<[^>]+>', ' ', text)[:60000]

    return ""


# ---------------------------------------------------------------------------
# Main scanner class
# ---------------------------------------------------------------------------

class MADealScanner:
    """
    Scan SEC EDGAR for real M&A deal signals.

    Strategy (all free, no API key, no third-party library):
      1. Full-text search (efts.sec.gov) with M&A trigger phrases across
         high-signal form types (S-4, DEFM14A, 8-K, 425, SC TO-T …).
      2. For each hit, optionally fetch the filing document to score text
         and extract deal metadata.
      3. Persist discovered filings in the SQLite database.
    """

    # M&A trigger phrases sent to EFTS — each returns REAL matching filings
    _SEARCH_QUERIES = [
        "definitive merger agreement",
        "definitive acquisition agreement",
        "tender offer",
        "merger of equals",
        "acquire all outstanding shares",
        "going private transaction",
        "leveraged buyout",
        "stock purchase agreement",
        "asset purchase agreement",
        "agreement and plan of merger",
    ]

    _SCAN_FORMS = ["S-4", "DEFM14A", "PREM14A", "8-K", "425", "SC TO-T", "SC TO-I", "SC 13E-3"]

    def __init__(self, db: Optional[MADatabase] = None):
        self.db = db or MADatabase()

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------

    def scan_recent_filings(self, days_back: int = 30,
                            max_workers: int = 4) -> List[Dict[str, Any]]:
        """
        Scan SEC EDGAR for M&A filings in the last `days_back` days.
        Returns list of filing dicts suitable for storing in sec_filings table.
        """
        end_date = datetime.now()
        start_date = end_date - timedelta(days=days_back)
        start_str = start_date.strftime("%Y-%m-%d")
        end_str = end_date.strftime("%Y-%m-%d")

        print(f"[Scanner] Scanning EDGAR {start_str} → {end_str}", file=sys.stderr)

        all_filings: Dict[str, Dict[str, Any]] = {}   # keyed by accession_number to deduplicate

        # Run all query terms in parallel
        with ThreadPoolExecutor(max_workers=max_workers) as pool:
            futures = {
                pool.submit(self._search_one_query, q, start_str, end_str): q
                for q in self._SEARCH_QUERIES
            }
            for future in as_completed(futures):
                try:
                    hits = future.result()
                    for h in hits:
                        acc = h["accession_number"]
                        if acc and acc not in all_filings:
                            all_filings[acc] = h
                except Exception as exc:
                    print(f"[Scanner] Query error: {exc}", file=sys.stderr)

        results = list(all_filings.values())
        print(f"[Scanner] Found {len(results)} unique filings", file=sys.stderr)

        # Persist to DB
        for filing in results:
            self._store_filing(filing)

        return results

    def scan_company_ma_history(self, ticker_or_cik: str,
                                years_back: int = 5) -> List[Dict[str, Any]]:
        """Scan a specific company's M&A filing history via data.sec.gov."""
        end_date = datetime.now()
        start_date = end_date - timedelta(days=years_back * 365)
        start_str = start_date.strftime("%Y-%m-%d")
        end_str = end_date.strftime("%Y-%m-%d")

        # Resolve ticker → CIK using EDGAR company search
        cik = self._resolve_cik(ticker_or_cik)
        if not cik:
            print(f"[Scanner] Could not resolve CIK for {ticker_or_cik}", file=sys.stderr)
            return []

        raw = _get_company_recent_filings(cik, self._SCAN_FORMS, start_str, end_str)
        results = []
        for hit in raw:
            scored = self._score_and_enrich(hit)
            if scored and scored["confidence_score"] >= 0.5:
                results.append(scored)
                self._store_filing(scored)
        return results

    def get_high_confidence_deals(self, min_confidence: float = 0.75) -> List[Dict[str, Any]]:
        """Return high-confidence M&A filings already stored in the DB."""
        conn = self.db._get_connection()
        cursor = conn.cursor()
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

    def _search_one_query(self, query: str, start_str: str,
                          end_str: str) -> List[Dict[str, Any]]:
        """Run one EFTS query and return enriched filing dicts."""
        hits = _efts_search(query, self._SCAN_FORMS, start_str, end_str, max_hits=40)
        enriched = []
        for hit in hits:
            scored = self._score_and_enrich(hit)
            if scored and scored["confidence_score"] >= 0.5:
                enriched.append(scored)
            time.sleep(0.05)   # Polite rate limiting for SEC servers
        return enriched

    def _score_and_enrich(self, hit: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        """
        Given a raw EFTS/submissions hit, compute confidence score and
        build the standard filing dict. Fetches document text only for
        medium-confidence forms (8-K, 425) where text analysis adds value.
        """
        form_type = hit.get("form_type", "")
        acc = hit.get("accession_number", "")
        cik = hit.get("cik", "")
        entity = hit.get("entity_name", "")
        file_date = hit.get("file_date", "")

        # For high-signal forms, score is already high from form type alone
        if form_type in _HIGH_CONFIDENCE_FORMS:
            score = 0.90
            indicators = [f"form:{form_type}"]
        else:
            # Check 8-K items from EFTS metadata first (no text fetch needed)
            items = hit.get("items", [])
            if form_type == "8-K" and any(i in MATERIAL_EVENT_ITEMS for i in items):
                # Item 1.01 = material definitive agreement, 2.01 = completion of acquisition
                score = 0.80
                indicators = [f"form:{form_type}", f"items:{','.join(items)}"]
            else:
                # Fetch document text for scoring (only for forms worth checking)
                text = _fetch_filing_text(acc, cik) if cik else ""
                score, indicators = _score_text(text or f"form_type:{form_type}", form_type)

        if score < 0.50:
            return None

        filing_url = (
            f"https://www.sec.gov/cgi-bin/browse-edgar"
            f"?action=getcompany&CIK={cik}&type={form_type}&dateb=&owner=include&count=10"
        ) if cik else f"https://www.sec.gov/cgi-bin/browse-edgar?action=getcompany&type={form_type}"

        return {
            "accession_number": acc,
            "filing_type": form_type,
            "filing_date": file_date,
            "company_name": entity,
            "cik": cik,
            "filing_url": filing_url,
            "confidence_score": round(score, 3),
            "contains_ma_flag": 1,
            "deal_indicators": ", ".join(indicators[:6]),
        }

    def _resolve_cik(self, ticker_or_cik: str) -> Optional[str]:
        """Resolve a ticker symbol or CIK string to a zero-padded CIK."""
        # If it's already numeric, use directly
        if ticker_or_cik.isdigit():
            return ticker_or_cik

        url = f"https://www.sec.gov/cgi-bin/browse-edgar?company={ticker_or_cik}&CIK=&type=&dateb=&owner=include&count=5&search_text=&action=getcompany&output=atom"
        # Use company tickers JSON — faster and more reliable
        tickers_url = "https://www.sec.gov/files/company_tickers.json"
        data = _get(tickers_url)
        if data:
            for entry in data.values():
                if entry.get("ticker", "").upper() == ticker_or_cik.upper():
                    return str(entry["cik_str"])
        return None

    def _store_filing(self, filing: Dict[str, Any]):
        """Persist filing to sec_filings table (silently skip duplicates)."""
        try:
            self.db.insert_sec_filing({
                "accession_number": filing["accession_number"],
                "filing_type": filing["filing_type"],
                "filing_date": filing["filing_date"],
                "company_name": filing["company_name"],
                "cik": filing.get("cik", ""),
                "filing_url": filing.get("filing_url", ""),
                "confidence_score": filing.get("confidence_score", 0.0),
                "contains_ma_flag": filing.get("contains_ma_flag", 1),
                "parsed_flag": 0,
            })
        except Exception as exc:
            print(f"[Scanner] Store error: {exc}", file=sys.stderr)


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------

def main():
    import json as _json

    if len(sys.argv) < 2:
        print(_json.dumps({"success": False, "error": "No command. Usage: deal_scanner.py <scan|scan_company|high_confidence> [args]"}))
        sys.exit(1)

    command = sys.argv[1]
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
