"""
Norges Bank (Bank of Norway) Data Wrapper
Fetches data from the Norges Bank open data SDMX API.

API Reference:
  Base URL:  https://data.norges-bank.no/api
  Format:    Semicolon-delimited CSV (SDMX)
  Auth:      None required — open data
  Docs:      https://www.norges-bank.no/en/topics/Statistics/open-data/

Data URL pattern:
  GET https://data.norges-bank.no/api/data/{flow}/{key}
    flow:        Dataflow ID (e.g. EXR, IR, ANN_KPRA)
    key:         Dot-separated dimension filter (e.g. B.USD+EUR.NOK.SP)
    format:      csv
    startPeriod: YYYY-MM-DD or YYYY-MM or YYYY
    endPeriod:   YYYY-MM-DD or YYYY-MM or YYYY
    locale:      en | no

Verified dataflows:
  ANN_KPRA            — Policy rate announcements (key rate, overnight lending, reserve)
  IR                  — Interest rates (NIBOR, NOWA historical, deposit rates)
  SHORT_RATES         — NOWA overnight rate + compounded averages
  EXR                 — Exchange rates (NOK per foreign currency, business day)
  FINANCIAL_INDICATORS— Financial Conditions Index and indicators
  GOVT_GENERIC_RATES  — Government bond yields by tenor (3M–10Y)
  MONEY_MARKET        — Interbank money market transactions

Returns JSON output for Rust/Tauri integration.
"""

import sys
import json
import csv
import io
import requests
import traceback
from typing import Dict, Any, List, Optional
from datetime import datetime, timezone


BASE_URL        = "https://data.norges-bank.no/api"
DEFAULT_TIMEOUT = 30

# ---------------------------------------------------------------------------
# Dataflow catalogue
# ---------------------------------------------------------------------------

DATAFLOWS = {
    "ANN_KPRA":           {"name": "Policy Rate Announcements",             "category": "monetary_policy", "freq": "per decision"},
    "IR":                 {"name": "Interest Rates (NIBOR, deposit rates)",  "category": "interest_rates",  "freq": "daily/monthly"},
    "SHORT_RATES":        {"name": "NOWA Overnight Rate",                   "category": "interest_rates",  "freq": "daily"},
    "EXR":                {"name": "Exchange Rates (NOK per foreign CCY)",   "category": "exchange_rates",  "freq": "daily/monthly"},
    "FINANCIAL_INDICATORS":{"name": "Financial Conditions Index",           "category": "financial",       "freq": "daily"},
    "GOVT_GENERIC_RATES": {"name": "Government Bond Yields",                "category": "govt_bonds",      "freq": "daily/monthly"},
    "MONEY_MARKET":       {"name": "Money Market Transactions",             "category": "money_market",    "freq": "daily"},
    "FAUCTION":           {"name": "F-Auction (liquidity)",                 "category": "liquidity",       "freq": "per auction"},
}

# Key series keys per flow
SERIES = {
    "EXR_MAJOR":  "B.USD+EUR+GBP+JPY+SEK+DKK+CHF+CNY.NOK.SP",
    "EXR_ALL":    "B..NOK.SP",
    "IR_NIBOR":   "M.NIBOR_1W+NIBOR_1M+NIBOR_3M+NIBOR_6M...",
    "GOVT_BONDS": "M.3Y+5Y+10Y.GBON.",
}


# ---------------------------------------------------------------------------
# Error container
# ---------------------------------------------------------------------------

class NorgesBankError:
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
            "type":        "NorgesBankError",
        }


# ---------------------------------------------------------------------------
# Main wrapper
# ---------------------------------------------------------------------------

class NorgesBankWrapper:
    """
    Wrapper for the Norges Bank open data SDMX/REST API.

    Exchange rates and interest rates are free, no auth required.
    CSV responses use semicolons; first row is the header with dimension names.
    The wrapper returns wide-format JSON rows.
    """

    def __init__(self):
        self.session = requests.Session()
        self.session.headers.update({
            "User-Agent": "Fincept-Terminal/3.3.1",
            "Accept":     "text/csv,text/plain,*/*",
        })

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _fetch(self, flow: str, key: str = "", start: Optional[str] = None,
               end: Optional[str] = None, locale: str = "en") -> str:
        path   = f"{BASE_URL}/data/{flow}"
        if key:
            path = f"{path}/{key}"
        params: Dict[str, str] = {"format": "csv", "locale": locale}
        if start:
            params["startPeriod"] = start
        if end:
            params["endPeriod"] = end
        resp = self.session.get(path, params=params, timeout=DEFAULT_TIMEOUT)
        resp.raise_for_status()
        return resp.text

    def _parse(self, text: str) -> Dict[str, Any]:
        """
        Parse Norges Bank SDMX CSV.

        Header row: FREQ;..label..;TENOR;...;TIME_PERIOD;OBS_VALUE
        Even-indexed cols are codes, odd-indexed are labels.
        TIME_PERIOD and OBS_VALUE are always the last two.
        Returns wide-format keyed on TIME_PERIOD.
        """
        reader = list(csv.reader(io.StringIO(text), delimiter=";"))
        if not reader:
            return {"data": [], "count": 0, "series": []}

        header = reader[0]

        # Find TIME_PERIOD and OBS_VALUE column indices
        try:
            tp_idx  = header.index("TIME_PERIOD")
            obs_idx = header.index("OBS_VALUE")
        except ValueError:
            return {"error": "Missing TIME_PERIOD/OBS_VALUE columns",
                    "raw_preview": text[:300]}

        # Dimension columns = everything before TIME_PERIOD (skip label cols)
        dim_cols = [i for i in range(0, tp_idx, 2)]   # code cols only
        dim_names = [header[i] for i in dim_cols]

        # Build rows — pivot by TIME_PERIOD + dimension combo
        wide: Dict[str, Dict[str, Any]] = {}
        series_set: List[str] = []

        for row in reader[1:]:
            if len(row) <= obs_idx:
                continue
            date_str = row[tp_idx].strip()
            if not date_str:
                continue
            raw_val  = row[obs_idx].strip()

            # Build dimension key
            dim_vals = [row[i].strip() for i in dim_cols]
            dim_key  = "_".join(v for v in dim_vals if v)

            if dim_key not in series_set:
                series_set.append(dim_key)

            if date_str not in wide:
                wide[date_str] = {"date": date_str}

            if raw_val in ("", "..", "N/A", "na"):
                wide[date_str][dim_key] = None
            else:
                try:
                    wide[date_str][dim_key] = float(raw_val.replace(",", ""))
                except ValueError:
                    wide[date_str][dim_key] = raw_val

        rows = sorted(wide.values(), key=lambda r: r["date"])
        return {"data": rows, "count": len(rows), "series": series_set,
                "dimensions": dim_names}

    def _get(self, flow: str, key: str = "", start: Optional[str] = None,
             end: Optional[str] = None) -> Dict[str, Any]:
        try:
            text   = self._fetch(flow, key, start, end)
            parsed = self._parse(text)
            if "error" in parsed:
                return {"success": False, **parsed}
            info = DATAFLOWS.get(flow, {})
            return {
                "success":    True,
                "flow":       flow,
                "flow_name":  info.get("name", flow),
                "category":   info.get("category", ""),
                "frequency":  info.get("freq", ""),
                "series":     parsed["series"],
                "dimensions": parsed.get("dimensions", []),
                "data":       parsed["data"],
                "count":      parsed["count"],
                "source":     "Norges Bank",
                "url":        f"{BASE_URL}/data/{flow}/{key}",
                "timestamp":  int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return NorgesBankError(flow, str(e), sc).to_dict()
        except Exception as e:
            return NorgesBankError(flow, str(e)).to_dict()

    # ------------------------------------------------------------------
    # Public convenience methods
    # ------------------------------------------------------------------

    def get_policy_rate(self, start: Optional[str] = None,
                        end: Optional[str] = None) -> Dict[str, Any]:
        """Policy rate announcements: key rate, overnight lending, reserve rate."""
        return self._get("ANN_KPRA", start=start, end=end)

    def get_exchange_rates(self, currencies: str = "USD+EUR+GBP+JPY+SEK+DKK+CHF+CNY",
                           start: Optional[str] = None,
                           end: Optional[str] = None) -> Dict[str, Any]:
        """Daily NOK exchange rates vs selected currencies."""
        key = f"B.{currencies}.NOK.SP"
        return self._get("EXR", key, start, end)

    def get_exchange_rates_monthly(self, currencies: str = "USD+EUR+GBP+JPY+SEK+DKK+CHF",
                                   start: Optional[str] = None,
                                   end: Optional[str] = None) -> Dict[str, Any]:
        """Monthly average NOK exchange rates."""
        key = f"M.{currencies}.NOK.SP"
        return self._get("EXR", key, start, end)

    def get_interest_rates(self, start: Optional[str] = None,
                           end: Optional[str] = None) -> Dict[str, Any]:
        """NIBOR rates and other interest rates (flow: IR)."""
        return self._get("IR", start=start, end=end)

    def get_nowa(self, start: Optional[str] = None,
                 end: Optional[str] = None) -> Dict[str, Any]:
        """NOWA overnight rate and compounded averages (flow: SHORT_RATES)."""
        return self._get("SHORT_RATES", start=start, end=end)

    def get_govt_bond_yields(self, tenors: str = "3Y+5Y+10Y",
                             start: Optional[str] = None,
                             end: Optional[str] = None) -> Dict[str, Any]:
        """Norwegian government bond yields (3Y, 5Y, 10Y)."""
        key = f"M.{tenors}.GBON."
        return self._get("GOVT_GENERIC_RATES", key, start, end)

    def get_govt_bond_yields_daily(self, tenors: str = "3Y+5Y+10Y",
                                   start: Optional[str] = None,
                                   end: Optional[str] = None) -> Dict[str, Any]:
        """Norwegian government bond yields — daily."""
        key = f"B.{tenors}.GBON."
        return self._get("GOVT_GENERIC_RATES", key, start, end)

    def get_financial_conditions(self, start: Optional[str] = None,
                                 end: Optional[str] = None) -> Dict[str, Any]:
        """Financial Conditions Index and related indicators."""
        return self._get("FINANCIAL_INDICATORS", start=start, end=end)

    def get_money_market(self, start: Optional[str] = None,
                         end: Optional[str] = None) -> Dict[str, Any]:
        """Interbank money market transaction volumes and rates."""
        return self._get("MONEY_MARKET", start=start, end=end)

    def get_overview(self, start: Optional[str] = None,
                     end: Optional[str] = None) -> Dict[str, Any]:
        """Snapshot: policy rate, NOK/USD, government bonds."""
        results: Dict[str, Any] = {}
        for name, flow, key in [
            ("policy_rate", "ANN_KPRA", ""),
            ("nok_usd",     "EXR",      "B.USD.NOK.SP"),
            ("nok_eur",     "EXR",      "B.EUR.NOK.SP"),
            ("govt_10y",    "GOVT_GENERIC_RATES", "B.10Y.GBON."),
        ]:
            r = self._get(flow, key, start, end)
            results[name] = {
                "success": r.get("success"),
                "count":   r.get("count"),
                "latest":  r.get("data", [{}])[-1] if r.get("data") else None,
            }
        return {
            "success":   True,
            "data":      results,
            "source":    "Norges Bank",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }

    def get_flow(self, flow: str, key: str = "",
                 start: Optional[str] = None,
                 end: Optional[str] = None) -> Dict[str, Any]:
        """Fetch any dataflow by ID with optional key filter."""
        return self._get(flow, key, start, end)

    def available_flows(self) -> Dict[str, Any]:
        """List all available dataflows."""
        by_cat: Dict[str, List] = {}
        for fid, info in DATAFLOWS.items():
            cat = info["category"]
            by_cat.setdefault(cat, []).append({
                "flow": fid, "name": info["name"], "frequency": info["freq"],
            })
        return {
            "success":   True,
            "data":      by_cat,
            "base_url":  BASE_URL,
            "source":    "Norges Bank",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

COMMANDS = {
    "policy_rate":    "[start] [end]                    — Policy rate announcements",
    "exchange_rates": "[currencies] [start] [end]        — Daily NOK FX rates",
    "fx_monthly":     "[currencies] [start] [end]        — Monthly avg NOK FX rates",
    "interest_rates": "[start] [end]                    — NIBOR & interest rates",
    "nowa":           "[start] [end]                    — NOWA overnight rate",
    "bond_yields":    "[tenors] [start] [end]            — Govt bond yields (monthly)",
    "bond_yields_d":  "[tenors] [start] [end]            — Govt bond yields (daily)",
    "financial":      "[start] [end]                    — Financial Conditions Index",
    "money_market":   "[start] [end]                    — Money market transactions",
    "overview":       "[start] [end]                    — Key indicators snapshot",
    "flow":           "<flow_id> [key] [start] [end]    — Any dataflow by ID",
    "available":      "                                 — List all dataflows",
}


def _a(n: int, d: Any = None) -> Any:
    return sys.argv[n] if len(sys.argv) > n and sys.argv[n] else d


def main() -> None:
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "No command provided.",
            "usage": "python norges_bank_data.py <command> [args...]",
            "commands": COMMANDS,
        }, indent=2))
        sys.exit(1)

    cmd     = sys.argv[1].lower()
    wrapper = NorgesBankWrapper()

    try:
        if cmd == "policy_rate":
            result = wrapper.get_policy_rate(_a(2), _a(3))
        elif cmd in ("exchange_rates", "fx"):
            result = wrapper.get_exchange_rates(_a(2, "USD+EUR+GBP+JPY+SEK+DKK+CHF+CNY"), _a(3), _a(4))
        elif cmd in ("fx_monthly", "exchange_rates_m"):
            result = wrapper.get_exchange_rates_monthly(_a(2, "USD+EUR+GBP+JPY+SEK+DKK+CHF"), _a(3), _a(4))
        elif cmd == "interest_rates":
            result = wrapper.get_interest_rates(_a(2), _a(3))
        elif cmd == "nowa":
            result = wrapper.get_nowa(_a(2), _a(3))
        elif cmd == "bond_yields":
            result = wrapper.get_govt_bond_yields(_a(2, "3Y+5Y+10Y"), _a(3), _a(4))
        elif cmd == "bond_yields_d":
            result = wrapper.get_govt_bond_yields_daily(_a(2, "3Y+5Y+10Y"), _a(3), _a(4))
        elif cmd == "financial":
            result = wrapper.get_financial_conditions(_a(2), _a(3))
        elif cmd == "money_market":
            result = wrapper.get_money_market(_a(2), _a(3))
        elif cmd == "overview":
            result = wrapper.get_overview(_a(2), _a(3))
        elif cmd == "flow":
            if len(sys.argv) < 3:
                result = {"error": "flow requires <flow_id>"}
            else:
                result = wrapper.get_flow(sys.argv[2], _a(3, ""), _a(4), _a(5))
        elif cmd in ("available", "flows"):
            result = wrapper.available_flows()
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
