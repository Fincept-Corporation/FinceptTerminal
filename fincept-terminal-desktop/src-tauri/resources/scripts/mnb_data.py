"""
Magyar Nemzeti Bank (MNB) — National Bank of Hungary Data Wrapper
Fetches exchange rate data from the MNB SOAP web service.

API Reference:
  Endpoint: http://www.mnb.hu/arfolyamok.asmx
  Protocol: SOAP 1.1
  Format:   XML (UTF-8) — inner result is HTML-entity-encoded XML
  Auth:     None required — fully public
  WSDL:     http://www.mnb.hu/arfolyamok.asmx?WSDL

SOAP Operations:
  GetCurrentExchangeRates()
    → Today's rates for all currencies

  GetExchangeRates(startDate, endDate, currencyNames)
    → Date-range rates for selected currencies (comma-separated codes)

  GetCurrencies()
    → List of available currency codes

Exchange rates expressed as HUF per foreign currency unit.
Comma is used as decimal separator in the raw XML (e.g. "379,08000").
Some currencies use a multiplier (unit): JPY=100, KRW=100, IDR=100.
The wrapper normalises all to HUF per 1 unit.

Published currencies (31):
  AUD, BRL, CAD, CHF, CNY, CZK, DKK, EUR, GBP, HKD, IDR (×100),
  ILS, INR, ISK, JPY (×100), KRW (×100), MXN, MYR, NOK, NZD, PHP,
  PLN, RON, RSD, RUB, SEK, SGD, THB, TRY, UAH, USD, ZAR

Returns JSON output for Rust/Tauri integration.
"""

import sys
import html
import json
import requests
import traceback
import xml.etree.ElementTree as ET
from typing import Dict, Any, List, Optional
from datetime import datetime, date, timedelta, timezone


SOAP_URL        = "http://www.mnb.hu/arfolyamok.asmx"
SOAP_NS         = "http://www.mnb.hu/webservices/"
DEFAULT_TIMEOUT = 30

MAJOR_CURRENCIES = ["USD", "EUR", "GBP", "JPY", "CHF", "CAD", "AUD",
                    "SEK", "NOK", "DKK", "CNY", "PLN", "CZK", "RON"]


# ---------------------------------------------------------------------------
# Error container
# ---------------------------------------------------------------------------

class MNBError:
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
            "type":        "MNBError",
        }


# ---------------------------------------------------------------------------
# Main wrapper
# ---------------------------------------------------------------------------

class MNBWrapper:
    """
    Wrapper for the Magyar Nemzeti Bank (MNB) SOAP exchange rate service.

    Rates are HUF (Hungarian Forint) per 1 unit of foreign currency.
    JPY, KRW, IDR are quoted per 100 units; the wrapper normalises them.
    Data is published on Hungarian business days (Mon–Fri).
    """

    def __init__(self):
        self.session = requests.Session()
        self.session.headers.update({
            "User-Agent":   "Fincept-Terminal/3.3.1",
            "Content-Type": "text/xml; charset=utf-8",
        })

    # ------------------------------------------------------------------
    # Internal SOAP helpers
    # ------------------------------------------------------------------

    def _soap_call(self, operation: str, body_inner: str) -> str:
        """
        Execute a SOAP 1.1 request and return the inner result XML string.
        The MNB service wraps its XML result in HTML-entity encoding.
        """
        soap_action = f"{SOAP_NS}MNBArfolyamServiceSoap/{operation}"
        envelope = (
            '<?xml version="1.0" encoding="utf-8"?>'
            '<soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">'
            f'<soap:Body><{operation} xmlns="{SOAP_NS}">'
            f'{body_inner}'
            f'</{operation}></soap:Body></soap:Envelope>'
        )
        resp = self.session.post(
            SOAP_URL,
            data=envelope.encode("utf-8"),
            headers={"SOAPAction": soap_action},
            timeout=DEFAULT_TIMEOUT,
        )
        resp.raise_for_status()

        root        = ET.fromstring(resp.content)
        result_tag  = f"{{{SOAP_NS}}}{operation}Result"
        result_el   = root.find(f".//{result_tag}")
        if result_el is None or result_el.text is None:
            raise ValueError(f"Empty SOAP result for {operation}")
        return html.unescape(result_el.text)

    def _parse_rate_value(self, text: str, unit: int) -> Optional[float]:
        """Convert MNB rate string ('379,08000') to float, normalised by unit."""
        if not text or text.strip() in ("", "N/A"):
            return None
        try:
            return round(float(text.strip().replace(",", ".")), 6) / unit
        except ValueError:
            return None

    def _parse_rates_xml(self, xml_str: str) -> List[Dict[str, Any]]:
        """
        Parse MNBExchangeRates or MNBCurrentExchangeRates XML into
        wide-format rows: [{date, USD: rate, EUR: rate, ...}, ...]
        """
        root = ET.fromstring(xml_str)
        rows: List[Dict[str, Any]] = []
        for day_el in root.findall("Day"):
            d   = day_el.get("date", "")
            row: Dict[str, Any] = {"date": d}
            for rate_el in day_el.findall("Rate"):
                code = rate_el.get("curr", "")
                unit = int(rate_el.get("unit", "1") or "1")
                val  = self._parse_rate_value(rate_el.text or "", unit)
                if code and val is not None:
                    row[code] = val
            rows.append(row)
        return sorted(rows, key=lambda r: r["date"])

    # ------------------------------------------------------------------
    # Public methods
    # ------------------------------------------------------------------

    def get_today(self) -> Dict[str, Any]:
        """Current exchange rates for all available currencies."""
        try:
            xml_str = self._soap_call("GetCurrentExchangeRates", "")
            rows    = self._parse_rates_xml(xml_str)
            latest  = rows[-1] if rows else {}
            return {
                "success":   True,
                "date":      latest.get("date", ""),
                "data":      {k: v for k, v in latest.items() if k != "date"},
                "note":      "HUF per 1 unit of foreign currency (JPY/KRW/IDR normalised from per-100)",
                "source":    "Magyar Nemzeti Bank",
                "url":       SOAP_URL,
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return MNBError("GetCurrentExchangeRates", str(e), sc).to_dict()
        except Exception as e:
            return MNBError("GetCurrentExchangeRates", str(e)).to_dict()

    def get_range(self, start_date: str, end_date: Optional[str] = None,
                  currencies: Optional[List[str]] = None) -> Dict[str, Any]:
        """
        Exchange rates for a date range and selected currencies.
        If currencies is None, fetches all available currencies.
        """
        if end_date is None:
            end_date = date.today().isoformat()
        if currencies is None:
            # MNB requires explicit list; use known full set
            currencies = [
                "AUD", "BRL", "CAD", "CHF", "CNY", "CZK", "DKK", "EUR",
                "GBP", "HKD", "IDR", "ILS", "INR", "ISK", "JPY", "KRW",
                "MXN", "MYR", "NOK", "NZD", "PHP", "PLN", "RON", "RSD",
                "RUB", "SEK", "SGD", "THB", "TRY", "UAH", "USD", "ZAR",
            ]
        ccy_str = ",".join(currencies)
        body = (
            f"<startDate>{start_date}</startDate>"
            f"<endDate>{end_date}</endDate>"
            f"<currencyNames>{ccy_str}</currencyNames>"
        )
        try:
            xml_str = self._soap_call("GetExchangeRates", body)
            rows    = self._parse_rates_xml(xml_str)
            return {
                "success":    True,
                "start_date": start_date,
                "end_date":   end_date,
                "currencies": currencies,
                "data":       rows,
                "count":      len(rows),
                "note":       "HUF per 1 unit of foreign currency",
                "source":     "Magyar Nemzeti Bank",
                "timestamp":  int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return MNBError("GetExchangeRates", str(e), sc).to_dict()
        except Exception as e:
            return MNBError("GetExchangeRates", str(e)).to_dict()

    def get_currency(self, currency: str,
                     start_date: Optional[str] = None,
                     end_date: Optional[str] = None) -> Dict[str, Any]:
        """Historical rates for a single currency vs HUF."""
        if start_date is None:
            start_date = (date.today() - timedelta(days=90)).isoformat()
        result = self.get_range(start_date, end_date, [currency.upper()])
        if result.get("success"):
            result["currency"] = currency.upper()
            result["note"]     = f"HUF per 1 {currency.upper()}"
        return result

    def get_usd_huf(self, start_date: Optional[str] = None,
                    end_date: Optional[str] = None) -> Dict[str, Any]:
        """USD/HUF exchange rate history."""
        return self.get_currency("USD", start_date, end_date)

    def get_eur_huf(self, start_date: Optional[str] = None,
                    end_date: Optional[str] = None) -> Dict[str, Any]:
        """EUR/HUF exchange rate history."""
        return self.get_currency("EUR", start_date, end_date)

    def get_major_currencies(self, start_date: Optional[str] = None) -> Dict[str, Any]:
        """Major currencies vs HUF — last 30 days if no start given."""
        if start_date is None:
            start_date = (date.today() - timedelta(days=30)).isoformat()
        return self.get_range(start_date, currencies=MAJOR_CURRENCIES)

    def get_available_currencies(self) -> Dict[str, Any]:
        """List all currency codes available from MNB."""
        try:
            xml_str = self._soap_call("GetCurrencies", "")
            root    = ET.fromstring(xml_str)
            codes   = [c.text.strip() for c in root.findall("Curr") if c.text]
            return {
                "success":    True,
                "currencies": codes,
                "count":      len(codes),
                "source":     "Magyar Nemzeti Bank",
                "timestamp":  int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return MNBError("GetCurrencies", str(e), sc).to_dict()
        except Exception as e:
            return MNBError("GetCurrencies", str(e)).to_dict()

    def get_overview(self) -> Dict[str, Any]:
        """Snapshot: today's major currency rates vs HUF."""
        today = self.get_today()
        if not today.get("success"):
            return today
        all_rates = today.get("data", {})
        snapshot  = {c: all_rates[c] for c in MAJOR_CURRENCIES if c in all_rates}
        return {
            "success":    True,
            "date":       today.get("date"),
            "data":       snapshot,
            "all_rates":  all_rates,
            "note":       "HUF per 1 unit of foreign currency",
            "source":     "Magyar Nemzeti Bank",
            "timestamp":  int(datetime.now(timezone.utc).timestamp()),
        }


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

COMMANDS = {
    "today":    "                              — Today's rates (all currencies)",
    "range":    "<start> [end] [currencies]   — Date range (comma-separated currencies)",
    "currency": "<CCY> [start] [end]           — Single currency vs HUF history",
    "usd":      "[start] [end]                 — USD/HUF history",
    "eur":      "[start] [end]                 — EUR/HUF history",
    "major":    "[start]                       — Major currencies last 30 days",
    "overview": "                              — Today's major currency snapshot",
    "available":"                              — List available currencies from MNB",
}


def _a(n: int, d: Any = None) -> Any:
    return sys.argv[n] if len(sys.argv) > n and sys.argv[n] else d


def main() -> None:
    if len(sys.argv) < 2:
        print(json.dumps({
            "error":    "No command provided.",
            "usage":    "python mnb_data.py <command> [args...]",
            "commands": COMMANDS,
        }, indent=2))
        sys.exit(1)

    cmd     = sys.argv[1].lower()
    wrapper = MNBWrapper()

    try:
        if cmd == "today":
            result = wrapper.get_today()
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
        elif cmd == "usd":
            result = wrapper.get_usd_huf(_a(2), _a(3))
        elif cmd == "eur":
            result = wrapper.get_eur_huf(_a(2), _a(3))
        elif cmd == "major":
            result = wrapper.get_major_currencies(_a(2))
        elif cmd == "overview":
            result = wrapper.get_overview()
        elif cmd in ("available", "currencies"):
            result = wrapper.get_available_currencies()
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
