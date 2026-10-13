"""
UN Comtrade Extended Data Fetcher
Detailed bilateral trade flows by HS chapter, trade balances, and services trade
via the UN Comtrade public preview API (500 records per call, no key required).
"""
import sys
import json
import os
import time
import requests
from datetime import datetime
from typing import Dict, Any

API_KEY = os.environ.get('COMTRADE_API_KEY', '')
BASE_URL = "https://comtradeapi.un.org/public/v1"
# Reference lists moved from /public/v1/references/* to static files.
REF_URL = "https://comtradeapi.un.org/files/v1/app/reference"
DEFAULT_YEAR = str(datetime.now().year - 2)  # annual data lags ~1-2 years

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        headers = {}
        if API_KEY:
            headers["Ocp-Apim-Subscription-Key"] = API_KEY
        for attempt in range(4):
            response = session.get(url, params=params, headers=headers, timeout=30)
            if response.status_code != 429 or attempt == 3:
                break
            time.sleep(3 + attempt * 2)  # public preview is rate-limited to ~1 call / few seconds
        if response.status_code == 400:
            try:
                det = response.json()
                return {"error": f"Invalid request: {det.get('error')} {det.get('details', '')}"}
            except ValueError:
                pass
        response.raise_for_status()
        data = response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}
    if isinstance(data, dict) and isinstance(data.get("data"), list):
        if data.get("error"):
            return {"error": data["error"]}
        return {"count": len(data["data"]), "data": [_slim(r) for r in data["data"]]}
    return data


def _slim(r: Dict) -> Dict:
    keep = ("period", "reporterISO", "reporterDesc", "flowCode", "flowDesc", "partnerISO", "partnerDesc",
            "cmdCode", "cmdDesc", "primaryValue", "netWgt", "qty", "qtyUnitAbbr")
    return {k: r.get(k) for k in keep if k in r}


_REPORTERS = None


def _reporter_code(code: str) -> str:
    """Accept ISO-3 / ISO-2 codes as well as the numeric M49 codes the API expects."""
    global _REPORTERS
    c = (code or "").strip()
    if not c or c.isdigit() or c.lower() == "all" or "," in c:
        return c
    if _REPORTERS is None:
        try:
            _REPORTERS = requests.get(f"{REF_URL}/Reporters.json", timeout=30).json().get("results", [])
        except (requests.exceptions.RequestException, ValueError):
            _REPORTERS = []
    # prefer current entries (e.g. DEU 276 over the historical Fed. Rep. of Germany 280)
    for r in sorted(_REPORTERS, key=lambda x: x.get("entryExpiredDate") is not None):
        if c.upper() in (r.get("reporterCodeIsoAlpha3"), r.get("reporterCodeIsoAlpha2")):
            return str(r.get("reporterCode"))
    return c


def _ref(name: str) -> Any:
    try:
        response = session.get(f"{REF_URL}/{name}", timeout=60)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def get_trade_data(reporter: str, partner: str = "0", commodity: str = "TOTAL",
                   year: str = None, flow: str = "X") -> Any:
    params = {
        "reporterCode": _reporter_code(reporter),
        "partnerCode": _reporter_code(partner),
        "cmdCode": commodity,
        "flowCode": flow,
        "period": year or DEFAULT_YEAR,
        "motCode": "0",
        "partner2Code": "0",
        "breakdownMode": "classic",
        "includeDesc": "true",
    }
    return _make_request("preview/C/A/HS", params)


def get_trade_balance(reporter: str, year: str = None) -> Any:
    params = {
        "reporterCode": _reporter_code(reporter),
        "partnerCode": "0",
        "cmdCode": "TOTAL",
        "flowCode": "X,M",
        "period": year or DEFAULT_YEAR,
        "motCode": "0",
        "partner2Code": "0",
        "breakdownMode": "classic",
        "includeDesc": "true",
    }
    return _make_request("preview/C/A/HS", params)


def _rank(res: Any, top: int = 50) -> Any:
    """Sort rows by trade value (largest first), drop the World aggregate, keep the top N."""
    if isinstance(res, dict) and isinstance(res.get("data"), list):
        rows = [r for r in res["data"] if r.get("partnerISO") != "W00" or r.get("cmdCode") != "TOTAL"]
        rows.sort(key=lambda r: r.get("primaryValue") or 0, reverse=True)
        res = {"count": len(rows), "data": rows[:top]}
        if len(rows) > top:
            res["note"] = f"top {top} of {len(rows)} rows by value"
    return res


def get_top_partners(reporter: str, year: str = None, flow: str = "X") -> Any:
    params = {
        "reporterCode": _reporter_code(reporter),
        "cmdCode": "TOTAL",
        "flowCode": flow,
        "period": year or DEFAULT_YEAR,
        "motCode": "0",
        "partner2Code": "0",
        "breakdownMode": "classic",
        "includeDesc": "true",
    }
    return _rank(_make_request("preview/C/A/HS", params))


def get_top_commodities(reporter: str, year: str = None, flow: str = "X") -> Any:
    params = {
        "reporterCode": _reporter_code(reporter),
        "partnerCode": "0",
        "cmdCode": "AG2",
        "flowCode": flow,
        "period": year or DEFAULT_YEAR,
        "motCode": "0",
        "partner2Code": "0",
        "breakdownMode": "classic",
        "includeDesc": "true",
    }
    return _rank(_make_request("preview/C/A/HS", params))


def get_reporters() -> Any:
    data = _ref("Reporters.json")
    if isinstance(data, dict) and "results" in data:
        rows = [{"code": r.get("reporterCode"), "iso3": r.get("reporterCodeIsoAlpha3"), "name": r.get("text")}
                for r in data["results"] if not r.get("isGroup")]
        return {"count": len(rows), "reporters": rows}
    return data


def get_commodities(hs_chapter: str = None) -> Any:
    """HS codes; hs_chapter filters by code prefix (e.g. 27) or by text."""
    data = _ref("HS.json")
    if isinstance(data, dict) and "results" in data:
        rows = [{"code": r.get("id"), "text": r.get("text")} for r in data["results"]]
        if hs_chapter:
            q = hs_chapter.lower()
            rows = [r for r in rows if str(r["code"]).startswith(hs_chapter) or q in (r["text"] or "").lower()]
        else:
            rows = [r for r in rows if len(str(r["code"])) == 2]  # chapters only
        return {"count": len(rows), "commodities": rows[:500]}
    return data


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "trade":
        if len(args) < 2:
            result = {"error": "reporter required"}
        else:
            partner = args[2] if len(args) > 2 else "0"
            commodity = args[3] if len(args) > 3 else "TOTAL"
            year = args[4] if len(args) > 4 else None
            flow = args[5] if len(args) > 5 else "X"
            result = get_trade_data(args[1], partner, commodity, year, flow)
    elif command == "balance":
        if len(args) < 2:
            result = {"error": "reporter required"}
        else:
            year = args[2] if len(args) > 2 else None
            result = get_trade_balance(args[1], year)
    elif command == "top_partners":
        if len(args) < 2:
            result = {"error": "reporter required"}
        else:
            year = args[2] if len(args) > 2 else None
            flow = args[3] if len(args) > 3 else "X"
            result = get_top_partners(args[1], year, flow)
    elif command == "top_commodities":
        if len(args) < 2:
            result = {"error": "reporter required"}
        else:
            year = args[2] if len(args) > 2 else None
            flow = args[3] if len(args) > 3 else "X"
            result = get_top_commodities(args[1], year, flow)
    elif command == "reporters":
        result = get_reporters()
    elif command == "commodities":
        hs_chapter = args[1] if len(args) > 1 else None
        result = get_commodities(hs_chapter)
    print(json.dumps(result))


if __name__ == "__main__":
    main()
