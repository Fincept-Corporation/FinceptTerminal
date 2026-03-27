"""
World Bank Commodity Price Data Fetcher
World Bank Pink Sheet: 70+ commodity prices monthly from 1960
via World Bank API (no key required).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://api.worldbank.org/v2/en/indicator"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

# World Bank commodity indicators (Pink Sheet)
COMMODITY_INDICATORS = {
    # Energy
    "crude_oil_avg": "PCOALBBGUSDM",
    "crude_oil_brent": "POILBREUSDM",
    "crude_oil_wti": "POILWTIUSDM",
    "crude_oil_dubai": "POILDUBAUSDM",
    "natural_gas_us": "PNGASUSDM",
    "natural_gas_eu": "PNGASEUUSDM",
    "coal": "PCOALBTUUSDM",
    # Agricultural
    "wheat": "PWHEAMTUSDM",
    "corn": "PMAIZMTUSDM",
    "rice": "PRICENPQUSDM",
    "soybean": "PSOYBUSDM",
    "soybeanmeal": "PSOYMEUSDM",
    "soybeantoil": "PSOYOUSDM",
    "sugar": "PSUGAISAUSDM",
    "coffee_arabica": "PCOFFAUSDM",
    "coffee_robusta": "PCOFFRBUSDM",
    "cocoa": "PCOCOUSDM",
    "tea": "PTEAUSDM",
    "palm_oil": "PPOILUSDM",
    "cotton": "PCOTTINDUSDM",
    "tobacco": "PTOBAUSDM",
    "rubber": "PRUBBINDNAUSDM",
    "bananas": "PBANSOPUSDM",
    "oranges": "PORANGCWUSDM",
    # Metals
    "aluminum": "PALUMUSDM",
    "copper": "PCOPPUSDM",
    "iron_ore": "PIORECRUSDM",
    "lead": "PLEADUSDM",
    "nickel": "PNICKUSDM",
    "tin": "PTINUSDM",
    "zinc": "PZINCUSDM",
    "gold": "PGOLDUSDM",
    "silver": "PSILVERUSDM",
    "platinum": "PPLATINUMUSDM",
    # Fertilizers
    "urea": "PUREAEURUSDM",
    "dap": "PDAPUSDM",
    "potassium_chloride": "PPOTUSDM",
    "phosphate_rock": "PPHOSBUSDM",
}

INDEX_INDICATORS = {
    "energy_index": "PENERGYINDEXM",
    "non_energy_index": "PNRGINDEXM",
    "agriculture_index": "PAGRIINDEXM",
    "metals_index": "PMETALSINDEXM",
    "fertilizer_index": "PFERTILIZERINDEXM"
}


def _make_request(indicator: str, start_year: int = 2000, end_year: int = 2024) -> Any:
    url = f"{BASE_URL}/{indicator}"
    params = {
        "format": "json",
        "date": f"{start_year}:{end_year}",
        "per_page": 1000
    }
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        data = response.json()
        if isinstance(data, list) and len(data) > 1:
            records = data[1] or []
            return [{"date": r.get("date"), "value": r.get("value"),
                     "country": r.get("country", {}).get("value")} for r in records]
        return data
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_oil_prices(start_year: int = 2000, end_year: int = 2024) -> Any:
    results = {}
    for name in ["crude_oil_brent", "crude_oil_wti", "crude_oil_dubai"]:
        indicator = COMMODITY_INDICATORS[name]
        results[name] = _make_request(indicator, start_year, end_year)
    return {"category": "oil", "start_year": start_year, "end_year": end_year,
            "unit": "USD/bbl", "data": results}


def get_agricultural_prices(commodity: str = "wheat", start_year: int = 2000,
                               end_year: int = 2024) -> Any:
    indicator = COMMODITY_INDICATORS.get(commodity.lower())
    if not indicator:
        ag_list = [k for k in COMMODITY_INDICATORS.keys()
                   if k not in ["crude_oil_avg", "crude_oil_brent", "crude_oil_wti",
                                  "crude_oil_dubai", "natural_gas_us", "natural_gas_eu", "coal",
                                  "aluminum", "copper", "iron_ore", "lead", "nickel", "tin",
                                  "zinc", "gold", "silver", "platinum",
                                  "urea", "dap", "potassium_chloride", "phosphate_rock"]]
        return {"error": f"Unknown commodity '{commodity}'. Agricultural options: {ag_list}"}
    data = _make_request(indicator, start_year, end_year)
    return {"commodity": commodity, "indicator": indicator, "start_year": start_year,
            "end_year": end_year, "data": data}


def get_metals_prices(metal: str = "gold", start_year: int = 2000, end_year: int = 2024) -> Any:
    metals_map = {k: COMMODITY_INDICATORS[k] for k in
                  ["aluminum", "copper", "iron_ore", "lead", "nickel", "tin", "zinc", "gold", "silver", "platinum"]
                  if k in COMMODITY_INDICATORS}
    indicator = metals_map.get(metal.lower())
    if not indicator:
        return {"error": f"Unknown metal '{metal}'. Options: {list(metals_map.keys())}"}
    data = _make_request(indicator, start_year, end_year)
    return {"metal": metal, "indicator": indicator, "start_year": start_year,
            "end_year": end_year, "data": data}


def get_fertilizer_prices(start_year: int = 2000, end_year: int = 2024) -> Any:
    results = {}
    fert_keys = ["urea", "dap", "potassium_chloride", "phosphate_rock"]
    for name in fert_keys:
        if name in COMMODITY_INDICATORS:
            results[name] = _make_request(COMMODITY_INDICATORS[name], start_year, end_year)
    return {"category": "fertilizers", "start_year": start_year, "end_year": end_year,
            "data": results}


def get_all_commodities(year: int = 2023) -> Any:
    results = {}
    for name, indicator in list(COMMODITY_INDICATORS.items())[:20]:  # limit for speed
        data = _make_request(indicator, year, year)
        if isinstance(data, list) and data:
            results[name] = data[0].get("value")
        else:
            results[name] = None
    return {"year": year, "commodities": results,
            "note": "Showing first 20 commodities. Use specific commands for full history."}


def get_commodity_index(start_year: int = 2000, end_year: int = 2024) -> Any:
    results = {}
    for name, indicator in INDEX_INDICATORS.items():
        results[name] = _make_request(indicator, start_year, end_year)
    return {"type": "commodity_indices", "start_year": start_year,
            "end_year": end_year, "data": results}


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
        end_year = int(args[2]) if len(args) > 2 else 2024
        result = get_oil_prices(start_year, end_year)
    elif command == "agricultural":
        commodity = args[1] if len(args) > 1 else "wheat"
        start_year = int(args[2]) if len(args) > 2 else 2000
        end_year = int(args[3]) if len(args) > 3 else 2024
        result = get_agricultural_prices(commodity, start_year, end_year)
    elif command == "metals":
        metal = args[1] if len(args) > 1 else "gold"
        start_year = int(args[2]) if len(args) > 2 else 2000
        end_year = int(args[3]) if len(args) > 3 else 2024
        result = get_metals_prices(metal, start_year, end_year)
    elif command == "fertilizers":
        start_year = int(args[1]) if len(args) > 1 else 2000
        end_year = int(args[2]) if len(args) > 2 else 2024
        result = get_fertilizer_prices(start_year, end_year)
    elif command == "all":
        year = int(args[1]) if len(args) > 1 else 2023
        result = get_all_commodities(year)
    elif command == "index":
        start_year = int(args[1]) if len(args) > 1 else 2000
        end_year = int(args[2]) if len(args) > 2 else 2024
        result = get_commodity_index(start_year, end_year)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
