"""
World Bank Commodity Price Data Fetcher
World Bank Pink Sheet: 70+ commodity prices and price indices, monthly from 1960 (no key required).
Read from the official CMO-Historical-Data-Monthly.xlsx workbook (the Pink Sheet is not served by the World Bank indicators API).
"""
import sys
import os
import re
import json
import time
import tempfile
import requests
from datetime import datetime
from typing import Dict, Any, List

COMMODITY_PAGE = "https://www.worldbank.org/en/research/commodity-markets"
# The workbook URL changes every year; it is discovered from the commodity-markets page and this
# last-known URL is only the fallback.
FALLBACK_XLSX = ("https://thedocs.worldbank.org/en/doc/74e8be41ceb20fa0da750cda2f6b9e4e-0050012026/"
                 "related/CMO-Historical-Data-Monthly.xlsx")
CACHE_PATH = os.path.join(tempfile.gettempdir(), "fincept_wb_pink_sheet_monthly.xlsx")
CACHE_TTL_SEC = 12 * 3600
HEADERS = {"User-Agent": "Mozilla/5.0 (FinceptTerminal/4.5; +https://fincept.in)"}

# Friendly keys -> Pink Sheet column titles (matched case-insensitively, ignoring '**').
COMMODITIES = {
    # Energy
    "crude_oil_avg": "Crude oil, average", "crude_oil_brent": "Crude oil, Brent",
    "crude_oil_wti": "Crude oil, WTI", "crude_oil_dubai": "Crude oil, Dubai",
    "natural_gas_us": "Natural gas, US", "natural_gas_eu": "Natural gas, Europe",
    "lng_japan": "Liquefied natural gas, Japan", "coal": "Coal, Australian",
    # Agriculture
    "wheat": "Wheat, US HRW", "wheat_srw": "Wheat, US SRW", "corn": "Maize", "maize": "Maize",
    "rice": "Rice, Thai 5%", "barley": "Barley", "sorghum": "Sorghum", "soybean": "Soybeans",
    "soybeanmeal": "Soybean meal", "soybean_oil": "Soybean oil", "palm_oil": "Palm oil",
    "sugar": "Sugar, world", "coffee_arabica": "Coffee, Arabica", "coffee_robusta": "Coffee, Robusta",
    "cocoa": "Cocoa", "tea": "Tea, avg 3 auctions", "cotton": "Cotton, A Index", "rubber": "Rubber, RSS3",
    "tobacco": "Tobacco, US import u.v.", "bananas": "Banana, US", "oranges": "Orange", "beef": "Beef",
    "chicken": "Chicken",
    # Metals
    "aluminum": "Aluminum", "copper": "Copper", "iron_ore": "Iron ore, cfr spot", "lead": "Lead",
    "nickel": "Nickel", "tin": "Tin", "zinc": "Zinc", "gold": "Gold", "silver": "Silver", "platinum": "Platinum",
    # Fertilizers
    "urea": "Urea", "dap": "DAP", "tsp": "TSP", "potassium_chloride": "Potassium chloride",
    "phosphate_rock": "Phosphate rock",
}
OIL = ["crude_oil_brent", "crude_oil_wti", "crude_oil_dubai"]
METALS = ["aluminum", "copper", "iron_ore", "lead", "nickel", "tin", "zinc", "gold", "silver", "platinum"]
FERTILIZERS = ["urea", "dap", "tsp", "potassium_chloride", "phosphate_rock"]
ENERGY = OIL + ["crude_oil_avg", "natural_gas_us", "natural_gas_eu", "lng_japan", "coal"]


def _xlsx_url() -> str:
    try:
        html = requests.get(COMMODITY_PAGE, headers=HEADERS, timeout=30).text
        m = re.search(r'https://thedocs\.worldbank\.org/[^"\']*CMO-Historical-Data-Monthly\.xlsx', html)
        if m:
            return m.group(0)
    except requests.exceptions.RequestException:
        pass
    return FALLBACK_XLSX


def _workbook_path() -> str:
    if os.path.exists(CACHE_PATH) and time.time() - os.path.getmtime(CACHE_PATH) < CACHE_TTL_SEC:
        return CACHE_PATH
    url = _xlsx_url()
    r = requests.get(url, headers=HEADERS, timeout=90)
    r.raise_for_status()
    if not r.content.startswith(b"PK"):
        raise ValueError(f"Pink Sheet download is not an .xlsx workbook ({url})")
    tmp = CACHE_PATH + ".part"
    with open(tmp, "wb") as f:
        f.write(r.content)
    os.replace(tmp, CACHE_PATH)
    return CACHE_PATH


def _norm(s: Any) -> str:
    return re.sub(r"\s+", " ", str(s or "").replace("*", "")).strip().lower()


def _period(v: Any) -> str:
    m = re.fullmatch(r"(\d{4})M(\d{2})", str(v or "").strip())
    return f"{m.group(1)}-{m.group(2)}" if m else ""


def _load(sheet: str) -> Dict[str, Any]:
    """Returns {"updated": str, "columns": [(title, unit)], "rows": [(period, [values...])]}"""
    import openpyxl
    wb = openpyxl.load_workbook(_workbook_path(), read_only=True, data_only=True)
    try:
        rows = list(wb[sheet].iter_rows(values_only=True))
    finally:
        wb.close()
    first = next(i for i, r in enumerate(rows) if r and _period(r[0]))
    header_rows = rows[4:first]
    ncol = max(len(r) for r in rows[:first + 1])
    titles, units = [], []
    for c in range(1, ncol):
        parts = [str(r[c]).strip() for r in header_rows if c < len(r) and r[c] not in (None, "") and str(r[c]).strip()]
        unit = next((p for p in parts if p.startswith("(")), "")
        names = [p for p in parts if not p.startswith("(")]
        titles.append(names[-1] if names else "")
        units.append(unit.strip("()"))
    updated = next((str(r[0]) for r in rows[:4] if r and r[0] and "Updated" in str(r[0])), "")
    data = [(_period(r[0]), list(r[1:ncol])) for r in rows[first:] if r and _period(r[0])]
    return {"updated": updated, "titles": titles, "units": units, "rows": data}


def _num(v: Any):
    return v if isinstance(v, (int, float)) else None


def _series(sheet_data: Dict, title: str, start_year: int, end_year: int) -> Dict[str, Any]:
    want = _norm(title)
    idx = next((i for i, t in enumerate(sheet_data["titles"]) if _norm(t) == want), None)
    if idx is None:
        idx = next((i for i, t in enumerate(sheet_data["titles"]) if want in _norm(t)), None)
    if idx is None:
        return {"error": f"Column '{title}' not found in the Pink Sheet"}
    obs = [{"date": p, "value": _num(vals[idx]) if idx < len(vals) else None}
           for p, vals in sheet_data["rows"] if start_year <= int(p[:4]) <= end_year]
    return {"title": sheet_data["titles"][idx].replace("**", "").strip(), "unit": sheet_data["units"][idx],
            "data": [o for o in obs if o["value"] is not None]}


def _many(keys: List[str], start_year: int, end_year: int, meta: Dict) -> Dict[str, Any]:
    try:
        sheet = _load("Monthly Prices")
    except Exception as e:
        return {"error": f"Could not read the World Bank Pink Sheet: {e}"}
    out = {k: _series(sheet, COMMODITIES[k], start_year, end_year) for k in keys}
    return {**meta, "start_year": start_year, "end_year": end_year, "source": "World Bank Pink Sheet",
            "updated": sheet["updated"], "frequency": "monthly", "data": out}


def get_oil_prices(start_year: int = 2000, end_year: int = None) -> Any:
    return _many(OIL, start_year, end_year or datetime.now().year, {"category": "oil", "unit": "USD/bbl"})


def get_agricultural_prices(commodity: str = "wheat", start_year: int = 2000, end_year: int = None) -> Any:
    key = commodity.lower()
    ag = [k for k in COMMODITIES if k not in ENERGY + METALS + FERTILIZERS]
    if key not in ag:
        return {"error": f"Unknown commodity '{commodity}'. Agricultural options: {ag}"}
    return _many([key], start_year, end_year or datetime.now().year, {"commodity": key})


def get_metals_prices(metal: str = "gold", start_year: int = 2000, end_year: int = None) -> Any:
    key = metal.lower()
    if key not in METALS:
        return {"error": f"Unknown metal '{metal}'. Options: {METALS}"}
    return _many([key], start_year, end_year or datetime.now().year, {"metal": key})


def get_fertilizer_prices(start_year: int = 2000, end_year: int = None) -> Any:
    return _many(FERTILIZERS, start_year, end_year or datetime.now().year, {"category": "fertilizers"})


def get_all_commodities(year: int = None) -> Any:
    """Annual average of every Pink Sheet price for one year (default: latest complete year)."""
    try:
        sheet = _load("Monthly Prices")
    except Exception as e:
        return {"error": f"Could not read the World Bank Pink Sheet: {e}"}
    year = year or datetime.now().year - 1
    rows = [vals for p, vals in sheet["rows"] if p.startswith(f"{year}-")]
    if not rows:
        return {"error": f"No Pink Sheet data for {year}"}
    out = {}
    for i, t in enumerate(sheet["titles"]):
        if not t:
            continue
        vals = [_num(r[i]) for r in rows if i < len(r) and _num(r[i]) is not None]
        if vals:
            out[t.replace("**", "").strip()] = {"avg": round(sum(vals) / len(vals), 4), "unit": sheet["units"][i],
                                                "months": len(vals)}
    return {"year": year, "source": "World Bank Pink Sheet", "updated": sheet["updated"], "commodities": out}


def get_commodity_index(start_year: int = 2000, end_year: int = None) -> Any:
    """Pink Sheet price indices (2010=100): total, energy, non-energy, agriculture, metals, ..."""
    try:
        sheet = _load("Monthly Indices")
    except Exception as e:
        return {"error": f"Could not read the World Bank Pink Sheet: {e}"}
    end_year = end_year or datetime.now().year
    out = {}
    for t in sheet["titles"]:
        if t:
            out[t.replace("**", "").strip()] = _series(sheet, t, start_year, end_year)["data"]
    return {"type": "commodity_indices", "base": "2010=100", "start_year": start_year, "end_year": end_year,
            "source": "World Bank Pink Sheet", "updated": sheet["updated"], "data": out}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "oil":
        start_year = int(args[1]) if len(args) > 1 else 2000
        end_year = int(args[2]) if len(args) > 2 else None
        result = get_oil_prices(start_year, end_year)
    elif command == "agricultural":
        commodity = args[1] if len(args) > 1 else "wheat"
        start_year = int(args[2]) if len(args) > 2 else 2000
        end_year = int(args[3]) if len(args) > 3 else None
        result = get_agricultural_prices(commodity, start_year, end_year)
    elif command == "metals":
        metal = args[1] if len(args) > 1 else "gold"
        start_year = int(args[2]) if len(args) > 2 else 2000
        end_year = int(args[3]) if len(args) > 3 else None
        result = get_metals_prices(metal, start_year, end_year)
    elif command == "fertilizers":
        start_year = int(args[1]) if len(args) > 1 else 2000
        end_year = int(args[2]) if len(args) > 2 else None
        result = get_fertilizer_prices(start_year, end_year)
    elif command == "all":
        year = int(args[1]) if len(args) > 1 else None
        result = get_all_commodities(year)
    elif command == "index":
        start_year = int(args[1]) if len(args) > 1 else 2000
        end_year = int(args[2]) if len(args) > 2 else None
        result = get_commodity_index(start_year, end_year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
