"""
EconDB Data Fetcher
Fetches global economic indicators from EconDB (econdb.com)
Returns JSON output for Rust integration
Based on OpenBB EconDB provider
"""

import sys
import json
import asyncio
from datetime import datetime, timedelta
from typing import Optional, List, Dict, Any
import aiohttp

# EconDB API Configuration
ECONDB_BASE_URL = "https://www.econdb.com"
ECONDB_API_BASE = f"{ECONDB_BASE_URL}/api/series"
ECONDB_CONTEXT_BASE = f"{ECONDB_BASE_URL}/series/context"
ECONDB_TRENDS_BASE = f"{ECONDB_BASE_URL}/trends"

# Country mapping (ISO 2-letter codes)
COUNTRIES = {
    "us": "US", "usa": "US", "united_states": "US",
    "uk": "UK", "united_kingdom": "UK", "gb": "UK",
    "cn": "CN", "china": "CN",
    "jp": "JP", "japan": "JP",
    "de": "DE", "germany": "DE",
    "fr": "FR", "france": "FR",
    "in": "IN", "india": "IN",
    "ca": "CA", "canada": "CA",
    "au": "AU", "australia": "AU",
    "br": "BR", "brazil": "BR",
    "mx": "MX", "mexico": "MX",
    "kr": "KR", "south_korea": "KR",
    "it": "IT", "italy": "IT",
    "es": "ES", "spain": "ES",
    "nl": "NL", "netherlands": "NL",
    "ch": "CH", "switzerland": "CH",
    "se": "SE", "sweden": "SE",
    "pl": "PL", "poland": "PL",
    "be": "BE", "belgium": "BE",
    "at": "AT", "austria": "AT",
    "no": "NO", "norway": "NO",
    "dk": "DK", "denmark": "DK",
    "fi": "FI", "finland": "FI",
    "pt": "PT", "portugal": "PT",
    "gr": "GR", "greece": "GR",
    "cz": "CZ", "czech_republic": "CZ", "czechia": "CZ",
    "ie": "IE", "ireland": "IE",
    "nz": "NZ", "new_zealand": "NZ",
    "sg": "SG", "singapore": "SG",
    "hk": "HK", "hong_kong": "HK",
    "ru": "RU", "russia": "RU",
    "tr": "TR", "turkey": "TR",
    "za": "ZA", "south_africa": "ZA",
    "ar": "AR", "argentina": "AR",
    "cl": "CL", "chile": "CL",
    "co": "CO", "colombia": "CO",
    "pe": "PE", "peru": "PE",
    "id": "ID", "indonesia": "ID",
    "th": "TH", "thailand": "TH",
    "my": "MY", "malaysia": "MY",
    "ph": "PH", "philippines": "PH",
    "vn": "VN", "vietnam": "VN",
    "eg": "EG", "egypt": "EG",
    "ng": "NG", "nigeria": "NG",
    "pk": "PK", "pakistan": "PK",
    "bd": "BD", "bangladesh": "BD",
    "ua": "UA", "ukraine": "UA",
    "ro": "RO", "romania": "RO",
    "hu": "HU", "hungary": "HU",
}

# Common economic indicators
INDICATORS = {
    "gdp": "GDP",           # GDP (nominal)
    "rgdp": "RGDP",         # GDP (real)
    "cpi": "CPI",           # Consumer Price Index
    "ppi": "PPI",           # Producer Price Index
    "core": "CORE",         # Core CPI
    "urate": "URATE",       # Unemployment Rate
    "emp": "EMP",           # Employment
    "ip": "IP",             # Industrial Production
    "reta": "RETA",         # Retail Sales
    "conf": "CONF",         # Consumer Confidence
    "polir": "POLIR",       # Policy Interest Rate
    "y10yd": "Y10YD",       # 10-Year Government Bond Yield
    "m3yd": "M3YD",         # 3-Month Treasury Yield
    "gdebt": "GDEBT",       # Government Debt
    "ca": "CA",             # Current Account
    "tb": "TB",             # Trade Balance
    "pop": "POP",           # Population
    "prc": "PRC",           # Private Consumption
    "gfcf": "GFCF",         # Gross Fixed Capital Formation
    "exp": "EXP",           # Exports
    "imp": "IMP",           # Imports
}

# Transformations
TRANSFORMATIONS = {
    "toya": "Change from one year ago (YoY)",
    "tpop": "Change from previous period",
    "tusd": "Values as US dollars",
    "tpgp": "Values as a percent of GDP",
    "level": "Level (no transformation)",
}

async def create_temp_token() -> str:
    """Create a temporary API token from EconDB"""
    url = f"{ECONDB_BASE_URL}/user/create_token/?reset=0"
    try:
        async with aiohttp.ClientSession() as session:
            async with session.get(url, timeout=10) as response:
                if response.status == 200:
                    data = await response.json()
                    return data.get("api_key", "")
                return ""
    except Exception as e:
        return ""

async def make_econdb_request(url: str, token: Optional[str] = None) -> Dict:
    """Make async request to EconDB API"""
    try:
        async with aiohttp.ClientSession() as session:
            # Add token to URL if provided
            request_url = f"{url}&token={token}" if token and "?" in url else url
            async with session.get(request_url, timeout=20) as response:
                if response.status == 200:
                    return await response.json()
                else:
                    return {"error": f"HTTP {response.status}: {response.reason}"}
    except asyncio.TimeoutError:
        return {"error": "Request timeout"}
    except Exception as e:
        return {"error": str(e)}

def normalize_country(country: str) -> Optional[str]:
    """Normalize country name to 2-letter ISO code"""
    country_lower = country.lower().strip()
    return COUNTRIES.get(country_lower, country.upper() if len(country) == 2 else None)

def normalize_indicator(indicator: str) -> Optional[str]:
    """Normalize indicator name to EconDB symbol"""
    indicator_lower = indicator.lower().strip()
    return INDICATORS.get(indicator_lower, indicator.upper())

async def get_economic_indicators(
    symbol: str,
    country: str,
    start_date: Optional[str] = None,
    end_date: Optional[str] = None,
    transform: Optional[str] = None
) -> Dict:
    """
    Fetch economic indicators from EconDB

    Args:
        symbol: Indicator symbol (e.g., 'GDP', 'CPI', 'URATE')
        country: Country code (2-letter ISO) or name
        start_date: Start date YYYY-MM-DD
        end_date: End date YYYY-MM-DD
        transform: Transformation (toya, tpop, tusd, tpgp)
    """
    try:
        # Normalize inputs
        country_code = normalize_country(country)
        if not country_code:
            return {"error": f"Invalid country code: {country}"}

        indicator = normalize_indicator(symbol)
        if not indicator:
            return {"error": f"Invalid indicator: {symbol}"}

        # Create temporary token (optional)
        token = await create_temp_token()

        # Build ticker symbol (e.g., GDPUS, CPICN)
        ticker = f"{indicator}{country_code}"
        if transform and transform.lower() in TRANSFORMATIONS:
            ticker += f"~{transform.upper()}"

        # Build URL
        url = f"{ECONDB_API_BASE}/?ticker=%5B{ticker}%5D&format=json"
        if start_date:
            url += f"&from={start_date}"
        if end_date:
            url += f"&to={end_date}"

        # Make request
        response = await make_econdb_request(url, token)

        if "error" in response:
            return response

        # Parse results
        if not response.get("results"):
            return {"error": "No data returned"}

        result = response["results"][0]

        # Extract metadata
        metadata = {
            "ticker": result.get("ticker", ""),
            "description": result.get("description", ""),
            "country": result.get("geography", ""),
            "frequency": result.get("frequency", ""),
            "dataset": result.get("dataset", ""),
            "last_update": result.get("last_update", ""),
        }

        # Extract time series data
        data_dict = result.get("data", {})
        dates = data_dict.get("dates", [])
        values = data_dict.get("values", [])

        observations = []
        for date, value in zip(dates, values):
            if value is not None and value != "nan":
                observations.append({
                    "date": date,
                    "value": float(value) if value != "" else None
                })

        return {
            "indicator": indicator,
            "country": country_code,
            "transform": transform.upper() if transform else "LEVEL",
            "metadata": metadata,
            "observations": observations,
            "observation_count": len(observations)
        }

    except Exception as e:
        return {"error": str(e)}

async def get_country_profile(
    country: str,
    latest: bool = True
) -> Dict:
    """
    Get comprehensive country economic profile

    Args:
        country: Country code (2-letter ISO) or name
        latest: If True, return only latest values
    """
    try:
        country_code = normalize_country(country)
        if not country_code:
            return {"error": f"Invalid country code: {country}"}

        token = await create_temp_token()

        # Key indicators for country profile
        indicators = [
            ("GDP", "tusd"),      # GDP in USD
            ("RGDP", "toya"),     # Real GDP YoY growth
            ("RGDP", "tpop"),     # Real GDP QoQ growth
            ("CPI", "toya"),      # Inflation YoY
            ("CORE", "toya"),     # Core inflation YoY
            ("URATE", None),      # Unemployment rate
            ("POLIR", None),      # Policy rate
            ("Y10YD", None),      # 10Y yield
            ("RETA", "toya"),     # Retail sales YoY
            ("IP", "toya"),       # Industrial production YoY
            ("POP", None),        # Population
        ]

        # Build tickers
        tickers = []
        for ind, trans in indicators:
            ticker = f"{ind}{country_code}"
            if trans:
                ticker += f"~{trans.upper()}"
            tickers.append(ticker)

        # Make request for all indicators
        tickers_string = ",".join(tickers)
        url = f"{ECONDB_API_BASE}/?ticker=%5B{tickers_string}%5D&format=json"

        response = await make_econdb_request(url, token)

        if "error" in response:
            return response

        if not response.get("results"):
            return {"error": "No data returned"}

        profile = {
            "country": country_code,
            "indicators": {}
        }

        for result in response["results"]:
            ticker = result.get("ticker", "")
            indicator_name = result.get("description", "")

            data_dict = result.get("data", {})
            dates = data_dict.get("dates", [])
            values = data_dict.get("values", [])

            if latest and dates and values:
                # Get most recent non-null value
                for date, value in zip(reversed(dates), reversed(values)):
                    if value is not None and value != "nan" and value != "":
                        profile["indicators"][indicator_name] = {
                            "date": date,
                            "value": float(value)
                        }
                        break
            else:
                observations = []
                for date, value in zip(dates, values):
                    if value is not None and value != "nan" and value != "":
                        observations.append({
                            "date": date,
                            "value": float(value)
                        })
                profile["indicators"][indicator_name] = observations

        return profile

    except Exception as e:
        return {"error": str(e)}

async def get_main_indicators(
    country: str,
    start_date: Optional[str] = None,
    end_date: Optional[str] = None,
    frequency: str = "quarter",
    transform: Optional[str] = "toya"
) -> Dict:
    """
    Get main economic indicators for a country from EconDB trends endpoint

    Args:
        country: Country code (2-letter ISO) or name
        start_date: Start date YYYY-MM-DD
        end_date: End date YYYY-MM-DD
        frequency: annual, quarter, month
        transform: toya, tpop, tusd, or None
    """
    try:
        country_code = normalize_country(country)
        if not country_code:
            return {"error": f"Invalid country code: {country}"}

        # Default dates
        if not start_date:
            start_date = (datetime.now() - timedelta(weeks=52 * 3)).strftime("%Y-%m-%d")
        if not end_date:
            end_date = datetime.now().strftime("%Y-%m-%d")

        # Map frequency
        freq_map = {"annual": "Y", "quarter": "Q", "month": "M"}
        freq = freq_map.get(frequency, "Q")

        # Map transform
        transform_map = {"tpop": 1, "toya": 2, "level": 3, "tusd": 9, None: 3}
        trans_num = transform_map.get(transform, 2)

        # Build URL
        url = (
            f"{ECONDB_TRENDS_BASE}/country_forecast/"
            f"?country={country_code}&freq={freq}&transform={trans_num}"
            f"&dateStart={start_date}&dateEnd={end_date}"
        )

        response = await make_econdb_request(url)

        if "error" in response:
            return response

        # Parse response
        row_names = response.get("row_names", [])
        units_col = response.get("units_col", [])
        data = response.get("data", [])

        # Build indicator mapping
        indicators_info = {}
        for idx, row_info in enumerate(row_names):
            code = row_info.get("code", "")
            indicators_info[code] = {
                "name": row_info.get("verbose", "").title(),
                "is_parent": row_info.get("is_parent", False),
                "units": units_col[idx] if idx < len(units_col) else ""
            }

        # Parse time series data
        time_series = {}
        for entry in data:
            indicator = entry.get("indicator", "")
            obs_time = entry.get("obs_time", "")
            obs_value = entry.get("obs_value", None)

            if indicator not in time_series:
                time_series[indicator] = []

            if obs_value is not None and obs_value != "":
                time_series[indicator].append({
                    "date": obs_time,
                    "value": float(obs_value)
                })

        return {
            "country": country_code,
            "frequency": frequency,
            "transform": transform,
            "start_date": start_date,
            "end_date": end_date,
            "indicators_info": indicators_info,
            "time_series": time_series,
            "footnote": response.get("footnote", [])
        }

    except Exception as e:
        return {"error": str(e)}

async def list_indicators() -> Dict:
    """List all available indicators with descriptions"""
    return {
        "indicators": [
            {"code": code, "name": name, "description": f"{name} indicator"}
            for code, name in INDICATORS.items()
        ],
        "transformations": [
            {"code": code, "description": desc}
            for code, desc in TRANSFORMATIONS.items()
        ]
    }

async def list_countries() -> Dict:
    """List all supported countries"""
    # Get unique country codes
    unique_countries = {}
    for name, code in COUNTRIES.items():
        if code not in unique_countries:
            unique_countries[code] = name.replace("_", " ").title()

    return {
        "countries": [
            {"code": code, "name": name}
            for code, name in sorted(unique_countries.items())
        ],
        "count": len(unique_countries)
    }

def main():
    """Main CLI entry point"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python econdb_data.py <command> <args>",
            "commands": [
                "indicator <symbol> <country> [start_date] [end_date] [transform]",
                "profile <country> [latest]",
                "main <country> [start_date] [end_date] [frequency] [transform]",
                "list-indicators",
                "list-countries"
            ]
        }))
        sys.exit(1)

    command = sys.argv[1]

    try:
        if command == "indicator":
            if len(sys.argv) < 4:
                print(json.dumps({"error": "Usage: python econdb_data.py indicator <symbol> <country> [start_date] [end_date] [transform]"}))
                sys.exit(1)

            symbol = sys.argv[2]
            country = sys.argv[3]
            start_date = sys.argv[4] if len(sys.argv) > 4 else None
            end_date = sys.argv[5] if len(sys.argv) > 5 else None
            transform = sys.argv[6] if len(sys.argv) > 6 else None

            result = asyncio.run(get_economic_indicators(symbol, country, start_date, end_date, transform))
            print(json.dumps(result, indent=2))

        elif command == "profile":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python econdb_data.py profile <country> [latest]"}))
                sys.exit(1)

            country = sys.argv[2]
            latest = sys.argv[3].lower() == "true" if len(sys.argv) > 3 else True

            result = asyncio.run(get_country_profile(country, latest))
            print(json.dumps(result, indent=2))

        elif command == "main":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python econdb_data.py main <country> [start_date] [end_date] [frequency] [transform]"}))
                sys.exit(1)

            country = sys.argv[2]
            start_date = sys.argv[3] if len(sys.argv) > 3 else None
            end_date = sys.argv[4] if len(sys.argv) > 4 else None
            frequency = sys.argv[5] if len(sys.argv) > 5 else "quarter"
            transform = sys.argv[6] if len(sys.argv) > 6 else "toya"

            result = asyncio.run(get_main_indicators(country, start_date, end_date, frequency, transform))
            print(json.dumps(result, indent=2))

        elif command == "list-indicators":
            result = asyncio.run(list_indicators())
            print(json.dumps(result, indent=2))

        elif command == "list-countries":
            result = asyncio.run(list_countries())
            print(json.dumps(result, indent=2))

        else:
            print(json.dumps({"error": f"Unknown command: {command}"}))
            sys.exit(1)

    except Exception as e:
        print(json.dumps({"error": str(e)}))
        sys.exit(1)

if __name__ == "__main__":
    main()
