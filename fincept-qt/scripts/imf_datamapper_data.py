"""
IMF DataMapper Data Fetcher
IMF DataMapper API: WEO forecasts, global debt, fiscal monitor, and financial
soundness indicators for 190+ countries.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('IMF_API_KEY', '')
BASE_URL = "https://www.imf.org/external/datamapper/api/v1"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def _observations(payload: Any) -> List[Dict]:
    """{"values": {INDICATOR: {COUNTRY: {YEAR: value}}}} -> one row per observation."""
    rows = []
    values = payload.get("values") if isinstance(payload, dict) else None
    for indicator, by_country in (values or {}).items():
        if not isinstance(by_country, dict):
            continue
        for country, by_year in by_country.items():
            if not isinstance(by_year, dict):
                continue
            for year, value in by_year.items():
                rows.append({"indicator": indicator, "country": country,
                             "year": int(year) if str(year).isdigit() else year, "value": value})
    return rows


def _with_rows(payload: Any, countries: str = None, start_year: str = None, end_year: str = None) -> Any:
    """Add the observations as a long table ("data": [{indicator, country, year, value}]) next to
    the DataMapper's own pivot ("values"), which the Economics IMF panel reads. The DataMapper
    answers with every country and year whatever the request names (checked 2026-11-17), so the
    table keeps only the countries / years asked for; "values" stays as the API sent it."""
    if isinstance(payload, dict) and "error" not in payload:
        rows = _observations(payload)
        wanted = {c.strip().upper() for c in str(countries or "").replace("/", ",").split(",") if c.strip()}
        lo = int(start_year) if str(start_year or "").isdigit() else None
        hi = int(end_year) if str(end_year or "").isdigit() else None
        payload["data"] = [r for r in rows
                           if (not wanted or str(r["country"]).upper() in wanted)
                           and (lo is None or not isinstance(r["year"], int) or r["year"] >= lo)
                           and (hi is None or not isinstance(r["year"], int) or r["year"] <= hi)]
        payload["count"] = len(payload["data"])
    return payload


def get_indicators() -> Any:
    """One row per DataMapper indicator: code, label, unit, dataset, source, description."""
    raw = _make_request("indicators")
    if not isinstance(raw, dict) or "error" in raw:
        return raw
    rows = []
    for code, info in (raw.get("indicators") or {}).items():
        if not code or not isinstance(info, dict):
            continue
        rows.append({"code": code, "label": info.get("label"), "unit": info.get("unit"),
                     "dataset": info.get("dataset"), "source": info.get("source"),
                     "description": info.get("description")})
    return {"data": rows, "count": len(rows), "source": "IMF DataMapper"}


def get_datasets() -> Any:
    """The datasets the indicators belong to (the API's own /datasets list is empty), one row each."""
    ind = get_indicators()
    if not isinstance(ind, dict) or "error" in ind:
        return ind
    counts: Dict[str, int] = {}
    for row in ind["data"]:
        if row.get("dataset"):
            counts[row["dataset"]] = counts.get(row["dataset"], 0) + 1
    rows = [{"dataset": k, "indicators": v} for k, v in sorted(counts.items())]
    return {"data": rows, "count": len(rows), "source": "IMF DataMapper"}


def get_indicator_data(indicator: str, countries: str = None) -> Any:
    if countries:
        endpoint = f"{indicator}/{countries}"
    else:
        endpoint = indicator
    return _with_rows(_make_request(endpoint), countries)


def get_weo_data(indicator: str, country: str, start_year: str = None, end_year: str = None) -> Any:
    # startYear/endYear are not DataMapper parameters (the API answers them with an error page that
    # is not JSON): the year range is applied to the rows instead.
    endpoint = f"{indicator}/{country.upper()}"
    return _with_rows(_make_request(endpoint), country, start_year, end_year)


def get_fiscal_data(indicator: str, country: str) -> Any:
    fiscal_indicators = {
        "overall_balance": "GGXCNL_NGDP",
        "gross_debt": "GGXWDG_NGDP",
        "net_debt": "GGXWDN_NGDP",
        "revenue": "GGR_NGDP",
        "expenditure": "GGX_NGDP",
        "primary_balance": "GGXONLB_NGDP",
    }
    imf_indicator = fiscal_indicators.get(indicator, indicator)
    return _with_rows(_make_request(f"{imf_indicator}/{country.upper()}"), country)


def get_financial_soundness(country: str) -> Any:
    fsi_indicators = ["FSI_NIBT_MCA", "FSI_CAR_MCA", "FSI_NPL_NLSS", "FSI_ROA_MCA"]
    results = {}
    rows = []
    for ind in fsi_indicators:
        results[ind] = _make_request(f"{ind}/{country.upper()}")
        rows += [r for r in _observations(results[ind]) if str(r["country"]).upper() == country.upper()]
    # The per-indicator responses stay under their codes; "data" is all of them as one table.
    results["data"] = rows
    results["count"] = len(rows)
    return results


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}

    if command == "indicators":
        result = get_indicators()
    elif command == "datasets":
        result = get_datasets()
    elif command == "data":
        indicator = args[1] if len(args) > 1 else "NGDP_RPCH"
        countries = args[2] if len(args) > 2 else None
        result = get_indicator_data(indicator, countries)
    elif command == "weo":
        # DataMapper country codes are ISO 3166 alpha-3 ("USA"); an unknown code such as "US"
        # makes it answer with every country.
        indicator = args[1] if len(args) > 1 else "NGDP_RPCH"
        country = args[2] if len(args) > 2 else "USA"
        start_year = args[3] if len(args) > 3 else None
        end_year = args[4] if len(args) > 4 else None
        result = get_weo_data(indicator, country, start_year, end_year)
    elif command == "fiscal":
        indicator = args[1] if len(args) > 1 else "gross_debt"
        country = args[2] if len(args) > 2 else "USA"
        result = get_fiscal_data(indicator, country)
    elif command == "fsi":
        country = args[1] if len(args) > 1 else "USA"
        result = get_financial_soundness(country)

    print(json.dumps(result))


if __name__ == "__main__":
    main()
