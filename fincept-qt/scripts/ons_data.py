"""
ONS Data Fetcher
UK Office for National Statistics (ONS): UK GDP, CPI, labour market, trade, earnings, debt, house prices.

No API key required. Time series come from the ONS website's public JSON feed
(https://www.ons.gov.uk/<topic path>/timeseries/<id>/<dataset>/data). The old
api.ons.gov.uk/v1 host this script used to call no longer exists (every request was a 404).
Dataset metadata (dataset / search commands) comes from the ONS beta API.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://www.ons.gov.uk"
BETA_API_URL = "https://api.beta.ons.gov.uk/v1"
HPI_URL = "https://landregistry.data.gov.uk/data/ukhpi/region/united-kingdom.json"

ONS_DATASETS = {
    "gdp": "qna",
    "cpi": "mm23",
    "unemployment": "lms",
    "trade": "mret",
    "population": "mid-year-pop-est",
    "housing": "house-prices-local-authority"
}

# command -> ONS time series. `path` is the topic path the series lives under.
ONS_SERIES = {
    "gdp": {"id": "abmi", "dataset": "qna", "path": "economy/grossdomesticproductgdp",
            "unit": "GBP million"},
    "cpi": {"id": "d7g7", "dataset": "mm23", "path": "economy/inflationandpriceindices", "unit": "%"},
    "cpih": {"id": "l55o", "dataset": "mm23", "path": "economy/inflationandpriceindices", "unit": "%"},
    "rpi": {"id": "czbh", "dataset": "mm23", "path": "economy/inflationandpriceindices", "unit": "%"},
    "unemployment": {"id": "mgsx", "dataset": "lms",
                     "path": "employmentandlabourmarket/peoplenotinwork/unemployment", "unit": "%"},
    "employment": {"id": "lf24", "dataset": "lms",
                   "path": "employmentandlabourmarket/peopleinwork/employmentandemployeetypes", "unit": "%"},
    "trade_balance": {"id": "ikbj", "dataset": "mret", "path": "economy/nationalaccounts/balanceofpayments",
                      "unit": "GBP million"},
    "avg_earnings": {"id": "kab9", "dataset": "emp",
                     "path": "employmentandlabourmarket/peopleinwork/earningsandworkinghours", "unit": "GBP / week"},
    "public_debt": {"id": "hf6x", "dataset": "pusf", "path": "economy/governmentpublicsectorandtaxes/publicsectorfinance",
                    "unit": "% of GDP"},
}

# Series ids usable with `timeseries <dataset> <id>` without having to pass a topic path.
ONS_SERIES_BY_ID = {f"{s['id']}/{s['dataset']}": s for s in ONS_SERIES.values()}

session = requests.Session()
session.headers.update({"User-Agent": "Mozilla/5.0 (Fincept Terminal)"})
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

_MONTHS = {"JAN": "01", "FEB": "02", "MAR": "03", "APR": "04", "MAY": "05", "JUN": "06",
           "JUL": "07", "AUG": "08", "SEP": "09", "OCT": "10", "NOV": "11", "DEC": "12"}


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = endpoint if endpoint.startswith('http') else f"{BETA_API_URL}/{endpoint}"
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


def _iso_date(label: str) -> str:
    """'2026 AUG' -> '2026-08', '2026 Q2' -> '2026-Q2', '2026' -> '2026' (sortable)."""
    parts = str(label).strip().split()
    if len(parts) == 2:
        year, period = parts
        if period.upper() in _MONTHS:
            return f"{year}-{_MONTHS[period.upper()]}"
        if period.upper().startswith("Q"):
            return f"{year}-{period.upper()}"
    return str(label).strip()


def _fetch_series(command: str, start_year: str = None, end_year: str = None,
                  frequency: str = None) -> Dict[str, Any]:
    """Fetch one ONS time series and normalise it to {success, label, unit, count, data:[{date, value}]}."""
    spec = ONS_SERIES[command]
    return get_timeseries(spec["dataset"], spec["id"], start_year, end_year, spec["path"], command, frequency)


def get_timeseries(dataset_id: str, timeseries_id: str, start_year: str = None, end_year: str = None,
                   topic_path: str = None, series_name: str = None, frequency: str = None) -> Any:
    key = f"{str(timeseries_id).lower()}/{str(dataset_id).lower()}"
    spec = ONS_SERIES_BY_ID.get(key)
    path = topic_path or (spec["path"] if spec else None)
    if not path:
        return {"error": f"Unknown ONS series {timeseries_id}/{dataset_id}: pass its topic path as the 5th argument "
                         f"(e.g. economy/grossdomesticproductgdp)"}
    url = f"{BASE_URL}/{path.strip('/')}/timeseries/{str(timeseries_id).lower()}/{str(dataset_id).lower()}/data"
    raw = _make_request(url)
    if "error" in raw and "description" not in raw:
        return raw

    # Prefer the most granular periodicity the series offers (months > quarters > years),
    # unless the caller asked for a specific one.
    order = ["months", "quarters", "years"]
    wanted = {"monthly": "months", "quarterly": "quarters", "quarter": "quarters", "annual": "years",
              "yearly": "years"}.get((frequency or "").lower())
    if wanted and raw.get(wanted):
        order = [wanted]
    points = []
    period_kind = None
    for kind in order:
        if raw.get(kind):
            points = raw[kind]
            period_kind = kind
            break

    rows = []
    for p in points:
        try:
            value = float(str(p.get("value", "")).replace(",", ""))
        except ValueError:
            continue  # blank / suppressed observation - a gap stays a gap
        date = _iso_date(p.get("date", ""))
        year = date[:4]
        if start_year and year.isdigit() and int(year) < int(start_year):
            continue
        if end_year and year.isdigit() and int(year) > int(end_year):
            continue
        rows.append({"date": date, "value": value})

    desc = raw.get("description", {})
    return {
        "success": True,
        "series": series_name or str(timeseries_id).lower(),
        "series_id": str(timeseries_id).upper(),
        "dataset": str(dataset_id).upper(),
        "label": desc.get("title", "").strip(),
        "unit": (spec or {}).get("unit") or desc.get("unit", ""),
        "frequency": {"months": "monthly", "quarters": "quarterly", "years": "annual"}.get(period_kind, ""),
        "release_date": desc.get("releaseDate", ""),
        "count": len(rows),
        "data": rows,
        "source": "UK Office for National Statistics",
        "url": url,
    }


def get_dataset(dataset_id: str) -> Any:
    return _make_request(f"datasets/{dataset_id}")


def get_gdp(frequency: str = "quarterly", start: str = None, end: str = None) -> Any:
    return _fetch_series("gdp", start, end, frequency)


def get_cpi(category: str = "all_items", start: str = None, end: str = None) -> Any:
    return _fetch_series("cpi", start, end)


def get_unemployment(measure: str = "rate", start: str = None, end: str = None) -> Any:
    return _fetch_series("unemployment", start, end)


def get_house_prices(start: str = None, end: str = None) -> Any:
    """UK House Price Index (HM Land Registry / ONS / Registers of Scotland): monthly average price."""
    # The linked-data API caps a page at 200 items (oldest first), so page through all of them
    # or the most recent months - the ones that matter - would be silently missing.
    items = []
    for page in range(10):
        raw = _make_request(HPI_URL, {"_pageSize": 200, "_page": page, "_sort": "refMonth", "_view": "all"})
        if "error" in raw:
            if items:
                break  # keep what we have rather than failing the whole series
            return raw
        result = raw.get("result", {})
        batch = result.get("items", [])
        items.extend(batch)
        if not batch or len(items) >= int(result.get("totalResults", 0) or 0):
            break
    rows = []
    for item in items:
        if not isinstance(item, dict) or item.get("averagePrice") is None:
            continue
        date = item.get("refMonth", "")
        year = date[:4]
        if start and year.isdigit() and int(year) < int(start):
            continue
        if end and year.isdigit() and int(year) > int(end):
            continue
        rows.append({"date": date, "value": item.get("averagePrice"),
                     "house_price_index": item.get("housePriceIndex"),
                     "annual_change_pct": item.get("percentageAnnualChange")})
    return {
        "success": True,
        "series": "house_prices",
        "label": "UK House Price Index: average price, all property types",
        "unit": "GBP",
        "frequency": "monthly",
        "count": len(rows),
        "data": rows,
        "source": "UK House Price Index (HM Land Registry, ONS, Registers of Scotland)",
        "url": HPI_URL,
    }


def search(query: str) -> Any:
    """Case-insensitive title/id search over the ONS beta-API dataset catalogue."""
    raw = _make_request("datasets", {"limit": 1000})
    if "error" in raw:
        return raw
    needle = query.lower()
    matches = [{"id": d.get("id"), "title": d.get("title"), "description": d.get("description", "")}
               for d in raw.get("items", [])
               if needle in str(d.get("title", "")).lower() or needle in str(d.get("id", "")).lower()
               or needle in str(d.get("description", "")).lower()]
    return {"success": True, "query": query, "count": len(matches), "data": matches}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "dataset":
        dataset_id = args[1] if len(args) > 1 else "mm23"
        result = get_dataset(dataset_id)
    elif command == "timeseries":
        dataset_id = args[1] if len(args) > 1 else "qna"
        timeseries_id = args[2] if len(args) > 2 else "ABMI"
        start_year = args[3] if len(args) > 3 else None
        end_year = args[4] if len(args) > 4 else None
        topic_path = args[5] if len(args) > 5 else None
        result = get_timeseries(dataset_id, timeseries_id, start_year, end_year, topic_path)
    elif command == "gdp":
        frequency = args[1] if len(args) > 1 else "quarterly"
        start = args[2] if len(args) > 2 else None
        end = args[3] if len(args) > 3 else None
        result = get_gdp(frequency, start, end)
    elif command == "cpi":
        category = args[1] if len(args) > 1 else "all_items"
        start = args[2] if len(args) > 2 else None
        end = args[3] if len(args) > 3 else None
        result = get_cpi(category, start, end)
    elif command == "unemployment":
        measure = args[1] if len(args) > 1 else "rate"
        start = args[2] if len(args) > 2 else None
        end = args[3] if len(args) > 3 else None
        result = get_unemployment(measure, start, end)
    elif command == "house_prices":
        start = args[1] if len(args) > 1 else None
        end = args[2] if len(args) > 2 else None
        result = get_house_prices(start, end)
    elif command in ONS_SERIES:
        # cpih, rpi, employment, trade_balance, avg_earnings, public_debt: [start_year] [end_year]
        start = args[1] if len(args) > 1 else None
        end = args[2] if len(args) > 2 else None
        result = _fetch_series(command, start, end)
    elif command == "search":
        query = args[1] if len(args) > 1 else "GDP"
        result = search(query)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
