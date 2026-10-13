"""
USDA Foreign Agricultural Service (FAS) PSD Data Fetcher
Production, Supply and Distribution (PSD) balance sheets - area, yield, production, trade, consumption and stocks - by commodity and country, plus world totals, from the USDA FAS OpenData API (api.data.gov key: USDA_FAS_API_KEY; falls back to the rate-limited public DEMO_KEY).
"""
import sys
import json
import os
import time
import tempfile
import requests
from urllib3.util.retry import Retry
from typing import Dict, Any, List

API_KEY = os.environ.get('USDA_FAS_API_KEY', '')
# The PSD Online /psdonline/api paths are gone; the OpenData API is served through api.data.gov.
BASE_URL = "https://api.fas.usda.gov/api/psd"
# api.data.gov's documented public demo key (30 requests/hour per IP) - used only when no key is set.
PUBLIC_DEMO_KEY = "DEMO_KEY"
LOOKUP_CACHE = os.path.join(tempfile.gettempdir(), "fincept_usda_psd_lookups.json")

COMMODITIES = {"corn": "0440000", "wheat": "0410000", "soybeans": "2222000", "cotton": "2631000",
               "rice": "0422110", "barley": "0430000", "sorghum": "0459200", "soybean_meal": "0813100",
               "soybean_oil": "4232000", "palm_oil": "4243000", "sugar": "0612000", "coffee": "0711100"}

session = requests.Session()
# No status retries: on HTTP 429 urllib3 would otherwise sleep for the Retry-After (up to an hour).
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10,
                                        max_retries=Retry(total=2, status=0, respect_retry_after_header=False))
session.mount('https://', adapter)
session.headers.update({"User-Agent": "FinceptTerminal/4.5 (+https://fincept.in)"})


def _make_request(endpoint: str, params: Dict = None) -> Any:
    params = dict(params or {})
    params["api_key"] = API_KEY or PUBLIC_DEMO_KEY
    try:
        response = session.get(f"{BASE_URL}/{endpoint}", params=params, timeout=60)
        if response.status_code == 429:
            return {"error": "api.data.gov rate limit reached" + (
                "" if API_KEY else " for the shared DEMO_KEY - get a free key at https://api.data.gov/signup "
                                   "and save it as USDA_FAS_API_KEY in Settings > Credentials")}
        if response.status_code in (401, 403):
            return {"error": "api.data.gov rejected the API key (USDA_FAS_API_KEY)"}
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def _lookups() -> Dict[str, Dict]:
    """Attribute / unit / country names (cached for a week - they rarely change)."""
    try:
        if time.time() - os.path.getmtime(LOOKUP_CACHE) < 7 * 86400:
            with open(LOOKUP_CACHE, encoding="utf-8") as f:
                return json.load(f)
    except (OSError, ValueError):
        pass
    out = {"attributes": {}, "units": {}}
    a = _make_request("commodityAttributes")
    u = _make_request("unitsOfMeasure")
    if isinstance(a, list):
        out["attributes"] = {str(x["attributeId"]): x["attributeName"].strip() for x in a}
    if isinstance(u, list):
        out["units"] = {str(x["unitId"]): x["unitDescription"].strip() for x in u}
    if out["attributes"] and out["units"]:
        try:
            with open(LOOKUP_CACHE, "w", encoding="utf-8") as f:
                json.dump(out, f)
        except OSError:
            pass
    return out


def _code(commodity: str) -> str:
    return COMMODITIES.get(commodity.lower(), commodity)


def _label(rows: Any, attribute_filter: List[str] = None) -> Any:
    if not isinstance(rows, list):
        return rows
    lk = _lookups()
    out = []
    for r in rows:
        attr = lk["attributes"].get(str(r.get("attributeId")), str(r.get("attributeId")))
        if attribute_filter and attr.lower() not in attribute_filter:
            continue
        out.append({"market_year": r.get("marketYear"), "country": r.get("countryCode"), "attribute": attr,
                    "value": r.get("value"), "unit": lk["units"].get(str(r.get("unitId")), r.get("unitId")),
                    "report": f"{r.get('calendarYear')}-{r.get('month')}"})
    return {"count": len(out), "data": out, "source": "USDA FAS PSD"}


def _year(year: str = None) -> str:
    if year:
        return str(year)
    t = time.gmtime()
    return str(t.tm_year if t.tm_mon >= 9 else t.tm_year - 1)


def get_commodities() -> Any:
    data = _make_request("commodities")
    return {"count": len(data), "commodities": data, "aliases": COMMODITIES} if isinstance(data, list) else data


def get_countries() -> Any:
    data = _make_request("countries")
    return {"count": len(data), "countries": data} if isinstance(data, list) else data


def get_psd_data(commodity_code: str, country_code: str, year: str = None) -> Any:
    """PSD balance sheet for one commodity, country (PSD 2-letter code) and marketing year."""
    res = _label(_make_request(f"commodity/{_code(commodity_code)}/country/{country_code.upper()}/year/{_year(year)}"))
    if isinstance(res, dict) and "error" not in res:
        res.update({"commodity": _code(commodity_code), "country_code": country_code.upper(), "market_year": _year(year)})
    return res


def get_commodity_data(commodity_code: str, year: str = None) -> Any:
    """World PSD balance sheet for one commodity and marketing year."""
    res = _label(_make_request(f"commodity/{_code(commodity_code)}/world/year/{_year(year)}"))
    if isinstance(res, dict) and "error" not in res:
        res.update({"commodity": _code(commodity_code), "scope": "world", "market_year": _year(year)})
    return res


def get_export_data(commodity_code: str, country_code: str, year: str = None) -> Any:
    """Exports, imports and ending stocks for one commodity/country/marketing year."""
    res = _label(_make_request(f"commodity/{_code(commodity_code)}/country/{country_code.upper()}/year/{_year(year)}"),
                 ["exports", "imports", "my exports", "my imports", "ty exports", "ty imports", "ending stocks"])
    if isinstance(res, dict) and "error" not in res:
        res.update({"commodity": _code(commodity_code), "country_code": country_code.upper(), "market_year": _year(year)})
    return res


def get_regions() -> Any:
    data = _make_request("regions")
    return {"count": len(data), "regions": data} if isinstance(data, list) else data


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "commodities":
        result = get_commodities()
    elif command == "countries":
        result = get_countries()
    elif command == "psd":
        commodity_code = args[1] if len(args) > 1 else "0440000"
        country_code = args[2] if len(args) > 2 else "US"
        year = args[3] if len(args) > 3 else None
        result = get_psd_data(commodity_code, country_code, year)
    elif command == "commodity":
        commodity_code = args[1] if len(args) > 1 else "0440000"
        year = args[2] if len(args) > 2 else None
        result = get_commodity_data(commodity_code, year)
    elif command == "exports":
        commodity_code = args[1] if len(args) > 1 else "0440000"
        country_code = args[2] if len(args) > 2 else "US"
        year = args[3] if len(args) > 3 else None
        result = get_export_data(commodity_code, country_code, year)
    elif command == "regions":
        result = get_regions()

    print(json.dumps(result))


if __name__ == "__main__":
    main()
