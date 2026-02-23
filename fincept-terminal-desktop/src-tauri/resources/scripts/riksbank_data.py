"""
Sveriges Riksbank (Sweden) Data Wrapper
Fetches data from the Riksbank SWEA REST API.

API Reference:
  Base URL:  https://api.riksbank.se/swea/v1
  Format:    JSON (array of {date, value})
  Auth:      None required — fully public
  Docs:      https://api.riksbank.se/swea/v1/swagger-ui/index.html
  Rate limit: ~5 req/s — wrapper includes retry logic

Endpoints:
  GET /Series                         — Full series catalogue
  GET /Observations/{seriesId}/{from} — Observations from date
  GET /Observations/{seriesId}/{from}/{to} — Observations between dates
  GET /CrossRates/{from}/{to}         — Cross rates on a date

Key series IDs:
  Policy rates:
    SECBREPOEFF  — Policy rate (repo rate)
    SECBDEPOEFF  — Deposit rate
    SECBLENDEFF  — Lending rate
  Exchange rates (SEK per 1 foreign currency unit):
    SEKUSDPMI, SEKEURPMI, SEKGBPPMI, SEKJPYPMI
    SEKNOKPMI, SEKDKKPMI, SEKCHFPMI
  STIBOR interbank rates:
    SEDPT_NSTIBORDELAYC  — T/N
    SEDP1MSTIBORDELAYC   — 1 Month
    SEDP3MSTIBORDELAYC   — 3 Months
    SEDP6MSTIBORDELAYC   — 6 Months
  Government bond yields:
    SEGVB2YC, SEGVB5YC, SEGVB7YC, SEGVB10YC

Returns JSON output for Rust/Tauri integration.
"""

import sys
import json
import time
import requests
import traceback
from typing import Dict, Any, List, Optional
from datetime import datetime, date, timedelta, timezone


BASE_URL        = "https://api.riksbank.se/swea/v1"
DEFAULT_TIMEOUT = 30
RATE_LIMIT_WAIT = 15   # seconds to wait on 429


# ---------------------------------------------------------------------------
# Series catalogue
# ---------------------------------------------------------------------------

SERIES = {
    # Policy rates
    "SECBREPOEFF":  {"label": "Policy rate (repo)",      "category": "policy_rates"},
    "SECBDEPOEFF":  {"label": "Deposit rate",            "category": "policy_rates"},
    "SECBLENDEFF":  {"label": "Lending rate",            "category": "policy_rates"},
    "SECBLIKVEFF":  {"label": "Liquidity facility rate", "category": "policy_rates"},
    "SECBREFEFF":   {"label": "Reference rate",          "category": "policy_rates"},
    # Exchange rates (SEK per 1 foreign currency unit)
    "SEKUSDPMI":    {"label": "SEK/USD",    "category": "exchange_rates"},
    "SEKEURPMI":    {"label": "SEK/EUR",    "category": "exchange_rates"},
    "SEKGBPPMI":    {"label": "SEK/GBP",    "category": "exchange_rates"},
    "SEKJPYPMI":    {"label": "SEK/JPY",    "category": "exchange_rates"},
    "SEKNOKPMI":    {"label": "SEK/NOK",    "category": "exchange_rates"},
    "SEKDKKPMI":    {"label": "SEK/DKK",    "category": "exchange_rates"},
    "SEKCHFPMI":    {"label": "SEK/CHF",    "category": "exchange_rates"},
    "SEKCNYPMI":    {"label": "SEK/CNY",    "category": "exchange_rates"},
    "SEKCADPMI":    {"label": "SEK/CAD",    "category": "exchange_rates"},
    "SEKAUDPMI":    {"label": "SEK/AUD",    "category": "exchange_rates"},
    "SEKKIX92":     {"label": "KIX index",  "category": "exchange_rates"},
    # Treasury bills
    "SETB1MBENCHC": {"label": "SE T-bill 1 Month",  "category": "treasury_bills"},
    "SETB3MBENCH":  {"label": "SE T-bill 3 Months", "category": "treasury_bills"},
    "SETB6MBENCH":  {"label": "SE T-bill 6 Months", "category": "treasury_bills"},
    # Government bond yields
    "SEGVB2YC":     {"label": "SE GVB 2 Year",  "category": "govt_bonds"},
    "SEGVB5YC":     {"label": "SE GVB 5 Year",  "category": "govt_bonds"},
    "SEGVB7YC":     {"label": "SE GVB 7 Year",  "category": "govt_bonds"},
    "SEGVB10YC":    {"label": "SE GVB 10 Year", "category": "govt_bonds"},
    # Mortgage bonds
    "SEMB2YCACOMB": {"label": "SE Mortgage Bond 2 Year", "category": "mortgage_bonds"},
    "SEMB5YCACOMB": {"label": "SE Mortgage Bond 5 Year", "category": "mortgage_bonds"},
}


# ---------------------------------------------------------------------------
# Error container
# ---------------------------------------------------------------------------

class RiksbankError:
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
            "type":        "RiksbankError",
        }


# ---------------------------------------------------------------------------
# Main wrapper
# ---------------------------------------------------------------------------

class RiksbankWrapper:
    """
    Wrapper for the Sveriges Riksbank SWEA REST API.

    Exchange rates expressed as SEK per 1 unit of foreign currency.
    Interest rates are in percent per annum.
    API has rate limits; wrapper retries once on 429.
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

    def _get(self, path: str, retries: int = 1) -> Any:
        url  = f"{BASE_URL}/{path.lstrip('/')}"
        resp = self.session.get(url, timeout=DEFAULT_TIMEOUT)
        if resp.status_code == 429 and retries > 0:
            retry_after = int(resp.headers.get("Retry-After", RATE_LIMIT_WAIT))
            time.sleep(retry_after)
            return self._get(path, retries - 1)
        resp.raise_for_status()
        return resp.json()

    def _fetch_series(self, series_id: str,
                      from_date: Optional[str] = None,
                      to_date: Optional[str] = None) -> Dict[str, Any]:
        """Fetch observations for a single series."""
        if from_date is None:
            from_date = (date.today() - timedelta(days=365)).isoformat()
        if to_date:
            path = f"Observations/{series_id}/{from_date}/{to_date}"
        else:
            path = f"Observations/{series_id}/{from_date}"
        try:
            raw   = self._get(path)
            # raw is list of {date, value} or an error dict
            if isinstance(raw, dict) and raw.get("statusCode"):
                return RiksbankError(path, raw.get("message", str(raw)),
                                     raw.get("statusCode")).to_dict()
            rows = [{"date": r["date"], "value": r.get("value")} for r in raw]
            info = SERIES.get(series_id, {})
            return {
                "success":    True,
                "series_id":  series_id,
                "label":      info.get("label", series_id),
                "category":   info.get("category", ""),
                "from_date":  from_date,
                "to_date":    to_date or rows[-1]["date"] if rows else None,
                "data":       rows,
                "count":      len(rows),
                "source":     "Sveriges Riksbank",
                "url":        f"{BASE_URL}/{path}",
                "timestamp":  int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return RiksbankError(series_id, str(e), sc).to_dict()
        except Exception as e:
            return RiksbankError(series_id, str(e)).to_dict()

    def _multi(self, series_ids: List[str],
               from_date: Optional[str] = None,
               to_date: Optional[str] = None,
               delay: float = 0.5) -> Dict[str, Any]:
        """
        Fetch multiple series and pivot to wide format.
        Inserts a small delay between requests to respect rate limits.
        """
        if from_date is None:
            from_date = (date.today() - timedelta(days=365)).isoformat()

        wide: Dict[str, Dict[str, Any]] = {}
        fetched: List[str] = []
        errors: List[str]  = []

        for sid in series_ids:
            r = self._fetch_series(sid, from_date, to_date)
            if r.get("success"):
                fetched.append(sid)
                for row in r.get("data", []):
                    d = row["date"]
                    if d not in wide:
                        wide[d] = {"date": d}
                    wide[d][sid] = row.get("value")
            else:
                errors.append(f"{sid}: {r.get('error','unknown error')}")
            if delay and sid != series_ids[-1]:
                time.sleep(delay)

        rows = sorted(wide.values(), key=lambda x: x["date"])
        return {
            "success":   True,
            "series":    fetched,
            "errors":    errors,
            "from_date": from_date,
            "to_date":   to_date,
            "data":      rows,
            "count":     len(rows),
            "source":    "Sveriges Riksbank",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }

    # ------------------------------------------------------------------
    # Public methods
    # ------------------------------------------------------------------

    def get_policy_rate(self, from_date: Optional[str] = None,
                        to_date: Optional[str] = None) -> Dict[str, Any]:
        """Policy rate (repo rate) history."""
        return self._fetch_series("SECBREPOEFF", from_date, to_date)

    def get_policy_rates_all(self, from_date: Optional[str] = None,
                             to_date: Optional[str] = None) -> Dict[str, Any]:
        """Policy rate, deposit rate, and lending rate (wide format)."""
        ids = ["SECBREPOEFF", "SECBDEPOEFF", "SECBLENDEFF"]
        return self._multi(ids, from_date, to_date)

    def get_exchange_rate(self, currency: str = "USD",
                          from_date: Optional[str] = None,
                          to_date: Optional[str] = None) -> Dict[str, Any]:
        """SEK exchange rate for a single currency (e.g. USD, EUR, GBP)."""
        sid = f"SEK{currency.upper()}PMI"
        return self._fetch_series(sid, from_date, to_date)

    def get_exchange_rates(self, currencies: Optional[List[str]] = None,
                           from_date: Optional[str] = None,
                           to_date: Optional[str] = None) -> Dict[str, Any]:
        """SEK exchange rates for multiple currencies (wide format)."""
        if currencies is None:
            currencies = ["USD", "EUR", "GBP", "JPY", "CHF", "NOK", "DKK"]
        ids = [f"SEK{c.upper()}PMI" for c in currencies]
        result = self._multi(ids, from_date, to_date)
        result["currencies"] = currencies
        result["note"] = "SEK per 1 unit of foreign currency"
        return result

    def get_tbills(self, from_date: Optional[str] = None,
                   to_date: Optional[str] = None) -> Dict[str, Any]:
        """Swedish treasury bill benchmark rates: 1M, 3M, 6M (wide format)."""
        ids = ["SETB1MBENCHC", "SETB3MBENCH", "SETB6MBENCH"]
        result = self._multi(ids, from_date, to_date)
        result["tenors"] = ["1M", "3M", "6M"]
        return result

    def get_mortgage_bonds(self, from_date: Optional[str] = None,
                           to_date: Optional[str] = None) -> Dict[str, Any]:
        """Swedish mortgage bond yields: 2Y, 5Y (wide format)."""
        ids = ["SEMB2YCACOMB", "SEMB5YCACOMB"]
        return self._multi(ids, from_date, to_date)

    def get_govt_bond_yields(self, from_date: Optional[str] = None,
                             to_date: Optional[str] = None) -> Dict[str, Any]:
        """Swedish government bond yields: 2Y, 5Y, 7Y, 10Y (wide format)."""
        ids = ["SEGVB2YC", "SEGVB5YC", "SEGVB7YC", "SEGVB10YC"]
        return self._multi(ids, from_date, to_date)

    def get_series(self, series_id: str,
                   from_date: Optional[str] = None,
                   to_date: Optional[str] = None) -> Dict[str, Any]:
        """Fetch any series by its Riksbank series ID."""
        return self._fetch_series(series_id, from_date, to_date)

    def get_series_catalogue(self) -> Dict[str, Any]:
        """Full list of all series available in the Riksbank SWEA API."""
        try:
            data = self._get("Series")
            return {
                "success":   True,
                "count":     len(data),
                "data":      data,
                "source":    "Sveriges Riksbank",
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return RiksbankError("Series", str(e), sc).to_dict()
        except Exception as e:
            return RiksbankError("Series", str(e)).to_dict()

    def get_overview(self) -> Dict[str, Any]:
        """Snapshot: latest policy rate, EUR/SEK, USD/SEK, 10Y bond."""
        results: Dict[str, Any] = {}
        from_date = (date.today() - timedelta(days=7)).isoformat()
        for name, sid in [
            ("policy_rate", "SECBREPOEFF"),
            ("deposit_rate","SECBDEPOEFF"),
            ("eur_sek",     "SEKEURPMI"),
            ("usd_sek",     "SEKUSDPMI"),
            ("gbp_sek",     "SEKGBPPMI"),
            ("bond_10y",    "SEGVB10YC"),
            ("tbill_3m",    "SETB3MBENCH"),
        ]:
            r = self._fetch_series(sid, from_date)
            results[name] = {
                "success": r.get("success"),
                "label":   r.get("label", sid),
                "latest":  r.get("data", [{}])[-1] if r.get("data") else None,
            }
            time.sleep(0.3)  # be gentle with rate limits
        return {
            "success":   True,
            "data":      results,
            "source":    "Sveriges Riksbank",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }

    def available_series(self) -> Dict[str, Any]:
        """Built-in series catalogue by category."""
        by_cat: Dict[str, List] = {}
        for sid, info in SERIES.items():
            cat = info["category"]
            by_cat.setdefault(cat, []).append({"series_id": sid, "label": info["label"]})
        return {
            "success":   True,
            "data":      by_cat,
            "total":     len(SERIES),
            "note":      "Use get_series(series_id) for any Riksbank series",
            "source":    "Sveriges Riksbank",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

COMMANDS = {
    "policy_rate":  "[from_date] [to_date]              — Policy rate (repo rate) history",
    "policy_all":   "[from_date] [to_date]              — Policy/deposit/lending rates",
    "fx":           "[currencies] [from_date] [to_date] — SEK exchange rates (multi)",
    "fx_single":    "<currency> [from_date] [to_date]   — SEK rate for one currency",
    "tbills":       "[from_date] [to_date]              — T-bill benchmark rates (1M-6M)",
    "mortgage":     "[from_date] [to_date]              — Mortgage bond yields (2Y, 5Y)",
    "bonds":        "[from_date] [to_date]              — Govt bond yields (2Y-10Y)",
    "series":       "<series_id> [from_date] [to_date]  — Any series by ID",
    "catalogue":    "                                   — Full series catalogue from API",
    "overview":     "                                   — Key indicators snapshot",
    "available":    "                                   — Built-in series catalogue",
}


def _a(n: int, d: Any = None) -> Any:
    return sys.argv[n] if len(sys.argv) > n and sys.argv[n] else d


def main() -> None:
    if len(sys.argv) < 2:
        print(json.dumps({
            "error":    "No command provided.",
            "usage":    "python riksbank_data.py <command> [args...]",
            "commands": COMMANDS,
        }, indent=2))
        sys.exit(1)

    cmd     = sys.argv[1].lower()
    wrapper = RiksbankWrapper()

    try:
        if cmd == "policy_rate":
            result = wrapper.get_policy_rate(_a(2), _a(3))
        elif cmd in ("policy_all", "policy_rates"):
            result = wrapper.get_policy_rates_all(_a(2), _a(3))
        elif cmd in ("fx", "exchange_rates"):
            raw  = _a(2, "USD,EUR,GBP,JPY,CHF,NOK,DKK")
            ccys = [c.strip().upper() for c in raw.split(",")]
            result = wrapper.get_exchange_rates(ccys, _a(3), _a(4))
        elif cmd in ("fx_single", "fx_one"):
            if len(sys.argv) < 3:
                result = {"error": "fx_single requires <currency>"}
            else:
                result = wrapper.get_exchange_rate(sys.argv[2].upper(), _a(3), _a(4))
        elif cmd in ("tbills", "tbill"):
            result = wrapper.get_tbills(_a(2), _a(3))
        elif cmd in ("mortgage", "mortgage_bonds"):
            result = wrapper.get_mortgage_bonds(_a(2), _a(3))
        elif cmd in ("bonds", "bond_yields"):
            result = wrapper.get_govt_bond_yields(_a(2), _a(3))
        elif cmd == "series":
            if len(sys.argv) < 3:
                result = {"error": "series requires <series_id>"}
            else:
                result = wrapper.get_series(sys.argv[2], _a(3), _a(4))
        elif cmd == "catalogue":
            result = wrapper.get_series_catalogue()
        elif cmd == "overview":
            result = wrapper.get_overview()
        elif cmd in ("available", "list"):
            result = wrapper.available_series()
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
