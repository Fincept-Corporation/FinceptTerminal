"""
Public APIs Finance Data Fetcher
Aggregated open financial APIs: NBP Poland, Croatian National Bank, Czech National Bank — no key required.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('PUBLIC_APIS_FINANCE_KEY', '')
BASE_URL = "https://api.nbp.pl/api"

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

def get_nbp_rates(table: str = "A", code: str = None) -> Any:
    if code:
        return _make_request(f"exchangerates/rates/{table}/{code}/?format=json")
    return _make_request(f"exchangerates/tables/{table}/?format=json")

def get_nbp_gold_price(count: int = 10) -> Any:
    return _make_request(f"cenyzlota/last/{count}/?format=json")

def get_cnb_rates(date: str = None) -> Any:
    url = "https://www.cnb.cz/en/financial-markets/foreign-exchange-market/central-bank-exchange-rate-fixing/central-bank-exchange-rate-fixing/daily.txt"
    params = {}
    if date:
        params["date"] = date
    try:
        headers = {"Accept": "text/plain"}
        response = session.get(url, params=params, headers=headers, timeout=30)
        response.raise_for_status()
        lines = response.text.strip().split("\n")
        rates = []
        for line in lines[2:]:
            parts = line.split("|")
            if len(parts) >= 5:
                rates.append({"country": parts[0], "currency": parts[1], "amount": parts[2], "code": parts[3], "rate": parts[4]})
        return {"date": lines[0] if lines else "", "rates": rates}
    except Exception as e:
        return {"error": f"Request failed: {str(e)}"}

def get_hnb_rates(date: str = None) -> Any:
    url = "https://api.hnb.hr/tecajn-eur/v3"
    params = {}
    if date:
        params["datum-primjene"] = date
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}

def get_mnb_rates(date: str = None) -> Any:
    url = "https://api.mnb.hu/arfolyamok.asmx"
    soap_body = f"""<?xml version="1.0" encoding="utf-8"?>
<soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/" xmlns:web="http://www.mnb.hu/webservices/">
  <soap:Body><web:GetCurrentExchangeRates/></soap:Body>
</soap:Envelope>"""
    try:
        headers = {"Content-Type": "text/xml; charset=utf-8", "SOAPAction": "http://www.mnb.hu/webservices/MNBArfolyamServiceSoap/GetCurrentExchangeRates"}
        response = session.post(url, data=soap_body, headers=headers, timeout=30)
        return {"raw_xml": response.text[:2000], "status": response.status_code}
    except Exception as e:
        return {"error": f"Request failed: {str(e)}"}

def get_boi_rates() -> Any:
    url = "https://www.bankofisrael.org.il/api/current-exchange-rates"
    try:
        response = session.get(url, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}

def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "nbp":
        table = args[1] if len(args) > 1 else "A"
        code = args[2] if len(args) > 2 else None
        result = get_nbp_rates(table, code)
    elif command == "nbp_gold":
        count = int(args[1]) if len(args) > 1 else 10
        result = get_nbp_gold_price(count)
    elif command == "cnb":
        date = args[1] if len(args) > 1 else None
        result = get_cnb_rates(date)
    elif command == "hnb":
        date = args[1] if len(args) > 1 else None
        result = get_hnb_rates(date)
    elif command == "mnb":
        date = args[1] if len(args) > 1 else None
        result = get_mnb_rates(date)
    elif command == "boi":
        result = get_boi_rates()
    print(json.dumps(result))

if __name__ == "__main__":
    main()
