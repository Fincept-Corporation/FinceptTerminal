"""
Redfin Data Center Fetcher
Redfin Data Center: US housing market metrics — median sale price, days on market,
sale-to-list ratio, new listings by metro via public S3 bucket.
Files: redfin-public-data.s3.us-west-2.amazonaws.com/redfin_data_center/housing_market/ (plain CSV, newest
period first; the old redfin_market_tracker/*.tsv000.gz files were withdrawn - S3 answers HTTP 403).
"""
import csv
import re
import sys
import json
import time
import requests
from typing import Dict, Any, Optional, List

import fincept_connector as fc
import fincept_json

BASE_URL = "https://redfin-public-data.s3.us-west-2.amazonaws.com/redfin_data_center/housing_market"
SERVICE = "Redfin Data Center"

session = fc.http_session()
session.headers.update({"User-Agent": "Mozilla/5.0 (FinceptTerminal; +https://fincept.in)"})

# Housing Market Tracker files (see https://www.redfin.com/news/data-center/downloads/). The all_* files of the
# small geographies are huge (cities 1.7 GB, zips 2 GB, neighborhoods 2.4 GB), so they are streamed and only
# their newest part is scanned (SCAN_BYTES); the top-50 files are read whole.
REDFIN_PATHS = {
    "national":        "monthly/country.csv",
    "state":           "monthly/all_states.csv",
    "metro":           "monthly/all_metros.csv",
    "metro_top50":     "monthly/top_50_metros.csv",
    "county":          "monthly/all_counties.csv",
    "city":            "monthly/all_cities.csv",
    "city_top50":      "monthly/top_50_cities.csv",
    "zip":             "monthly/all_zips.csv",
    "neighborhood":    "monthly/all_neighborhoods.csv",
    "weekly":          "weekly/country.csv",
}
TRACKER_TYPES = ["national", "state", "metro", "county", "city", "zip", "neighborhood"]
SCAN_BYTES = 48 * 1024 * 1024   # newest ~4-5 monthly periods of the city/zip files
DEADLINE_S = 40                 # stop scanning after this long; the host kills scripts at 5 min
TEXT_COLS = {"last_updated", "frequency", "period_begin", "period_end", "region_id", "region_type",
             "region_name", "metro", "state", "state_code", "parent_metro_region"}
_T0 = time.monotonic()


def _col(h: str) -> str:
    """'MEDIAN SALE PRICE NSA ($)' -> 'median_sale_price_nsa'."""
    return re.sub(r"[^a-z0-9]+", "_", h.lower()).strip("_")


def _value(col: str, v: str):
    if v in ("NA", ""):
        return None
    if col in TEXT_COLS:
        return v
    if v in ("true", "false"):
        return v == "true"
    try:
        f = float(v)
        return int(f) if f.is_integer() and "." not in v else f
    except ValueError:
        return v


def _scan(path: str, needle: str = None, match=None, start_date: str = None, end_date: str = None,
          limit: int = 200, max_bytes: Optional[int] = None):
    """(records, note) from one CSV, streamed. `needle` is a cheap case-insensitive pre-filter on the raw
    line; `match(row)` the exact filter. Stops at `limit` rows, `max_bytes` or the deadline."""
    url = f"{BASE_URL}/{path}"
    try:
        response = session.get(url, stream=True, timeout=(10, 30))
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e)}, None
    with response:
        if response.status_code in (403, 404):
            return {"error": f"{SERVICE}: the file {path} is no longer published (HTTP {response.status_code}) - "
                             "Redfin has moved its downloads again"}, None
        if not response.ok:
            return {"error": fc.http_error(SERVICE, response)}, None
        lines = response.iter_lines(chunk_size=1 << 16)
        try:
            header = [_col(h) for h in next(csv.reader([next(lines).decode("utf-8-sig")]))]
        except StopIteration:
            return {"error": f"{SERVICE}: {path} is empty"}, None
        needle_l = needle.lower().encode() if needle else None
        records, read, note = [], 0, None
        try:
            for raw in lines:
                read += len(raw) + 1
                if max_bytes and read > max_bytes:
                    note = f"scanned the newest {max_bytes // (1024 * 1024)} MB of {path} only (older periods omitted)"
                    break
                if time.monotonic() - _T0 > DEADLINE_S:
                    note = f"stopped scanning {path} after {DEADLINE_S} s (older periods omitted)"
                    break
                if needle_l and needle_l not in raw.lower():
                    continue
                parts = next(csv.reader([raw.decode("utf-8", "replace")]))
                if len(parts) != len(header):
                    continue
                row = {h: _value(h, p) for h, p in zip(header, parts)}
                period = row.get("period_begin") or ""
                if (start_date and period < start_date) or (end_date and period > end_date):
                    continue
                if match and not match(row):
                    continue
                records.append(row)
                if len(records) >= limit:
                    break
        except requests.exceptions.RequestException as e:
            if not records:
                return {"error": fc.request_error(SERVICE, e)}, None
            note = f"download of {path} interrupted ({fc.request_error(SERVICE, e)})"
    return records, note


def _out(records, note, **meta) -> Dict:
    if isinstance(records, dict):
        return records
    out = {**meta, "count": len(records), "data": records}
    if note:
        out["note"] = note
    return out


def _name_match(text: Optional[str]):
    t = (text or "").strip().lower()
    return (lambda r: t in (r.get("region_name") or "").lower()) if t else None


def get_national_data(start_date: str = None, end_date: str = None) -> Any:
    records, note = _scan(REDFIN_PATHS["national"], start_date=start_date, end_date=end_date, limit=200)
    return _out(records, note, region="national")


def get_metro_data(metro: str = None, start_date: str = None, end_date: str = None) -> Any:
    records, note = _scan(REDFIN_PATHS["metro_top50"], needle=metro, match=_name_match(metro),
                          start_date=start_date, end_date=end_date, limit=200)
    if metro and isinstance(records, list) and not records:   # not one of the 50 largest metros
        records, note = _scan(REDFIN_PATHS["metro"], needle=metro, match=_name_match(metro),
                              start_date=start_date, end_date=end_date, limit=200)
    return _out(records, note, region_type="metro", filter=metro)


def get_city_data(city: str = None, state: str = None) -> Any:
    filter_str = city or None
    name = _name_match(city)
    st = (state or "").strip().upper()

    def match(r):
        rn = r.get("region_name") or ""
        return (not name or name(r)) and (not st or rn.upper().endswith(f", {st}"))

    records, note = _scan(REDFIN_PATHS["city_top50"], needle=city, match=match, limit=200)
    if (city or st) and isinstance(records, list) and not records:  # not one of the 50 largest cities
        records, note = _scan(REDFIN_PATHS["city"], needle=city or f", {st}", match=match, limit=200,
                              max_bytes=SCAN_BYTES)
    return _out(records, note, region_type="city", filter=filter_str, state=state)


def get_zip_data(zip_code: str = None) -> Any:
    z = (zip_code or "").strip()
    match = (lambda r: r.get("region_name") == z) if z else None
    records, note = _scan(REDFIN_PATHS["zip"], needle=f'"{z}"' if z else None, match=match, limit=200,
                          max_bytes=SCAN_BYTES)
    return _out(records, note, region_type="zip", filter=zip_code)


def get_market_tracker(region_type: str = "metro") -> Any:
    key = region_type.lower()
    if key not in TRACKER_TYPES:
        return {"error": f"Unknown region_type '{region_type}'. Options: {TRACKER_TYPES}"}
    records, note = _scan(REDFIN_PATHS[key], limit=100)
    return _out(records, note, region_type=region_type)


def get_weekly_update() -> Any:
    records, note = _scan(REDFIN_PATHS["weekly"], limit=500)
    if isinstance(records, dict):
        return records
    # Return the most recent 4 rolling four-week periods
    records_sorted = sorted(records, key=lambda x: x.get("period_begin") or "", reverse=True)
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
    print(fincept_json.dumps(result))


if __name__ == "__main__":
    main()
