"""
ENTSO-E Data Fetcher
ENTSO-E (European electricity grid): power generation, load, cross-border flows, capacity, prices.
"""
import sys
import json
import os
import re
import xml.etree.ElementTree as ET
from datetime import datetime, timedelta
import requests
from typing import Dict, Any, Optional, List

import fincept_connector as fc
import fincept_json

API_KEY = os.environ.get('ENTSOE_API_KEY', '')
BASE_URL = "https://web-api.tp.entsoe.eu/api"

SERVICE = "ENTSO-E Transparency Platform"
KEY_ENV = "ENTSOE_API_KEY"

session = fc.http_session()

_RESOLUTION = re.compile(r"^P(?:(\d+)D)?(?:T(?:(\d+)H)?(?:(\d+)M)?)?$")


def _local(tag: str) -> str:
    """Element tag without its XML namespace."""
    return tag.rsplit("}", 1)[-1]


def _child_text(elem, name: str) -> Optional[str]:
    for child in elem:
        if _local(child.tag) == name:
            return (child.text or "").strip()
    return None


def _step(resolution: str) -> Optional[timedelta]:
    m = _RESOLUTION.match(resolution or "")
    if not m or not any(m.groups()):
        return None  # e.g. P1M / P1Y: no fixed step
    d, h, mi = (int(g) if g else 0 for g in m.groups())
    return timedelta(days=d, hours=h, minutes=mi)


def _parse_xml(text: str) -> Any:
    """ENTSO-E answers with IEC 62325 XML market documents, not JSON: flatten them.

    An Acknowledgement_MarketDocument is the API's error reply (e.g. "No matching data found").
    Every other document becomes {"document": ..., "time_series": [{<scalar fields>, "periods":
    [{"start", "end", "resolution", "points": [{"position", "timestamp", "value"}]}]}]}.
    """
    try:
        root = ET.fromstring(text)
    except ET.ParseError:
        return {"error": f"{SERVICE} returned a response that is neither JSON nor XML"}
    doc_type = _local(root.tag)
    if doc_type == "Acknowledgement_MarketDocument":
        reasons = [(_child_text(r, "text") or _child_text(r, "code") or "")
                   for r in root if _local(r.tag) == "Reason"]
        return {"error": f"{SERVICE}: {'; '.join(x for x in reasons if x) or 'request rejected'}"}
    series = []
    for ts in root.iter():
        if _local(ts.tag) != "TimeSeries":
            continue
        entry: Dict[str, Any] = {}
        periods = []
        for child in ts:
            name = _local(child.tag)
            if name == "Period":
                interval = next((c for c in child if _local(c.tag) == "timeInterval"), None)
                start = _child_text(interval, "start") if interval is not None else None
                end = _child_text(interval, "end") if interval is not None else None
                resolution = _child_text(child, "resolution") or ""
                step = _step(resolution)
                try:
                    t0 = datetime.strptime(start, "%Y-%m-%dT%H:%MZ") if start else None
                except ValueError:
                    t0 = None
                points = []
                for pt in child:
                    if _local(pt.tag) != "Point":
                        continue
                    pos = _child_text(pt, "position")
                    raw = (_child_text(pt, "quantity") or _child_text(pt, "price.amount")
                           or _child_text(pt, "imbalance_Price.amount"))
                    try:
                        value = float(raw) if raw not in (None, "") else None
                    except ValueError:
                        value = None
                    ts_str = None
                    if t0 is not None and step is not None and pos and pos.isdigit():
                        ts_str = (t0 + step * (int(pos) - 1)).strftime("%Y-%m-%dT%H:%MZ")
                    points.append({"position": int(pos) if pos and pos.isdigit() else pos,
                                   "timestamp": ts_str, "value": value})
                periods.append({"start": start, "end": end, "resolution": resolution, "points": points})
            elif name == "MktPSRType":
                entry["psrType"] = _child_text(child, "psrType")
            elif len(child) == 0 and child.text and child.text.strip():
                entry[name] = child.text.strip()
        entry["periods"] = periods
        series.append(entry)
    return {"document": doc_type, "count": len(series), "time_series": series}


def _make_request(endpoint: str, params: Dict = None) -> Any:
    # Every Transparency Platform request needs a security token (HTTP 401 without one).
    if not API_KEY:
        return fc.missing_key(SERVICE, "https://transparency.entsoe.eu/content/static_content/Static%20content/"
                                       "web%20api/Guide.html", KEY_ENV, "a security token")
    # The API lives at /api exactly: "/api/" (what an empty endpoint used to produce) is a 404.
    if not endpoint:
        url = BASE_URL
    else:
        url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    if params is None:
        params = {}
    params["securityToken"] = API_KEY
    try:
        response = session.get(url, params=params, timeout=(10, 45))
        if response.status_code in (400, 401, 409) and "xml" in response.headers.get("Content-Type", ""):
            parsed = _parse_xml(response.text)
            if isinstance(parsed, dict) and "error" in parsed:
                if response.status_code == 401:
                    parsed["error"] += f" - check the token saved as {KEY_ENV} in Settings > Credentials"
                return parsed
        response.raise_for_status()
    except requests.exceptions.RequestException as e:
        return {"error": fc.request_error(SERVICE, e, KEY_ENV, API_KEY)}
    if "json" in response.headers.get("Content-Type", ""):
        try:
            return response.json()
        except ValueError:
            return {"error": f"{SERVICE} returned a response that is not JSON"}
    return _parse_xml(response.text)


def get_load(bidding_zone: str, start_date: str, end_date: str) -> Any:
    params = {"documentType": "A65", "processType": "A16", "outBiddingZone_Domain": bidding_zone, "periodStart": start_date, "periodEnd": end_date}
    return _make_request("", params)


def get_generation(bidding_zone: str, start_date: str, end_date: str) -> Any:
    params = {"documentType": "A75", "processType": "A16", "in_Domain": bidding_zone, "periodStart": start_date, "periodEnd": end_date}
    return _make_request("", params)


def get_day_ahead_prices(bidding_zone: str, start_date: str, end_date: str) -> Any:
    params = {"documentType": "A44", "in_Domain": bidding_zone, "out_Domain": bidding_zone, "periodStart": start_date, "periodEnd": end_date}
    return _make_request("", params)


def get_cross_border_flows(in_domain: str, out_domain: str, start_date: str, end_date: str) -> Any:
    params = {"documentType": "A11", "in_Domain": in_domain, "out_Domain": out_domain, "periodStart": start_date, "periodEnd": end_date}
    return _make_request("", params)


def get_installed_capacity(bidding_zone: str, year: str) -> Any:
    params = {"documentType": "A68", "processType": "A33", "in_Domain": bidding_zone, "periodStart": f"{year}01010000", "periodEnd": f"{year}12312300"}
    return _make_request("", params)


def get_balancing_data(bidding_zone: str, start_date: str, end_date: str) -> Any:
    params = {"documentType": "A81", "processType": "A16", "controlArea_Domain": bidding_zone, "periodStart": start_date, "periodEnd": end_date}
    return _make_request("", params)


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "load":
        bidding_zone = args[1] if len(args) > 1 else "10YDE-EON------1"
        start_date = args[2] if len(args) > 2 else "202401010000"
        end_date = args[3] if len(args) > 3 else "202401020000"
        result = get_load(bidding_zone, start_date, end_date)
    elif command == "generation":
        bidding_zone = args[1] if len(args) > 1 else "10YDE-EON------1"
        start_date = args[2] if len(args) > 2 else "202401010000"
        end_date = args[3] if len(args) > 3 else "202401020000"
        result = get_generation(bidding_zone, start_date, end_date)
    elif command == "prices":
        bidding_zone = args[1] if len(args) > 1 else "10YDE-EON------1"
        start_date = args[2] if len(args) > 2 else "202401010000"
        end_date = args[3] if len(args) > 3 else "202401020000"
        result = get_day_ahead_prices(bidding_zone, start_date, end_date)
    elif command == "flows":
        in_domain = args[1] if len(args) > 1 else "10YDE-EON------1"
        out_domain = args[2] if len(args) > 2 else "10YFR-RTE------C"
        start_date = args[3] if len(args) > 3 else "202401010000"
        end_date = args[4] if len(args) > 4 else "202401020000"
        result = get_cross_border_flows(in_domain, out_domain, start_date, end_date)
    elif command == "capacity":
        bidding_zone = args[1] if len(args) > 1 else "10YDE-EON------1"
        year = args[2] if len(args) > 2 else "2024"
        result = get_installed_capacity(bidding_zone, year)
    elif command == "balancing":
        bidding_zone = args[1] if len(args) > 1 else "10YDE-EON------1"
        start_date = args[2] if len(args) > 2 else "202401010000"
        end_date = args[3] if len(args) > 3 else "202401020000"
        result = get_balancing_data(bidding_zone, start_date, end_date)
    print(fincept_json.dumps(result))


if __name__ == "__main__":
    main()
