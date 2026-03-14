"""
Reserve Bank of Australia (RBA) Data Wrapper
Fetches statistical data from RBA published CSV tables.

API Reference:
  Base URL:  https://www.rba.gov.au/statistics/tables/csv/
  Format:    CSV files, no auth required, no rate limits published
  Tables:    https://www.rba.gov.au/statistics/tables/

Authentication: None required — all data is freely available.

CSV Structure:
  Row 1: Series IDs (column headers)
  Row 2: Series descriptions
  Row 3: Frequency (Monthly / Quarterly / Daily / Annual)
  Row 4: Data type (Orig / Seasonally Adjusted / Trend)
  Row 5: Units
  Row 6: Source
  Row 7: Publication date
  Row 8: Blank separator
  Row 9+: DATE, value1, value2, ...  (date format: DD-Mon-YYYY or Mon-YYYY or YYYY)

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


BASE_URL = "https://www.rba.gov.au/statistics/tables/csv"
DEFAULT_TIMEOUT = 30

# ---------------------------------------------------------------------------
# RBA Table catalogue
# ---------------------------------------------------------------------------

TABLES = {
    # Interest rates
    "f1":  {"name": "Money Market Interest Rates",              "category": "interest_rates"},
    "f2":  {"name": "Capital Market Yields - Government Bonds", "category": "interest_rates"},
    "f3":  {"name": "Capital Market Yields - Non-Government",   "category": "interest_rates"},
    "f4":  {"name": "Retail Deposit and Investment Rates",      "category": "interest_rates"},
    "f5":  {"name": "Indicator Lending Rates",                  "category": "interest_rates"},
    "f6":  {"name": "Housing Lending Rates",                    "category": "interest_rates"},
    "f7":  {"name": "Borrowing and Deposit Rates (Business)",   "category": "interest_rates"},
    # Exchange rates
    "f11": {"name": "Exchange Rates - Daily",                   "category": "exchange_rates"},
    "f12": {"name": "USD Exchange Rates",                       "category": "exchange_rates"},
    "f15": {"name": "Real Exchange Rate Measures",              "category": "exchange_rates"},
    # Monetary & credit aggregates
    "d1":  {"name": "Growth in Selected Financial Aggregates",  "category": "monetary"},
    "d2":  {"name": "Lending and Credit Aggregates",            "category": "monetary"},
    "d3":  {"name": "Monetary Aggregates",                      "category": "monetary"},
    # Balance of payments / external
    "b1":  {"name": "RBA Balance Sheet",                        "category": "balance_sheet"},
    "b2":  {"name": "Banknotes on Issue",                       "category": "balance_sheet"},
    # Inflation / prices
    "g1":  {"name": "Consumer Price Inflation",                 "category": "inflation"},
    "g2":  {"name": "Consumer Price Inflation Expectations",    "category": "inflation"},
    "g3":  {"name": "Inflation Expectations Survey",            "category": "inflation"},
    # Housing
    "f20": {"name": "House Price Growth",                       "category": "housing"},
    # Labour / economic activity
    "h1":  {"name": "Labour Market",                            "category": "labour"},
    "h3":  {"name": "Gross Domestic Product",                   "category": "gdp"},
    "h5":  {"name": "Business Indicators",                      "category": "business"},
    # Payments
    "c1":  {"name": "Credit and Charge Cards",                  "category": "payments"},
}

# Pre-built groups for convenience commands
GROUPS = {
    "cash_rate":       ["f1"],
    "bond_yields":     ["f2"],
    "exchange_rates":  ["f11"],
    "aud_usd":         ["f11"],
    "lending_rates":   ["f5", "f6"],
    "deposit_rates":   ["f4"],
    "monetary":        ["d1", "d3"],
    "credit":          ["d2"],
    "inflation":       ["g1"],
    "housing":         ["f6", "f20"],
    "labour":          ["h1"],
    "gdp":             ["h3"],
    "balance_sheet":   ["b1"],
    "overview":        ["f1", "f2", "f11", "d3", "g1"],
}


# ---------------------------------------------------------------------------
# Error container
# ---------------------------------------------------------------------------

class RBAError:
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
            "type":        "RBAError",
        }


# ---------------------------------------------------------------------------
# Main wrapper
# ---------------------------------------------------------------------------

class RBAWrapper:
    """
    Wrapper for Reserve Bank of Australia statistical CSV tables.

    Data is published as CSV files with a 7-row metadata header followed by data rows.
    No authentication required.
    """

    def __init__(self):
        pass  # RBA CDN blocks persistent sessions; use plain requests.get()

    # ------------------------------------------------------------------
    # Internal helpers
    # ------------------------------------------------------------------

    def _fetch(self, table_code: str) -> str:
        code = table_code.lower().replace("-", "")
        url  = f"{BASE_URL}/{code}-data.csv"
        resp = requests.get(url, timeout=DEFAULT_TIMEOUT)
        resp.raise_for_status()
        return resp.text

    def _parse(self, text: str, start_date: Optional[str] = None,
               end_date: Optional[str] = None) -> Dict[str, Any]:
        """
        Parse RBA CSV. Returns dict with series metadata and data rows.

        RBA CSV layout (as of 2026):
          Row 0: Table title (e.g. "F1 INTEREST RATES AND YIELDS")
          Row 1: "Title", col1_name, col2_name, ...
          Row 2: "Description", col1_desc, ...
          Row 3: "Frequency", Daily, Monthly, ...
          Row 4: "Type", Original, ...
          Row 5: "Units", Per cent, ...
          Row 6: blank
          Row 7: blank
          Row 8: "Source", RBA, ...
          Row 9: "Publication date", ...
          Row 10: "Series ID", FIRMMCRTD, ...
          Row 11+: data rows  "DD-Mon-YYYY", val, val, ...
        """
        reader = list(csv.reader(io.StringIO(text)))

        meta_rows: Dict[str, List[str]] = {}
        series_ids: List[str] = []
        data_start = 0

        for i, row in enumerate(reader):
            if not row:
                continue
            label = row[0].strip()
            if label == "Series ID":
                series_ids = [c.strip() for c in row]
                data_start = i + 1
                break
            if label in ("Title", "Description", "Frequency", "Type",
                         "Units", "Source", "Publication date"):
                meta_rows[label] = [c.strip() for c in row]

        if not series_ids:
            return {"error": "Could not find 'Series ID' row", "raw_preview": text[:300]}

        # Build series metadata
        titles       = meta_rows.get("Title",            [""] * len(series_ids))
        descriptions = meta_rows.get("Description",      [""] * len(series_ids))
        frequencies  = meta_rows.get("Frequency",        [""] * len(series_ids))
        data_types   = meta_rows.get("Type",             [""] * len(series_ids))
        units        = meta_rows.get("Units",            [""] * len(series_ids))
        sources      = meta_rows.get("Source",           [""] * len(series_ids))

        meta: Dict[str, Any] = {}
        for i in range(1, len(series_ids)):
            sid = series_ids[i]
            if sid:
                meta[sid] = {
                    "title":       titles[i]       if i < len(titles)       else "",
                    "description": descriptions[i] if i < len(descriptions) else "",
                    "frequency":   frequencies[i]  if i < len(frequencies)  else "",
                    "type":        data_types[i]   if i < len(data_types)   else "",
                    "unit":        units[i]         if i < len(units)        else "",
                    "source":      sources[i]       if i < len(sources)      else "",
                }

        rows = []
        for row in reader[data_start:]:
            if not row or not row[0].strip():
                continue
            date_str = row[0].strip()
            # Skip any remaining metadata rows
            if date_str in ("Series ID", "Title", "Description", "Frequency",
                            "Type", "Units", "Source", "Publication date"):
                continue
            entry = {"date": date_str}
            for i in range(1, len(series_ids)):
                if i >= len(row):
                    break
                sid = series_ids[i]
                if not sid:
                    continue
                raw = row[i].strip()
                if raw in ("", "..", "N/a", "N/A", "na", "NA"):
                    entry[sid] = None
                else:
                    try:
                        entry[sid] = float(raw.replace(",", ""))
                    except ValueError:
                        entry[sid] = raw
            rows.append(entry)

        if start_date:
            rows = [r for r in rows if r["date"] >= start_date]
        if end_date:
            rows = [r for r in rows if r["date"] <= end_date]

        return {
            "series_metadata": meta,
            "data":            rows,
            "count":           len(rows),
        }

    def _fetch_table(self, table_code: str, start_date: Optional[str] = None,
                     end_date: Optional[str] = None) -> Dict[str, Any]:
        try:
            text   = self._fetch(table_code)
            parsed = self._parse(text, start_date, end_date)
            if "error" in parsed:
                return {"success": False, **parsed}
            info = TABLES.get(table_code.lower(), {})
            return {
                "success":         True,
                "table":           table_code.lower(),
                "table_name":      info.get("name", table_code),
                "category":        info.get("category", ""),
                "data":            parsed["data"],
                "count":           parsed["count"],
                "series_metadata": parsed["series_metadata"],
                "source":          "Reserve Bank of Australia",
                "url":             f"{BASE_URL}/{table_code.lower()}-data.csv",
                "timestamp":       int(datetime.now(timezone.utc).timestamp()),
            }
        except requests.exceptions.HTTPError as e:
            return RBAError(table_code, str(e), e.response.status_code if e.response else None).to_dict()
        except Exception as e:
            return RBAError(table_code, str(e)).to_dict()

    def _fetch_group(self, tables: List[str], start_date: Optional[str] = None,
                     end_date: Optional[str] = None) -> Dict[str, Any]:
        """Fetch multiple tables and merge into single response."""
        all_data    = {}
        all_meta    = {}
        total_count = 0
        errors      = []
        for tbl in tables:
            res = self._fetch_table(tbl, start_date, end_date)
            if res.get("success"):
                all_meta.update(res.get("series_metadata", {}))
                for row in res.get("data", []):
                    date = row["date"]
                    if date not in all_data:
                        all_data[date] = {"date": date}
                    all_data[date].update({k: v for k, v in row.items() if k != "date"})
                total_count = max(total_count, res.get("count", 0))
            else:
                errors.append({"table": tbl, "error": res.get("error", "unknown")})

        merged = sorted(all_data.values(), key=lambda r: r["date"])
        return {
            "success":         len(errors) < len(tables),
            "tables":          tables,
            "data":            merged,
            "count":           len(merged),
            "series_metadata": all_meta,
            "errors":          errors if errors else None,
            "source":          "Reserve Bank of Australia",
            "timestamp":       int(datetime.now(timezone.utc).timestamp()),
        }

    # ------------------------------------------------------------------
    # Public convenience methods
    # ------------------------------------------------------------------

    def get_cash_rate(self, start_date: Optional[str] = None,
                      end_date: Optional[str] = None) -> Dict[str, Any]:
        """Official cash rate target and interbank overnight rate (Table F1)."""
        return self._fetch_table("f1", start_date, end_date)

    def get_bond_yields(self, start_date: Optional[str] = None,
                        end_date: Optional[str] = None) -> Dict[str, Any]:
        """Australian Government bond yields (Table F2)."""
        return self._fetch_table("f2", start_date, end_date)

    def get_exchange_rates(self, start_date: Optional[str] = None,
                           end_date: Optional[str] = None) -> Dict[str, Any]:
        """AUD exchange rates vs 24 currencies + TWI (Table F11)."""
        return self._fetch_table("f11", start_date, end_date)

    def get_lending_rates(self, start_date: Optional[str] = None,
                          end_date: Optional[str] = None) -> Dict[str, Any]:
        """Indicator lending rates for housing and business (Tables F5 + F6)."""
        return self._fetch_group(["f5", "f6"], start_date, end_date)

    def get_deposit_rates(self, start_date: Optional[str] = None,
                          end_date: Optional[str] = None) -> Dict[str, Any]:
        """Retail deposit and investment rates (Table F4)."""
        return self._fetch_table("f4", start_date, end_date)

    def get_monetary_aggregates(self, start_date: Optional[str] = None,
                                end_date: Optional[str] = None) -> Dict[str, Any]:
        """M1, M3, broad money aggregates (Table D3)."""
        return self._fetch_table("d3", start_date, end_date)

    def get_credit_aggregates(self, start_date: Optional[str] = None,
                              end_date: Optional[str] = None) -> Dict[str, Any]:
        """Lending and credit aggregates by sector (Table D2)."""
        return self._fetch_table("d2", start_date, end_date)

    def get_inflation(self, start_date: Optional[str] = None,
                      end_date: Optional[str] = None) -> Dict[str, Any]:
        """Consumer price inflation (Table G1)."""
        return self._fetch_table("g1", start_date, end_date)

    def get_housing(self, start_date: Optional[str] = None,
                    end_date: Optional[str] = None) -> Dict[str, Any]:
        """Housing lending rates and house price growth (Tables F6 + F20)."""
        return self._fetch_group(["f6", "f20"], start_date, end_date)

    def get_labour(self, start_date: Optional[str] = None,
                   end_date: Optional[str] = None) -> Dict[str, Any]:
        """Labour market data (Table H1)."""
        return self._fetch_table("h1", start_date, end_date)

    def get_gdp(self, start_date: Optional[str] = None,
                end_date: Optional[str] = None) -> Dict[str, Any]:
        """Gross Domestic Product (Table H3)."""
        return self._fetch_table("h3", start_date, end_date)

    def get_balance_sheet(self, start_date: Optional[str] = None,
                          end_date: Optional[str] = None) -> Dict[str, Any]:
        """RBA Balance Sheet (Table B1)."""
        return self._fetch_table("b1", start_date, end_date)

    def get_overview(self, start_date: Optional[str] = None,
                     end_date: Optional[str] = None) -> Dict[str, Any]:
        """Key indicators: cash rate, bond yields, exchange rates, M3, CPI."""
        return self._fetch_group(GROUPS["overview"], start_date, end_date)

    def get_table(self, table_code: str, start_date: Optional[str] = None,
                  end_date: Optional[str] = None) -> Dict[str, Any]:
        """Fetch any RBA table by code (e.g. 'f1', 'd3', 'g1')."""
        return self._fetch_table(table_code, start_date, end_date)

    def get_group(self, group_name: str, start_date: Optional[str] = None,
                  end_date: Optional[str] = None) -> Dict[str, Any]:
        """Fetch a named group of tables."""
        tables = GROUPS.get(group_name.lower())
        if not tables:
            return {"success": False, "error": f"Unknown group '{group_name}'",
                    "available_groups": list(GROUPS.keys())}
        return self._fetch_group(tables, start_date, end_date)

    def available_tables(self) -> Dict[str, Any]:
        """Return catalogue of all available RBA tables."""
        by_category: Dict[str, List] = {}
        for code, info in TABLES.items():
            cat = info["category"]
            by_category.setdefault(cat, []).append({"code": code, "name": info["name"]})
        return {
            "success":    True,
            "data":       by_category,
            "groups":     {k: v for k, v in GROUPS.items()},
            "base_url":   BASE_URL,
            "source":     "Reserve Bank of Australia",
            "timestamp":  int(datetime.now(timezone.utc).timestamp()),
        }


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------

COMMANDS = {
    "cash_rate":         "[start_date] [end_date]  — Official cash rate (Table F1)",
    "bond_yields":       "[start_date] [end_date]  — Govt bond yields (Table F2)",
    "exchange_rates":    "[start_date] [end_date]  — AUD exchange rates (Table F11)",
    "lending_rates":     "[start_date] [end_date]  — Lending rates (F5+F6)",
    "deposit_rates":     "[start_date] [end_date]  — Deposit rates (Table F4)",
    "monetary":          "[start_date] [end_date]  — Monetary aggregates (Table D3)",
    "credit":            "[start_date] [end_date]  — Credit aggregates (Table D2)",
    "inflation":         "[start_date] [end_date]  — CPI inflation (Table G1)",
    "housing":           "[start_date] [end_date]  — Housing lending + prices (F6+F20)",
    "labour":            "[start_date] [end_date]  — Labour market (Table H1)",
    "gdp":               "[start_date] [end_date]  — GDP (Table H3)",
    "balance_sheet":     "[start_date] [end_date]  — RBA balance sheet (Table B1)",
    "overview":          "[start_date] [end_date]  — Key indicators snapshot",
    "table":             "<code> [start_date] [end_date]  — Any table by code",
    "group":             "<group_name> [start_date] [end_date]  — Named group of tables",
    "available":         "List all available tables and groups",
}


def _a(n: int, d: Any = None) -> Any:
    return sys.argv[n] if len(sys.argv) > n and sys.argv[n] else d


def main() -> None:
    if len(sys.argv) < 2:
        print(json.dumps({
            "error":    "No command provided.",
            "usage":    "python rba_data.py <command> [args...]",
            "commands": COMMANDS,
        }, indent=2))
        sys.exit(1)

    cmd     = sys.argv[1].lower()
    wrapper = RBAWrapper()

    try:
        if cmd == "cash_rate":
            result = wrapper.get_cash_rate(_a(2), _a(3))
        elif cmd == "bond_yields":
            result = wrapper.get_bond_yields(_a(2), _a(3))
        elif cmd == "exchange_rates":
            result = wrapper.get_exchange_rates(_a(2), _a(3))
        elif cmd == "lending_rates":
            result = wrapper.get_lending_rates(_a(2), _a(3))
        elif cmd == "deposit_rates":
            result = wrapper.get_deposit_rates(_a(2), _a(3))
        elif cmd in ("monetary", "monetary_aggregates"):
            result = wrapper.get_monetary_aggregates(_a(2), _a(3))
        elif cmd in ("credit", "credit_aggregates"):
            result = wrapper.get_credit_aggregates(_a(2), _a(3))
        elif cmd == "inflation":
            result = wrapper.get_inflation(_a(2), _a(3))
        elif cmd == "housing":
            result = wrapper.get_housing(_a(2), _a(3))
        elif cmd == "labour":
            result = wrapper.get_labour(_a(2), _a(3))
        elif cmd == "gdp":
            result = wrapper.get_gdp(_a(2), _a(3))
        elif cmd == "balance_sheet":
            result = wrapper.get_balance_sheet(_a(2), _a(3))
        elif cmd == "overview":
            result = wrapper.get_overview(_a(2), _a(3))
        elif cmd == "table":
            if len(sys.argv) < 3:
                result = {"error": "table requires <code>", "example": "python rba_data.py table f1"}
            else:
                result = wrapper.get_table(sys.argv[2], _a(3), _a(4))
        elif cmd == "group":
            if len(sys.argv) < 3:
                result = {"error": "group requires <group_name>", "available": list(GROUPS.keys())}
            else:
                result = wrapper.get_group(sys.argv[2], _a(3), _a(4))
        elif cmd in ("available", "tables"):
            result = wrapper.available_tables()
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
