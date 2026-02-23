"""
Czech National Bank (CNB) Data Wrapper
Fetches data from the CNB public REST API.

API Reference:
  Base URL:  https://api.cnb.cz/cnbapi
  Format:    JSON
  Auth:      None required — fully public
  Docs:      https://api.cnb.cz/cnbapi/swagger-ui.html

Verified Endpoints:
  GET /exrates/daily?[date=YYYY-MM-DD]&lang=EN
      Daily CZK exchange rate fixing (33 currencies)
  GET /exrates/daily-year?year=YYYY&lang=EN
      All daily fixings for a year
  GET /exrates/daily-currency-month?year=YYYY&month=MM&currencyCode=USD&lang=EN
      Single currency by month
  GET /exrates/monthly-averages-year?year=YYYY&lang=EN
      Monthly average rates for a year
  GET /czeonia/daily?[date=YYYY-MM-DD]&lang=EN
      CZEONIA overnight rate (Czech equivalent of EONIA)
  GET /czeonia/daily-year?year=YYYY&lang=EN
      CZEONIA for a full year
  GET /pribor/daily?[date=YYYY-MM-DD]&lang=EN
      PRIBOR fixing (1D, 1W, 2W, 1M, 2M, 3M, 6M, 9M, 12M)
  GET /pribor/daily-year?year=YYYY&lang=EN
      PRIBOR for a full year
  GET /pribor/daily-year-term?year=YYYY&term=THREE_MONTH&lang=EN
      PRIBOR for a term and year
  GET /omo/daily?[date=YYYY-MM-DD]&lang=EN
      Open market operations (repo, deposit facility)
  GET /forward/daily?[date=YYYY-MM-DD]&lang=EN
      Forward exchange rates (EUR/CZK, USD/CZK)
  GET /fxrates/daily-year?year=YYYY&lang=EN
      FX rates for currencies not in fixing table
  GET /skd/daily?[date=YYYY-MM-DD]&lang=EN
      Short-term government debt (SKD) rates

Returns JSON output for Rust/Tauri integration.
"""

import sys
import json
import requests
import traceback
from typing import Dict, Any, List, Optional
from datetime import datetime, date, timedelta, timezone


BASE_URL        = "https://api.cnb.cz/cnbapi"
DEFAULT_TIMEOUT = 30
DEFAULT_LANG    = "EN"

# PRIBOR tenor codes
PRIBOR_TERMS = ["ONE_DAY", "ONE_WEEK", "TWO_WEEKS", "ONE_MONTH",
                "TWO_MONTHS", "THREE_MONTHS", "SIX_MONTHS", "NINE_MONTHS", "TWELVE_MONTHS"]

# Main fixing currencies (Table A equivalent)
FIXING_CURRENCIES = [
    "AUD", "BGN", "BRL", "CAD", "CHF", "CNY", "DKK", "EUR", "GBP",
    "HKD", "HRK", "HUF", "IDR", "ILS", "INR", "ISK", "JPY", "KRW",
    "MXN", "MYR", "NOK", "NZD", "PHP", "PLN", "RON", "RSD", "RUB",
    "SEK", "SGD", "THB", "TRY", "USD", "ZAR",
]


# ---------------------------------------------------------------------------
# Error container
# ---------------------------------------------------------------------------

class CNBError:
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
            "type":        "CNBError",
        }


# ---------------------------------------------------------------------------
# Main wrapper
# ---------------------------------------------------------------------------

class CNBWrapper:
    """
    Wrapper for the Czech National Bank (CNB) public REST API.

    Provides exchange rates, PRIBOR, CZEONIA, OMO, and forward rates.
    All rates are expressed as CZK per foreign currency (or per amount).
    No authentication required.
    """

    def __init__(self, lang: str = DEFAULT_LANG):
        self.lang    = lang
        self.session = requests.Session()
        self.session.headers.update({
            "User-Agent": "Fincept-Terminal/3.3.1",
            "Accept":     "application/json",
        })

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _get(self, path: str, params: Optional[Dict] = None) -> Any:
        url = f"{BASE_URL}/{path.lstrip('/')}"
        p   = {"lang": self.lang, **(params or {})}
        resp = self.session.get(url, params=p, timeout=DEFAULT_TIMEOUT)
        resp.raise_for_status()
        return resp.json()

    def _safe(self, path: str, label: str,
              params: Optional[Dict] = None) -> Dict[str, Any]:
        try:
            data = self._get(path, params)
            return {
                "success":   True,
                "data":      data,
                "source":    "Czech National Bank",
                "url":       f"{BASE_URL}/{path}",
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return CNBError(label, str(e), sc).to_dict()
        except Exception as e:
            return CNBError(label, str(e)).to_dict()

    def _flatten_rates(self, raw_rates: List[Dict],
                       amount_key: str = "amount") -> List[Dict]:
        """Normalize a rates list to {date, currencyCode: rate_per_unit}."""
        by_date: Dict[str, Dict] = {}
        for r in raw_rates:
            d    = r.get("validFor", "")
            code = r.get("currencyCode", "")
            rate = r.get("rate")
            amt  = r.get(amount_key, 1)
            if not d or not code:
                continue
            by_date.setdefault(d, {"date": d})
            if rate is not None and amt:
                by_date[d][code] = round(rate / amt, 6) if amt != 1 else rate
        return sorted(by_date.values(), key=lambda x: x["date"])

    # ------------------------------------------------------------------
    # Public methods
    # ------------------------------------------------------------------

    def get_exchange_rates(self, rate_date: Optional[str] = None) -> Dict[str, Any]:
        """Daily CZK exchange rate fixing. rate_date: YYYY-MM-DD (default=latest)."""
        params = {}
        if rate_date:
            params["date"] = rate_date
        try:
            raw   = self._get("exrates/daily", params)
            rates = raw.get("rates", [])
            rows  = self._flatten_rates(rates)
            return {
                "success":   True,
                "date":      rates[0].get("validFor", rate_date) if rates else rate_date,
                "data":      rows[0] if rows else {},
                "all_rates": rates,
                "count":     len(rates),
                "note":      "CZK per 1 unit of foreign currency (adjusted for amount)",
                "source":    "Czech National Bank",
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return CNBError("exrates/daily", str(e), sc).to_dict()
        except Exception as e:
            return CNBError("exrates/daily", str(e)).to_dict()

    def get_exchange_rates_year(self, year: Optional[int] = None) -> Dict[str, Any]:
        """All daily CZK fixings for a year."""
        y = year or datetime.now(timezone.utc).year
        try:
            raw   = self._get("exrates/daily-year", {"year": y})
            rates = raw.get("rates", [])
            rows  = self._flatten_rates(rates)
            return {
                "success":   True,
                "year":      y,
                "data":      rows,
                "count":     len(rates),
                "source":    "Czech National Bank",
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return CNBError("exrates/daily-year", str(e), sc).to_dict()
        except Exception as e:
            return CNBError("exrates/daily-year", str(e)).to_dict()

    def get_exchange_rates_monthly_avg(self, year: Optional[int] = None) -> Dict[str, Any]:
        """Monthly average CZK rates for a year."""
        y = year or datetime.now(timezone.utc).year
        try:
            raw = self._get("exrates/monthly-averages-year", {"year": y})
            avgs = raw.get("averages", [])
            # Pivot by month
            by_month: Dict[str, Dict] = {}
            for r in avgs:
                month = r.get("month", "")
                code  = r.get("currencyCode", "")
                val   = r.get("average")
                amt   = r.get("amount", 1)
                if not month or not code:
                    continue
                key = f"{y}-{month}"
                by_month.setdefault(key, {"date": key})
                if val is not None and amt:
                    by_month[key][code] = round(val / amt, 6) if amt != 1 else val
            rows = sorted(by_month.values(), key=lambda x: x["date"])
            return {
                "success":   True,
                "year":      y,
                "data":      rows,
                "count":     len(rows),
                "source":    "Czech National Bank",
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return CNBError("exrates/monthly-averages-year", str(e), sc).to_dict()
        except Exception as e:
            return CNBError("exrates/monthly-averages-year", str(e)).to_dict()

    def get_czeonia(self, rate_date: Optional[str] = None) -> Dict[str, Any]:
        """CZEONIA overnight rate (latest or specific date)."""
        params = {}
        if rate_date:
            params["date"] = rate_date
        return self._safe("czeonia/daily", "czeonia/daily", params)

    def get_czeonia_year(self, year: Optional[int] = None) -> Dict[str, Any]:
        """CZEONIA for full year."""
        y = year or datetime.now(timezone.utc).year
        return self._safe("czeonia/daily-year", "czeonia/daily-year", {"year": y})

    def get_pribor(self, rate_date: Optional[str] = None) -> Dict[str, Any]:
        """PRIBOR fixing — all tenors (1D to 12M)."""
        params = {}
        if rate_date:
            params["date"] = rate_date
        try:
            raw   = self._get("pribor/daily", params)
            pribs = raw.get("pribs", [])
            # Build wide row by period
            if not pribs:
                return {"success": True, "data": {}, "source": "Czech National Bank",
                        "timestamp": int(datetime.now(timezone.utc).timestamp())}
            entry: Dict[str, Any] = {"date": pribs[0].get("validFor", "")}
            for p in pribs:
                period = p.get("period", "")
                val    = p.get("pribor")
                if period and val is not None:
                    entry[period] = val
            return {
                "success":   True,
                "data":      entry,
                "source":    "Czech National Bank",
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return CNBError("pribor/daily", str(e), sc).to_dict()
        except Exception as e:
            return CNBError("pribor/daily", str(e)).to_dict()

    def get_pribor_year(self, year: Optional[int] = None,
                        term: str = "THREE_MONTHS") -> Dict[str, Any]:
        """PRIBOR for a specific term and year. term: ONE_DAY, THREE_MONTHS, etc."""
        y      = year or datetime.now(timezone.utc).year
        params = {"year": y, "term": term}
        try:
            raw   = self._get("pribor/daily-year-term", params)
            pribs = raw.get("pribs", [])
            rows  = [{"date": p.get("validFor", ""), "pribor": p.get("pribor"),
                      "pribid": p.get("pribid")} for p in pribs]
            return {
                "success":   True,
                "year":      y,
                "term":      term,
                "data":      rows,
                "count":     len(rows),
                "source":    "Czech National Bank",
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return CNBError("pribor/daily-year-term", str(e), sc).to_dict()
        except Exception as e:
            return CNBError("pribor/daily-year-term", str(e)).to_dict()

    def get_omo(self, trade_date: Optional[str] = None) -> Dict[str, Any]:
        """Open market operations (repo, deposit facility)."""
        params = {}
        if trade_date:
            params["date"] = trade_date
        return self._safe("omo/daily", "omo/daily", params)

    def get_forward_rates(self, rate_date: Optional[str] = None) -> Dict[str, Any]:
        """Forward exchange rates (EUR/CZK, USD/CZK)."""
        params = {}
        if rate_date:
            params["date"] = rate_date
        return self._safe("forward/daily", "forward/daily", params)

    def get_skd(self, trade_date: Optional[str] = None) -> Dict[str, Any]:
        """Short-term government debt (SKD) rates."""
        params = {}
        if trade_date:
            params["date"] = trade_date
        return self._safe("skd/daily", "skd/daily", params)

    def get_overview(self) -> Dict[str, Any]:
        """Snapshot: today's FX rates, PRIBOR, CZEONIA, OMO."""
        results: Dict[str, Any] = {}
        for name, call in [
            ("exchange_rates", self.get_exchange_rates),
            ("pribor",         self.get_pribor),
            ("czeonia",        self.get_czeonia),
            ("omo",            self.get_omo),
            ("forward_rates",  self.get_forward_rates),
        ]:
            r = call()
            results[name] = {
                "success": r.get("success"),
                "data":    r.get("data"),
            }
        return {
            "success":   True,
            "data":      results,
            "source":    "Czech National Bank",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }

    def get_pribor_history(self, years: int = 2,
                           term: str = "THREE_MONTHS") -> Dict[str, Any]:
        """PRIBOR for multiple years merged into one series."""
        current_year = datetime.now(timezone.utc).year
        all_rows: List[Dict] = []
        for y in range(current_year - years + 1, current_year + 1):
            r = self.get_pribor_year(y, term)
            all_rows.extend(r.get("data", []))
        return {
            "success":   True,
            "term":      term,
            "years":     years,
            "data":      sorted(all_rows, key=lambda x: x["date"]),
            "count":     len(all_rows),
            "source":    "Czech National Bank",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }

    def available_endpoints(self) -> Dict[str, Any]:
        """List all available endpoints."""
        return {
            "success": True,
            "data": {
                "exchange_rates": {
                    "today":         "/exrates/daily",
                    "year":          "/exrates/daily-year?year=YYYY",
                    "monthly_avg":   "/exrates/monthly-averages-year?year=YYYY",
                    "currency_month":"/exrates/daily-currency-month?year=YYYY&month=MM&currencyCode=USD",
                },
                "interest_rates": {
                    "pribor_today":  "/pribor/daily",
                    "pribor_year":   "/pribor/daily-year?year=YYYY",
                    "pribor_term":   "/pribor/daily-year-term?year=YYYY&term=THREE_MONTHS",
                    "czeonia_today": "/czeonia/daily",
                    "czeonia_year":  "/czeonia/daily-year?year=YYYY",
                },
                "market_operations": {
                    "omo":           "/omo/daily",
                    "forward":       "/forward/daily",
                    "skd":           "/skd/daily",
                },
            },
            "pribor_terms":        PRIBOR_TERMS,
            "fixing_currencies":   FIXING_CURRENCIES,
            "source":              "Czech National Bank",
            "swagger":             f"{BASE_URL}/swagger-ui.html",
            "timestamp":           int(datetime.now(timezone.utc).timestamp()),
        }


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

COMMANDS = {
    "exchange_rates":   "[date YYYY-MM-DD]         — Daily CZK fixing (latest or date)",
    "exchange_rates_y": "[year]                    — All daily fixings for a year",
    "monthly_avg":      "[year]                    — Monthly average rates",
    "pribor":           "[date]                    — PRIBOR all tenors (1D–12M)",
    "pribor_year":      "[year] [term=THREE_MONTHS] — PRIBOR for a term + year",
    "pribor_history":   "[years=2] [term=THREE_MONTHS] — PRIBOR multi-year history",
    "czeonia":          "[date]                    — CZEONIA overnight rate",
    "czeonia_year":     "[year]                    — CZEONIA for full year",
    "omo":              "[date]                    — Open market operations",
    "forward":          "[date]                    — Forward exchange rates",
    "skd":              "[date]                    — Short-term govt debt rates",
    "overview":         "                          — Today's snapshot",
    "available":        "                          — List all endpoints",
}


def _a(n: int, d: Any = None) -> Any:
    return sys.argv[n] if len(sys.argv) > n and sys.argv[n] else d


def main() -> None:
    if len(sys.argv) < 2:
        print(json.dumps({
            "error":    "No command provided.",
            "usage":    "python cnb_data.py <command> [args...]",
            "commands": COMMANDS,
        }, indent=2))
        sys.exit(1)

    cmd     = sys.argv[1].lower()
    wrapper = CNBWrapper()

    try:
        if cmd == "exchange_rates":
            result = wrapper.get_exchange_rates(_a(2))
        elif cmd == "exchange_rates_y":
            result = wrapper.get_exchange_rates_year(int(_a(2, datetime.now().year)))
        elif cmd == "monthly_avg":
            result = wrapper.get_exchange_rates_monthly_avg(int(_a(2, datetime.now().year)))
        elif cmd == "pribor":
            result = wrapper.get_pribor(_a(2))
        elif cmd == "pribor_year":
            result = wrapper.get_pribor_year(int(_a(2, datetime.now().year)), _a(3, "THREE_MONTHS"))
        elif cmd == "pribor_history":
            result = wrapper.get_pribor_history(int(_a(2, 2)), _a(3, "THREE_MONTHS"))
        elif cmd == "czeonia":
            result = wrapper.get_czeonia(_a(2))
        elif cmd == "czeonia_year":
            result = wrapper.get_czeonia_year(int(_a(2, datetime.now().year)))
        elif cmd == "omo":
            result = wrapper.get_omo(_a(2))
        elif cmd == "forward":
            result = wrapper.get_forward_rates(_a(2))
        elif cmd == "skd":
            result = wrapper.get_skd(_a(2))
        elif cmd == "overview":
            result = wrapper.get_overview()
        elif cmd in ("available", "endpoints"):
            result = wrapper.available_endpoints()
        else:
            result = {"error": f"Unknown command: {cmd}", "commands": COMMANDS}

        print(json.dumps(result, indent=2, ensure_ascii=False))

    except Exception as exc:
        print(json.dumps({
            "success":   False,
            "error":     str(exc),
            "traceback": traceback.format_exc(),
        }, indent=2))
        sys.exit(1)


if __name__ == "__main__":
    main()
