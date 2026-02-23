"""
Hrvatska Narodna Banka (HNB) — Croatian National Bank Data Wrapper
Fetches exchange rate data from the HNB public REST API.

API Reference:
  Base URL:  https://api.hnb.hr
  Format:    JSON (UTF-8)
  Auth:      None required — fully public
  Docs:      https://api.hnb.hr/

Endpoints:
  GET /tecajn-eur/v3                         — Latest EUR-based exchange rate list
    ?broj_tecajnice=N                         — Specific bulletin number
    ?broj_tecajnice_od=N&broj_tecajnice_do=M  — Range of bulletins
    ?valuta=XXX                               — Filter by currency code (ISO 4217)
    ?datum_primjene=YYYY-MM-DD                — Rates valid on date

Notes:
  - Croatia joined the Eurozone on 1 Jan 2023; EUR is the base currency.
  - HRK (Kuna) rates available via /tecajn/v2 (legacy endpoint, EUR fixed at 7.5345).
  - "tecajnica" = rate bulletin; numbered sequentially per year starting from 1.
  - Rates are EUR per foreign currency unit.
  - Published every business day; 13 currencies:
      AUD, CAD, CZK, DKK, HUF, JPY, NOK, SEK, CHF, GBP, USD, BAM, PLN

Fields per item:
  broj_tecajnice  — bulletin number
  datum_primjene  — date rates apply (YYYY-MM-DD)
  drzava          — country name (Croatian)
  drzava_iso      — ISO 3166-1 alpha-3 country code
  sifra_valute    — ISO 4217 numeric currency code
  valuta          — ISO 4217 alpha currency code
  kupovni_tecaj   — buying rate  (comma decimal)
  prodajni_tecaj  — selling rate
  srednji_tecaj   — middle/reference rate

Returns JSON output for Rust/Tauri integration.
"""

import sys
import json
import requests
import traceback
from typing import Dict, Any, List, Optional
from datetime import datetime, date, timedelta, timezone


BASE_URL        = "https://api.hnb.hr"
DEFAULT_TIMEOUT = 30

ALL_CURRENCIES = ["AUD", "CAD", "CZK", "DKK", "HUF", "JPY", "NOK",
                  "SEK", "CHF", "GBP", "USD", "BAM", "PLN"]
MAJOR_CURRENCIES = ["USD", "GBP", "CHF", "JPY", "CAD", "AUD", "NOK",
                    "SEK", "DKK", "CZK", "PLN", "HUF"]


# ---------------------------------------------------------------------------
# Error container
# ---------------------------------------------------------------------------

class HNBError:
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
            "type":        "HNBError",
        }


# ---------------------------------------------------------------------------
# Main wrapper
# ---------------------------------------------------------------------------

class HNBWrapper:
    """
    Wrapper for the Hrvatska Narodna Banka (HNB) public REST API.

    All exchange rates are EUR per 1 unit of foreign currency
    (e.g. USD = 1.1767 means 1 USD = 1.1767 EUR, or equivalently
    1 EUR ≈ 0.8498 USD).
    Decimal separator in raw JSON is a comma; wrapper converts to float.
    Data is published every Croatian business day.
    """

    def __init__(self):
        self.session = requests.Session()
        self.session.headers.update({
            "User-Agent": "Fincept-Terminal/3.3.1",
            "Accept":     "application/json",
        })

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _get(self, path: str, params: Optional[Dict] = None) -> Any:
        url  = f"{BASE_URL}/{path.lstrip('/')}"
        resp = self.session.get(url, params=params or {}, timeout=DEFAULT_TIMEOUT)
        resp.raise_for_status()
        return resp.json()

    def _parse_rate(self, text: str) -> Optional[float]:
        """Convert comma-decimal rate string to float."""
        if not text or text.strip() in ("", "N/A"):
            return None
        try:
            return float(text.strip().replace(",", "."))
        except ValueError:
            return None

    def _normalise(self, items: List[Dict]) -> List[Dict[str, Any]]:
        """
        Convert raw HNB list to clean list with float rates.
        Groups by date into wide-format rows when multiple currencies returned.
        """
        wide: Dict[str, Dict[str, Any]] = {}
        for item in items:
            d    = item.get("datum_primjene", "")
            code = item.get("valuta", "")
            if d not in wide:
                wide[d] = {
                    "date":             d,
                    "broj_tecajnice":   item.get("broj_tecajnice", ""),
                }
            if code:
                wide[d][f"{code}_buy"]  = self._parse_rate(item.get("kupovni_tecaj", ""))
                wide[d][f"{code}_sell"] = self._parse_rate(item.get("prodajni_tecaj", ""))
                wide[d][f"{code}_mid"]  = self._parse_rate(item.get("srednji_tecaj", ""))
        return sorted(wide.values(), key=lambda r: r["date"])

    def _normalise_single(self, items: List[Dict]) -> List[Dict[str, Any]]:
        """For single-currency queries: return [{date, buy, sell, mid}, ...]."""
        rows = []
        for item in items:
            rows.append({
                "date":             item.get("datum_primjene", ""),
                "broj_tecajnice":   item.get("broj_tecajnice", ""),
                "currency":         item.get("valuta", ""),
                "buy":              self._parse_rate(item.get("kupovni_tecaj", "")),
                "sell":             self._parse_rate(item.get("prodajni_tecaj", "")),
                "mid":              self._parse_rate(item.get("srednji_tecaj", "")),
            })
        return sorted(rows, key=lambda r: r["date"])

    # ------------------------------------------------------------------
    # Public methods
    # ------------------------------------------------------------------

    def get_today(self) -> Dict[str, Any]:
        """Latest exchange rate bulletin — all 13 currencies vs EUR."""
        try:
            items = self._get("tecajn-eur/v3", {"broj_tecajnice": 1})
            rows  = self._normalise(items)
            latest = rows[-1] if rows else {}
            # Also build a simple flat dict: {USD: mid, EUR: 1, GBP: mid, ...}
            flat: Dict[str, Any] = {}
            for item in items:
                code = item.get("valuta", "")
                mid  = self._parse_rate(item.get("srednji_tecaj", ""))
                if code and mid is not None:
                    flat[code] = mid
            return {
                "success":        True,
                "date":           latest.get("date", ""),
                "broj_tecajnice": latest.get("broj_tecajnice", ""),
                "data":           flat,
                "full_data":      latest,
                "note":           "EUR per 1 unit of foreign currency",
                "source":         "Hrvatska Narodna Banka",
                "url":            f"{BASE_URL}/tecajn-eur/v3",
                "timestamp":      int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return HNBError("tecajn-eur/v3", str(e), sc).to_dict()
        except Exception as e:
            return HNBError("tecajn-eur/v3", str(e)).to_dict()

    def get_currency(self, currency: str,
                     start_bulletin: Optional[int] = None,
                     end_bulletin: Optional[int] = None,
                     date_from: Optional[str] = None) -> Dict[str, Any]:
        """
        Historical rates for a single currency.
        Use bulletin numbers (1 = most recent, higher = older within year)
        or date_from for approximate date filtering.
        Default: last 30 bulletins.
        """
        currency = currency.upper()
        try:
            params: Dict[str, Any] = {"valuta": currency}
            if start_bulletin and end_bulletin:
                params["broj_tecajnice_od"] = start_bulletin
                params["broj_tecajnice_do"] = end_bulletin
            elif start_bulletin:
                params["broj_tecajnice"] = start_bulletin
            # date_from not natively supported — fetch all and filter
            items = self._get("tecajn-eur/v3", params)
            rows  = self._normalise_single(items)
            if date_from:
                rows = [r for r in rows if r["date"] >= date_from]
            return {
                "success":    True,
                "currency":   currency,
                "data":       rows,
                "count":      len(rows),
                "note":       f"EUR per 1 {currency}",
                "source":     "Hrvatska Narodna Banka",
                "timestamp":  int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return HNBError(f"currency/{currency}", str(e), sc).to_dict()
        except Exception as e:
            return HNBError(f"currency/{currency}", str(e)).to_dict()

    def get_bulletin_range(self, from_num: int, to_num: int) -> Dict[str, Any]:
        """
        All currencies for a range of bulletin numbers.
        Returns wide-format rows keyed by date.
        Each bulletin covers one business day.
        """
        try:
            params = {
                "broj_tecajnice_od": from_num,
                "broj_tecajnice_do": to_num,
            }
            items = self._get("tecajn-eur/v3", params)
            rows  = self._normalise(items)
            return {
                "success":    True,
                "from_num":   from_num,
                "to_num":     to_num,
                "data":       rows,
                "count":      len(rows),
                "note":       "EUR per 1 unit of foreign currency",
                "source":     "Hrvatska Narodna Banka",
                "timestamp":  int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return HNBError("bulletin_range", str(e), sc).to_dict()
        except Exception as e:
            return HNBError("bulletin_range", str(e)).to_dict()

    def get_usd_eur(self, bulletins: int = 20) -> Dict[str, Any]:
        """USD/EUR rate history — last N bulletins."""
        return self.get_currency("USD", end_bulletin=bulletins)

    def get_gbp_eur(self, bulletins: int = 20) -> Dict[str, Any]:
        """GBP/EUR rate history — last N bulletins."""
        return self.get_currency("GBP", end_bulletin=bulletins)

    def get_overview(self) -> Dict[str, Any]:
        """Snapshot: today's middle rates for all currencies vs EUR."""
        return self.get_today()

    def available_currencies(self) -> Dict[str, Any]:
        """List of currencies published by HNB."""
        return {
            "success":          True,
            "currencies":       ALL_CURRENCIES,
            "major_currencies": MAJOR_CURRENCIES,
            "base_currency":    "EUR",
            "note":             "Rates expressed as EUR per 1 unit of foreign currency",
            "source":           "Hrvatska Narodna Banka",
            "timestamp":        int(datetime.now(timezone.utc).timestamp()),
        }


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

COMMANDS = {
    "today":    "                               — Latest bulletin (all currencies vs EUR)",
    "currency": "<CCY> [from_bulletin] [to_bul] [date_from] — Single currency history",
    "range":    "<from_bulletin> <to_bulletin>  — Bulletin range (wide format)",
    "usd":      "[bulletins=20]                 — USD/EUR last N bulletins",
    "gbp":      "[bulletins=20]                 — GBP/EUR last N bulletins",
    "overview": "                               — Today's rates snapshot",
    "available":"                               — List available currencies",
}


def _a(n: int, d: Any = None) -> Any:
    return sys.argv[n] if len(sys.argv) > n and sys.argv[n] else d


def main() -> None:
    if len(sys.argv) < 2:
        print(json.dumps({
            "error":    "No command provided.",
            "usage":    "python hnb_data.py <command> [args...]",
            "commands": COMMANDS,
        }, indent=2))
        sys.exit(1)

    cmd     = sys.argv[1].lower()
    wrapper = HNBWrapper()

    try:
        if cmd == "today":
            result = wrapper.get_today()
        elif cmd == "currency":
            if len(sys.argv) < 3:
                result = {"error": "currency requires <CCY>"}
            else:
                from_b = int(_a(3)) if _a(3) else None
                to_b   = int(_a(4)) if _a(4) else None
                result = wrapper.get_currency(sys.argv[2], from_b, to_b, _a(5))
        elif cmd == "range":
            if len(sys.argv) < 4:
                result = {"error": "range requires <from_bulletin> <to_bulletin>"}
            else:
                result = wrapper.get_bulletin_range(int(sys.argv[2]), int(sys.argv[3]))
        elif cmd == "usd":
            result = wrapper.get_usd_eur(int(_a(2, 20)))
        elif cmd == "gbp":
            result = wrapper.get_gbp_eur(int(_a(2, 20)))
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
