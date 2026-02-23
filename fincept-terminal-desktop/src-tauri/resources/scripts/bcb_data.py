"""
Banco Central do Brasil (BCB) Data Wrapper
Fetches data from the BCB SGS (Sistema Gerenciador de Series Temporais) API.

API Reference:
  Base URL:  https://api.bcb.gov.br/dados/serie/bcdata.sgs.{series_id}/dados
  Format:    JSON array [{data: "DD/MM/YYYY", valor: "N.NN"}, ...]
  Auth:      None required — fully public

Endpoint patterns:
  All data:       GET /dados/serie/bcdata.sgs.{id}/dados?formato=json
  Date range:     GET /dados/serie/bcdata.sgs.{id}/dados?formato=json&dataInicial=DD/MM/YYYY&dataFinal=DD/MM/YYYY
  Last N:         GET /dados/serie/bcdata.sgs.{id}/dados/ultimos/{n}?formato=json
  Multiple SGS:   GET /dados/conjuntos/dados?codigoseries={id1},{id2}&formato=json

Key series IDs (verified):
  432   — Selic target rate (% per year)
  11    — Selic daily rate
  433   — IPCA monthly inflation (%)
  189   — IGP-M monthly inflation (%)
  1     — USD/BRL exchange rate (PTAX selling)
  21619 — EUR/BRL exchange rate
  4192  — EUR/BRL (older series)
  7326  — GDP annual growth rate (%)
  27791 — M1 (currency + demand deposits, R$ thousands)
  28000 — M2 monetary aggregate
  29037 — M3 monetary aggregate
  24369 — Unemployment rate (PNAD) %
  20539 — Total credit outstanding (R$ millions)
  13621 — International reserves (USD millions)
  4189  — Primary fiscal surplus/deficit (R$ millions)
  7478  — Net public debt (% GDP)
  4390  — Trade balance (USD millions, monthly)
  22707 — Current account (USD millions)
  3541  — TJLP (long-term interest rate)
  226   — TR (referential rate)
  1178  — CDB 1-day rate
  7809  — Bovespa index (monthly avg)

Returns JSON output for Rust/Tauri integration.
"""

import sys
import json
import requests
import traceback
from typing import Dict, Any, List, Optional
from datetime import datetime, timezone


BASE_URL        = "https://api.bcb.gov.br/dados/serie/bcdata.sgs"
MULTI_URL       = "https://api.bcb.gov.br/dados/conjuntos/dados"
DEFAULT_TIMEOUT = 30

# ---------------------------------------------------------------------------
# Series catalogue
# ---------------------------------------------------------------------------

SERIES = {
    # Monetary policy
    "selic_target":      {"id": 432,   "name": "Selic Target Rate",            "category": "monetary_policy", "unit": "% p.a.",      "freq": "daily"},
    "selic_daily":       {"id": 11,    "name": "Selic Daily Rate",              "category": "monetary_policy", "unit": "% p.a.",      "freq": "daily"},
    "tjlp":              {"id": 3541,  "name": "TJLP Long-Term Rate",           "category": "monetary_policy", "unit": "% p.a.",      "freq": "monthly"},
    "tr":                {"id": 226,   "name": "TR Referential Rate",           "category": "monetary_policy", "unit": "%",           "freq": "monthly"},
    # Inflation
    "ipca":              {"id": 433,   "name": "IPCA Monthly Inflation",        "category": "inflation",       "unit": "%",           "freq": "monthly"},
    "igpm":              {"id": 189,   "name": "IGP-M Monthly Inflation",       "category": "inflation",       "unit": "%",           "freq": "monthly"},
    # Exchange rates
    "usd_brl":           {"id": 1,     "name": "USD/BRL PTAX (selling)",        "category": "exchange_rates",  "unit": "BRL per USD", "freq": "daily"},
    "eur_brl":           {"id": 21619, "name": "EUR/BRL PTAX (selling)",        "category": "exchange_rates",  "unit": "BRL per EUR", "freq": "daily"},
    # Monetary aggregates
    "m1":                {"id": 27791, "name": "M1 Monetary Aggregate",         "category": "monetary",        "unit": "R$ thousands","freq": "monthly"},
    "m2":                {"id": 28000, "name": "M2 Monetary Aggregate",         "category": "monetary",        "unit": "R$ thousands","freq": "monthly"},
    "m3":                {"id": 29037, "name": "M3 Monetary Aggregate",         "category": "monetary",        "unit": "R$ thousands","freq": "monthly"},
    # GDP / real economy
    "gdp_growth":        {"id": 7326,  "name": "GDP Annual Growth Rate",        "category": "gdp",             "unit": "%",           "freq": "annual"},
    "unemployment":      {"id": 24369, "name": "Unemployment Rate (PNAD)",      "category": "labour",          "unit": "%",           "freq": "monthly"},
    # Credit
    "credit_total":      {"id": 20539, "name": "Total Credit Outstanding",      "category": "credit",          "unit": "R$ millions", "freq": "monthly"},
    # External sector
    "reserves":          {"id": 13621, "name": "International Reserves",        "category": "external",        "unit": "USD millions","freq": "daily"},
    "trade_balance":     {"id": 4390,  "name": "Trade Balance (monthly)",       "category": "external",        "unit": "USD millions","freq": "monthly"},
    "current_account":   {"id": 22707, "name": "Current Account Balance",       "category": "external",        "unit": "USD millions","freq": "monthly"},
    # Fiscal
    "primary_surplus":   {"id": 4189,  "name": "Primary Fiscal Surplus/Deficit","category": "fiscal",          "unit": "R$ millions", "freq": "monthly"},
    "net_public_debt":   {"id": 7478,  "name": "Net Public Debt (% GDP)",       "category": "fiscal",          "unit": "% GDP",       "freq": "monthly"},
    # Capital markets
    "bovespa":           {"id": 7809,  "name": "Bovespa Index Monthly Avg",     "category": "markets",         "unit": "index",       "freq": "monthly"},
    "cdb_1d":            {"id": 1178,  "name": "CDB 1-Day Rate",                "category": "interest_rates",  "unit": "% p.a.",      "freq": "daily"},
}

# Convenience groups
GROUPS = {
    "monetary_policy": ["selic_target", "selic_daily", "tjlp"],
    "inflation":       ["ipca", "igpm"],
    "exchange_rates":  ["usd_brl", "eur_brl"],
    "monetary":        ["m1", "m2", "m3"],
    "external":        ["reserves", "trade_balance", "current_account"],
    "fiscal":          ["primary_surplus", "net_public_debt"],
    "overview":        ["selic_target", "ipca", "usd_brl", "gdp_growth", "unemployment", "reserves"],
}


# ---------------------------------------------------------------------------
# Error container
# ---------------------------------------------------------------------------

class BCBError:
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
            "type":        "BCBError",
        }


# ---------------------------------------------------------------------------
# Main wrapper
# ---------------------------------------------------------------------------

class BCBWrapper:
    """
    Wrapper for Banco Central do Brasil SGS (Time Series Management System).

    The BCB SGS API returns JSON arrays: [{data: "DD/MM/YYYY", valor: "N.NN"}]
    All series are accessible without authentication.
    """

    def __init__(self):
        self.session = requests.Session()
        self.session.headers.update({
            "User-Agent": "Fincept-Terminal/3.3.1",
            "Accept":     "*/*",
        })

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _fetch_series(self, series_id: int, start_date: Optional[str] = None,
                      end_date: Optional[str] = None,
                      last_n: Optional[int] = None) -> List[Dict]:
        """
        Fetch a single SGS series.
        start_date/end_date: "DD/MM/YYYY" format
        last_n: fetch only last N observations
        """
        if last_n:
            url = f"{BASE_URL}.{series_id}/dados/ultimos/{last_n}"
        else:
            url = f"{BASE_URL}.{series_id}/dados"

        params: Dict[str, str] = {"formato": "json"}
        if start_date and not last_n:
            params["dataInicial"] = start_date
        if end_date and not last_n:
            params["dataFinal"] = end_date

        resp = self.session.get(url, params=params, timeout=DEFAULT_TIMEOUT)
        resp.raise_for_status()
        return resp.json()

    def _parse_rows(self, raw: List[Dict], series_name: str) -> List[Dict]:
        rows = []
        for item in raw:
            date_str = item.get("data", "")
            val_str  = item.get("valor", "")
            val      = None
            if val_str not in ("", None):
                try:
                    val = float(str(val_str).replace(",", "."))
                except ValueError:
                    val = val_str
            rows.append({"date": date_str, series_name: val})
        return rows

    def _get_series(self, series_key: str, start_date: Optional[str] = None,
                    end_date: Optional[str] = None,
                    last_n: Optional[int] = None) -> Dict[str, Any]:
        info = SERIES.get(series_key)
        if not info:
            return {"success": False, "error": f"Unknown series key: {series_key}",
                    "available_series": list(SERIES.keys())}
        sid = info["id"]
        # Daily series require a date window (BCB enforces max 10-year window)
        if info.get("freq") == "daily" and not start_date and not last_n:
            from datetime import timedelta
            start_date = (datetime.now(timezone.utc) - timedelta(days=5*365)).strftime("%d/%m/%Y")
        try:
            raw  = self._fetch_series(sid, start_date, end_date, last_n)
            rows = self._parse_rows(raw, series_key)
            return {
                "success":    True,
                "series":     series_key,
                "series_id":  sid,
                "name":       info["name"],
                "category":   info["category"],
                "unit":       info["unit"],
                "frequency":  info["freq"],
                "data":       rows,
                "count":      len(rows),
                "source":     "Banco Central do Brasil",
                "url":        f"{BASE_URL}.{sid}/dados",
                "timestamp":  int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return BCBError(series_key, str(e), sc).to_dict()
        except Exception as e:
            return BCBError(series_key, str(e)).to_dict()

    def _get_by_id(self, series_id: int, name: str, start_date: Optional[str] = None,
                   end_date: Optional[str] = None, last_n: Optional[int] = None) -> Dict[str, Any]:
        try:
            raw  = self._fetch_series(series_id, start_date, end_date, last_n)
            rows = self._parse_rows(raw, f"series_{series_id}")
            return {
                "success":   True,
                "series_id": series_id,
                "name":      name,
                "data":      rows,
                "count":     len(rows),
                "source":    "Banco Central do Brasil",
                "timestamp": int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            sc = e.response.status_code if e.response is not None else None
            return BCBError(str(series_id), str(e), sc).to_dict()
        except Exception as e:
            return BCBError(str(series_id), str(e)).to_dict()

    # ------------------------------------------------------------------
    # Public convenience methods
    # ------------------------------------------------------------------

    def get_selic(self, start_date: Optional[str] = None,
                  end_date: Optional[str] = None) -> Dict[str, Any]:
        """Selic target rate — monetary policy rate."""
        return self._get_series("selic_target", start_date, end_date)

    def get_selic_daily(self, start_date: Optional[str] = None,
                        end_date: Optional[str] = None) -> Dict[str, Any]:
        """Selic daily effective rate."""
        return self._get_series("selic_daily", start_date, end_date)

    def get_ipca(self, start_date: Optional[str] = None,
                 end_date: Optional[str] = None) -> Dict[str, Any]:
        """IPCA monthly consumer price inflation."""
        return self._get_series("ipca", start_date, end_date)

    def get_igpm(self, start_date: Optional[str] = None,
                 end_date: Optional[str] = None) -> Dict[str, Any]:
        """IGP-M monthly inflation index."""
        return self._get_series("igpm", start_date, end_date)

    def get_usd_brl(self, start_date: Optional[str] = None,
                    end_date: Optional[str] = None) -> Dict[str, Any]:
        """USD/BRL PTAX selling rate (daily)."""
        return self._get_series("usd_brl", start_date, end_date)

    def get_exchange_rates(self, start_date: Optional[str] = None,
                           end_date: Optional[str] = None) -> Dict[str, Any]:
        """USD/BRL and EUR/BRL exchange rates merged."""
        usd = self._get_series("usd_brl", start_date, end_date)
        eur = self._get_series("eur_brl", start_date, end_date)
        merged: Dict[str, Dict] = {}
        for row in usd.get("data", []):
            d = row["date"]
            merged[d] = {"date": d, "usd_brl": row.get("usd_brl")}
        for row in eur.get("data", []):
            d = row["date"]
            merged.setdefault(d, {"date": d})
            merged[d]["eur_brl"] = row.get("eur_brl")
        rows = sorted(merged.values(), key=lambda r: datetime.strptime(r["date"], "%d/%m/%Y"))
        return {
            "success":   True,
            "data":      rows,
            "count":     len(rows),
            "source":    "Banco Central do Brasil",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }

    def get_monetary_aggregates(self, start_date: Optional[str] = None,
                                end_date: Optional[str] = None) -> Dict[str, Any]:
        """M1, M2, M3 monetary aggregates merged."""
        merged: Dict[str, Dict] = {}
        for key in ["m1", "m2", "m3"]:
            r = self._get_series(key, start_date, end_date)
            for row in r.get("data", []):
                d = row["date"]
                merged.setdefault(d, {"date": d})
                merged[d].update({k: v for k, v in row.items() if k != "date"})
        rows = sorted(merged.values(), key=lambda r: datetime.strptime(r["date"], "%d/%m/%Y"))
        return {
            "success":   True,
            "data":      rows,
            "count":     len(rows),
            "source":    "Banco Central do Brasil",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }

    def get_gdp(self, start_date: Optional[str] = None,
                end_date: Optional[str] = None) -> Dict[str, Any]:
        """GDP annual growth rate."""
        return self._get_series("gdp_growth", start_date, end_date)

    def get_unemployment(self, start_date: Optional[str] = None,
                         end_date: Optional[str] = None) -> Dict[str, Any]:
        """Unemployment rate (PNAD survey)."""
        return self._get_series("unemployment", start_date, end_date)

    def get_credit(self, start_date: Optional[str] = None,
                   end_date: Optional[str] = None) -> Dict[str, Any]:
        """Total credit outstanding."""
        return self._get_series("credit_total", start_date, end_date)

    def get_reserves(self, start_date: Optional[str] = None,
                     end_date: Optional[str] = None) -> Dict[str, Any]:
        """International reserves (USD millions)."""
        return self._get_series("reserves", start_date, end_date)

    def get_trade_balance(self, start_date: Optional[str] = None,
                          end_date: Optional[str] = None) -> Dict[str, Any]:
        """Monthly trade balance (USD millions)."""
        return self._get_series("trade_balance", start_date, end_date)

    def get_fiscal(self, start_date: Optional[str] = None,
                   end_date: Optional[str] = None) -> Dict[str, Any]:
        """Primary fiscal surplus/deficit and net public debt merged."""
        merged: Dict[str, Dict] = {}
        for key in ["primary_surplus", "net_public_debt"]:
            r = self._get_series(key, start_date, end_date)
            for row in r.get("data", []):
                d = row["date"]
                merged.setdefault(d, {"date": d})
                merged[d].update({k: v for k, v in row.items() if k != "date"})
        rows = sorted(merged.values(), key=lambda r: r["date"])
        return {
            "success":   True,
            "data":      rows,
            "count":     len(rows),
            "source":    "Banco Central do Brasil",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }

    def get_overview(self) -> Dict[str, Any]:
        """Latest values for key indicators."""
        results: Dict[str, Any] = {}
        for key in GROUPS["overview"]:
            r = self._get_series(key, last_n=3)
            info = SERIES.get(key, {})
            results[key] = {
                "name":    info.get("name", key),
                "unit":    info.get("unit", ""),
                "success": r.get("success"),
                "latest":  r.get("data", [{}])[-1] if r.get("data") else None,
            }
        return {
            "success":   True,
            "data":      results,
            "source":    "Banco Central do Brasil",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }

    def get_series(self, series_key: str, start_date: Optional[str] = None,
                   end_date: Optional[str] = None,
                   last_n: Optional[int] = None) -> Dict[str, Any]:
        """Fetch any known series by key name."""
        return self._get_series(series_key, start_date, end_date, last_n)

    def get_series_by_id(self, series_id: int, start_date: Optional[str] = None,
                         end_date: Optional[str] = None,
                         last_n: Optional[int] = None) -> Dict[str, Any]:
        """Fetch any SGS series directly by numeric ID."""
        return self._get_by_id(series_id, f"series_{series_id}", start_date, end_date, last_n)

    def available_series(self) -> Dict[str, Any]:
        """Return full catalogue of available series."""
        by_cat: Dict[str, List] = {}
        for key, info in SERIES.items():
            cat = info["category"]
            by_cat.setdefault(cat, []).append({
                "key": key, "id": info["id"], "name": info["name"],
                "unit": info["unit"], "frequency": info["freq"],
            })
        return {
            "success":   True,
            "data":      by_cat,
            "groups":    GROUPS,
            "base_url":  BASE_URL,
            "source":    "Banco Central do Brasil",
            "timestamp": int(datetime.now(timezone.utc).timestamp()),
        }


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

COMMANDS = {
    "selic":          "[start DD/MM/YYYY] [end DD/MM/YYYY]  — Selic target rate",
    "selic_daily":    "[start] [end]                         — Selic daily rate",
    "ipca":           "[start] [end]                         — IPCA monthly inflation",
    "igpm":           "[start] [end]                         — IGP-M inflation",
    "usd_brl":        "[start] [end]                         — USD/BRL PTAX rate",
    "exchange_rates": "[start] [end]                         — USD/BRL + EUR/BRL merged",
    "monetary":       "[start] [end]                         — M1/M2/M3 aggregates",
    "gdp":            "[start] [end]                         — GDP annual growth",
    "unemployment":   "[start] [end]                         — Unemployment rate (PNAD)",
    "credit":         "[start] [end]                         — Total credit outstanding",
    "reserves":       "[start] [end]                         — International reserves",
    "trade_balance":  "[start] [end]                         — Monthly trade balance",
    "fiscal":         "[start] [end]                         — Fiscal surplus + public debt",
    "overview":       "                                       — Latest key indicators",
    "series":         "<key> [start] [end] [last_n]          — Any series by key name",
    "series_id":      "<id> [start] [end] [last_n]           — Any series by SGS number",
    "available":      "                                       — List all series",
}


def _a(n: int, d: Any = None) -> Any:
    return sys.argv[n] if len(sys.argv) > n and sys.argv[n] else d


def main() -> None:
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "No command provided.",
            "usage": "python bcb_data.py <command> [args...]",
            "commands": COMMANDS,
        }, indent=2))
        sys.exit(1)

    cmd     = sys.argv[1].lower()
    wrapper = BCBWrapper()

    try:
        if cmd == "selic":
            result = wrapper.get_selic(_a(2), _a(3))
        elif cmd == "selic_daily":
            result = wrapper.get_selic_daily(_a(2), _a(3))
        elif cmd == "ipca":
            result = wrapper.get_ipca(_a(2), _a(3))
        elif cmd == "igpm":
            result = wrapper.get_igpm(_a(2), _a(3))
        elif cmd == "usd_brl":
            result = wrapper.get_usd_brl(_a(2), _a(3))
        elif cmd == "exchange_rates":
            result = wrapper.get_exchange_rates(_a(2), _a(3))
        elif cmd in ("monetary", "monetary_aggregates"):
            result = wrapper.get_monetary_aggregates(_a(2), _a(3))
        elif cmd == "gdp":
            result = wrapper.get_gdp(_a(2), _a(3))
        elif cmd == "unemployment":
            result = wrapper.get_unemployment(_a(2), _a(3))
        elif cmd == "credit":
            result = wrapper.get_credit(_a(2), _a(3))
        elif cmd == "reserves":
            result = wrapper.get_reserves(_a(2), _a(3))
        elif cmd == "trade_balance":
            result = wrapper.get_trade_balance(_a(2), _a(3))
        elif cmd == "fiscal":
            result = wrapper.get_fiscal(_a(2), _a(3))
        elif cmd == "overview":
            result = wrapper.get_overview()
        elif cmd == "series":
            if len(sys.argv) < 3:
                result = {"error": "series requires <key>", "available": list(SERIES.keys())}
            else:
                ln = int(_a(5)) if _a(5) else None
                result = wrapper.get_series(sys.argv[2], _a(3), _a(4), ln)
        elif cmd == "series_id":
            if len(sys.argv) < 3:
                result = {"error": "series_id requires <numeric_id>"}
            else:
                ln = int(_a(5)) if _a(5) else None
                result = wrapper.get_series_by_id(int(sys.argv[2]), _a(3), _a(4), ln)
        elif cmd in ("available", "list"):
            result = wrapper.available_series()
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
