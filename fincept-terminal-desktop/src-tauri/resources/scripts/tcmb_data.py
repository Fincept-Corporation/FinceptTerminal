"""
Central Bank of the Republic of Turkey (TCMB / CBRT) Data Wrapper
Fetches exchange rate data from the TCMB public XML bulletin service.

API Reference:
  Base URL:  https://www.tcmb.gov.tr/kurlar
  Format:    XML (UTF-8)
  Auth:      None required — fully public
  Docs:      https://www.tcmb.gov.tr/wps/wcm/connect/EN/TCMB+EN/Main+Menu/Statistics/Exchange+Rates/

URL patterns:
  Today:     GET /kurlar/today.xml
  By date:   GET /kurlar/{YYYYMM}/{DDMMYYYY}.xml   (business days only)

Exchange rates published on Turkish business days (Mon–Fri, excluding holidays).
Rates are TRY per foreign currency unit.
Each currency entry includes:
  ForexBuying   — wholesale foreign exchange buying
  ForexSelling  — wholesale foreign exchange selling
  BanknoteBuying  — banknote buying
  BanknoteSelling — banknote selling
  CrossRateUSD  — cross rate in USD (if applicable)

Currencies available (22):
  USD, AUD, DKK, EUR, GBP, CHF, SEK, CAD, KWD, NOK, SAR,
  JPY (per 100), RON, RUB, CNY, PKR, QAR, KRW, AZN, AED, KZT, XDR

Returns JSON output for Rust/Tauri integration.
"""

import sys
import json
import requests
import traceback
import xml.etree.ElementTree as ET
from typing import Dict, Any, List, Optional
from datetime import datetime, date, timedelta, timezone


BASE_URL        = "https://www.tcmb.gov.tr/kurlar"
DEFAULT_TIMEOUT = 30

MAJOR_CURRENCIES = ["USD", "EUR", "GBP", "JPY", "CHF", "CAD", "AUD",
                    "SEK", "NOK", "DKK", "CNY", "SAR", "AED"]


# ---------------------------------------------------------------------------
# Error container
# ---------------------------------------------------------------------------

class TCMBError:
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
            "type":        "TCMBError",
        }


# ---------------------------------------------------------------------------
# Main wrapper
# ---------------------------------------------------------------------------

class TCMBWrapper:
    """
    Wrapper for the TCMB (Central Bank of Turkey) XML exchange rate bulletin.

    Rates are TRY (Turkish Lira) per 1 unit of foreign currency.
    JPY is quoted per 100 JPY; the wrapper normalises to per 1 JPY.
    Data is published on Turkish business days only.
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

    def _fetch(self, url: str) -> bytes:
        resp = self.session.get(url, timeout=DEFAULT_TIMEOUT)
        resp.raise_for_status()
        return resp.content

    def _parse_bulletin(self, content: bytes) -> Dict[str, Any]:
        """
        Parse a single TCMB XML bulletin into a dict of currency data.
        Returns {date, USD: {buy, sell, banknote_buy, banknote_sell}, ...}
        JPY values are normalised (÷100).
        """
        root       = ET.fromstring(content)
        date_str   = root.get("Date", "")          # MM/DD/YYYY
        bulten_no  = root.get("Bulten_No", "")

        # Normalise date to ISO format
        try:
            parsed_date = datetime.strptime(date_str, "%m/%d/%Y").date().isoformat()
        except ValueError:
            parsed_date = date_str

        rates: Dict[str, Any] = {}
        for cur in root.findall("Currency"):
            code = cur.get("CurrencyCode", "")
            unit = int(cur.findtext("Unit", "1") or "1")

            def _val(tag: str) -> Optional[float]:
                txt = cur.findtext(tag, "")
                if not txt or txt.strip() == "":
                    return None
                try:
                    return round(float(txt.replace(",", ".")), 6) / unit
                except ValueError:
                    return None

            rates[code] = {
                "forex_buying":      _val("ForexBuying"),
                "forex_selling":     _val("ForexSelling"),
                "banknote_buying":   _val("BanknoteBuying"),
                "banknote_selling":  _val("BanknoteSelling"),
                "unit":              unit,
            }
            cross = cur.findtext("CrossRateUSD", "").strip()
            if cross:
                try:
                    rates[code]["cross_rate_usd"] = float(cross.replace(",", "."))
                except ValueError:
                    pass

        return {
            "date":      parsed_date,
            "bulten_no": bulten_no,
            "rates":     rates,
        }

    def _date_url(self, d: date) -> str:
        return f"{BASE_URL}/{d.strftime('%Y%m')}/{d.strftime('%d%m%Y')}.xml"

    def _find_latest_business_day(self, max_lookback: int = 7) -> Optional[date]:
        """Walk back from today to find the most recent published bulletin."""
        today = date.today()
        for i in range(max_lookback):
            d = today - timedelta(days=i)
            try:
                content = self._fetch(self._date_url(d))
                ET.fromstring(content)   # verify it's valid XML
                return d
            except Exception:
                continue
        return None

    def _flatten_rates(self, bulletin: Dict[str, Any],
                       fields: Optional[List[str]] = None) -> Dict[str, Any]:
        """Flatten bulletin to {date, USD_buy, USD_sell, EUR_buy, ...} row."""
        if fields is None:
            fields = ["forex_buying", "forex_selling"]
        row: Dict[str, Any] = {"date": bulletin["date"]}
        for code, data in bulletin["rates"].items():
            for f in fields:
                val = data.get(f)
                if val is not None:
                    key = f"{code}_{f}" if len(fields) > 1 else code
                    row[key] = val
        return row

    # ------------------------------------------------------------------
    # Public methods
    # ------------------------------------------------------------------

    def get_today(self) -> Dict[str, Any]:
        """Latest exchange rate bulletin (most recent business day)."""
        try:
            content  = self._fetch(f"{BASE_URL}/today.xml")
            bulletin = self._parse_bulletin(content)
            return {
                "success":    True,
                "date":       bulletin["date"],
                "bulten_no":  bulletin["bulten_no"],
                "data":       bulletin["rates"],
                "note":       "TRY per 1 unit of foreign currency (JPY normalised from per-100)",
                "source":     "Central Bank of the Republic of Turkey",
                "url":        f"{BASE_URL}/today.xml",
                "timestamp":  int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return TCMBError("today", str(e), sc).to_dict()
        except Exception as e:
            return TCMBError("today", str(e)).to_dict()

    def get_date(self, rate_date: str) -> Dict[str, Any]:
        """Exchange rates for a specific date (YYYY-MM-DD). Must be a business day."""
        try:
            d        = datetime.strptime(rate_date, "%Y-%m-%d").date()
            url      = self._date_url(d)
            content  = self._fetch(url)
            bulletin = self._parse_bulletin(content)
            return {
                "success":   True,
                "date":      bulletin["date"],
                "bulten_no": bulletin["bulten_no"],
                "data":      bulletin["rates"],
                "note":      "TRY per 1 unit of foreign currency",
                "source":    "Central Bank of the Republic of Turkey",
                "url":       url,
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return TCMBError(f"date/{rate_date}", str(e), sc).to_dict()
        except Exception as e:
            return TCMBError(f"date/{rate_date}", str(e)).to_dict()

    def get_range(self, start_date: str, end_date: Optional[str] = None,
                  currencies: Optional[List[str]] = None) -> Dict[str, Any]:
        """
        Exchange rates for a date range — fetches each business day individually.
        Returns wide-format rows with forex_buying rates.
        """
        try:
            start = datetime.strptime(start_date, "%Y-%m-%d").date()
            end   = datetime.strptime(end_date, "%Y-%m-%d").date() if end_date else date.today()

            rows: List[Dict[str, Any]] = []
            errors: List[str] = []
            d = start
            while d <= end:
                if d.weekday() < 5:   # Mon–Fri only
                    try:
                        content  = self._fetch(self._date_url(d))
                        bulletin = self._parse_bulletin(content)
                        row = {"date": bulletin["date"]}
                        for code, data in bulletin["rates"].items():
                            if currencies is None or code in currencies:
                                buy = data.get("forex_buying")
                                if buy is not None:
                                    row[code] = buy
                        rows.append(row)
                    except Exception:
                        pass   # holiday / no data for this day
                d += timedelta(days=1)

            return {
                "success":    True,
                "start_date": start_date,
                "end_date":   end.isoformat(),
                "currencies": currencies or "all",
                "data":       rows,
                "count":      len(rows),
                "note":       "TRY per 1 unit (forex buying rate)",
                "source":     "Central Bank of the Republic of Turkey",
                "timestamp":  int(datetime.now(timezone.utc).timestamp()),
            }
        except Exception as e:
            return TCMBError("range", str(e)).to_dict()

    def get_currency(self, currency: str,
                     start_date: Optional[str] = None,
                     end_date: Optional[str] = None) -> Dict[str, Any]:
        """
        Buying and selling rates for a single currency over a date range.
        Defaults to the last 30 calendar days.
        """
        currency = currency.upper()
        if start_date is None:
            start_date = (date.today() - timedelta(days=30)).isoformat()
        try:
            result = self.get_range(start_date, end_date, [currency])
            # Enrich each row with both buy and sell
            start  = datetime.strptime(start_date, "%Y-%m-%d").date()
            end_d  = datetime.strptime(end_date, "%Y-%m-%d").date() if end_date else date.today()
            rows: List[Dict[str, Any]] = []
            d = start
            while d <= end_d:
                if d.weekday() < 5:
                    try:
                        content  = self._fetch(self._date_url(d))
                        bulletin = self._parse_bulletin(content)
                        cdata    = bulletin["rates"].get(currency, {})
                        if cdata.get("forex_buying") is not None:
                            rows.append({
                                "date":              bulletin["date"],
                                "forex_buying":      cdata.get("forex_buying"),
                                "forex_selling":     cdata.get("forex_selling"),
                                "banknote_buying":   cdata.get("banknote_buying"),
                                "banknote_selling":  cdata.get("banknote_selling"),
                            })
                    except Exception:
                        pass
                d += timedelta(days=1)

            return {
                "success":    True,
                "currency":   currency,
                "start_date": start_date,
                "end_date":   end_d.isoformat(),
                "data":       rows,
                "count":      len(rows),
                "note":       f"TRY per 1 {currency}",
                "source":     "Central Bank of the Republic of Turkey",
                "timestamp":  int(datetime.now(timezone.utc).timestamp()),
            }
        except Exception as e:
            return TCMBError(f"currency/{currency}", str(e)).to_dict()

    def get_major_currencies(self, start_date: Optional[str] = None) -> Dict[str, Any]:
        """Major currencies vs TRY — last 30 days if no start given."""
        if start_date is None:
            start_date = (date.today() - timedelta(days=30)).isoformat()
        return self.get_range(start_date, currencies=MAJOR_CURRENCIES)

    def get_overview(self) -> Dict[str, Any]:
        """Snapshot: today's rates for major currencies (buying & selling)."""
        today = self.get_today()
        if not today.get("success"):
            return today
        all_rates = today.get("data", {})
        snapshot  = {}
        for c in MAJOR_CURRENCIES:
            if c in all_rates:
                snapshot[c] = {
                    "buy":  all_rates[c].get("forex_buying"),
                    "sell": all_rates[c].get("forex_selling"),
                }
        return {
            "success":    True,
            "date":       today.get("date"),
            "bulten_no":  today.get("bulten_no"),
            "data":       snapshot,
            "all_rates":  all_rates,
            "note":       "TRY per 1 unit of foreign currency",
            "source":     "Central Bank of the Republic of Turkey",
            "timestamp":  int(datetime.now(timezone.utc).timestamp()),
        }

    def available_currencies(self) -> Dict[str, Any]:
        """List of currencies published in TCMB bulletins."""
        return {
            "success": True,
            "major_currencies": MAJOR_CURRENCIES,
            "all_currencies": [
                "USD", "AUD", "DKK", "EUR", "GBP", "CHF", "SEK", "CAD",
                "KWD", "NOK", "SAR", "JPY", "RON", "RUB", "CNY", "PKR",
                "QAR", "KRW", "AZN", "AED", "KZT", "XDR",
            ],
            "note":   "TRY per 1 unit; JPY is per 100 in raw XML but normalised here",
            "source": "Central Bank of the Republic of Turkey",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

COMMANDS = {
    "today":    "                              — Latest bulletin (most recent business day)",
    "date":     "<YYYY-MM-DD>                 — Bulletin for a specific date",
    "range":    "<start> [end] [currencies]   — Date range, wide format (forex buying)",
    "currency": "<CCY> [start] [end]           — Single currency buy/sell history",
    "major":    "[start]                       — Major currencies last 30 days",
    "overview": "                              — Today's major currency snapshot",
    "available":"                              — List available currencies",
}


def _a(n: int, d: Any = None) -> Any:
    return sys.argv[n] if len(sys.argv) > n and sys.argv[n] else d


def main() -> None:
    if len(sys.argv) < 2:
        print(json.dumps({
            "error":    "No command provided.",
            "usage":    "python tcmb_data.py <command> [args...]",
            "commands": COMMANDS,
        }, indent=2))
        sys.exit(1)

    cmd     = sys.argv[1].lower()
    wrapper = TCMBWrapper()

    try:
        if cmd == "today":
            result = wrapper.get_today()
        elif cmd == "date":
            if len(sys.argv) < 3:
                result = {"error": "date requires <YYYY-MM-DD>"}
            else:
                result = wrapper.get_date(sys.argv[2])
        elif cmd == "range":
            if len(sys.argv) < 3:
                result = {"error": "range requires <start_date>"}
            else:
                ccys = [c.strip().upper() for c in _a(4, "").split(",")] if _a(4) else None
                result = wrapper.get_range(sys.argv[2], _a(3), ccys)
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
