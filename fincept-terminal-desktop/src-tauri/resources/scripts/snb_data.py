"""
Swiss National Bank (SNB) Data Wrapper
Fetches data from the SNB Data Portal REST API.

API Reference:
  Base URL:  https://data.snb.ch/api
  Format:    Semicolon-delimited CSV
  Auth:      None required — open data
  Docs:      https://data.snb.ch/en/help_api

Data URL pattern:
  GET https://data.snb.ch/api/cube/{cube_id}/data/csv/{lang}
    lang:      en | de | fr | it
    startDate: YYYY-MM  (or YYYY for annual)
    endDate:   YYYY-MM
    dimSel:    D0(code1,code2),D1(code3)  — filter specific dimension values

Dimensions URL (for discovering series codes):
  GET https://data.snb.ch/api/cube/{cube_id}/dimensions/{lang}
  Returns JSON with all dimension IDs and their allowed values.

CSV structure (semicolons):
  Row 1:  "CubeId";"<id>"
  Row 2:  "PublishingDate";"<date>"
  Row 3:  "Date";"D0"[;"D1"];"Value"   <- column header
  Row 4+: "YYYY-MM";"<code>"[;"<code>"];"<value>"

Multi-dimension cubes produce one row per (date × d0 × d1) combination.
The wrapper pivots these to wide format: date → {d0_d1: value}.

Returns JSON output for Rust/Tauri integration.
"""

import sys
import json
import io
import csv
import requests
import traceback
from typing import Dict, Any, List, Optional
from datetime import datetime, timezone


BASE_URL        = "https://data.snb.ch/api"
DEFAULT_LANG    = "en"
DEFAULT_TIMEOUT = 30

# ---------------------------------------------------------------------------
# Verified SNB Cube catalogue  (tested 2026-02)
# ---------------------------------------------------------------------------

CUBES = {
    # --- Monetary policy / interest rates ---
    "snbgwdzid":  {"name": "SNB Policy Rate & Money Market Rates (daily)",
                   "category": "monetary_policy", "freq": "daily",
                   "series": "LZ=Policy rate; SARON=SARON rate; ENG=Repo rate; ZIGBL=Sight deposit rate"},
    "zimoma":     {"name": "Money Market Reference Rates — Monthly",
                   "category": "interest_rates",  "freq": "monthly",
                   "series": "SARON=SARON; 1TGT=1M target; EG3M=3M Euromarket; EG6M=6M; EG12M=12M"},
    "rendoblid":  {"name": "Confederation Bond Yields — Daily",
                   "category": "interest_rates",  "freq": "daily",
                   "series": "1J-30J = 1Y to 30Y maturity"},
    "rendoblim":  {"name": "Confederation Bond Yields — Monthly",
                   "category": "interest_rates",  "freq": "monthly",
                   "series": "1J-30J = 1Y to 30Y maturity"},
    "snbiprogq":  {"name": "SNB Conditional Inflation Forecast — Quarterly",
                   "category": "inflation",        "freq": "quarterly",
                   "series": "Inflation forecasts from past MPAs"},
    # --- Exchange rates ---
    "devkua":     {"name": "CHF Exchange Rates — Annual",
                   "category": "exchange_rates",   "freq": "annual",
                   "series": "EUR1, USD1, JPY100, GBP1, CNY1, CAD1, AUD1, etc."},
    "devkum":     {"name": "CHF Exchange Rates — Monthly",
                   "category": "exchange_rates",   "freq": "monthly",
                   "series": "D0: M0=monthly avg / M1=end-of-month; D1: EUR1,USD1,JPY100,GBP1,CNY1..."},
    # --- Monetary aggregates ---
    "snbmonagg":  {"name": "Monetary Aggregates M1, M2, M3",
                   "category": "monetary",         "freq": "monthly",
                   "series": "D0: B=level/VV=YoY change; D1: GM1,GM2,GM3,ET,S0,S1,S2"},
    # --- External / reserves ---
    "snbimfra":   {"name": "Foreign Exchange Reserves (IMF template)",
                   "category": "external",         "freq": "monthly",
                   "series": "T0=total reserves; T1=foreign currency; T2=gold; T3=SDR"},
    # --- Bonds / capital market ---
    "capcollvf":  {"name": "Capital Market — Bond Issues",
                   "category": "capital_market",   "freq": "quarterly",
                   "series": "Domestic and foreign bond issues by sector"},
}

# Convenience groups
GROUPS = {
    "policy_rate":    ["snbgwdzid"],
    "interest_rates": ["snbgwdzid", "zimoma", "rendoblim"],
    "exchange_rates": ["devkum"],
    "monetary":       ["snbmonagg"],
    "fx_reserves":    ["snbimfra"],
    "inflation":      ["snbiprogq"],
    "bond_yields":    ["rendoblid"],
    "overview":       ["snbgwdzid", "devkum", "snbmonagg", "snbimfra"],
}


# ---------------------------------------------------------------------------
# Error container
# ---------------------------------------------------------------------------

class SNBError:
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
            "type":        "SNBError",
        }


# ---------------------------------------------------------------------------
# Main wrapper
# ---------------------------------------------------------------------------

class SNBWrapper:
    """
    Wrapper for the Swiss National Bank (SNB) Data Portal REST API.

    No authentication required. Data is in semicolon-delimited CSV.
    Cubes may have 1 or 2 dimension columns (D0, optionally D1).
    Multi-dimension data is pivoted to wide format: {date: {dim_key: value}}.
    """

    def __init__(self, lang: str = DEFAULT_LANG):
        self.lang    = lang
        self.session = requests.Session()
        self.session.headers.update({
            "User-Agent": "Fincept-Terminal/3.3.1 (SNB-wrapper)",
            "Accept":     "text/csv,text/plain,*/*",
        })

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _fetch_csv(self, cube_id: str, start_date: Optional[str] = None,
                   end_date: Optional[str] = None,
                   dim_sel: Optional[str] = None) -> str:
        url    = f"{BASE_URL}/cube/{cube_id}/data/csv/{self.lang}"
        params: Dict[str, str] = {}
        if start_date:
            params["startDate"] = start_date
        if end_date:
            params["endDate"] = end_date
        if dim_sel:
            params["dimSel"] = dim_sel
        resp = self.session.get(url, params=params, timeout=DEFAULT_TIMEOUT)
        resp.raise_for_status()
        return resp.text

    def _get_dimensions(self, cube_id: str) -> Dict[str, Any]:
        """Return dimension metadata JSON for a cube."""
        url  = f"{BASE_URL}/cube/{cube_id}/dimensions/{self.lang}"
        resp = self.session.get(url, timeout=DEFAULT_TIMEOUT)
        resp.raise_for_status()
        return resp.json()

    def _parse_csv(self, text: str) -> Dict[str, Any]:
        """
        Parse SNB semicolon CSV into wide-format rows.

        Handles 1-dim (Date;D0;Value) and 2-dim (Date;D0;D1;Value) layouts.
        Pivot: key = D0  for 1-dim, or  D0__D1  for 2-dim.
        """
        lines = text.strip().splitlines()
        cube_id  = ""
        pub_date = ""
        data_start = 0
        num_dims   = 1  # default

        for i, line in enumerate(lines):
            stripped = line.strip().strip('"')
            if line.startswith('"CubeId"') or line.startswith('CubeId'):
                parts = line.split(";")
                cube_id = parts[1].strip().strip('"') if len(parts) > 1 else ""
            elif line.startswith('"PublishingDate"') or line.startswith('PublishingDate'):
                parts = line.split(";")
                pub_date = parts[1].strip().strip('"') if len(parts) > 1 else ""
            elif line.startswith('"Date"') or line.startswith('Date'):
                # Count dimension columns
                header_parts = line.split(";")
                num_dims = len(header_parts) - 2  # subtract Date and Value
                data_start = i + 1
                break

        reader = csv.reader(lines[data_start:], delimiter=";")
        wide: Dict[str, Dict[str, Any]] = {}
        series_set: List[str] = []

        for row in reader:
            if len(row) < 3:
                continue
            date_str = row[0].strip().strip('"')
            if not date_str:
                continue

            raw_val = row[-1].strip().strip('"')  # last column always Value

            # Build pivot key from dimension columns
            if num_dims == 1:
                d0       = row[1].strip().strip('"')
                dim_key  = d0
            else:
                dims     = [row[j].strip().strip('"') for j in range(1, num_dims + 1)]
                dim_key  = "__".join(dims)

            if dim_key and dim_key not in series_set:
                series_set.append(dim_key)

            if date_str not in wide:
                wide[date_str] = {"date": date_str}

            if raw_val in ("", "..", "N/a", "N/A", "na", "NA", "-"):
                wide[date_str][dim_key] = None
            else:
                try:
                    wide[date_str][dim_key] = float(raw_val.replace(",", ""))
                except ValueError:
                    wide[date_str][dim_key] = raw_val

        rows = sorted(wide.values(), key=lambda r: r["date"])
        return {
            "cube_id":         cube_id,
            "publishing_date": pub_date,
            "series":          series_set,
            "data":            rows,
            "count":           len(rows),
        }

    def _fetch_cube(self, cube_id: str, start_date: Optional[str] = None,
                    end_date: Optional[str] = None,
                    dim_sel: Optional[str] = None) -> Dict[str, Any]:
        try:
            text   = self._fetch_csv(cube_id, start_date, end_date, dim_sel)
            parsed = self._parse_csv(text)
            info   = CUBES.get(cube_id, {})
            return {
                "success":         True,
                "cube":            cube_id,
                "cube_name":       info.get("name", cube_id),
                "category":        info.get("category", ""),
                "frequency":       info.get("freq", ""),
                "publishing_date": parsed["publishing_date"],
                "series":          parsed["series"],
                "series_notes":    info.get("series", ""),
                "data":            parsed["data"],
                "count":           parsed["count"],
                "source":          "Swiss National Bank",
                "url":             f"{BASE_URL}/cube/{cube_id}/data/csv/{self.lang}",
                "timestamp":       int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return SNBError(cube_id, str(e), sc).to_dict()
        except Exception as e:
            return SNBError(cube_id, str(e)).to_dict()

    # ------------------------------------------------------------------
    # Public convenience methods
    # ------------------------------------------------------------------

    def get_policy_rate(self, start_date: Optional[str] = None,
                        end_date: Optional[str] = None) -> Dict[str, Any]:
        """SNB policy rate, SARON, and sight deposit rate (cube: snbgwdzid)."""
        return self._fetch_cube("snbgwdzid", start_date, end_date,
                                dim_sel="D0(LZ,SARON,ENG,ZIGBL)")

    def get_money_market_rates(self, start_date: Optional[str] = None,
                               end_date: Optional[str] = None) -> Dict[str, Any]:
        """SARON and money market reference rates — monthly (cube: zimoma)."""
        return self._fetch_cube("zimoma", start_date, end_date,
                                dim_sel="D0(SARON,EG3M,EG6M,EG12M,1TGT)")

    def get_bond_yields_daily(self, start_date: Optional[str] = None,
                              end_date: Optional[str] = None) -> Dict[str, Any]:
        """Swiss Confederation bond yields — daily (cube: rendoblid)."""
        return self._fetch_cube("rendoblid", start_date, end_date,
                                dim_sel="D0(2J,5J,10J,20J,30J)")

    def get_bond_yields_monthly(self, start_date: Optional[str] = None,
                                end_date: Optional[str] = None) -> Dict[str, Any]:
        """Swiss Confederation bond yields — monthly (cube: rendoblim)."""
        return self._fetch_cube("rendoblim", start_date, end_date,
                                dim_sel="D0(2J,5J,10J,20J,30J)")

    def get_exchange_rates_monthly(self, start_date: Optional[str] = None,
                                   end_date: Optional[str] = None) -> Dict[str, Any]:
        """CHF exchange rates vs major currencies — monthly averages (cube: devkum)."""
        return self._fetch_cube("devkum", start_date, end_date,
                                dim_sel="D0(M0),D1(EUR1,USD1,JPY100,GBP1,CNY1,CAD1,AUD1,SEK1,NOK1)")

    def get_exchange_rates_annual(self, start_date: Optional[str] = None,
                                  end_date: Optional[str] = None) -> Dict[str, Any]:
        """CHF exchange rates vs major currencies — annual (cube: devkua)."""
        return self._fetch_cube("devkua", start_date, end_date)

    def get_monetary_aggregates(self, start_date: Optional[str] = None,
                                end_date: Optional[str] = None) -> Dict[str, Any]:
        """M1, M2, M3 monetary aggregates — levels (cube: snbmonagg)."""
        return self._fetch_cube("snbmonagg", start_date, end_date,
                                dim_sel="D0(B),D1(GM1,GM2,GM3)")

    def get_monetary_aggregates_full(self, start_date: Optional[str] = None,
                                     end_date: Optional[str] = None) -> Dict[str, Any]:
        """Full monetary aggregates with all sub-components (cube: snbmonagg)."""
        return self._fetch_cube("snbmonagg", start_date, end_date)

    def get_fx_reserves(self, start_date: Optional[str] = None,
                        end_date: Optional[str] = None) -> Dict[str, Any]:
        """Foreign exchange reserves — IMF template (cube: snbimfra)."""
        return self._fetch_cube("snbimfra", start_date, end_date)

    def get_inflation_forecast(self, start_date: Optional[str] = None,
                               end_date: Optional[str] = None) -> Dict[str, Any]:
        """SNB conditional inflation forecast (cube: snbiprogq)."""
        return self._fetch_cube("snbiprogq", start_date, end_date)

    def get_overview(self, start_date: Optional[str] = None,
                     end_date: Optional[str] = None) -> Dict[str, Any]:
        """Key snapshot: policy rate, FX, M3, reserves."""
        results: Dict[str, Any] = {}
        for name, cube, ds in [
            ("policy_rate",   "snbgwdzid", "D0(LZ,SARON)"),
            ("exchange_rates","devkum",    "D0(M0),D1(EUR1,USD1,GBP1,JPY100)"),
            ("monetary_m3",   "snbmonagg", "D0(B),D1(GM3)"),
            ("fx_reserves",   "snbimfra",  None),
        ]:
            r = self._fetch_cube(cube, start_date, end_date, dim_sel=ds)
            results[name] = {
                "success": r.get("success"),
                "count":   r.get("count"),
                "latest":  r.get("data", [{}])[-1] if r.get("data") else None,
            }
        return {
            "success":   True,
            "data":      results,
            "source":    "Swiss National Bank",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }

    def get_cube(self, cube_id: str, start_date: Optional[str] = None,
                 end_date: Optional[str] = None,
                 dim_sel: Optional[str] = None) -> Dict[str, Any]:
        """Fetch any SNB cube by ID with optional dimSel filter."""
        return self._fetch_cube(cube_id, start_date, end_date, dim_sel)

    def get_dimensions(self, cube_id: str) -> Dict[str, Any]:
        """Return all dimension codes for a cube (use to discover series codes)."""
        try:
            data = self._get_dimensions(cube_id)
            return {"success": True, "cube": cube_id, "data": data,
                    "source": "Swiss National Bank"}
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return SNBError(cube_id, str(e), sc).to_dict()
        except Exception as e:
            return SNBError(cube_id, str(e)).to_dict()

    def available_cubes(self) -> Dict[str, Any]:
        """Return catalogue of all known SNB cubes."""
        by_category: Dict[str, List] = {}
        for cid, info in CUBES.items():
            cat = info["category"]
            by_category.setdefault(cat, []).append({
                "cube":      cid,
                "name":      info["name"],
                "frequency": info["freq"],
                "series":    info.get("series", ""),
            })
        return {
            "success":   True,
            "data":      by_category,
            "groups":    GROUPS,
            "base_url":  BASE_URL,
            "source":    "Swiss National Bank",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------

COMMANDS = {
    "policy_rate":      "[start] [end]              — SNB policy rate + SARON",
    "saron":            "[start] [end]              — Money market reference rates",
    "bond_yields":      "[start] [end]              — Confederation bond yields (monthly)",
    "bond_yields_d":    "[start] [end]              — Confederation bond yields (daily)",
    "exchange_rates":   "[start] [end]              — CHF monthly FX rates",
    "exchange_rates_a": "[start] [end]              — CHF annual FX rates",
    "monetary":         "[start] [end]              — M1/M2/M3 aggregates",
    "monetary_full":    "[start] [end]              — All monetary aggregate components",
    "fx_reserves":      "[start] [end]              — Foreign exchange reserves",
    "inflation":        "[start] [end]              — SNB inflation forecast",
    "overview":         "[start] [end]              — Key indicators snapshot",
    "cube":             "<cube_id> [start] [end] [dimSel]  — Any cube by ID",
    "dimensions":       "<cube_id>                  — Show dimension codes for a cube",
    "available":        "                           — List all cubes and groups",
}


def _a(n: int, d: Any = None) -> Any:
    return sys.argv[n] if len(sys.argv) > n and sys.argv[n] else d


def main() -> None:
    if len(sys.argv) < 2:
        print(json.dumps({
            "error":    "No command provided.",
            "usage":    "python snb_data.py <command> [args...]",
            "commands": COMMANDS,
        }, indent=2))
        sys.exit(1)

    cmd     = sys.argv[1].lower()
    wrapper = SNBWrapper()

    try:
        if cmd == "policy_rate":
            result = wrapper.get_policy_rate(_a(2), _a(3))
        elif cmd in ("saron", "money_market"):
            result = wrapper.get_money_market_rates(_a(2), _a(3))
        elif cmd == "bond_yields":
            result = wrapper.get_bond_yields_monthly(_a(2), _a(3))
        elif cmd == "bond_yields_d":
            result = wrapper.get_bond_yields_daily(_a(2), _a(3))
        elif cmd in ("exchange_rates", "fx"):
            result = wrapper.get_exchange_rates_monthly(_a(2), _a(3))
        elif cmd in ("exchange_rates_a", "fx_annual"):
            result = wrapper.get_exchange_rates_annual(_a(2), _a(3))
        elif cmd in ("monetary", "monetary_aggregates"):
            result = wrapper.get_monetary_aggregates(_a(2), _a(3))
        elif cmd == "monetary_full":
            result = wrapper.get_monetary_aggregates_full(_a(2), _a(3))
        elif cmd == "fx_reserves":
            result = wrapper.get_fx_reserves(_a(2), _a(3))
        elif cmd == "inflation":
            result = wrapper.get_inflation_forecast(_a(2), _a(3))
        elif cmd == "overview":
            result = wrapper.get_overview(_a(2), _a(3))
        elif cmd == "cube":
            if len(sys.argv) < 3:
                result = {"error": "cube requires <cube_id>"}
            else:
                result = wrapper.get_cube(sys.argv[2], _a(3), _a(4), _a(5))
        elif cmd == "dimensions":
            if len(sys.argv) < 3:
                result = {"error": "dimensions requires <cube_id>"}
            else:
                result = wrapper.get_dimensions(sys.argv[2])
        elif cmd in ("available", "cubes"):
            result = wrapper.available_cubes()
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
