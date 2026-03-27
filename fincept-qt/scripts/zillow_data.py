"""
Zillow Research Data Fetcher
Zillow Research: median home values, rents, inventory, price cuts by zip/metro/state
via direct CSV download and parse from Zillow static files.
"""
import sys
import json
import os
import io
import requests
from typing import Dict, Any, Optional, List

BASE_URL = "https://files.zillowstatic.com/research/public_csvs"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

DATASETS = {
    "zhvi_zip":       "zhvi/Zip_zhvi_uc_sfrcondo_tier_0.33_0.67_sm_sa_month.csv",
    "zhvi_metro":     "zhvi/Metro_zhvi_uc_sfrcondo_tier_0.33_0.67_sm_sa_month.csv",
    "zhvi_state":     "zhvi/State_zhvi_uc_sfrcondo_tier_0.33_0.67_sm_sa_month.csv",
    "zori_zip":       "zori/Zip_ZORI_AllHomesPlusMultifamily_Smoothed.csv",
    "zori_metro":     "zori/Metro_ZORI_AllHomesPlusMultifamily_Smoothed.csv",
    "inventory_zip":  "market_summary/Zip_invt_fs_uc_sfrcondo_sm_month.csv",
    "inventory_metro":"market_summary/Metro_invt_fs_uc_sfrcondo_sm_month.csv",
    "dom_zip":        "market_summary/Zip_median_dom_uc_sfrcondo_sm_month.csv",
    "dom_metro":      "market_summary/Metro_median_dom_uc_sfrcondo_sm_month.csv",
    "price_cut_zip":  "market_summary/Zip_perc_listings_price_cut_uc_sfrcondo_sm_month.csv",
    "price_cut_metro":"market_summary/Metro_perc_listings_price_cut_uc_sfrcondo_sm_month.csv",
    "zori_1br":       "zori/Metro_ZORI_AllHomesPlusMultifamily_1Bedroom_Smoothed.csv",
    "zori_2br":       "zori/Metro_ZORI_AllHomesPlusMultifamily_2Bedroom_Smoothed.csv",
    "zori_3br":       "zori/Metro_ZORI_AllHomesPlusMultifamily_3Bedroom_Smoothed.csv",
    "zori_4br":       "zori/Metro_ZORI_AllHomesPlusMultifamily_4Bedroom_Smoothed.csv",
    "zori_5br":       "zori/Metro_ZORI_AllHomesPlusMultifamily_5BedroomOrMore_Smoothed.csv",
}


def _fetch_csv(path: str) -> Any:
    url = f"{BASE_URL}/{path}"
    try:
        response = session.get(url, timeout=60)
        response.raise_for_status()
        return response.text
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}


def _parse_csv_to_records(csv_text: str, region_filter: str = None,
                           region_col: str = "RegionName",
                           start_date: str = None, end_date: str = None) -> Any:
    try:
        lines = csv_text.strip().split('\n')
        if not lines:
            return {"error": "Empty CSV"}
        headers = [h.strip().strip('"') for h in lines[0].split(',')]
        records = []
        for line in lines[1:]:
            parts = line.split(',')
            if len(parts) < len(headers):
                continue
            row = {headers[i]: parts[i].strip().strip('"') for i in range(len(headers))}
            if region_filter:
                col_val = row.get(region_col, row.get("RegionName", ""))
                if region_filter.lower() not in col_val.lower():
                    continue
            # filter date columns
            date_cols = {}
            for k, v in row.items():
                if len(k) == 10 and k[4] == '-' and k[7] == '-':
                    if start_date and k < start_date:
                        continue
                    if end_date and k > end_date:
                        continue
                    date_cols[k] = v
            meta = {k: v for k, v in row.items() if not (len(k) == 10 and k[4] == '-')}
            meta["time_series"] = date_cols
            records.append(meta)
        return records
    except Exception as e:
        return {"error": f"Parse error: {str(e)}"}


def get_home_value_index(region_type: str = "metro", start_date: str = None, end_date: str = None) -> Any:
    key = f"zhvi_{region_type.lower()}"
    if key not in DATASETS:
        return {"error": f"Unknown region_type '{region_type}'. Use: zip, metro, state"}
    csv_text = _fetch_csv(DATASETS[key])
    if isinstance(csv_text, dict):
        return csv_text
    records = _parse_csv_to_records(csv_text, start_date=start_date, end_date=end_date)
    return {"region_type": region_type, "dataset": "ZHVI", "count": len(records), "data": records[:50]}


def get_rental_index(region_type: str = "metro", bedroom_size: str = "all") -> Any:
    bedroom_map = {"all": "zori_metro", "1": "zori_1br", "2": "zori_2br",
                   "3": "zori_3br", "4": "zori_4br", "5": "zori_5br"}
    if bedroom_size == "all" and region_type.lower() == "zip":
        key = "zori_zip"
    else:
        key = bedroom_map.get(str(bedroom_size), "zori_metro")
    if key not in DATASETS:
        return {"error": f"Unknown bedroom_size '{bedroom_size}'"}
    csv_text = _fetch_csv(DATASETS[key])
    if isinstance(csv_text, dict):
        return csv_text
    records = _parse_csv_to_records(csv_text)
    return {"region_type": region_type, "bedroom_size": bedroom_size, "dataset": "ZORI",
            "count": len(records), "data": records[:50]}


def get_inventory(region_type: str = "metro") -> Any:
    key = f"inventory_{region_type.lower()}"
    if key not in DATASETS:
        return {"error": f"Unknown region_type '{region_type}'. Use: zip, metro"}
    csv_text = _fetch_csv(DATASETS[key])
    if isinstance(csv_text, dict):
        return csv_text
    records = _parse_csv_to_records(csv_text)
    return {"region_type": region_type, "dataset": "Inventory", "count": len(records), "data": records[:50]}


def get_days_on_market(region_type: str = "metro") -> Any:
    key = f"dom_{region_type.lower()}"
    if key not in DATASETS:
        return {"error": f"Unknown region_type '{region_type}'. Use: zip, metro"}
    csv_text = _fetch_csv(DATASETS[key])
    if isinstance(csv_text, dict):
        return csv_text
    records = _parse_csv_to_records(csv_text)
    return {"region_type": region_type, "dataset": "Days On Market", "count": len(records), "data": records[:50]}


def get_price_cut_pct(region_type: str = "metro") -> Any:
    key = f"price_cut_{region_type.lower()}"
    if key not in DATASETS:
        return {"error": f"Unknown region_type '{region_type}'. Use: zip, metro"}
    csv_text = _fetch_csv(DATASETS[key])
    if isinstance(csv_text, dict):
        return csv_text
    records = _parse_csv_to_records(csv_text)
    return {"region_type": region_type, "dataset": "Price Cut %", "count": len(records), "data": records[:50]}


def get_available_datasets() -> Any:
    return {
        "datasets": list(DATASETS.keys()),
        "base_url": BASE_URL,
        "description": "Zillow Research public CSV datasets",
        "region_types": ["zip", "metro", "state"],
        "bedroom_sizes": ["all", "1", "2", "3", "4", "5"]
    }


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "home_values":
        region_type = args[1] if len(args) > 1 else "metro"
        start_date = args[2] if len(args) > 2 else None
        end_date = args[3] if len(args) > 3 else None
        result = get_home_value_index(region_type, start_date, end_date)
    elif command == "rental":
        region_type = args[1] if len(args) > 1 else "metro"
        bedroom_size = args[2] if len(args) > 2 else "all"
        result = get_rental_index(region_type, bedroom_size)
    elif command == "inventory":
        region_type = args[1] if len(args) > 1 else "metro"
        result = get_inventory(region_type)
    elif command == "days_on_market":
        region_type = args[1] if len(args) > 1 else "metro"
        result = get_days_on_market(region_type)
    elif command == "price_cuts":
        region_type = args[1] if len(args) > 1 else "metro"
        result = get_price_cut_pct(region_type)
    elif command == "datasets":
        result = get_available_datasets()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
