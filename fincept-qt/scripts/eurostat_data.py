"""
Eurostat SDMX 2.1 Data Fetcher
GDP, employment, trade, prices, population for all EU/EEA countries.
No API key required.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

# The JSON dissemination API returns JSON-stat 2.0 (the old SDMX 2.1 path with format=JSON also
# answered JSON-stat, so an SDMX-JSON parser found "No dataSets").
BASE_URL = "https://ec.europa.eu/eurostat/api/dissemination/statistics/1.0/data"
TOC_URL = "https://ec.europa.eu/eurostat/api/dissemination/catalogue/toc/txt"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"Accept": "application/json", "User-Agent": "FinceptTerminal/4.5 (+https://fincept.in)"})


def _make_request(endpoint: str, params: Dict = None, base: str = None) -> Any:
    """Make HTTP request with error handling."""
    base_url = base if base else BASE_URL
    url = f"{base_url}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, timeout=60)
        if response.status_code >= 400:
            try:
                err = response.json().get("error", {})
                if isinstance(err, list):
                    err = err[0] if err else {}
                msg = err.get("label") or response.text[:200]
            except ValueError:
                msg = response.text[:200]
            return {"error": f"HTTP {response.status_code}: {msg}"}
        return response.json()
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def _parse_jsonstat(data: Any, label: str = "series") -> Any:
    """Parse a Eurostat JSON-stat 2.0 dataset into one entry per non-time dimension combination."""
    if isinstance(data, dict) and "error" in data:
        return data
    try:
        ids = data.get("id", [])
        sizes = data.get("size", [])
        dims = data.get("dimension", {})
        values = data.get("value", {})
        status = data.get("status", {}) or {}
        if not ids or not sizes:
            return {"error": "Unexpected response (no JSON-stat dimensions)", "keys": list(data.keys())}
        if isinstance(values, list):
            values = {str(i): v for i, v in enumerate(values) if v is not None}
        cats = []
        for d in ids:
            cat = dims.get(d, {}).get("category", {})
            index = cat.get("index", {})
            if isinstance(index, list):
                order = index
            else:
                order = [k for k, _ in sorted(index.items(), key=lambda kv: kv[1])]
            cats.append((order, cat.get("label", {})))
        tpos = ids.index("time") if "time" in ids else len(ids) - 1
        strides = [1] * len(sizes)
        for i in range(len(sizes) - 2, -1, -1):
            strides[i] = strides[i + 1] * sizes[i + 1]
        series = {}
        for flat_s, v in values.items():
            flat = int(flat_s)
            coords = [(flat // strides[i]) % sizes[i] for i in range(len(sizes))]
            skey = tuple(c for i, c in enumerate(coords) if i != tpos)
            if skey not in series:
                dim_labels = {}
                for i, c in enumerate(coords):
                    if i == tpos:
                        continue
                    code = cats[i][0][c]
                    dim_labels[ids[i]] = {"id": code, "name": cats[i][1].get(code, code)}
                series[skey] = {"dimensions": dim_labels, "observations": []}
            period = cats[tpos][0][coords[tpos]]
            obs = {"period": period, "value": v}
            if flat_s in status:
                obs["flag"] = status[flat_s]
            series[skey]["observations"].append(obs)
        out = []
        for s in series.values():
            s["observations"].sort(key=lambda x: x["period"])
            s["count"] = len(s["observations"])
            out.append(s)
        return {label: out, "total_series": len(out), "title": data.get("label"), "updated": data.get("updated")}
    except Exception as e:
        return {"error": f"Parse error: {str(e)}"}


MAX_DATASET_SERIES = 200


def get_dataset(dataset_code: str, params: Dict = None) -> Any:
    """Fetch a Eurostat dataset by code (e.g. nama_10_gdp, une_rt_m, prc_hicp_minr).
    params: dimension filters such as {"geo": "DE", "unit": "CP_MEUR"}; without filters whole
    datasets can be tens of MB, so at most MAX_DATASET_SERIES series are returned (flagged)."""
    if params is None:
        params = {}
    params.setdefault("lang", "EN")
    params.setdefault("lastTimePeriod", 10)
    data = _make_request(dataset_code, params=params)
    result = _parse_jsonstat(data, "series")
    if isinstance(result.get("series"), list) and len(result["series"]) > MAX_DATASET_SERIES:
        result["truncated"] = True
        result["note"] = (f"{len(result['series'])} series matched; only the first {MAX_DATASET_SERIES} are "
                          "returned - add dimension filters (e.g. geo=DE,unit=CP_MEUR)")
        result["series"] = result["series"][:MAX_DATASET_SERIES]
    return result


def _parse_filters(text: str) -> Dict:
    """'geo=DE,unit=CP_MEUR,na_item=B1GQ' -> {"geo": "DE", ...}; repeated keys become lists."""
    out: Dict = {}
    for part in (text or "").split(","):
        if "=" not in part:
            continue
        k, v = part.split("=", 1)
        k, v = k.strip(), v.strip()
        if not k:
            continue
        if k in out:
            out[k] = out[k] if isinstance(out[k], list) else [out[k]]
            out[k].append(v)
        else:
            out[k] = v
    return out


def get_gdp(country: str = "DE", freq: str = "A") -> Any:
    """Get GDP data for EU/EEA countries.
    country: DE, FR, IT, ES, NL, BE, AT, PL, SE, DK, FI, etc. Or EU27_2020 for EU total.
    freq: A=annual, Q=quarterly.
    """
    params = {
        "lang": "EN",
        "geo": country.upper(),
        "unit": "CP_MEUR",
        "na_item": "B1GQ",
        "lastTimePeriod": 30 if freq == "A" else 40,
    }
    dataset = "nama_10_gdp" if freq == "A" else "namq_10_gdp"
    data = _make_request(dataset, params=params)
    result = _parse_jsonstat(data, "gdp_series")
    result.update({"country": country.upper(), "frequency": freq, "dataset": dataset})
    return result


def get_unemployment(country: str = "DE", freq: str = "M") -> Any:
    """Get unemployment rate for EU/EEA countries.
    country: EU27_2020, DE, FR, IT, ES, etc.
    freq: M=monthly, Q=quarterly, A=annual.
    """
    params = {
        "lang": "EN",
        "geo": country.upper(),
        "unit": "PC_ACT",
        "sex": "T",
        "age": "TOTAL",
        "s_adj": "SA" if freq != "A" else "NSA",
        "lastTimePeriod": 60,
    }
    dataset = "une_rt_m" if freq == "M" else ("une_rt_q" if freq == "Q" else "une_rt_a")
    data = _make_request(dataset, params=params)
    result = _parse_jsonstat(data, "unemployment_series")
    result.update({"country": country.upper(), "frequency": freq, "dataset": dataset})
    return result


def get_inflation(country: str = "DE", freq: str = "M") -> Any:
    """Get HICP inflation index data for EU/EEA countries.
    country: EU27_2020, DE, FR, IT, ES, NL, PL, etc.
    freq: M=monthly, A=annual.
    """
    # prc_hicp_midx/aind stopped at 2025; the ECOICOP ver.2 datasets carry the 2025=100 index
    # plus the annual rate of change.
    params = {
        "lang": "EN",
        "geo": country.upper(),
        "unit": ["I25", "RCH_A"] if freq == "M" else ["INX_A_AVG", "RCH_A_AVG"],
        "coicop18": "TOTAL",
        "lastTimePeriod": 120 if freq == "M" else 30,
    }
    dataset = "prc_hicp_minr" if freq == "M" else "prc_hicp_ainr"
    data = _make_request(dataset, params=params)
    result = _parse_jsonstat(data, "inflation_series")
    result.update({"country": country.upper(), "frequency": freq, "dataset": dataset})
    return result


def get_population(country: str = "DE") -> Any:
    """Get annual population data for EU/EEA countries."""
    params = {
        "lang": "EN",
        "geo": country.upper(),
        "sex": "T",
        "age": "TOTAL",
        "lastTimePeriod": 30,
    }
    data = _make_request("demo_pjan", params=params)
    result = _parse_jsonstat(data, "population_series")
    result.update({"country": country.upper(), "dataset": "demo_pjan"})
    return result


def search_datasets(keyword: str) -> Any:
    """Search Eurostat dataset codes/titles (from the public table of contents) by keyword."""
    try:
        response = session.get(TOC_URL, params={"lang": "en"}, headers={"Accept": "text/plain"}, timeout=60)
        response.raise_for_status()
    except requests.exceptions.RequestException as e:
        return {"error": f"Search failed: {str(e)}"}
    kw = keyword.lower()
    results, seen = [], set()
    for line in response.text.splitlines()[1:]:
        cols = [c.strip().strip('"').strip() for c in line.split("	")]
        if len(cols) < 7 or cols[2] not in ("dataset", "table"):
            continue
        title, code = cols[0], cols[1]
        if code in seen or (kw not in title.lower() and kw not in code.lower()):
            continue
        seen.add(code)
        results.append({"code": code, "title": title, "type": cols[2], "last_update": cols[3],
                        "data_start": cols[5], "data_end": cols[6]})
    return {"keyword": keyword, "count": len(results), "results": results[:50]}


def get_trade(reporter: str = "DE", partner: str = "WORLD", flow: str = "X") -> Any:
    """Get annual intra/extra-EU trade of a Member State (ext_lt_intratrd), million EUR.
    reporter: DE, FR, IT, ES, NL, etc.
    partner: WORLD, EU27_2020 (intra-EU) or EXT_EU27_2020 (extra-EU).
    flow: X=exports, M=imports, B=balance.
    """
    indic = {"X": "MIO_EXP_VAL", "M": "MIO_IMP_VAL", "B": "MIO_BAL_VAL"}.get(flow.upper(), "MIO_EXP_VAL")
    params = {
        "lang": "EN",
        "geo": reporter.upper(),
        "partner": partner.upper(),
        "indic_et": indic,
        "sitc06": "TOTAL",
        "lastTimePeriod": 20,
    }
    data = _make_request("ext_lt_intratrd", params=params)
    result = _parse_jsonstat(data, "trade_series")
    result.update({"reporter": reporter.upper(), "partner": partner.upper(), "flow": flow.upper()})
    return result


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided. Available: dataset, gdp, unemployment, inflation, population, search, trade"}))
        return

    command = args[0]

    if command == "dataset":
        if len(args) < 2:
            result = {"error": "Usage: dataset <dataset_code> [filters e.g. geo=DE,unit=CP_MEUR] [last_n_periods]"}
        else:
            filters = _parse_filters(args[2]) if len(args) > 2 else {}
            if len(args) > 3:
                filters["lastTimePeriod"] = int(args[3])
            result = get_dataset(args[1], filters)
    elif command == "gdp":
        country = args[1] if len(args) > 1 else "DE"
        freq = args[2] if len(args) > 2 else "A"
        result = get_gdp(country, freq)
    elif command == "unemployment":
        country = args[1] if len(args) > 1 else "EU27_2020"
        freq = args[2] if len(args) > 2 else "M"
        result = get_unemployment(country, freq)
    elif command == "inflation":
        country = args[1] if len(args) > 1 else "EU27_2020"
        freq = args[2] if len(args) > 2 else "M"
        result = get_inflation(country, freq)
    elif command == "population":
        country = args[1] if len(args) > 1 else "DE"
        result = get_population(country)
    elif command == "search":
        if len(args) < 2:
            result = {"error": "Usage: search <keyword>"}
        else:
            result = search_datasets(args[1])
    elif command == "trade":
        reporter = args[1] if len(args) > 1 else "DE"
        partner = args[2] if len(args) > 2 else "WORLD"
        flow = args[3] if len(args) > 3 else "X"
        result = get_trade(reporter, partner, flow)
    else:
        result = {"error": f"Unknown command: {command}. Available: dataset, gdp, unemployment, inflation, population, search, trade"}

    print(json.dumps(result))


if __name__ == "__main__":
    main()
