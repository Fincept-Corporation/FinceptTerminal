"""
Macro Trends Data Fetcher
Long-term US market valuation and macro history (no key): Shiller CAPE, S&P 500 P/E and price levels (multpl.com tables) and a Buffett indicator, Fed funds rate, GDP and CPI from FRED's public CSV download.
"""
import sys
import re
import json
import html
import requests
from typing import Any, List, Dict

FRED_CSV = "https://fred.stlouisfed.org/graph/fredgraph.csv"
MULTPL = "https://www.multpl.com"
HEADERS = {"User-Agent": "Mozilla/5.0 (compatible; FinceptTerminal/4.5; +https://fincept.in)"}

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update(HEADERS)

_MONTHS = {m: i for i, m in enumerate(["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct",
                                        "Nov", "Dec"], 1)}


def _in_range(date: str, start_year: int, end_year: int) -> bool:
    y = int(date[:4])
    return (not start_year or y >= start_year) and (not end_year or y <= end_year)


def _fred(series_id: str, start_year: int = None, end_year: int = None) -> Any:
    """Keyless FRED series via fredgraph.csv (fredgraph.json does not exist)."""
    try:
        r = session.get(FRED_CSV, params={"id": series_id}, timeout=30)
        r.raise_for_status()
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    lines = r.text.strip().splitlines()
    if not lines or "," not in lines[0]:
        return {"error": f"Unexpected FRED response for {series_id}"}
    rows = []
    for line in lines[1:]:
        d, _, v = line.partition(",")
        if not d or v in ("", "."):
            continue
        if _in_range(d, start_year, end_year):
            try:
                rows.append({"date": d, "value": float(v)})
            except ValueError:
                pass
    return rows


def _multpl(slug: str, start_year: int = None, end_year: int = None) -> Any:
    """Annual table from multpl.com (/<slug>/table/by-year), newest first on the site."""
    try:
        r = session.get(f"{MULTPL}/{slug}/table/by-year", timeout=30)
        r.raise_for_status()
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    rows = []
    for d, v in re.findall(r"<td>\s*([A-Z][a-z]{2} \d{1,2}, \d{4})\s*</td>\s*<td>(.*?)</td>", r.text, re.S):
        txt = html.unescape(re.sub(r"<[^>]+>", "", v)).strip().replace(",", "").replace("%", "")
        m = re.search(r"-?\d+(\.\d+)?", txt)
        if not m:
            continue
        mon, day, year = d.replace(",", "").split()
        date = f"{year}-{_MONTHS.get(mon, 1):02d}-{int(day):02d}"
        if _in_range(date, start_year, end_year):
            rows.append({"date": date, "value": float(m.group(0))})
    if not rows:
        return {"error": f"No rows parsed from multpl.com/{slug} (page layout may have changed)"}
    rows.sort(key=lambda x: x["date"])
    return rows


def _wrap(name: str, unit: str, source: str, rows: Any) -> Any:
    if isinstance(rows, dict):
        return rows
    return {"series": name, "unit": unit, "source": source, "count": len(rows), "data": rows}


def get_shiller_pe(start_year: int = None, end_year: int = None) -> Any:
    return _wrap("Shiller PE (CAPE)", "ratio", "multpl.com", _multpl("shiller-pe", start_year, end_year))


def get_buffett_indicator(start_year: int = None, end_year: int = None) -> Any:
    """Market value of US nonfinancial corporate equities (Fed Z.1, NCBEILQ027S) / nominal GDP, %.
    (FRED's Wilshire 5000 series used previously was discontinued.)"""
    eq = _fred("NCBEILQ027S", start_year, end_year)
    gdp = _fred("GDP", start_year, end_year)
    if isinstance(eq, dict):
        return eq
    if isinstance(gdp, dict):
        return gdp
    g = {r["date"]: r["value"] for r in gdp}
    rows = [{"date": r["date"], "value": round(r["value"] / 1000.0 / g[r["date"]] * 100.0, 2),
             "equities_bn_usd": round(r["value"] / 1000.0, 1), "gdp_bn_usd": g[r["date"]]}
            for r in eq if g.get(r["date"])]
    return _wrap("Buffett indicator (corporate equities / GDP)", "% of GDP", "FRED (Fed Z.1, BEA)", rows)


def get_sp500_historical(start_year: int = None, end_year: int = None) -> Any:
    prices = _multpl("s-p-500-historical-prices", start_year, end_year)
    pe = _multpl("s-p-500-pe-ratio", start_year, end_year)
    if isinstance(prices, dict):
        return prices
    pe_map = {r["date"][:4]: r["value"] for r in pe} if isinstance(pe, list) else {}
    rows: List[Dict] = []
    prev = None
    for r in prices:
        row = {"date": r["date"], "price": r["value"], "pe_ratio": pe_map.get(r["date"][:4])}
        if prev:
            row["change_pct"] = round((r["value"] / prev - 1) * 100, 2)
        prev = r["value"]
        rows.append(row)
    return _wrap("S&P 500 annual level and P/E", "index points", "multpl.com", rows)


def get_interest_rates_history(start_year: int = None, end_year: int = None) -> Any:
    return _wrap("Effective federal funds rate (FEDFUNDS)", "%", "FRED", _fred("FEDFUNDS", start_year, end_year))


def get_gdp_historical(start_year: int = None, end_year: int = None) -> Any:
    return _wrap("US nominal GDP (GDP)", "USD bn, SAAR", "FRED", _fred("GDP", start_year, end_year))


def get_inflation_historical(start_year: int = None, end_year: int = None) -> Any:
    rows = _fred("CPIAUCSL")
    if isinstance(rows, list):
        for i, r in enumerate(rows):
            if i >= 12:
                r["yoy_pct"] = round((r["value"] / rows[i - 12]["value"] - 1) * 100, 2)
        rows = [r for r in rows if _in_range(r["date"], start_year, end_year)]
    return _wrap("US CPI, all urban consumers (CPIAUCSL)", "index 1982-84=100", "FRED", rows)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    start_year = int(args[1]) if len(args) > 1 else None
    end_year = int(args[2]) if len(args) > 2 else None
    if command == "shiller_pe":
        result = get_shiller_pe(start_year, end_year)
    elif command == "buffett":
        result = get_buffett_indicator(start_year, end_year)
    elif command == "sp500":
        result = get_sp500_historical(start_year, end_year)
    elif command == "rates":
        result = get_interest_rates_history(start_year, end_year)
    elif command == "gdp":
        result = get_gdp_historical(start_year, end_year)
    elif command == "inflation":
        result = get_inflation_historical(start_year, end_year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
