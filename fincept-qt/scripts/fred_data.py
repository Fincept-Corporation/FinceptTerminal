"""FRED connector for Embedded Python JSON bridge."""

from __future__ import annotations

import calendar
import hashlib
import json
import os
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

import requests

FRED_API_BASE = "https://api.stlouisfed.org/fred"
FRED_API_KEY = os.environ.get("FRED_API_KEY", "")
CACHE_TTL_SEC = int(os.environ.get("FRED_CACHE_TTL_SEC", "300"))
CACHE_DIR = Path(tempfile.gettempdir()) / "fincept_fred_cache"
REQUEST_TIMEOUT_SEC = 1.8

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=2)
session.mount("https://", adapter)


def _error(message: str, code: str, **extra: Any) -> Dict[str, Any]:
    payload: Dict[str, Any] = {"success": False, "error": message, "error_code": code}
    payload.update(extra)
    return payload


def _cache_key(endpoint: str, params: Dict[str, Any]) -> str:
    stable = json.dumps({"endpoint": endpoint, "params": params}, sort_keys=True)
    return hashlib.sha256(stable.encode("utf-8")).hexdigest()


def _read_cache(cache_file: Path) -> Optional[Dict[str, Any]]:
    if not cache_file.exists():
        return None
    age_sec = time.time() - cache_file.stat().st_mtime
    if age_sec > CACHE_TTL_SEC:
        return None
    try:
        return json.loads(cache_file.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None


def _write_cache(cache_file: Path, payload: Dict[str, Any]) -> None:
    try:
        CACHE_DIR.mkdir(parents=True, exist_ok=True)
        cache_file.write_text(json.dumps(payload), encoding="utf-8")
    except OSError:
        pass


def make_fred_request(endpoint: str, params: Dict[str, Any]) -> Dict[str, Any]:
    """Make cached FRED request and normalize common failures."""
    if not FRED_API_KEY:
        return _error(
            "FRED API key not configured (set FRED_API_KEY).",
            "FRED_AUTH_MISSING",
        )

    full_params = dict(params)
    full_params["api_key"] = FRED_API_KEY
    full_params["file_type"] = "json"

    cache_file = CACHE_DIR / f"{_cache_key(endpoint, full_params)}.json"
    cached = _read_cache(cache_file)
    if cached is not None:
        return cached

    url = f"{FRED_API_BASE}/{endpoint}"
    try:
        response = session.get(url, params=full_params, timeout=REQUEST_TIMEOUT_SEC)
        if response.status_code == 429:
            return _error("FRED API rate limit exceeded.", "FRED_RATE_LIMIT", status=429)
        response.raise_for_status()
        payload = response.json()
        if "error_code" in payload:
            code = str(payload.get("error_code", ""))
            if code == "400":
                msg = str(payload.get("error_message", "FRED request failed."))
                if "series" in msg.lower() and "not exist" in msg.lower():
                    return _error(msg, "FRED_SERIES_NOT_FOUND", status=400)
                return _error(msg, "FRED_BAD_REQUEST", status=400)
            return _error(
                str(payload.get("error_message", "FRED API error.")),
                "FRED_API_ERROR",
                status=payload.get("error_code"),
            )

        _write_cache(cache_file, payload)
        return payload
    except requests.exceptions.Timeout:
        return _error("FRED request timed out.", "FRED_TIMEOUT")
    except requests.exceptions.RequestException as exc:
        return _error(f"FRED request failed: {exc}", "FRED_REQUEST_FAILED")
    except ValueError as exc:
        return _error(f"Invalid FRED JSON response: {exc}", "FRED_BAD_JSON")


def _parse_date(date_str: str) -> datetime:
    return datetime.strptime(date_str, "%Y-%m-%d")


def _month_end(dt: datetime) -> datetime:
    last_day = calendar.monthrange(dt.year, dt.month)[1]
    return datetime(dt.year, dt.month, last_day)


def _first_of_month(dt: datetime) -> datetime:
    return datetime(dt.year, dt.month, 1)


def _add_months(dt: datetime, months: int) -> datetime:
    month = dt.month - 1 + months
    year = dt.year + month // 12
    month = month % 12 + 1
    day = min(dt.day, calendar.monthrange(year, month)[1])
    return datetime(year, month, day)


def _align_to_monthly(observations: List[Dict[str, Any]]) -> List[Dict[str, Any]]:
    """Forward-fill lower-frequency values to month-end points."""
    if len(observations) < 2:
        return observations

    parsed: List[Tuple[datetime, float]] = []
    for obs in observations:
        parsed.append((_parse_date(obs["date"]), float(obs["value"])))
    parsed.sort(key=lambda item: item[0])

    aligned: List[Dict[str, Any]] = []
    for idx, (current_date, value) in enumerate(parsed):
        next_date = parsed[idx + 1][0] if idx + 1 < len(parsed) else None
        current_month = _first_of_month(current_date)

        if next_date is None:
            aligned.append({"date": _month_end(current_date).strftime("%Y-%m-%d"), "value": value})
            continue

        end_limit = _first_of_month(next_date)
        month_cursor = current_month
        while month_cursor < end_limit:
            aligned.append({"date": _month_end(month_cursor).strftime("%Y-%m-%d"), "value": value})
            month_cursor = _add_months(month_cursor, 1)

    deduped: Dict[str, Dict[str, Any]] = {item["date"]: item for item in aligned}
    return [deduped[k] for k in sorted(deduped.keys())]


def get_series(
    series_id: str,
    start_date: Optional[str] = None,
    end_date: Optional[str] = None,
    target_frequency: Optional[str] = None,
    transform: Optional[str] = None,
) -> Dict[str, Any]:
    """Fetch one FRED series with metadata and optional frequency alignment."""
    params: Dict[str, Any] = {"series_id": series_id}
    if start_date:
        params["observation_start"] = start_date
    if end_date:
        params["observation_end"] = end_date
    if transform:
        params["units"] = transform

    obs_data = make_fred_request("series/observations", params)
    if obs_data.get("success") is False:
        obs_data["series_id"] = series_id
        return obs_data

    metadata = make_fred_request("series", {"series_id": series_id})
    if metadata.get("success") is False:
        metadata["series_id"] = series_id
        return metadata

    series_info = metadata.get("seriess", [{}])[0] if metadata.get("seriess") else {}
    source_frequency = str(series_info.get("frequency_short", series_info.get("frequency", ""))).lower()

    observations: List[Dict[str, Any]] = []
    for obs in obs_data.get("observations", []):
        raw = obs.get("value", ".")
        if raw in (".", "", None):
            continue
        try:
            value = float(raw)
        except (TypeError, ValueError):
            continue
        observations.append({"date": obs["date"], "value": value})

    if target_frequency == "m" and source_frequency.startswith(("q", "a", "sa")):
        observations = _align_to_monthly(observations)

    return {
        "success": True,
        "series_id": series_id,
        "title": series_info.get("title", "N/A"),
        "units": series_info.get("units", "N/A"),
        "source_frequency": series_info.get("frequency", "N/A"),
        "target_frequency": target_frequency or series_info.get("frequency_short", "N/A"),
        "seasonal_adjustment": series_info.get("seasonal_adjustment", "N/A"),
        "last_updated": series_info.get("last_updated", "N/A"),
        "observation_start": obs_data.get("observation_start", start_date),
        "observation_end": obs_data.get("observation_end", end_date),
        "observation_count": len(observations),
        "observations": observations,
        "latest": observations[-1] if observations else None,
    }


def get_latest_series_value(series_id: str, target_frequency: Optional[str], transform: Optional[str]) -> Dict[str, Any]:
    result = get_series(series_id, start_date=None, end_date=None, target_frequency=target_frequency, transform=transform)
    if result.get("success") is False:
        return result
    latest = result.get("latest")
    if latest is None:
        return _error("No observations returned for requested series.", "FRED_NO_DATA", series_id=series_id)
    return {
        "success": True,
        "series_id": series_id,
        "title": result.get("title", ""),
        "units": result.get("units", ""),
        "latest": latest,
        "observation_count": result.get("observation_count", 0),
    }


def search_series(search_text: str, limit: int = 10) -> Dict[str, Any]:
    params = {"search_text": search_text, "limit": limit, "order_by": "popularity", "sort_order": "desc"}
    result = make_fred_request("series/search", params)
    if result.get("success") is False:
        return result
    series_list = []
    for series in result.get("seriess", []):
        series_list.append(
            {
                "id": series.get("id"),
                "title": series.get("title"),
                "frequency": series.get("frequency"),
                "units": series.get("units"),
                "seasonal_adjustment": series.get("seasonal_adjustment"),
                "last_updated": series.get("last_updated"),
                "popularity": series.get("popularity"),
            }
        )
    return {"success": True, "search_text": search_text, "count": len(series_list), "series": series_list}


def get_categories(category_id: int = 0) -> Dict[str, Any]:
    result = make_fred_request("category/children", {"category_id": category_id})
    if result.get("success") is False:
        return result
    categories = [{"id": cat.get("id"), "name": cat.get("name"), "parent_id": cat.get("parent_id")} for cat in result.get("categories", [])]
    return {"success": True, "category_id": category_id, "categories": categories}


def get_category_series(category_id: int, limit: int = 50) -> Dict[str, Any]:
    params = {"category_id": category_id, "limit": limit, "order_by": "popularity", "sort_order": "desc"}
    result = make_fred_request("category/series", params)
    if result.get("success") is False:
        return result
    series_list = []
    for series in result.get("seriess", []):
        series_list.append(
            {
                "id": series.get("id"),
                "title": series.get("title"),
                "frequency": series.get("frequency"),
                "units": series.get("units"),
                "seasonal_adjustment": series.get("seasonal_adjustment"),
                "last_updated": series.get("last_updated"),
                "popularity": series.get("popularity", 0),
            }
        )
    return {"success": True, "category_id": category_id, "count": len(series_list), "series": series_list}


def get_release_dates(release_id: int, limit: int = 10) -> Dict[str, Any]:
    result = make_fred_request("release/dates", {"release_id": release_id, "limit": limit})
    if result.get("success") is False:
        return result
    dates = [{"release_id": date.get("release_id"), "date": date.get("date")} for date in result.get("release_dates", [])]
    return {"success": True, "release_id": release_id, "dates": dates}


def get_multiple_series(
    series_ids: List[str],
    start_date: Optional[str] = None,
    end_date: Optional[str] = None,
    target_frequency: Optional[str] = None,
    transform: Optional[str] = None,
) -> List[Dict[str, Any]]:
    results: List[Dict[str, Any]] = []
    if not series_ids:
        return results
    max_workers = min(5, len(series_ids))
    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        futures = {
            executor.submit(get_series, series_id, start_date, end_date, target_frequency, transform): series_id
            for series_id in series_ids
        }
        ordered: Dict[str, Dict[str, Any]] = {}
        for future in as_completed(futures):
            series_id = futures[future]
            try:
                ordered[series_id] = future.result()
            except Exception as exc:
                ordered[series_id] = _error(str(exc), "FRED_INTERNAL_ERROR", series_id=series_id)
    for sid in series_ids:
        results.append(ordered.get(sid, _error("Unknown error.", "FRED_INTERNAL_ERROR", series_id=sid)))
    return results


def _is_date(token: str) -> bool:
    try:
        _parse_date(token)
        return True
    except ValueError:
        return False


def _usage() -> Dict[str, Any]:
    return _error(
        (
            "Usage: fred_data.py <command> ... "
            "commands: series, latest, historical, multiple, search, categories, category_series, releases"
        ),
        "USAGE_ERROR",
    )


def main(args: Optional[List[str]] = None) -> None:
    args = args if args is not None else sys.argv[1:]
    if not args:
        print(json.dumps(_usage()))
        sys.exit(1)

    command = args[0]

    try:
        if command == "series":
            if len(args) < 2:
                print(json.dumps(_usage()))
                sys.exit(1)
            series_id = args[1]
            start_date = args[2] if len(args) > 2 else None
            end_date = args[3] if len(args) > 3 else None
            target_frequency = args[4] if len(args) > 4 else None
            transform = args[5] if len(args) > 5 else None
            result = get_series(series_id, start_date, end_date, target_frequency, transform)
            print(json.dumps(result))
            return

        if command == "latest":
            if len(args) < 2:
                print(json.dumps(_usage()))
                sys.exit(1)
            series_id = args[1]
            target_frequency = args[2] if len(args) > 2 else None
            transform = args[3] if len(args) > 3 else None
            result = get_latest_series_value(series_id, target_frequency, transform)
            print(json.dumps(result))
            return

        if command == "historical":
            if len(args) < 3:
                print(json.dumps(_usage()))
                sys.exit(1)
            series_id = args[1]
            start_date = args[2]
            end_date = args[3] if len(args) > 3 and _is_date(args[3]) else datetime.utcnow().strftime("%Y-%m-%d")
            target_frequency = args[4] if len(args) > 4 else None
            transform = args[5] if len(args) > 5 else None
            result = get_series(series_id, start_date, end_date, target_frequency, transform)
            print(json.dumps(result))
            return

        if command == "multiple":
            if len(args) < 2:
                print(json.dumps(_usage()))
                sys.exit(1)
            series_ids: List[str] = []
            start_date: Optional[str] = None
            end_date: Optional[str] = None
            for token in args[1:]:
                if _is_date(token):
                    if start_date is None:
                        start_date = token
                    else:
                        end_date = token
                else:
                    series_ids.append(token)
            result = get_multiple_series(series_ids, start_date, end_date)
            print(json.dumps(result))
            return

        if command == "search":
            if len(args) < 2:
                print(json.dumps(_usage()))
                sys.exit(1)
            search_text = args[1]
            limit = int(args[2]) if len(args) > 2 else 10
            print(json.dumps(search_series(search_text, limit)))
            return

        if command == "categories":
            category_id = int(args[1]) if len(args) > 1 else 0
            print(json.dumps(get_categories(category_id)))
            return

        if command == "category_series":
            if len(args) < 2:
                print(json.dumps(_usage()))
                sys.exit(1)
            category_id = int(args[1])
            limit = int(args[2]) if len(args) > 2 else 50
            print(json.dumps(get_category_series(category_id, limit)))
            return

        if command == "releases":
            if len(args) < 2:
                print(json.dumps(_usage()))
                sys.exit(1)
            release_id = int(args[1])
            limit = int(args[2]) if len(args) > 2 else 10
            print(json.dumps(get_release_dates(release_id, limit)))
            return

        print(json.dumps(_error(f"Unknown command: {command}", "USAGE_ERROR")))
        sys.exit(1)
    except Exception as exc:
        print(json.dumps(_error(f"Unexpected error: {exc}", "FRED_INTERNAL_ERROR")))
        sys.exit(1)


if __name__ == "__main__":
    main()
