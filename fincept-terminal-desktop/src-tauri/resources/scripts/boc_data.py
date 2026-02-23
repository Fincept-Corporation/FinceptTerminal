"""
Bank of Canada (BoC) Data Wrapper
Fetches data from the Bank of Canada Valet API.

API Reference:
  Base URL:  https://www.bankofcanada.ca/valet
  Format:    JSON
  Auth:      None required — fully public
  Docs:      https://www.bankofcanada.ca/valet/docs

Endpoints:
  GET /lists/series/json              — Full catalogue of all 15,000+ series
  GET /lists/groups/json              — Groups/categories
  GET /observations/{series}/json     — Observation history
    ?recent=N                         — Last N observations
    ?start_date=YYYY-MM-DD            — From date
    ?end_date=YYYY-MM-DD              — To date
  GET /observations/group/{group}/json — All series in a group

Key series IDs:
  FXUSDCAD, FXEURCAD, FXGBPCAD, FXJPYCAD, FXCHFCAD, FXAUDCAD — Exchange rates
  STATIC_ATABLE_V39079  — Overnight rate target (policy rate, end of month)
  AVG.INTWO             — CORRA (Canadian Overnight Repo Rate Average)
  V80691342             — 1-month treasury bill yield
  V80691344             — 3-month treasury bill yield
  V80691346             — 6-month treasury bill yield
  V80691348             — 1-year treasury bill yield
  V122530               — Prime rate
  A.BCPI                — Bank of Canada commodity price index
  A.ENER                — Energy commodity price sub-index

Returns JSON output for Rust/Tauri integration.
"""

import sys
import json
import requests
import traceback
from typing import Dict, Any, List, Optional
from datetime import datetime, date, timedelta, timezone


BASE_URL        = "https://www.bankofcanada.ca/valet"
DEFAULT_TIMEOUT = 30


# ---------------------------------------------------------------------------
# Key series catalogue
# ---------------------------------------------------------------------------

SERIES = {
    # Exchange rates (CAD per 1 foreign currency unit)
    "FXUSDCAD": {"label": "USD/CAD",  "category": "exchange_rates"},
    "FXEURCAD": {"label": "EUR/CAD",  "category": "exchange_rates"},
    "FXGBPCAD": {"label": "GBP/CAD",  "category": "exchange_rates"},
    "FXJPYCAD": {"label": "JPY/CAD",  "category": "exchange_rates"},
    "FXCHFCAD": {"label": "CHF/CAD",  "category": "exchange_rates"},
    "FXAUDCAD": {"label": "AUD/CAD",  "category": "exchange_rates"},
    "FXNZDCAD": {"label": "NZD/CAD",  "category": "exchange_rates"},
    "FXHKDCAD": {"label": "HKD/CAD",  "category": "exchange_rates"},
    "FXSEKCAD": {"label": "SEK/CAD",  "category": "exchange_rates"},
    "FXNOKCAD": {"label": "NOK/CAD",  "category": "exchange_rates"},
    "FXDKKCAD": {"label": "DKK/CAD",  "category": "exchange_rates"},
    "FXSGDCAD": {"label": "SGD/CAD",  "category": "exchange_rates"},
    "FXCNYCAD": {"label": "CNY/CAD",  "category": "exchange_rates"},
    "FXINRCAD": {"label": "INR/CAD",  "category": "exchange_rates"},
    "FXMXNCAD": {"label": "MXN/CAD",  "category": "exchange_rates"},
    # Interest rates / policy
    "STATIC_ATABLE_V39079": {"label": "Overnight rate target",    "category": "interest_rates"},
    "AVG.INTWO":            {"label": "CORRA overnight repo rate", "category": "interest_rates"},
    "V80691342":            {"label": "T-bill 1 month",           "category": "interest_rates"},
    "V80691344":            {"label": "T-bill 3 month",           "category": "interest_rates"},
    "V80691346":            {"label": "T-bill 6 month",           "category": "interest_rates"},
    "V80691348":            {"label": "T-bill 1 year",            "category": "interest_rates"},
    "V122530":              {"label": "Prime rate",                "category": "interest_rates"},
    # Commodity prices
    "A.BCPI":               {"label": "Commodity Price Index",     "category": "commodities"},
    "A.ENER":               {"label": "Energy sub-index",          "category": "commodities"},
    "A.MTLS":               {"label": "Metals sub-index",          "category": "commodities"},
    "A.AGRI":               {"label": "Agriculture sub-index",     "category": "commodities"},
}

FX_SERIES = [k for k, v in SERIES.items() if v["category"] == "exchange_rates"]
RATE_SERIES = [k for k, v in SERIES.items() if v["category"] == "interest_rates"]


# ---------------------------------------------------------------------------
# Error container
# ---------------------------------------------------------------------------

class BoCError:
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
            "type":        "BoCError",
        }


# ---------------------------------------------------------------------------
# Main wrapper
# ---------------------------------------------------------------------------

class BoCWrapper:
    """
    Wrapper for the Bank of Canada Valet REST API.

    All data is free, no authentication required.
    Exchange rates are expressed as CAD per 1 unit of foreign currency.
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

    def _parse_observations(self, data: Dict[str, Any]) -> List[Dict[str, Any]]:
        """Flatten Valet observations into wide-format list of {date, series: value}."""
        obs_raw  = data.get("observations", [])
        rows: List[Dict[str, Any]] = []
        for o in obs_raw:
            row: Dict[str, Any] = {"date": o.get("d", "")}
            for k, v in o.items():
                if k == "d":
                    continue
                val = v.get("v") if isinstance(v, dict) else v
                if val is not None:
                    try:
                        row[k] = float(val)
                    except (ValueError, TypeError):
                        row[k] = val
            rows.append(row)
        return rows

    def _obs(self, series_ids: str, recent: Optional[int] = None,
             start_date: Optional[str] = None,
             end_date: Optional[str] = None) -> Dict[str, Any]:
        """Fetch observations for one or more comma-joined series IDs."""
        params: Dict[str, Any] = {}
        if recent:
            params["recent"] = recent
        if start_date:
            params["start_date"] = start_date
        if end_date:
            params["end_date"] = end_date
        try:
            data = self._get(f"observations/{series_ids}/json", params)
            rows = self._parse_observations(data)
            detail = data.get("seriesDetail", {})
            return {
                "success":    True,
                "series":     series_ids,
                "detail":     detail,
                "data":       rows,
                "count":      len(rows),
                "source":     "Bank of Canada",
                "url":        f"{BASE_URL}/observations/{series_ids}/json",
                "timestamp":  int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return BoCError(series_ids, str(e), sc).to_dict()
        except Exception as e:
            return BoCError(series_ids, str(e)).to_dict()

    # ------------------------------------------------------------------
    # Public methods
    # ------------------------------------------------------------------

    def get_exchange_rates(self, currencies: Optional[List[str]] = None,
                           recent: int = 30,
                           start_date: Optional[str] = None,
                           end_date: Optional[str] = None) -> Dict[str, Any]:
        """Daily CAD exchange rates for major currencies."""
        if currencies is None:
            currencies = ["USD", "EUR", "GBP", "JPY", "CHF", "AUD", "CNY"]
        ids = ",".join(f"FX{c}CAD" for c in currencies)
        result = self._obs(ids, recent if not start_date else None, start_date, end_date)
        result["currencies"] = currencies
        result["note"] = "CAD per 1 unit of foreign currency"
        return result

    def get_usd_cad(self, recent: int = 30,
                    start_date: Optional[str] = None,
                    end_date: Optional[str] = None) -> Dict[str, Any]:
        """USD/CAD exchange rate history."""
        return self._obs("FXUSDCAD", recent if not start_date else None, start_date, end_date)

    def get_eur_cad(self, recent: int = 30,
                    start_date: Optional[str] = None,
                    end_date: Optional[str] = None) -> Dict[str, Any]:
        """EUR/CAD exchange rate history."""
        return self._obs("FXEURCAD", recent if not start_date else None, start_date, end_date)

    def get_policy_rate(self, recent: int = 24,
                        start_date: Optional[str] = None,
                        end_date: Optional[str] = None) -> Dict[str, Any]:
        """Overnight rate target (policy rate) history."""
        return self._obs("STATIC_ATABLE_V39079",
                         recent if not start_date else None, start_date, end_date)

    def get_corra(self, recent: int = 30,
                  start_date: Optional[str] = None,
                  end_date: Optional[str] = None) -> Dict[str, Any]:
        """CORRA — Canadian Overnight Repo Rate Average."""
        return self._obs("AVG.INTWO",
                         recent if not start_date else None, start_date, end_date)

    def get_prime_rate(self, recent: int = 24,
                       start_date: Optional[str] = None,
                       end_date: Optional[str] = None) -> Dict[str, Any]:
        """Bank prime lending rate history."""
        return self._obs("V122530",
                         recent if not start_date else None, start_date, end_date)

    def get_tbill_yields(self, recent: int = 30,
                         start_date: Optional[str] = None,
                         end_date: Optional[str] = None) -> Dict[str, Any]:
        """Government of Canada T-bill yields: 1M, 3M, 6M, 1Y."""
        ids = "V80691342,V80691344,V80691346,V80691348"
        result = self._obs(ids, recent if not start_date else None, start_date, end_date)
        result["tenors"] = ["1M", "3M", "6M", "1Y"]
        return result

    def get_commodity_prices(self, recent: int = 12,
                             start_date: Optional[str] = None,
                             end_date: Optional[str] = None) -> Dict[str, Any]:
        """Bank of Canada Commodity Price Index and sub-indices."""
        ids = "A.BCPI,A.ENER,A.MTLS,A.AGRI"
        return self._obs(ids, recent if not start_date else None, start_date, end_date)

    def get_series(self, series_id: str, recent: Optional[int] = None,
                   start_date: Optional[str] = None,
                   end_date: Optional[str] = None) -> Dict[str, Any]:
        """Fetch any series by its Valet series ID."""
        return self._obs(series_id, recent, start_date, end_date)

    def get_overview(self) -> Dict[str, Any]:
        """Snapshot: latest policy rate, USD/CAD, EUR/CAD, CORRA, prime rate."""
        results: Dict[str, Any] = {}
        for name, sid in [
            ("policy_rate", "STATIC_ATABLE_V39079"),
            ("corra",       "AVG.INTWO"),
            ("prime_rate",  "V122530"),
            ("usd_cad",     "FXUSDCAD"),
            ("eur_cad",     "FXEURCAD"),
            ("gbp_cad",     "FXGBPCAD"),
        ]:
            r = self._obs(sid, recent=1)
            results[name] = {
                "success": r.get("success"),
                "latest":  r.get("data", [{}])[-1] if r.get("data") else None,
            }
        return {
            "success":   True,
            "data":      results,
            "source":    "Bank of Canada",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }

    def available_series(self) -> Dict[str, Any]:
        """Return the built-in series catalogue."""
        by_cat: Dict[str, List] = {}
        for sid, info in SERIES.items():
            cat = info["category"]
            by_cat.setdefault(cat, []).append({"series_id": sid, "label": info["label"]})
        return {
            "success":    True,
            "data":       by_cat,
            "total":      len(SERIES),
            "note":       "Use get_series(series_id) for any of the 15,000+ Valet series",
            "source":     "Bank of Canada",
            "timestamp":  int(datetime.now(timezone.utc).timestamp()),
        }

    def list_all_series(self) -> Dict[str, Any]:
        """Fetch the full Valet series catalogue (15,000+ series)."""
        try:
            data  = self._get("lists/series/json")
            items = data.get("series", {})
            return {
                "success":   True,
                "count":     len(items),
                "data":      items,
                "source":    "Bank of Canada",
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return BoCError("lists/series", str(e), sc).to_dict()
        except Exception as e:
            return BoCError("lists/series", str(e)).to_dict()


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

COMMANDS = {
    "fx":          "[currencies...] [recent=30] [start] [end]  — CAD exchange rates",
    "usd":         "[recent=30] [start] [end]                  — USD/CAD history",
    "eur":         "[recent=30] [start] [end]                  — EUR/CAD history",
    "policy_rate": "[recent=24] [start] [end]                  — Overnight rate target",
    "corra":       "[recent=30] [start] [end]                  — CORRA overnight repo rate",
    "prime":       "[recent=24] [start] [end]                  — Prime lending rate",
    "tbills":      "[recent=30] [start] [end]                  — T-bill yields (1M-1Y)",
    "commodities": "[recent=12] [start] [end]                  — Commodity price indices",
    "series":      "<series_id> [recent] [start] [end]         — Any series by ID",
    "overview":    "                                            — Key indicators snapshot",
    "available":   "                                            — Built-in series catalogue",
    "list":        "                                            — Full Valet series catalogue",
}


def _a(n: int, d: Any = None) -> Any:
    return sys.argv[n] if len(sys.argv) > n and sys.argv[n] else d


def main() -> None:
    if len(sys.argv) < 2:
        print(json.dumps({
            "error":    "No command provided.",
            "usage":    "python boc_data.py <command> [args...]",
            "commands": COMMANDS,
        }, indent=2))
        sys.exit(1)

    cmd     = sys.argv[1].lower()
    wrapper = BoCWrapper()

    try:
        if cmd in ("fx", "exchange_rates"):
            # fx [cur1,cur2,...] [recent] [start] [end]
            raw = _a(2, "USD,EUR,GBP,JPY,CHF,AUD,CNY")
            ccys = [c.strip().upper() for c in raw.split(",")]
            n    = int(_a(3, 30))
            result = wrapper.get_exchange_rates(ccys, n, _a(4), _a(5))
        elif cmd == "usd":
            result = wrapper.get_usd_cad(int(_a(2, 30)), _a(3), _a(4))
        elif cmd == "eur":
            result = wrapper.get_eur_cad(int(_a(2, 30)), _a(3), _a(4))
        elif cmd in ("policy_rate", "policy"):
            result = wrapper.get_policy_rate(int(_a(2, 24)), _a(3), _a(4))
        elif cmd == "corra":
            result = wrapper.get_corra(int(_a(2, 30)), _a(3), _a(4))
        elif cmd in ("prime", "prime_rate"):
            result = wrapper.get_prime_rate(int(_a(2, 24)), _a(3), _a(4))
        elif cmd in ("tbills", "tbill"):
            result = wrapper.get_tbill_yields(int(_a(2, 30)), _a(3), _a(4))
        elif cmd in ("commodities", "commodity"):
            result = wrapper.get_commodity_prices(int(_a(2, 12)), _a(3), _a(4))
        elif cmd == "series":
            if len(sys.argv) < 3:
                result = {"error": "series requires <series_id>"}
            else:
                n = int(_a(3)) if _a(3) else None
                result = wrapper.get_series(sys.argv[2], n, _a(4), _a(5))
        elif cmd == "overview":
            result = wrapper.get_overview()
        elif cmd in ("available", "catalogue"):
            result = wrapper.available_series()
        elif cmd in ("list", "list_all"):
            result = wrapper.list_all_series()
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
