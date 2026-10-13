"""
Australian Bureau of Statistics Data Fetcher
ABS Data API (SDMX, keyless): Australian GDP, CPI, labour force, merchandise exports, population and any of the ~1,200 ABS dataflows.
"""
import sys
import json
import requests
from typing import Dict, Any

from sdmx_json import flatten, compact

# The ABS Data API lives at data.api.abs.gov.au/rest (the old api.data.abs.gov.au host is gone).
BASE_URL = "https://data.api.abs.gov.au/rest"
DATA_ACCEPT = "application/vnd.sdmx.data+json"
STRUCT_ACCEPT = "application/vnd.sdmx.structure+json"

ABS_DATAFLOWS = {
    "gdp": "ANA_AGG",
    "cpi": "CPI",
    "labour": "LF",
    "trade": "MERCH_EXP",
    "population": "ERP_Q"
}

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)
session.headers.update({"User-Agent": "FinceptTerminal/4.5 (+https://fincept.in)"})


def _make_request(endpoint: str, params: Dict = None, accept: str = DATA_ACCEPT) -> Any:
    url = f"{BASE_URL}/{endpoint}"
    try:
        response = session.get(url, params=params or {}, headers={"Accept": accept}, timeout=60)
        if response.status_code == 404 and accept == DATA_ACCEPT:
            return {"error": "No data for this query (HTTP 404) — check the dataflow id, key and period range"}
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {e.response.text[:200] or str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def _series(dataflow_id: str, key: str, params: Dict, meta: Dict) -> Any:
    raw = _make_request(f"data/ABS,{dataflow_id}/{key}", params=params)
    if isinstance(raw, dict) and "error" in raw:
        return {**meta, **raw}
    series = compact(flatten(raw))
    return {**meta, "dataflow": dataflow_id, "key": key, "series_count": len(series), "series": series}


def get_dataflows(search: str = None) -> Any:
    """List all ABS dataflows (id + name), optionally filtered by a case-insensitive substring."""
    raw = _make_request("dataflow/ABS", accept=STRUCT_ACCEPT)
    if isinstance(raw, dict) and "error" in raw:
        return raw
    flows = [{"id": f.get("id"), "name": f.get("name")} for f in raw.get("data", {}).get("dataflows", [])]
    if search:
        s = search.lower()
        flows = [f for f in flows if s in (f["id"] or "").lower() or s in (f["name"] or "").lower()]
    return {"count": len(flows), "dataflows": flows}


def get_dataflow(dataflow_id: str) -> Any:
    """Describe a dataflow: its dimensions (in key order) and their codes."""
    raw = _make_request(f"dataflow/ABS/{dataflow_id}/latest", params={"references": "all"}, accept=STRUCT_ACCEPT)
    if isinstance(raw, dict) and "error" in raw:
        return raw
    d = raw.get("data", {})
    flows = d.get("dataflows", [])
    dsds = d.get("dataStructures", [])
    if not flows or not dsds:
        return {"error": f"Unknown dataflow: {dataflow_id}"}
    codelists = {c.get("id"): c for c in d.get("codelists", [])}
    dims = []
    for dim in dsds[0].get("dataStructureComponents", {}).get("dimensionList", {}).get("dimensions", []):
        enum = dim.get("localRepresentation", {}).get("enumeration", "")
        cl_id = enum.split("(")[0].split(".")[-1].split(":")[-1]
        codes = [{"id": c.get("id"), "name": c.get("name")} for c in codelists.get(cl_id, {}).get("codes", [])]
        dims.append({"id": dim.get("id"), "position": dim.get("position"), "codes": codes[:200],
                     "code_count": len(codes)})
    return {"id": flows[0].get("id"), "name": flows[0].get("name"),
            "description": flows[0].get("description"),
            "key_template": ".".join(x["id"] for x in dims),
            "dimensions": dims}


def get_data(dataflow_id: str, key: str = "all", start_period: str = None, end_period: str = None) -> Any:
    params = {}
    if start_period:
        params["startPeriod"] = start_period
    if end_period:
        params["endPeriod"] = end_period
    if not start_period and not end_period:
        params["lastNObservations"] = 20
    return _series(dataflow_id, key, params, {})


def get_gdp(state: str = "AUS", frequency: str = "Q", start: str = "2020-Q1", end: str = None) -> Any:
    # ANA_AGG key: MEASURE.DATA_ITEM.TSEST.REGION.FREQ — M1 = chain volume, GPM = GDP, 20 = seasonally adjusted
    params = {"startPeriod": start}
    if end:
        params["endPeriod"] = end
    return _series(ABS_DATAFLOWS["gdp"], f"M1+M2.GPM.20.{state}.{frequency}", params,
                   {"indicator": "GDP, chain volume measures (seasonally adjusted)"})


def get_cpi(product_group: str = "10001", state: str = "50", start: str = "2020-Q1", end: str = None) -> Any:
    # CPI key: MEASURE.INDEX.TSEST.REGION.FREQ — 1 = index numbers, 3 = % change from a year ago;
    # INDEX 10001 = All groups CPI; REGION 50 = weighted average of eight capital cities
    if product_group == "1":  # legacy default
        product_group = "10001"
    params = {"startPeriod": start}
    if end:
        params["endPeriod"] = end
    return _series(ABS_DATAFLOWS["cpi"], f"1+3.{product_group}.10.{state}.Q", params,
                   {"indicator": "Consumer Price Index"})


def get_labour(state: str = "AUS", sex: str = "3", start: str = "2020-01", end: str = None) -> Any:
    # LF key: MEASURE.SEX.AGE.TSEST.REGION.FREQ — M3 employed, M13 unemployment rate, M12 participation rate
    params = {"startPeriod": start}
    if end:
        params["endPeriod"] = end
    return _series(ABS_DATAFLOWS["labour"], f"M3+M13+M12.{sex}.1599.20.{state}.M", params,
                   {"indicator": "Labour force (seasonally adjusted)"})


def get_trade(product: str = "TOT", partner: str = "TOT", start: str = "2024-01", end: str = None) -> Any:
    # MERCH_EXP key: COMMODITY_SITC.COUNTRY_DEST.STATE_ORIGIN.FREQ (values in AUD thousands)
    product = "TOT" if product in ("all", "") else product
    partner = "TOT" if partner in ("all", "") else partner
    params = {"startPeriod": start}
    if end:
        params["endPeriod"] = end
    return _series(ABS_DATAFLOWS["trade"], f"{product}.{partner}.TOT.M", params,
                   {"indicator": "Merchandise exports (AUD thousands)"})


def get_population(state: str = "AUS", start: str = "2015-Q1", end: str = None) -> Any:
    # ERP_Q key: MEASURE.SEX.AGE.REGION.FREQ
    params = {"startPeriod": start}
    if end:
        params["endPeriod"] = end
    return _series(ABS_DATAFLOWS["population"], f"1.3.TOT.{state}.Q", params,
                   {"indicator": "Estimated resident population"})


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "dataflows":
        search = args[1] if len(args) > 1 else None
        result = get_dataflows(search)
    elif command == "dataflow":
        dataflow_id = args[1] if len(args) > 1 else "ANA_AGG"
        result = get_dataflow(dataflow_id)
    elif command == "data":
        dataflow_id = args[1] if len(args) > 1 else "ANA_AGG"
        key = args[2] if len(args) > 2 else "all"
        start_period = args[3] if len(args) > 3 else None
        end_period = args[4] if len(args) > 4 else None
        result = get_data(dataflow_id, key, start_period, end_period)
    elif command == "gdp":
        state = args[1] if len(args) > 1 else "AUS"
        frequency = args[2] if len(args) > 2 else "Q"
        start = args[3] if len(args) > 3 else "2020-Q1"
        end = args[4] if len(args) > 4 else None
        result = get_gdp(state, frequency, start, end)
    elif command == "cpi":
        product_group = args[1] if len(args) > 1 else "10001"
        state = args[2] if len(args) > 2 else "50"
        start = args[3] if len(args) > 3 else "2020-Q1"
        end = args[4] if len(args) > 4 else None
        result = get_cpi(product_group, state, start, end)
    elif command == "labour":
        state = args[1] if len(args) > 1 else "AUS"
        sex = args[2] if len(args) > 2 else "3"
        start = args[3] if len(args) > 3 else "2020-01"
        end = args[4] if len(args) > 4 else None
        result = get_labour(state, sex, start, end)
    elif command == "trade":
        product = args[1] if len(args) > 1 else "TOT"
        partner = args[2] if len(args) > 2 else "TOT"
        start = args[3] if len(args) > 3 else "2024-01"
        end = args[4] if len(args) > 4 else None
        result = get_trade(product, partner, start, end)
    elif command == "population":
        state = args[1] if len(args) > 1 else "AUS"
        start = args[2] if len(args) > 2 else "2015-Q1"
        end = args[3] if len(args) > 3 else None
        result = get_population(state, start, end)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
