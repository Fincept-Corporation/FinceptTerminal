"""
Bank Negara Malaysia (BNM) Data Wrapper
Fetches data from the BNM Open API.

API Reference:
  Base URL:  https://api.bnm.gov.my/public
  Format:    JSON
  Auth:      None required — fully public
  Docs:      https://api.bnm.gov.my/

Accept header required: application/vnd.BNM.API.v1+json

Endpoints:
  GET /exchange-rate                    — All currency rates (current session)
    ?session=0900|1200|1130             — Rate session (0900=morning, 1200=noon, 1130=closing)
  GET /exchange-rate/{currency}         — Single currency rate
    ?session=0900|1200|1130
  GET /interest-rate                    — Base rate, BLR, OPR, KLIBOR
  GET /base-rate                        — Base rate and BLR
  GET /kijang-emas                      — Gold coin (Kijang Emas) prices
  GET /interbank-swap                   — Interbank FX swap
  GET /opr                             — OPR (overnight policy rate) latest

Exchange rates expressed as MYR per foreign currency unit.
Some currencies use unit=100 (AED, HKD, IDR, INR, JPY, KHR, KRW, MMK,
NPR, PHP, PKR, SAR, THB, TWD, VND); the wrapper normalises to MYR per 1 unit.

27 currencies: AED, AUD, BND, CAD, CHF, CNY, EGP, EUR, GBP, HKD, IDR,
               INR, JPY, KHR, KRW, MMK, NOK, NPR, NZD, PHP, PKR, SAR,
               SDR, SGD, THB, TWD, USD, VND

Returns JSON output for Rust/Tauri integration.
"""

import sys
import json
import requests
import traceback
from typing import Dict, Any, List, Optional
from datetime import datetime, date, timedelta, timezone


BASE_URL        = "https://api.bnm.gov.my/public"
ACCEPT_HEADER   = "application/vnd.BNM.API.v1+json"
DEFAULT_TIMEOUT = 30

# Sessions: morning fixing, noon, 1130 closing
SESSIONS = ["0900", "1200", "1130"]

MAJOR_CURRENCIES = ["USD", "EUR", "GBP", "JPY", "CHF", "AUD", "CAD",
                    "SGD", "CNY", "HKD", "INR", "NZD"]
ASEAN_CURRENCIES = ["SGD", "IDR", "THB", "PHP", "VND", "KHR", "MMK",
                    "BND", "AUD", "NZD"]

# Unit multipliers per currency (unit=100 → divide by 100 to get MYR per 1 unit)
UNIT_MAP: Dict[str, int] = {
    "AED": 100, "HKD": 100, "IDR": 100, "INR": 100,
    "JPY": 100, "KHR": 100, "KRW": 100, "MMK": 100,
    "NPR": 100, "PHP": 100, "PKR": 100, "SAR": 100,
    "THB": 100, "TWD": 100, "VND": 100,
}


# ---------------------------------------------------------------------------
# Error container
# ---------------------------------------------------------------------------

class BNMError:
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
            "type":        "BNMError",
        }


# ---------------------------------------------------------------------------
# Main wrapper
# ---------------------------------------------------------------------------

class BNMWrapper:
    """
    Wrapper for the Bank Negara Malaysia (BNM) Open API.

    Exchange rates are MYR (Malaysian Ringgit) per 1 unit of foreign currency.
    Currencies with unit=100 are normalised by the wrapper.
    Three rate sessions per day: 0900 (morning fix), 1200 (noon), 1130 (closing).
    """

    def __init__(self):
        self.session = requests.Session()
        self.session.headers.update({
            "User-Agent": "Fincept-Terminal/3.3.1",
            "Accept":     ACCEPT_HEADER,
        })

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _get(self, path: str, params: Optional[Dict] = None) -> Any:
        url  = f"{BASE_URL}/{path.lstrip('/')}"
        resp = self.session.get(url, params=params or {}, timeout=DEFAULT_TIMEOUT)
        resp.raise_for_status()
        return resp.json()

    def _normalise_rate(self, value: Optional[float],
                        unit: int) -> Optional[float]:
        """Normalise rate: if unit=100, divide by 100 to get per-1-unit."""
        if value is None:
            return None
        return round(value / unit, 6) if unit > 1 else value

    def _parse_fx_item(self, item: Dict[str, Any]) -> Dict[str, Any]:
        """Parse a single FX item from BNM API into clean dict."""
        code   = item.get("currency_code", "")
        raw    = item.get("unit", 1)
        unit   = int(raw) if raw else UNIT_MAP.get(code, 1)
        rate   = item.get("rate", {})
        return {
            "currency":   code,
            "date":       rate.get("date", ""),
            "buying":     self._normalise_rate(rate.get("buying_rate"),  unit),
            "selling":    self._normalise_rate(rate.get("selling_rate"), unit),
            "middle":     self._normalise_rate(rate.get("middle_rate"),  unit),
            "unit":       unit,
        }

    # ------------------------------------------------------------------
    # Public methods
    # ------------------------------------------------------------------

    def get_exchange_rates(self, session: str = "1130") -> Dict[str, Any]:
        """All currency rates vs MYR for the specified daily session."""
        session = session.strip()
        try:
            data    = self._get("exchange-rate", {"session": session})
            items   = data.get("data", [])
            meta    = data.get("meta", {})
            parsed  = [self._parse_fx_item(i) for i in items]
            # Build simple flat dict: {USD: middle_rate, ...}
            flat: Dict[str, Optional[float]] = {}
            for p in parsed:
                flat[p["currency"]] = p["middle"] or p["buying"]
            return {
                "success":    True,
                "session":    session,
                "date":       parsed[0]["date"] if parsed else "",
                "data":       flat,
                "full_data":  parsed,
                "count":      len(parsed),
                "note":       "MYR per 1 unit of foreign currency (normalised from unit=100 where applicable)",
                "source":     "Bank Negara Malaysia",
                "url":        f"{BASE_URL}/exchange-rate",
                "meta":       meta,
                "timestamp":  int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return BNMError("exchange-rate", str(e), sc).to_dict()
        except Exception as e:
            return BNMError("exchange-rate", str(e)).to_dict()

    def get_currency(self, currency: str, session: str = "1130") -> Dict[str, Any]:
        """Current rate for a single currency vs MYR."""
        currency = currency.upper()
        try:
            data   = self._get(f"exchange-rate/{currency}", {"session": session})
            item   = data.get("data", {})
            meta   = data.get("meta", {})
            parsed = self._parse_fx_item(item)
            return {
                "success":   True,
                "session":   session,
                "data":      parsed,
                "note":      f"MYR per 1 {currency}",
                "source":    "Bank Negara Malaysia",
                "meta":      meta,
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return BNMError(f"exchange-rate/{currency}", str(e), sc).to_dict()
        except Exception as e:
            return BNMError(f"exchange-rate/{currency}", str(e)).to_dict()

    def get_all_sessions(self, currency: str = "USD") -> Dict[str, Any]:
        """Fetch a currency rate across all three daily sessions."""
        currency = currency.upper()
        results: Dict[str, Any] = {}
        for sess in SESSIONS:
            r = self.get_currency(currency, sess)
            if r.get("success"):
                results[sess] = r.get("data", {})
            else:
                results[sess] = {"error": r.get("error", "failed")}
        return {
            "success":   True,
            "currency":  currency,
            "sessions":  results,
            "note":      "MYR per 1 unit; sessions: 0900=morning fix, 1200=noon, 1130=closing",
            "source":    "Bank Negara Malaysia",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }

    def get_interest_rates(self) -> Dict[str, Any]:
        """Base rate, BLR, OPR, and KLIBOR rates."""
        try:
            data = self._get("interest-rate")
            return {
                "success":   True,
                "data":      data.get("data", data),
                "source":    "Bank Negara Malaysia",
                "url":       f"{BASE_URL}/interest-rate",
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return BNMError("interest-rate", str(e), sc).to_dict()
        except Exception as e:
            return BNMError("interest-rate", str(e)).to_dict()

    def get_base_rate(self) -> Dict[str, Any]:
        """Bank lending base rate (BR) and base lending rate (BLR)."""
        try:
            data = self._get("base-rate")
            return {
                "success":   True,
                "data":      data.get("data", data),
                "source":    "Bank Negara Malaysia",
                "url":       f"{BASE_URL}/base-rate",
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return BNMError("base-rate", str(e), sc).to_dict()
        except Exception as e:
            return BNMError("base-rate", str(e)).to_dict()

    def get_opr(self) -> Dict[str, Any]:
        """Overnight Policy Rate (OPR) — Malaysia's key policy rate."""
        try:
            data = self._get("opr")
            return {
                "success":   True,
                "data":      data.get("data", data),
                "note":      "OPR is Bank Negara Malaysia's key monetary policy rate",
                "source":    "Bank Negara Malaysia",
                "url":       f"{BASE_URL}/opr",
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return BNMError("opr", str(e), sc).to_dict()
        except Exception as e:
            return BNMError("opr", str(e)).to_dict()

    def get_kijang_emas(self) -> Dict[str, Any]:
        """Kijang Emas gold coin buying and selling prices (MYR)."""
        try:
            data = self._get("kijang-emas")
            return {
                "success":   True,
                "data":      data.get("data", data),
                "note":      "Kijang Emas: Malaysian gold investment coin",
                "source":    "Bank Negara Malaysia",
                "url":       f"{BASE_URL}/kijang-emas",
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return BNMError("kijang-emas", str(e), sc).to_dict()
        except Exception as e:
            return BNMError("kijang-emas", str(e)).to_dict()

    def get_major_currencies(self, session: str = "1130") -> Dict[str, Any]:
        """Major currencies vs MYR."""
        result = self.get_exchange_rates(session)
        if not result.get("success"):
            return result
        flat = result.get("data", {})
        major = {c: flat[c] for c in MAJOR_CURRENCIES if c in flat}
        return {
            "success":    True,
            "session":    session,
            "date":       result.get("date", ""),
            "currencies": MAJOR_CURRENCIES,
            "data":       major,
            "note":       "MYR per 1 unit of foreign currency",
            "source":     "Bank Negara Malaysia",
            "timestamp":  int(datetime.now(timezone.utc).timestamp()),
        }

    def get_asean_currencies(self, session: str = "1130") -> Dict[str, Any]:
        """ASEAN & Asia-Pacific currencies vs MYR."""
        result = self.get_exchange_rates(session)
        if not result.get("success"):
            return result
        flat = result.get("data", {})
        asean = {c: flat[c] for c in ASEAN_CURRENCIES if c in flat}
        return {
            "success":    True,
            "session":    session,
            "date":       result.get("date", ""),
            "currencies": ASEAN_CURRENCIES,
            "data":       asean,
            "note":       "MYR per 1 unit of foreign currency",
            "source":     "Bank Negara Malaysia",
            "timestamp":  int(datetime.now(timezone.utc).timestamp()),
        }

    def get_overview(self) -> Dict[str, Any]:
        """Snapshot: major rates + OPR + gold (Kijang Emas)."""
        fx   = self.get_major_currencies()
        opr  = self.get_opr()
        gold = self.get_kijang_emas()
        return {
            "success":   True,
            "date":      fx.get("date", ""),
            "data": {
                "exchange_rates": fx.get("data", {}),
                "opr":            opr.get("data", {}),
                "kijang_emas":    gold.get("data", {}),
            },
            "source":    "Bank Negara Malaysia",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }

    def available_currencies(self) -> Dict[str, Any]:
        """List available currencies and their unit multipliers."""
        return {
            "success":          True,
            "all_currencies":   [
                "AED", "AUD", "BND", "CAD", "CHF", "CNY", "EGP", "EUR",
                "GBP", "HKD", "IDR", "INR", "JPY", "KHR", "KRW", "MMK",
                "NOK", "NPR", "NZD", "PHP", "PKR", "SAR", "SDR", "SGD",
                "THB", "TWD", "USD", "VND",
            ],
            "major_currencies": MAJOR_CURRENCIES,
            "asean_currencies": ASEAN_CURRENCIES,
            "unit_100_currencies": list(UNIT_MAP.keys()),
            "sessions":         SESSIONS,
            "note":             "Rates expressed as MYR per 1 unit (unit=100 currencies are normalised)",
            "source":           "Bank Negara Malaysia",
            "timestamp":        int(datetime.now(timezone.utc).timestamp()),
        }


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

COMMANDS = {
    "fx":        "[session=1130]               — All exchange rates vs MYR",
    "currency":  "<CCY> [session=1130]          — Single currency vs MYR",
    "sessions":  "<CCY>                         — Currency across all 3 daily sessions",
    "major":     "[session=1130]               — Major currencies vs MYR",
    "asean":     "[session=1130]               — ASEAN/APAC currencies vs MYR",
    "opr":       "                             — Overnight Policy Rate history",
    "base_rate": "                             — Base rate and BLR",
    "interest":  "                             — Interest rates (BR, BLR, OPR, KLIBOR)",
    "gold":      "                             — Kijang Emas gold coin prices",
    "overview":  "                             — Snapshot: FX + OPR + gold",
    "available": "                             — List available currencies",
}


def _a(n: int, d: Any = None) -> Any:
    return sys.argv[n] if len(sys.argv) > n and sys.argv[n] else d


def main() -> None:
    if len(sys.argv) < 2:
        print(json.dumps({
            "error":    "No command provided.",
            "usage":    "python bnm_data.py <command> [args...]",
            "commands": COMMANDS,
        }, indent=2))
        sys.exit(1)

    cmd     = sys.argv[1].lower()
    wrapper = BNMWrapper()

    try:
        if cmd in ("fx", "exchange_rates"):
            result = wrapper.get_exchange_rates(_a(2, "1130"))
        elif cmd == "currency":
            if len(sys.argv) < 3:
                result = {"error": "currency requires <CCY>"}
            else:
                result = wrapper.get_currency(sys.argv[2], _a(3, "1130"))
        elif cmd == "sessions":
            if len(sys.argv) < 3:
                result = {"error": "sessions requires <CCY>"}
            else:
                result = wrapper.get_all_sessions(sys.argv[2])
        elif cmd == "major":
            result = wrapper.get_major_currencies(_a(2, "1130"))
        elif cmd == "asean":
            result = wrapper.get_asean_currencies(_a(2, "1130"))
        elif cmd == "opr":
            result = wrapper.get_opr()
        elif cmd in ("base_rate", "base"):
            result = wrapper.get_base_rate()
        elif cmd in ("interest", "interest_rates"):
            result = wrapper.get_interest_rates()
        elif cmd in ("gold", "kijang_emas"):
            result = wrapper.get_kijang_emas()
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
