"""
Trading Economics Data Fetcher
Fetches sovereign credit ratings and government bond yields from the Trading Economics API.
Returns JSON output for Rust integration.
"""

import sys
import json
import os
import requests
from typing import Dict, Any, List, Optional
from datetime import datetime, timedelta

# 1. CONFIGURATION
API_KEY = os.environ.get('TRADING_ECONOMICS_API_KEY')
BASE_URL = "https://api.tradingeconomics.com"

def _make_request(endpoint: str, params: Dict[str, Any] = None) -> Any:
    """A private helper function to handle all API requests and errors."""
    full_url = f"{BASE_URL}/{endpoint}"

    if params is None:
        params = {}

    # Add API key to parameters
    if API_KEY:
        params['c'] = API_KEY
    else:
        return {"error": "Trading Economics API key is required. Set TRADING_ECONOMICS_API_KEY environment variable."}

    try:
        response = requests.get(full_url, params=params, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        if e.response.status_code == 401:
            return {"error": f"Invalid API key or unauthorized access: {e.response.text}"}
        elif e.response.status_code == 429:
            return {"error": f"API rate limit exceeded: {e.response.text}"}
        else:
            return {"error": f"HTTP Error: {e.response.status_code} - {e.response.text}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Network or request error: {str(e)}"}
    except json.JSONDecodeError:
        return {"error": "Failed to decode API response."}

# 2. CORE FUNCTIONS - CREDIT RATINGS

def test_connection() -> Dict[str, Any]:
    """Test API connection with a simple call."""
    return _make_request("credit-ratings")

def get_credit_ratings(country: str = None) -> Dict[str, Any]:
    """1. Get sovereign credit ratings for a specific country or all countries."""
    if country:
        endpoint = f"credit-ratings/country/{country}"
    else:
        endpoint = "credit-ratings"

    data = _make_request(endpoint)

    if data and "error" not in data and isinstance(data, list):
        # Add metadata
        return {
            "data": data,
            "count": len(data),
            "source": "Trading Economics",
            "timestamp": datetime.now().isoformat()
        }
    return data

def get_credit_ratings_by_agency(agency: str) -> Dict[str, Any]:
    """2. Get credit ratings filtered by rating agency (S&P, Moody's, Fitch)."""
    # Note: This endpoint might need adjustment based on actual API structure
    data = _make_request("credit-ratings", {"agency": agency})

    if data and "error" not in data and isinstance(data, list):
        return {
            "data": data,
            "agency": agency,
            "count": len(data),
            "timestamp": datetime.now().isoformat()
        }
    return data

def get_historical_credit_ratings(country: str, start_date: str = None, end_date: str = None) -> Dict[str, Any]:
    """3. Get historical credit ratings for a country."""
    params = {}
    if start_date:
        params['startDate'] = start_date
    if end_date:
        params['endDate'] = end_date

    endpoint = f"credit-ratings/country/{country}/historical"
    data = _make_request(endpoint, params)

    if data and "error" not in data:
        return {
            "data": data,
            "country": country,
            "period": {"start": start_date, "end": end_date},
            "timestamp": datetime.now().isoformat()
        }
    return data

def get_rating_changes() -> Dict[str, Any]:
    """4. Get recent credit rating changes (upgrades/downgrades)."""
    # Note: This endpoint might need adjustment based on actual API
    data = _make_request("credit-ratings/changes")

    if data and "error" not in data:
        return {
            "data": data,
            "timestamp": datetime.now().isoformat()
        }
    return data

# 3. CORE FUNCTIONS - GOVERNMENT BONDS

def get_government_bond_yields(country: str = None) -> Dict[str, Any]:
    """5. Get government bond yields for a specific country or all countries."""
    if country:
        endpoint = f"country/{country}/bond-yield"
    else:
        endpoint = "markets/bond"

    data = _make_request(endpoint)

    if data and "error" not in data and isinstance(data, list):
        return {
            "data": data,
            "country": country if country else "All",
            "count": len(data),
            "timestamp": datetime.now().isoformat()
        }
    return data

def get_bond_symbol(symbol: str) -> Dict[str, Any]:
    """6. Get specific government bond data by symbol (e.g., US10Y, UK10Y)."""
    data = _make_request(f"markets/symbol/{symbol}")

    if data and "error" not in data and isinstance(data, list) and len(data) > 0:
        return {
            "data": data[0],
            "symbol": symbol,
            "timestamp": datetime.now().isoformat()
        }
    elif isinstance(data, list) and len(data) == 0:
        return {"error": f"No data found for symbol: {symbol}"}
    return data

def get_us_treasury_yields() -> Dict[str, Any]:
    """7. Get US Treasury yields for all durations."""
    data = _make_request("markets/bond", {"country": "united states"})

    if data and "error" not in data and isinstance(data, list):
        return {
            "data": data,
            "country": "United States",
            "count": len(data),
            "timestamp": datetime.now().isoformat()
        }
    return data

def get_european_bond_yields() -> Dict[str, Any]:
    """8. Get European government bond yields."""
    data = _make_request("markets/bond", {"country": "european union"})

    if data and "error" not in data and isinstance(data, list):
        return {
            "data": data,
            "region": "European Union",
            "count": len(data),
            "timestamp": datetime.now().isoformat()
        }
    return data

def get_historical_bond_yields(symbol: str, start_date: str = None, end_date: str = None) -> Dict[str, Any]:
    """9. Get historical bond yields for a specific symbol."""
    params = {}
    if start_date:
        params['startDate'] = start_date
    if end_date:
        params['endDate'] = end_date

    endpoint = f"markets/symbol/{symbol}/historical"
    data = _make_request(endpoint, params)

    if data and "error" not in data:
        return {
            "data": data,
            "symbol": symbol,
            "period": {"start": start_date, "end": end_date},
            "timestamp": datetime.now().isoformat()
        }
    return data

def get_yield_curve(country: str) -> Dict[str, Any]:
    """10. Get yield curve data for a country."""
    data = _make_request(f"country/{country}/yield-curve")

    if data and "error" not in data:
        return {
            "data": data,
            "country": country,
            "timestamp": datetime.now().isoformat()
        }
    return data

# 4. COMBINED FUNCTIONS

def get_country_financial_data(country: str) -> Dict[str, Any]:
    """11. Get comprehensive financial data (credit ratings + bond yields) for a country."""
    try:
        # Fetch both data types independently
        credit_data = get_credit_ratings(country)
        bond_data = get_government_bond_yields(country)

        result = {
            "country": country,
            "timestamp": datetime.now().isoformat(),
            "credit_ratings": credit_data,
            "bond_yields": bond_data
        }

        # Add summary
        credit_success = credit_data and "error" not in credit_data
        bond_success = bond_data and "error" not in bond_data

        result["summary"] = {
            "credit_ratings_available": credit_success,
            "bond_yields_available": bond_success,
            "overall_success": credit_success or bond_success
        }

        return result
    except Exception as e:
        return {"error": f"Failed to get comprehensive data for {country}: {str(e)}"}

def get_global_summary() -> Dict[str, Any]:
    """12. Get global summary of credit ratings and bond markets."""
    try:
        # Get global ratings data
        ratings_data = _make_request("credit-ratings")

        if ratings_data and "error" not in ratings_data and isinstance(ratings_data, list):
            # Process ratings summary
            agencies = list(set(rating.get('Agency', 'Unknown') for rating in ratings_data))
            countries = list(set(rating.get('Country', 'Unknown') for rating in ratings_data))

            # Rating distribution
            rating_distribution = {}
            for rating in ratings_data:
                rating_val = rating.get('Rating', 'Unknown')
                rating_distribution[rating_val] = rating_distribution.get(rating_val, 0) + 1

            ratings_summary = {
                "total_ratings": len(ratings_data),
                "agencies": agencies,
                "countries_count": len(countries),
                "rating_distribution": rating_distribution
            }
        else:
            ratings_summary = {"error": "Failed to fetch ratings data"}

        # Get bond markets overview
        bond_data = _make_request("markets/bond-overview")

        if bond_data and "error" not in bond_data:
            bond_summary = bond_data
        else:
            bond_summary = {"error": "Failed to fetch bond overview"}

        return {
            "timestamp": datetime.now().isoformat(),
            "credit_ratings_summary": ratings_summary,
            "bond_markets_summary": bond_summary,
            "data_source": "Trading Economics"
        }
    except Exception as e:
        return {"error": f"Failed to generate global summary: {str(e)}"}

# 5. UTILITY FUNCTIONS

def search_bonds(query: str) -> Dict[str, Any]:
    """13. Search for bond symbols and instruments."""
    data = _make_request("search/bond", {"q": query})

    if data and "error" not in data:
        return {
            "data": data,
            "query": query,
            "timestamp": datetime.now().isoformat()
        }
    return data

def get_supported_countries() -> Dict[str, Any]:
    """14. Get list of supported countries for credit ratings and bonds."""
    # This might be a custom endpoint or derived from existing data
    ratings_data = _make_request("credit-ratings")

    if ratings_data and "error" not in ratings_data and isinstance(ratings_data, list):
        countries = sorted(list(set(rating.get('Country', 'Unknown') for rating in ratings_data)))
        return {
            "data": countries,
            "count": len(countries),
            "timestamp": datetime.now().isoformat()
        }
    return ratings_data

def get_rating_calendar() -> Dict[str, Any]:
    """15. Get calendar of upcoming rating events."""
    data = _make_request("calendar/ratings")

    if data and "error" not in data:
        return {
            "data": data,
            "timestamp": datetime.now().isoformat()
        }
    return data

# 6. CLI INTERFACE

def main():
    """Main Command-Line Interface entry point."""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python trading_economics_data.py <command> [args]",
            "commands": [
                "test - Test API connection",
                "ratings [country] - Get credit ratings (optional country)",
                "ratings_by_agency <agency> - Get ratings by agency (S&P, Moody's, Fitch)",
                "ratings_history <country> [start_date] [end_date] - Historical ratings",
                "rating_changes - Get recent rating changes",
                "bonds [country] - Get bond yields (optional country)",
                "bond <symbol> - Get specific bond data (e.g., US10Y)",
                "us_treasuries - Get US Treasury yields",
                "european_bonds - Get European bond yields",
                "bond_history <symbol> [start_date] [end_date] - Historical bond yields",
                "yield_curve <country> - Get yield curve data",
                "country_data <country> - Get comprehensive country data",
                "global_summary - Get global market summary",
                "search_bonds <query> - Search for bonds",
                "countries - Get supported countries",
                "calendar - Get rating calendar"
            ],
            "examples": [
                "python trading_economics_data.py test",
                "python trading_economics_data.py ratings Sweden",
                "python trading_economics_data.py bond US10Y",
                "python trading_economics_data.py country_data United States",
                "python trading_economics_data.py ratings_history Sweden 2023-01-01 2023-12-31"
            ]
        }, indent=2))
        sys.exit(1)

    command = sys.argv[1].lower()
    result = {}

    try:
        if command == "test":
            result = test_connection()
        elif command == "ratings":
            country = sys.argv[2] if len(sys.argv) > 2 else None
            result = get_credit_ratings(country)
        elif command == "ratings_by_agency":
            agency = sys.argv[2]
            result = get_credit_ratings_by_agency(agency)
        elif command == "ratings_history":
            country = sys.argv[2]
            start_date = sys.argv[3] if len(sys.argv) > 3 else None
            end_date = sys.argv[4] if len(sys.argv) > 4 else None
            result = get_historical_credit_ratings(country, start_date, end_date)
        elif command == "rating_changes":
            result = get_rating_changes()
        elif command == "bonds":
            country = sys.argv[2] if len(sys.argv) > 2 else None
            result = get_government_bond_yields(country)
        elif command == "bond":
            symbol = sys.argv[2]
            result = get_bond_symbol(symbol)
        elif command == "us_treasuries":
            result = get_us_treasury_yields()
        elif command == "european_bonds":
            result = get_european_bond_yields()
        elif command == "bond_history":
            symbol = sys.argv[2]
            start_date = sys.argv[3] if len(sys.argv) > 3 else None
            end_date = sys.argv[4] if len(sys.argv) > 4 else None
            result = get_historical_bond_yields(symbol, start_date, end_date)
        elif command == "yield_curve":
            country = sys.argv[2]
            result = get_yield_curve(country)
        elif command == "country_data":
            country = " ".join(sys.argv[2:])
            result = get_country_financial_data(country)
        elif command == "global_summary":
            result = get_global_summary()
        elif command == "search_bonds":
            query = " ".join(sys.argv[2:])
            result = search_bonds(query)
        elif command == "countries":
            result = get_supported_countries()
        elif command == "calendar":
            result = get_rating_calendar()
        else:
            result = {"error": f"Unknown command: {command}"}
    except IndexError:
        result = {"error": "Missing required arguments for command."}
    except Exception as e:
        result = {"error": f"An unexpected error occurred: {str(e)}"}

    print(json.dumps(result, indent=2))

if __name__ == "__main__":
    main()