"""
Bank of Israel (BOI) Data Wrapper
Fetches data from the Bank of Israel public APIs.

API References:
  SDMX REST API:
    Base URL: https://edge.boi.gov.il/FusionEdgeServer/sdmx/v2
    Format:   CSV (csvdata) or XML
    Auth:     None required — fully public
    Docs:     https://edge.boi.gov.il/FusionEdgeServer/sdmx/v2/swagger-ui/

  Public JSON API:
    Base URL: https://www.boi.org.il/PublicApi
    Auth:     None required

Dataflows (SDMX):
  EXR  — Exchange rates (ILS per foreign currency)
    Series pattern: RER_{CCY}_ILS  (e.g. RER_USD_ILS, RER_EUR_ILS)
    Special: NER_ILS_BSK_IDX — effective exchange rate index
             NER_ILS_BSK_PCT — percent change in effective rate
    Unit multiplier in column UNIT_MULT:
      0 → rate as-is  (e.g. USD/ILS = 3.12)
      1 → rate ÷ 10   (e.g. LBP)
      2 → rate ÷ 100  (e.g. JPY/ILS reported per 100 JPY)

  Public JSON /GetExchangeRates — latest spot rates with daily change

Key exchange rate series:
  RER_USD_ILS, RER_EUR_ILS, RER_GBP_ILS, RER_JPY_ILS,
  RER_CHF_ILS, RER_AUD_ILS, RER_CAD_ILS, RER_SEK_ILS,
  RER_NOK_ILS, RER_DKK_ILS, RER_ZAR_ILS, RER_JOD_ILS

Returns JSON output for Rust/Tauri integration.
"""

import sys
import csv
import io
import json
import requests
import traceback
from typing import Dict, Any, List, Optional
from datetime import datetime, date, timedelta, timezone


SDMX_URL        = "https://edge.boi.gov.il/FusionEdgeServer/sdmx/v2"
PUBLIC_URL      = "https://www.boi.org.il/PublicApi"
DEFAULT_TIMEOUT = 30

# All known EXR series codes with their base currency and unit multiplier
EXR_SERIES: Dict[str, Dict[str, Any]] = {
    "RER_USD_ILS": {"base": "USD", "label": "USD/ILS", "multiplier": 1},
    "RER_EUR_ILS": {"base": "EUR", "label": "EUR/ILS", "multiplier": 1},
    "RER_GBP_ILS": {"base": "GBP", "label": "GBP/ILS", "multiplier": 1},
    "RER_JPY_ILS": {"base": "JPY", "label": "JPY/ILS (per 100)", "multiplier": 100},
    "RER_CHF_ILS": {"base": "CHF", "label": "CHF/ILS", "multiplier": 1},
    "RER_AUD_ILS": {"base": "AUD", "label": "AUD/ILS", "multiplier": 1},
    "RER_CAD_ILS": {"base": "CAD", "label": "CAD/ILS", "multiplier": 1},
    "RER_SEK_ILS": {"base": "SEK", "label": "SEK/ILS", "multiplier": 1},
    "RER_NOK_ILS": {"base": "NOK", "label": "NOK/ILS", "multiplier": 1},
    "RER_DKK_ILS": {"base": "DKK", "label": "DKK/ILS", "multiplier": 1},
    "RER_ZAR_ILS": {"base": "ZAR", "label": "ZAR/ILS", "multiplier": 1},
    "RER_JOD_ILS": {"base": "JOD", "label": "JOD/ILS", "multiplier": 1},
    "RER_EGP_ILS": {"base": "EGP", "label": "EGP/ILS", "multiplier": 1},
    "NER_ILS_BSK_IDX": {"base": "BASKET", "label": "Nominal effective exchange rate index", "multiplier": 1},
    "NER_ILS_BSK_PCT":  {"base": "BASKET", "label": "Nominal effective exchange rate % change", "multiplier": 1},
}

MAJOR_SERIES = ["RER_USD_ILS", "RER_EUR_ILS", "RER_GBP_ILS", "RER_JPY_ILS",
                "RER_CHF_ILS", "RER_AUD_ILS", "RER_CAD_ILS"]


# ---------------------------------------------------------------------------
# Error container
# ---------------------------------------------------------------------------

class BOIError:
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
            "type":        "BOIError",
        }


# ---------------------------------------------------------------------------
# Main wrapper
# ---------------------------------------------------------------------------

class BOIWrapper:
    """
    Wrapper for the Bank of Israel public data APIs.

    Exchange rates are ILS (New Israeli Shekel) per 1 unit of foreign currency,
    except JPY which is per 100 JPY (multiplier=100).
    Data is published on Israeli business days (Sun–Thu).
    """

    def __init__(self):
        self.session = requests.Session()
        self.session.headers.update({
            "User-Agent": "Fincept-Terminal/3.3.1",
            "Accept":     "text/csv, application/json, */*",
        })

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _sdmx_csv(self, dataflow: str, series_key: str = "",
                  start: Optional[str] = None,
                  end: Optional[str] = None) -> str:
        """Fetch SDMX CSV data from the BOI FusionEdge server."""
        path = f"{SDMX_URL}/data/dataflow/BOI.STATISTICS/{dataflow}/1.0"
        if series_key:
            path = f"{path}/{series_key}"
        params: Dict[str, str] = {"format": "csvdata"}
        if start:
            params["startperiod"] = start
        if end:
            params["endperiod"] = end
        resp = self.session.get(path, params=params, timeout=DEFAULT_TIMEOUT)
        resp.raise_for_status()
        return resp.text

    def _parse_exr_csv(self, text: str) -> List[Dict[str, Any]]:
        """
        Parse BOI EXR CSV into wide-format rows keyed by date.
        Handles UNIT_MULT: obs value is already in ILS per currency unit,
        but for series with multiplier>1 (e.g. JPY), we note it in label.
        """
        reader = csv.DictReader(io.StringIO(text))
        wide: Dict[str, Dict[str, Any]] = {}
        for row in reader:
            series = row.get("SERIES_CODE", "").strip()
            d      = row.get("TIME_PERIOD", "").strip()
            val    = row.get("OBS_VALUE",  "").strip()
            if not d or not series:
                continue
            if d not in wide:
                wide[d] = {"date": d}
            if val not in ("", "..", "N/A"):
                try:
                    wide[d][series] = float(val)
                except ValueError:
                    wide[d][series] = val
        return sorted(wide.values(), key=lambda r: r["date"])

    def _public_json(self, path: str, params: Optional[Dict] = None) -> Any:
        """Fetch from the BOI PublicApi JSON endpoint."""
        url  = f"{PUBLIC_URL}/{path}"
        resp = self.session.get(url, params=params or {}, timeout=DEFAULT_TIMEOUT)
        resp.raise_for_status()
        return resp.json()

    # ------------------------------------------------------------------
    # Public methods
    # ------------------------------------------------------------------

    def get_exchange_rates_today(self) -> Dict[str, Any]:
        """Latest ILS exchange rates from the PublicApi (with daily change %)."""
        try:
            data  = self._public_json("GetExchangeRates")
            rates = data.get("exchangeRates", [])
            result_rows = []
            for r in rates:
                result_rows.append({
                    "currency":    r.get("key"),
                    "rate":        r.get("currentExchangeRate"),
                    "change_pct":  round(r.get("currentChange", 0), 6),
                    "unit":        r.get("unit", 1),
                    "last_update": r.get("lastUpdate"),
                })
            return {
                "success":   True,
                "data":      result_rows,
                "count":     len(result_rows),
                "note":      "ILS per 1 unit of foreign currency (unit=1) or per unit quantity",
                "source":    "Bank of Israel",
                "url":       f"{PUBLIC_URL}/GetExchangeRates",
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return BOIError("GetExchangeRates", str(e), sc).to_dict()
        except Exception as e:
            return BOIError("GetExchangeRates", str(e)).to_dict()

    def get_exchange_rate_history(self, currencies: Optional[List[str]] = None,
                                  start: Optional[str] = None,
                                  end: Optional[str] = None) -> Dict[str, Any]:
        """Historical ILS exchange rates via SDMX (wide format by date)."""
        if currencies is None:
            currencies = ["USD", "EUR", "GBP", "JPY", "CHF", "AUD", "CAD"]
        if start is None:
            start = (date.today() - timedelta(days=90)).isoformat()
        series_codes = "+".join(f"RER_{c}_ILS" for c in currencies)
        try:
            text = self._sdmx_csv("EXR", series_codes, start, end)
            rows = self._parse_exr_csv(text)
            return {
                "success":    True,
                "currencies": currencies,
                "start":      start,
                "end":        end or date.today().isoformat(),
                "data":       rows,
                "count":      len(rows),
                "note":       "ILS per foreign currency unit; JPY series is per 100 JPY",
                "source":     "Bank of Israel",
                "timestamp":  int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return BOIError("EXR history", str(e), sc).to_dict()
        except Exception as e:
            return BOIError("EXR history", str(e)).to_dict()

    def get_usd_ils(self, start: Optional[str] = None,
                    end: Optional[str] = None) -> Dict[str, Any]:
        """USD/ILS exchange rate history."""
        return self.get_exchange_rate_history(["USD"], start, end)

    def get_eur_ils(self, start: Optional[str] = None,
                    end: Optional[str] = None) -> Dict[str, Any]:
        """EUR/ILS exchange rate history."""
        return self.get_exchange_rate_history(["EUR"], start, end)

    def get_effective_rate(self, start: Optional[str] = None,
                           end: Optional[str] = None) -> Dict[str, Any]:
        """Nominal effective exchange rate index (basket) and % change."""
        if start is None:
            start = (date.today() - timedelta(days=365)).isoformat()
        try:
            text = self._sdmx_csv("EXR", "NER_ILS_BSK_IDX+NER_ILS_BSK_PCT", start, end)
            rows = self._parse_exr_csv(text)
            return {
                "success":   True,
                "series":    ["NER_ILS_BSK_IDX", "NER_ILS_BSK_PCT"],
                "start":     start,
                "data":      rows,
                "count":     len(rows),
                "note":      "Nominal effective exchange rate index (higher = stronger ILS)",
                "source":    "Bank of Israel",
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return BOIError("NER", str(e), sc).to_dict()
        except Exception as e:
            return BOIError("NER", str(e)).to_dict()

    def get_all_rates_history(self, start: Optional[str] = None,
                              end: Optional[str] = None) -> Dict[str, Any]:
        """All EXR series in wide format (all currencies vs ILS)."""
        if start is None:
            start = (date.today() - timedelta(days=30)).isoformat()
        try:
            text = self._sdmx_csv("EXR", start=start, end=end)
            rows = self._parse_exr_csv(text)
            return {
                "success":   True,
                "start":     start,
                "data":      rows,
                "count":     len(rows),
                "series":    list(EXR_SERIES.keys()),
                "note":      "ILS per foreign currency unit",
                "source":    "Bank of Israel",
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return BOIError("EXR all", str(e), sc).to_dict()
        except Exception as e:
            return BOIError("EXR all", str(e)).to_dict()

    def get_overview(self) -> Dict[str, Any]:
        """Snapshot: latest rates (PublicApi) + USD & EUR history."""
        today    = self.get_exchange_rates_today()
        usd_hist = self.get_usd_ils(
            start=(date.today() - timedelta(days=7)).isoformat()
        )
        return {
            "success":   True,
            "data": {
                "latest_rates": today,
                "usd_week":     usd_hist.get("data", [])[-5:],
            },
            "source":    "Bank of Israel",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }

    def available_series(self) -> Dict[str, Any]:
        """Built-in EXR series catalogue."""
        return {
            "success":   True,
            "data":      EXR_SERIES,
            "note":      "All rates expressed as ILS per 1 unit of foreign currency",
            "source":    "Bank of Israel",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

COMMANDS = {
    "today":     "                               — Latest ILS rates (all currencies) with daily change",
    "history":   "[currencies] [start] [end]     — Historical rates (wide format)",
    "usd":       "[start] [end]                  — USD/ILS history",
    "eur":       "[start] [end]                  — EUR/ILS history",
    "effective": "[start] [end]                  — Nominal effective exchange rate index",
    "all":       "[start] [end]                  — All series history (wide format)",
    "overview":  "                               — Snapshot: latest + USD week",
    "available": "                               — Series catalogue",
}


def _a(n: int, d: Any = None) -> Any:
    return sys.argv[n] if len(sys.argv) > n and sys.argv[n] else d


def main() -> None:
    if len(sys.argv) < 2:
        print(json.dumps({
            "error":    "No command provided.",
            "usage":    "python boi_data.py <command> [args...]",
            "commands": COMMANDS,
        }, indent=2))
        sys.exit(1)

    cmd     = sys.argv[1].lower()
    wrapper = BOIWrapper()

    try:
        if cmd == "today":
            result = wrapper.get_exchange_rates_today()
        elif cmd in ("history", "fx"):
            raw  = _a(2, "USD,EUR,GBP,JPY,CHF,AUD,CAD")
            ccys = [c.strip().upper() for c in raw.split(",")]
            result = wrapper.get_exchange_rate_history(ccys, _a(3), _a(4))
        elif cmd == "usd":
            result = wrapper.get_usd_ils(_a(2), _a(3))
        elif cmd == "eur":
            result = wrapper.get_eur_ils(_a(2), _a(3))
        elif cmd in ("effective", "ner"):
            result = wrapper.get_effective_rate(_a(2), _a(3))
        elif cmd == "all":
            result = wrapper.get_all_rates_history(_a(2), _a(3))
        elif cmd == "overview":
            result = wrapper.get_overview()
        elif cmd in ("available", "series"):
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
