"""
FRED Data Fetcher
Fetches economic data from Federal Reserve Economic Data (FRED)
Returns JSON output for Rust integration
Based on OpenBB FRED provider

Optimized with concurrent fetching for multiple series
"""

import sys
import json
import os
import requests
from datetime import datetime
from typing import Optional, List, Dict, Any
from concurrent.futures import ThreadPoolExecutor, as_completed

FRED_API_BASE = "https://api.stlouisfed.org/fred"
FRED_API_KEY = os.environ.get('FRED_API_KEY', '')  # API key from environment variable

# Connection pool for better performance
session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)


def make_fred_request(endpoint: str, params: Dict[str, Any]) -> Dict:
    """Make request to FRED API using connection pool"""
    if not FRED_API_KEY:
        return {"error": "FRED API key not configured"}

    params['api_key'] = FRED_API_KEY
    params['file_type'] = 'json'

    url = f"{FRED_API_BASE}/{endpoint}"
    try:
        response = session.get(url, params=params, timeout=15)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        return {"error": str(e)}


def get_series(series_id: str, start_date: Optional[str] = None, end_date: Optional[str] = None,
               frequency: Optional[str] = None, transform: Optional[str] = None) -> Dict:
    """
    Fetch FRED series data

    Args:
        series_id: FRED series ID (e.g., 'GDP', 'UNRATE', 'CPIAUCSL')
        start_date: Start date YYYY-MM-DD
        end_date: End date YYYY-MM-DD
        frequency: a=Annual, q=Quarterly, m=Monthly, w=Weekly, d=Daily
        transform: chg=Change, pch=Percent Change, log=Natural Log
    """
    try:
        # Get series observations
        params = {'series_id': series_id}
        if start_date:
            params['observation_start'] = start_date
        if end_date:
            params['observation_end'] = end_date
        if frequency:
            params['frequency'] = frequency
        if transform:
            params['units'] = transform

        obs_data = make_fred_request('series/observations', params)

        if 'error' in obs_data:
            return obs_data

        # Get series metadata
        metadata = make_fred_request('series', {'series_id': series_id})

        if 'error' in metadata:
            return obs_data  # Return observations even if metadata fails

        series_info = metadata.get('seriess', [{}])[0] if 'seriess' in metadata else {}

        # Format observations
        observations = []
        for obs in obs_data.get('observations', []):
            if obs['value'] != '.':  # Skip missing values
                observations.append({
                    'date': obs['date'],
                    'value': float(obs['value'])
                })

        return {
            'series_id': series_id,
            'title': series_info.get('title', 'N/A'),
            'units': series_info.get('units', 'N/A'),
            'frequency': series_info.get('frequency', 'N/A'),
            'seasonal_adjustment': series_info.get('seasonal_adjustment', 'N/A'),
            'last_updated': series_info.get('last_updated', 'N/A'),
            'observations': observations,
            'observation_count': len(observations)
        }
    except Exception as e:
        return {"error": str(e), "series_id": series_id}


def search_series(search_text: str, limit: int = 10) -> Dict:
    """Search for FRED series"""
    try:
        params = {
            'search_text': search_text,
            'limit': limit,
            'order_by': 'popularity',
            'sort_order': 'desc'
        }

        result = make_fred_request('series/search', params)

        if 'error' in result:
            return result

        series_list = []
        for series in result.get('seriess', []):
            series_list.append({
                'id': series.get('id'),
                'title': series.get('title'),
                'frequency': series.get('frequency'),
                'units': series.get('units'),
                'seasonal_adjustment': series.get('seasonal_adjustment'),
                'last_updated': series.get('last_updated'),
                'popularity': series.get('popularity')
            })

        return {
            'search_text': search_text,
            'count': len(series_list),
            'series': series_list
        }
    except Exception as e:
        return {"error": str(e), "search_text": search_text}


def get_categories(category_id: int = 0) -> Dict:
    """Get FRED categories"""
    try:
        params = {'category_id': category_id}
        result = make_fred_request('category/children', params)

        if 'error' in result:
            return result

        categories = []
        for cat in result.get('categories', []):
            categories.append({
                'id': cat.get('id'),
                'name': cat.get('name'),
                'parent_id': cat.get('parent_id')
            })

        return {
            'category_id': category_id,
            'categories': categories
        }
    except Exception as e:
        return {"error": str(e), "category_id": category_id}


def get_category_series(category_id: int, limit: int = 50) -> Dict:
    """Get series in a category"""
    try:
        params = {
            'category_id': category_id,
            'limit': limit,
            'order_by': 'popularity',
            'sort_order': 'desc'
        }
        result = make_fred_request('category/series', params)

        if 'error' in result:
            return result

        series_list = []
        for series in result.get('seriess', []):
            series_list.append({
                'id': series.get('id'),
                'title': series.get('title'),
                'frequency': series.get('frequency'),
                'units': series.get('units'),
                'seasonal_adjustment': series.get('seasonal_adjustment'),
                'last_updated': series.get('last_updated'),
                'popularity': series.get('popularity', 0)
            })

        return {
            'category_id': category_id,
            'count': len(series_list),
            'series': series_list
        }
    except Exception as e:
        return {"error": str(e), "category_id": category_id}


def get_release_dates(release_id: int, limit: int = 10) -> Dict:
    """Get FRED release dates"""
    try:
        params = {
            'release_id': release_id,
            'limit': limit
        }

        result = make_fred_request('release/dates', params)

        if 'error' in result:
            return result

        dates = []
        for date in result.get('release_dates', []):
            dates.append({
                'release_id': date.get('release_id'),
                'date': date.get('date')
            })

        return {
            'release_id': release_id,
            'dates': dates
        }
    except Exception as e:
        return {"error": str(e), "release_id": release_id}


def get_multiple_series(series_ids: List[str], start_date: Optional[str] = None,
                        end_date: Optional[str] = None) -> List[Dict]:
    """Fetch multiple FRED series concurrently for better performance"""
    results = []

    # Use ThreadPoolExecutor for concurrent fetching
    # Limit to 5 concurrent requests to avoid rate limiting
    max_workers = min(5, len(series_ids))

    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        # Submit all tasks
        future_to_series = {
            executor.submit(get_series, series_id, start_date, end_date): series_id
            for series_id in series_ids
        }

        # Collect results as they complete
        results_dict = {}
        for future in as_completed(future_to_series):
            series_id = future_to_series[future]
            try:
                result = future.result()
                results_dict[series_id] = result
            except Exception as e:
                results_dict[series_id] = {"error": str(e), "series_id": series_id}

    # Return results in original order
    for series_id in series_ids:
        results.append(results_dict.get(series_id, {"error": "Unknown error", "series_id": series_id}))

    return results


def main(args=None):
    # Support both PyO3 (args parameter) and subprocess (sys.argv)
    if args is None:
        args = sys.argv[1:]

    if len(args) < 1:
        print(json.dumps({"error": "Usage: python fred_data.py <command> <args>"}))
        sys.exit(1)

    command = args[0]

    if command == "series":
        if len(args) < 2:
            print(json.dumps({"error": "Usage: python fred_data.py series <series_id> [start_date] [end_date] [frequency] [transform]"}))
            sys.exit(1)

        series_id = args[1]
        start_date = args[2] if len(args) > 2 else None
        end_date = args[3] if len(args) > 3 else None
        frequency = args[4] if len(args) > 4 else None
        transform = args[5] if len(args) > 5 else None

        result = get_series(series_id, start_date, end_date, frequency, transform)
        print(json.dumps(result, indent=2))

    elif command == "multiple":
        if len(args) < 2:
            print(json.dumps({"error": "Usage: python fred_data.py multiple <series_id1> <series_id2> ... [start_date] [end_date]"}))
            sys.exit(1)

        # Find where dates start (if they exist)
        series_ids = []
        start_date = None
        end_date = None

        for i in range(1, len(args)):
            arg = args[i]
            if '-' in arg and len(arg) == 10:  # Date format YYYY-MM-DD
                if start_date is None:
                    start_date = arg
                else:
                    end_date = arg
            else:
                series_ids.append(arg)

        result = get_multiple_series(series_ids, start_date, end_date)
        print(json.dumps(result, indent=2))

    elif command == "search":
        if len(args) < 2:
            print(json.dumps({"error": "Usage: python fred_data.py search <search_text> [limit]"}))
            sys.exit(1)

        search_text = args[1]
        limit = int(args[2]) if len(args) > 2 else 10

        result = search_series(search_text, limit)
        print(json.dumps(result, indent=2))

    elif command == "categories":
        category_id = int(args[1]) if len(args) > 1 else 0
        result = get_categories(category_id)
        print(json.dumps(result, indent=2))

    elif command == "category_series":
        if len(args) < 2:
            print(json.dumps({"error": "Usage: python fred_data.py category_series <category_id> [limit]"}))
            sys.exit(1)

        category_id = int(args[1])
        limit = int(args[2]) if len(args) > 2 else 50

        result = get_category_series(category_id, limit)
        print(json.dumps(result, indent=2))

    elif command == "releases":
        if len(args) < 2:
            print(json.dumps({"error": "Usage: python fred_data.py releases <release_id> [limit]"}))
            sys.exit(1)

        release_id = int(args[1])
        limit = int(args[2]) if len(args) > 2 else 10

        result = get_release_dates(release_id, limit)
        print(json.dumps(result, indent=2))

    else:
        print(json.dumps({"error": f"Unknown command: {command}"}))
        sys.exit(1)


if __name__ == "__main__":
    main()
