"""
Eurostat Extra Data Fetcher
Eurostat additional datasets: energy balance, industrial production, retail trade,
construction output, trade in goods, and tourism statistics.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('EUROSTAT_API_KEY', '')
BASE_URL = "https://ec.europa.eu/eurostat/api/dissemination/statistics/1.0/data"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["format"] = "JSON"
    params["lang"] = "EN"
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        return _with_rows(response.json())
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def _with_rows(doc: Any) -> Any:
    """Add the observations of a JSON-stat answer as a table ("data": [{period, <dimension>...,
    value}], a column for every dimension with more than one category) next to the JSON-stat keys
    (id / size / dimension / value), which the Economics Eurostat panel decodes itself."""
    if not isinstance(doc, dict) or "error" in doc:
        return doc
    ids, sizes, dims, values = doc.get("id"), doc.get("size"), doc.get("dimension"), doc.get("value")
    if not (isinstance(ids, list) and isinstance(sizes, list) and isinstance(dims, dict) and len(ids) == len(sizes)):
        return doc
    if isinstance(values, list):
        values = {str(i): v for i, v in enumerate(values) if v is not None}
    if not isinstance(values, dict):
        return doc
    labels = []
    for dim_id, size in zip(ids, sizes):
        cat = (dims.get(dim_id) or {}).get("category") or {}
        index, label = cat.get("index") or {}, cat.get("label") or {}
        if isinstance(index, list):
            index = {code: i for i, code in enumerate(index)}
        by_pos = [None] * max(1, int(size or 1))
        for code, pos in index.items():
            if isinstance(pos, int) and 0 <= pos < len(by_pos):
                by_pos[pos] = code if dim_id == "time" else label.get(code, code)
        labels.append((dim_id, by_pos))
    rows = []
    for flat_key, value in values.items():
        try:
            flat = int(flat_key)
        except (TypeError, ValueError):
            continue
        row = {}
        for dim_id, by_pos in reversed(labels):
            pos = flat % len(by_pos)
            flat //= len(by_pos)
            if dim_id == "time":
                row["period"] = by_pos[pos]
            elif len(by_pos) > 1:
                row[dim_id] = by_pos[pos]
        row["value"] = value
        rows.append(row)
    rows.sort(key=lambda r: str(r.get("period") or ""))
    doc["data"] = rows
    return doc


# Short-term business statistics moved to base year 2021: the 2015=100 index ("I15") is still
# served but stops at 2023-12, while "I21" (2021=100) runs to the latest month.
STS_INDEX_UNIT = "I21"


def get_industrial_production(country: str = "DE", nace: str = "B-D", freq: str = "M") -> Any:
    params = {"geo": country.upper(), "nace_r2": nace, "s_adj": "SCA", "unit": STS_INDEX_UNIT}
    return _make_request("STS_INPR_M" if freq == "M" else "STS_INPR_Q", params)


def get_retail_trade(country: str = "DE", freq: str = "M") -> Any:
    params = {"geo": country.upper(), "s_adj": "SCA", "unit": STS_INDEX_UNIT, "nace_r2": "G47"}
    return _make_request("STS_TRTU_M" if freq == "M" else "STS_TRTU_Q", params)


def get_energy_balance(country: str = "DE", product: str = "TOTAL", freq: str = "A") -> Any:
    params = {"geo": country.upper(), "nrg_bal": "PPRD", "siec": product, "unit": "GWH"}
    return _make_request("nrg_bal_c", params)


# DS-059341 is a Comext dataset: it is served by the Comext API, not the dissemination API above
# (which answers 404 for it), and its flow codes are 1 = import, 2 = export.
COMEXT_URL = "https://ec.europa.eu/eurostat/api/comext/dissemination/statistics/1.0/data"


def get_trade_in_goods(country: str = "DE", partner: str = "US", product: str = "TOTAL", flow: str = "EXP") -> Any:
    flow_code = {"EXP": "2", "EXPORT": "2", "IMP": "1", "IMPORT": "1"}.get(flow.upper(), flow)
    params = {
        "reporter": country.upper(),
        "partner": partner.upper(),
        "product": product,
        "flow": flow_code,
        "freq": "M",
        "indicators": "VALUE_EUR",
    }
    result = _make_request(f"{COMEXT_URL}/DS-059341", params)
    if isinstance(result, dict) and "error" not in result and result.get("label"):
        direction = "exports to" if flow_code == "2" else "imports from"
        result["label"] = f"{result['label']} - {direction} {partner.upper()}, value in EUR"
    return result


def get_construction_output(country: str = "DE", freq: str = "M") -> Any:
    params = {"geo": country.upper(), "s_adj": "SCA", "unit": STS_INDEX_UNIT}
    return _make_request("STS_COPR_M" if freq == "M" else "STS_COPR_Q", params)


def get_tourism_stats(country: str = "DE", freq: str = "M") -> Any:
    params = {"geo": country.upper(), "c_resid": "TOTAL", "unit": "NR"}
    return _make_request("tour_occ_nim", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "industrial":
        country = args[1] if len(args) > 1 else "DE"
        nace = args[2] if len(args) > 2 else "B-D"
        freq = args[3] if len(args) > 3 else "M"
        result = get_industrial_production(country, nace, freq)
    elif command == "retail":
        country = args[1] if len(args) > 1 else "DE"
        freq = args[2] if len(args) > 2 else "M"
        result = get_retail_trade(country, freq)
    elif command == "energy":
        country = args[1] if len(args) > 1 else "DE"
        product = args[2] if len(args) > 2 else "TOTAL"
        freq = args[3] if len(args) > 3 else "A"
        result = get_energy_balance(country, product, freq)
    elif command == "trade":
        country = args[1] if len(args) > 1 else "DE"
        partner = args[2] if len(args) > 2 else "US"
        product = args[3] if len(args) > 3 else "TOTAL"
        flow = args[4] if len(args) > 4 else "EXP"
        result = get_trade_in_goods(country, partner, product, flow)
    elif command == "construction":
        country = args[1] if len(args) > 1 else "DE"
        freq = args[2] if len(args) > 2 else "M"
        result = get_construction_output(country, freq)
    elif command == "tourism":
        country = args[1] if len(args) > 1 else "DE"
        freq = args[2] if len(args) > 2 else "M"
        result = get_tourism_stats(country, freq)

    print(json.dumps(result))


if __name__ == "__main__":
    main()
