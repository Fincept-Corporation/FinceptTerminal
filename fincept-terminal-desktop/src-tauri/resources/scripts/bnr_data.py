"""
National Bank of Romania (BNR) Data Wrapper
Fetches data from the BNR public XML API.

API Reference:
  Base URL:  https://www.bnr.ro
  Format:    XML (utf-8)
  Auth:      None required — fully public
  Docs:      https://www.bnr.ro/Cursuri-de-schimb-524.aspx

Endpoints:
  GET /nbrfxrates.xml                    — Latest reference rates (today)
  GET /nbrfxrates.xml?date=YYYYMMDD      — Rates for a specific date
  GET /files/xml/years/nbrfxrates{Y}.xml — All rates for a calendar year

Exchange rates expressed as RON per foreign currency unit.
Some currencies use a multiplier (e.g. JPY per 100, HUF per 100, KRW per 100).

Currencies published (36+):
  AED, AUD, BGN, BRL, CAD, CHF, CNY, CZK, DKK, EGP, EUR, GBP, HKD,
  HUF, IDR, ILS, INR, ISK, JPY, KRW, MDL, MXN, MYR, NOK, NZD, PHP,
  PLN, RON (1), RSD, RUB, SEK, SGD, THB, TRY, UAH, USD, XAU, XDR, ZAR

Returns JSON output for Rust/Tauri integration.
"""

import sys
import json
import requests
import traceback
import xml.etree.ElementTree as ET
from typing import Dict, Any, List, Optional
from datetime import datetime, date, timedelta, timezone


BASE_URL        = "https://www.bnr.ro"
XML_NS          = "http://www.bnr.ro/xsd"
DEFAULT_TIMEOUT = 30

MAJOR_CURRENCIES = ["USD", "EUR", "GBP", "JPY", "CHF", "CAD", "AUD",
                    "NOK", "SEK", "DKK", "CNY", "PLN", "HUF", "CZK"]


# ---------------------------------------------------------------------------
# Error container
# ---------------------------------------------------------------------------

class BNRError:
    def __init__(self, endpoint: str, error: str, status_code: Optional[int] = None):
        self.endpoint    = endpoint
        self.error       = error
        self.status_code = status_code
        self.timestamp   = int(datetime.now(timezone.utc).timestamp())

    def to_dict(self) -> Dict[str, Any]:
        return {
            "success":     False,
            "endpoint":    self.endpoint,
            "error":       self.error,
            "status_code": self.status_code,
            "timestamp":   self.timestamp,
            "type":        "BNRError",
        }


# ---------------------------------------------------------------------------
# Main wrapper
# ---------------------------------------------------------------------------

class BNRWrapper:
    """
    Wrapper for the National Bank of Romania XML data feed.

    All rates are RON per 1 unit of foreign currency.
    For currencies with multiplier > 1 (e.g. JPY, HUF, KRW, IDR, ISK):
      actual_rate = obs_value / multiplier
    The wrapper normalises all rates to RON per 1 unit.
    """

    def __init__(self):
        self.session = requests.Session()
        self.session.headers.update({
            "User-Agent": "Fincept-Terminal/3.3.1",
            "Accept":     "application/xml, text/xml, */*",
        })

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _fetch_xml(self, url: str) -> bytes:
        resp = self.session.get(url, timeout=DEFAULT_TIMEOUT)
        resp.raise_for_status()
        return resp.content

    def _parse_dataset(self, content: bytes) -> List[Dict[str, Any]]:
        """
        Parse a BNR XML DataSet (single date or full-year).
        Returns list of {date, CCY1: rate, CCY2: rate, ...} rows.
        RON rates are normalised: value / multiplier.
        """
        root = ET.fromstring(content)
        ns   = {"b": XML_NS}
        rows: List[Dict[str, Any]] = []

        for cube in root.findall(".//b:Cube", ns):
            d = cube.get("date", "")
            row: Dict[str, Any] = {"date": d}
            for rate_el in cube.findall("b:Rate", ns):
                ccy  = rate_el.get("currency", "")
                mult = int(rate_el.get("multiplier", "1"))
                try:
                    val = float(rate_el.text or "0")
                    row[ccy] = round(val / mult, 6)
                except (ValueError, TypeError):
                    pass
            if d:
                rows.append(row)

        return sorted(rows, key=lambda r: r["date"])

    def _parse_header(self, content: bytes) -> Dict[str, str]:
        """Extract header metadata (publisher, publish date, message type)."""
        root   = ET.fromstring(content)
        ns     = {"b": XML_NS}
        header = root.find("b:Header", ns)
        if header is None:
            return {}
        return {
            "publisher":      (header.findtext("b:Publisher",  namespaces=ns) or "").strip(),
            "publish_date":   (header.findtext("b:PublishingDate", namespaces=ns) or "").strip(),
            "orig_currency":  (header.findtext("b:Body/b:OrigCurrency", namespaces={"b": XML_NS}) or "RON").strip(),
        }

    def _today_url(self) -> str:
        return f"{BASE_URL}/nbrfxrates.xml"

    def _date_url(self, d: str) -> str:
        # BNR expects YYYYMMDD
        clean = d.replace("-", "")
        return f"{BASE_URL}/nbrfxrates.xml?date={clean}"

    def _year_url(self, year: int) -> str:
        return f"{BASE_URL}/files/xml/years/nbrfxrates{year}.xml"

    # ------------------------------------------------------------------
    # Public methods
    # ------------------------------------------------------------------

    def get_today(self) -> Dict[str, Any]:
        """Latest reference rates (today's publication)."""
        url = self._today_url()
        try:
            content = self._fetch_xml(url)
            rows    = self._parse_dataset(content)
            latest  = rows[-1] if rows else {}
            return {
                "success":   True,
                "date":      latest.get("date", ""),
                "data":      latest,
                "note":      "RON per 1 unit of foreign currency (normalised for multiplied currencies)",
                "source":    "National Bank of Romania",
                "url":       url,
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return BNRError("today", str(e), sc).to_dict()
        except Exception as e:
            return BNRError("today", str(e)).to_dict()

    def get_date(self, rate_date: str) -> Dict[str, Any]:
        """Reference rates for a specific date (YYYY-MM-DD)."""
        url = self._date_url(rate_date)
        try:
            content = self._fetch_xml(url)
            rows    = self._parse_dataset(content)
            entry   = rows[-1] if rows else {}
            return {
                "success":   True,
                "date":      entry.get("date", rate_date),
                "data":      entry,
                "note":      "RON per 1 unit of foreign currency",
                "source":    "National Bank of Romania",
                "url":       url,
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return BNRError(f"date/{rate_date}", str(e), sc).to_dict()
        except Exception as e:
            return BNRError(f"date/{rate_date}", str(e)).to_dict()

    def get_year(self, year: Optional[int] = None) -> Dict[str, Any]:
        """All reference rates for a full calendar year."""
        if year is None:
            year = date.today().year
        url = self._year_url(year)
        try:
            content = self._fetch_xml(url)
            rows    = self._parse_dataset(content)
            return {
                "success":   True,
                "year":      year,
                "data":      rows,
                "count":     len(rows),
                "note":      "RON per 1 unit of foreign currency",
                "source":    "National Bank of Romania",
                "url":       url,
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return BNRError(f"year/{year}", str(e), sc).to_dict()
        except Exception as e:
            return BNRError(f"year/{year}", str(e)).to_dict()

    def get_range(self, start_date: str, end_date: Optional[str] = None) -> Dict[str, Any]:
        """
        Reference rates for a date range.
        Fetches the relevant year file(s) and filters to the requested window.
        """
        try:
            start = datetime.strptime(start_date, "%Y-%m-%d").date()
            end   = datetime.strptime(end_date, "%Y-%m-%d").date() if end_date else date.today()

            years = list(range(start.year, end.year + 1))
            all_rows: List[Dict[str, Any]] = []

            for y in years:
                content = self._fetch_xml(self._year_url(y))
                rows    = self._parse_dataset(content)
                all_rows.extend(rows)

            filtered = [r for r in all_rows
                        if start_date <= r.get("date", "") <= end.isoformat()]
            return {
                "success":    True,
                "start_date": start_date,
                "end_date":   end.isoformat(),
                "data":       filtered,
                "count":      len(filtered),
                "note":       "RON per 1 unit of foreign currency",
                "source":     "National Bank of Romania",
                "timestamp":  int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return BNRError("range", str(e), sc).to_dict()
        except Exception as e:
            return BNRError("range", str(e)).to_dict()

    def get_currency(self, currency: str,
                     start_date: Optional[str] = None,
                     end_date: Optional[str] = None) -> Dict[str, Any]:
        """
        Historical rates for a single currency vs RON.
        Defaults to current year if no dates given.
        """
        try:
            currency = currency.upper()
            if start_date is None:
                start_date = date(date.today().year, 1, 1).isoformat()
            end   = end_date or date.today().isoformat()
            start = datetime.strptime(start_date, "%Y-%m-%d").date()
            endd  = datetime.strptime(end, "%Y-%m-%d").date()

            years = list(range(start.year, endd.year + 1))
            all_rows: List[Dict[str, Any]] = []
            for y in years:
                content = self._fetch_xml(self._year_url(y))
                rows    = self._parse_dataset(content)
                for r in rows:
                    if start_date <= r.get("date", "") <= end and currency in r:
                        all_rows.append({"date": r["date"], currency: r[currency]})

            return {
                "success":    True,
                "currency":   currency,
                "start_date": start_date,
                "end_date":   end,
                "data":       all_rows,
                "count":      len(all_rows),
                "note":       f"RON per 1 {currency}",
                "source":     "National Bank of Romania",
                "timestamp":  int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return BNRError(f"currency/{currency}", str(e), sc).to_dict()
        except Exception as e:
            return BNRError(f"currency/{currency}", str(e)).to_dict()

    def get_major_currencies(self, start_date: Optional[str] = None) -> Dict[str, Any]:
        """Major currencies vs RON for current year (or from start_date)."""
        try:
            if start_date is None:
                start_date = date(date.today().year, 1, 1).isoformat()
            content = self._fetch_xml(self._year_url(date.today().year))
            rows    = self._parse_dataset(content)
            filtered = []
            for r in rows:
                if r.get("date", "") >= start_date:
                    entry = {"date": r["date"]}
                    for c in MAJOR_CURRENCIES:
                        if c in r:
                            entry[c] = r[c]
                    filtered.append(entry)
            return {
                "success":    True,
                "currencies": MAJOR_CURRENCIES,
                "start_date": start_date,
                "data":       filtered,
                "count":      len(filtered),
                "note":       "RON per 1 unit of foreign currency",
                "source":     "National Bank of Romania",
                "timestamp":  int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return BNRError("major_currencies", str(e), sc).to_dict()
        except Exception as e:
            return BNRError("major_currencies", str(e)).to_dict()

    def get_overview(self) -> Dict[str, Any]:
        """Snapshot: today's rates for major currencies."""
        today = self.get_today()
        if not today.get("success"):
            return today
        data = today.get("data", {})
        snapshot = {c: data[c] for c in MAJOR_CURRENCIES if c in data}
        return {
            "success":   True,
            "date":      today.get("date"),
            "data":      snapshot,
            "full_data": data,
            "note":      "RON per 1 unit of foreign currency",
            "source":    "National Bank of Romania",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }

    def available_currencies(self) -> Dict[str, Any]:
        """List currencies available in BNR data."""
        return {
            "success": True,
            "major_currencies": MAJOR_CURRENCIES,
            "all_currencies": [
                "AED", "AUD", "BRL", "CAD", "CHF", "CNY", "CZK", "DKK",
                "EGP", "EUR", "GBP", "HKD", "HUF", "IDR", "ILS", "INR",
                "ISK", "JPY", "KRW", "MDL", "MXN", "MYR", "NOK", "NZD",
                "PHP", "PLN", "RSD", "RUB", "SEK", "SGD", "THB", "TRY",
                "UAH", "USD", "XAU", "XDR", "ZAR",
            ],
            "note":      "Rates expressed as RON per 1 unit; multiplied currencies (JPY, HUF, KRW, IDR, ISK) are normalised",
            "source":    "National Bank of Romania",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

COMMANDS = {
    "today":    "                              — Today's reference rates (all currencies)",
    "date":     "<YYYY-MM-DD>                 — Rates for a specific date",
    "year":     "[year]                        — All rates for a calendar year",
    "range":    "<start YYYY-MM-DD> [end]      — Rates for a date range",
    "currency": "<CCY> [start] [end]           — Single currency vs RON history",
    "major":    "[start YYYY-MM-DD]            — Major currencies this year",
    "overview": "                              — Today's major currency snapshot",
    "available":"                              — List available currencies",
}


def _a(n: int, d: Any = None) -> Any:
    return sys.argv[n] if len(sys.argv) > n and sys.argv[n] else d


def main() -> None:
    if len(sys.argv) < 2:
        print(json.dumps({
            "error":    "No command provided.",
            "usage":    "python bnr_data.py <command> [args...]",
            "commands": COMMANDS,
        }, indent=2))
        sys.exit(1)

    cmd     = sys.argv[1].lower()
    wrapper = BNRWrapper()

    try:
        if cmd == "today":
            result = wrapper.get_today()
        elif cmd == "date":
            if len(sys.argv) < 3:
                result = {"error": "date requires <YYYY-MM-DD>"}
            else:
                result = wrapper.get_date(sys.argv[2])
        elif cmd == "year":
            y = int(_a(2, date.today().year))
            result = wrapper.get_year(y)
        elif cmd == "range":
            if len(sys.argv) < 3:
                result = {"error": "range requires <start_date>"}
            else:
                result = wrapper.get_range(sys.argv[2], _a(3))
        elif cmd == "currency":
            if len(sys.argv) < 3:
                result = {"error": "currency requires <CCY>"}
            else:
                result = wrapper.get_currency(sys.argv[2], _a(3), _a(4))
        elif cmd == "major":
            result = wrapper.get_major_currencies(_a(2))
        elif cmd == "overview":
            result = wrapper.get_overview()
        elif cmd in ("available", "currencies"):
            result = wrapper.available_currencies()
        else:
            result = {"error": f"Unknown command: {cmd}", "commands": COMMANDS}

        print(json.dumps(result, indent=2, ensure_ascii=True))

    except Exception as exc:
        print(json.dumps({
            "success":   False,
            "error":     str(exc),
            "traceback": traceback.format_exc(),
        }, indent=2))
        sys.exit(1)


if __name__ == "__main__":
    main()
