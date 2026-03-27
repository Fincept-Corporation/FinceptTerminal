"""
Redfin Data Center Fetcher
Redfin Data Center: US housing market metrics — median sale price, days on market,
sale-to-list ratio, new listings by metro via public S3 bucket.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://redfin-public-data.s3.us-west-2.amazonaws.com"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

REDFIN_PATHS = {
    "national":        "redfin_market_tracker/us_national_market_tracker.tsv000.gz",
    "metro":           "redfin_market_tracker/redfin_metro_market_tracker.tsv000.gz",
    "city":            "redfin_market_tracker/redfin_city_market_tracker.tsv000.gz",
    "zip":             "redfin_market_tracker/redfin_zip_code_market_tracker.tsv000.gz",
    "neighborhood":    "redfin_market_tracker/neighborhood_market_tracker.tsv000.gz",
    "county":          "redfin_market_tracker/county_market_tracker.tsv000.gz",
}


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, timeout=60)
        response.raise_for_status()
        return response
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}


def _parse_tsv_gz(response_obj, region_filter: str = None, region_col: str = "region_name",
                  start_date: str = None, end_date: str = None, limit: int = 100) -> Any:
    try:
        import gzip
        import io
        content = gzip.decompress(response_obj.content).decode('utf-8', errors='replace')
        lines = content.strip().split('\n')
        if not lines:
            return {"error": "Empty data"}
        headers = [h.strip() for h in lines[0].split('\t')]
        records = []
        for line in lines[1:]:
            if len(records) >= limit:
                break
            parts = line.split('\t')
            if len(parts) < len(headers):
                continue
            row = {headers[i]: parts[i].strip() for i in range(len(headers))}
            if region_filter:
                col_val = row.get(region_col, row.get("region_name", ""))
                if region_filter.lower() not in col_val.lower():
                    continue
            if start_date or end_date:
                period = row.get("period_begin", row.get("period_end", ""))
                if start_date and period < start_date:
                    continue
                if end_date and period > end_date:
                    continue
            records.append(row)
        return records
    except Exception as e:
        return {"error": f"Parse error: {str(e)}"}


def get_national_data(start_date: str = None, end_date: str = None) -> Any:
    resp = _make_request(REDFIN_PATHS["national"])
    if isinstance(resp, dict):
        return resp
    records = _parse_tsv_gz(resp, start_date=start_date, end_date=end_date, limit=200)
    if isinstance(records, dict):
        return records
    return {"region": "national", "count": len(records), "data": records}


def get_metro_data(metro: str = None, start_date: str = None, end_date: str = None) -> Any:
    resp = _make_request(REDFIN_PATHS["metro"])
    if isinstance(resp, dict):
        return resp
    records = _parse_tsv_gz(resp, region_filter=metro, start_date=start_date,
                            end_date=end_date, limit=200)
    if isinstance(records, dict):
        return records
    return {"region_type": "metro", "filter": metro, "count": len(records), "data": records}


def get_city_data(city: str = None, state: str = None) -> Any:
    resp = _make_request(REDFIN_PATHS["city"])
    if isinstance(resp, dict):
        return resp
    filter_str = None
    if city:
        filter_str = city
    records = _parse_tsv_gz(resp, region_filter=filter_str, limit=200)
    if isinstance(records, dict):
        return records
    if state:
        records = [r for r in records if state.upper() in r.get("state_code", "").upper()]
    return {"region_type": "city", "filter": filter_str, "state": state,
            "count": len(records), "data": records}


def get_zip_data(zip_code: str = None) -> Any:
    resp = _make_request(REDFIN_PATHS["zip"])
    if isinstance(resp, dict):
        return resp
    records = _parse_tsv_gz(resp, region_filter=zip_code, region_col="region_name", limit=200)
    if isinstance(records, dict):
        return records
    return {"region_type": "zip", "filter": zip_code, "count": len(records), "data": records}


def get_market_tracker(region_type: str = "metro") -> Any:
    key = region_type.lower()
    if key not in REDFIN_PATHS:
        return {"error": f"Unknown region_type '{region_type}'. Options: {list(REDFIN_PATHS.keys())}"}
    resp = _make_request(REDFIN_PATHS[key])
    if isinstance(resp, dict):
        return resp
    records = _parse_tsv_gz(resp, limit=100)
    if isinstance(records, dict):
        return records
    return {"region_type": region_type, "count": len(records), "data": records}


def get_weekly_update() -> Any:
    resp = _make_request(REDFIN_PATHS["national"])
    if isinstance(resp, dict):
        return resp
    records = _parse_tsv_gz(resp, limit=500)
    if isinstance(records, dict):
        return records
    # Return the most recent 4 weeks
    records_sorted = sorted(records, key=lambda x: x.get("period_begin", ""), reverse=True)
    latest = records_sorted[:4]
    return {"type": "weekly_update", "weeks": len(latest), "data": latest}


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "national":
        start_date = args[1] if len(args) > 1 else None
        end_date = args[2] if len(args) > 2 else None
        result = get_national_data(start_date, end_date)
    elif command == "metro":
        metro = args[1] if len(args) > 1 else None
        start_date = args[2] if len(args) > 2 else None
        end_date = args[3] if len(args) > 3 else None
        result = get_metro_data(metro, start_date, end_date)
    elif command == "city":
        city = args[1] if len(args) > 1 else None
        state = args[2] if len(args) > 2 else None
        result = get_city_data(city, state)
    elif command == "zip":
        zip_code = args[1] if len(args) > 1 else None
        result = get_zip_data(zip_code)
    elif command == "tracker":
        region_type = args[1] if len(args) > 1 else "metro"
        result = get_market_tracker(region_type)
    elif command == "weekly":
        result = get_weekly_update()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
