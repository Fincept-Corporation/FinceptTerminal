"""
Financial Modeling Prep (FMP) Data Fetcher
Fetches fundamental financial statements (Income Statement, Balance Sheet, Cash Flow)
for publicly traded companies.
Returns standardized JSON output for Rust integration.
"""

import sys
import json
import os
import requests
from typing import Dict, Any, List

# API Configuration
# Get your free API key from Financial Modeling Prep and set it as an environment variable
BASE_URL = "https://financialmodelingprep.com/api/v3"
API_KEY = os.environ.get("FMP_API_KEY")

# Mapping of user commands to FMP API endpoints
ENDPOINT_MAP = {
    "income": "income-statement",
    "balance": "balance-sheet-statement",
    "cash": "cash-flow-statement",
}

def create_error_response(message: str, symbol: str = "N/A") -> Dict[str, Any]:
    """Creates a standardized error response dictionary."""
    return {"error": message, "symbol": symbol, "data": []}

def get_fundamentals(symbol: str, statement_type: str, period: str = 'annual', limit: int = 1) -> Dict[str, Any]:
    """
    Fetch the latest fundamental financial statement data.

    Args:
        symbol: The stock ticker symbol (e.g., 'AAPL', 'MSFT').
        statement_type: The type of statement (e.g., 'income', 'balance', 'cash').
        period: The frequency of the statement ('annual' or 'quarter').
        limit: The number of recent statements to retrieve (default is 1 - the latest).

    Returns:
        Dictionary with a list of statements or an error message.
    """
    if not API_KEY:
        return create_error_response("FMP_API_KEY environment variable not set.", symbol)

    if statement_type not in ENDPOINT_MAP:
        return create_error_response(f"Invalid statement type: {statement_type}. Must be one of {list(ENDPOINT_MAP.keys())}", symbol)

    if period not in ['annual', 'quarter']:
        return create_error_response(f"Invalid period: {period}. Must be 'annual' or 'quarter'", symbol)

    endpoint = ENDPOINT_MAP[statement_type]

    try:
        # 1. Build API Request
        url = f"{BASE_URL}/{endpoint}/{symbol}"
        params = {
            'period': period,
            'limit': limit,
            'apikey': API_KEY
        }

        # 2. Make Request
        response = requests.get(url, params=params, timeout=15)
        response.raise_for_status() # Raise exception for 4xx or 5xx status codes

        # 3. Parse JSON response
        data: List[Dict[str, Any]] = response.json()

        # 4. Handle Empty Data/No Ticker Found
        if not data:
            return create_error_response(f"No {statement_type} statements found for symbol: {symbol}", symbol)

        # 5. Format Standardized Response (Extracting relevant fields)
        formatted_statements = []
        for statement in data:
            # We standardize the date field and include the essential metrics
            formatted_statement = {
                "symbol": statement.get('symbol', symbol),
                "date": statement.get('date', statement.get('fillingDate')), # Use fillingDate if 'date' is missing
                "period": period,
                "data": {}
            }

            # Map important statement fields
            if statement_type == 'income':
                formatted_statement['data'] = {
                    "revenue": statement.get('revenue'),
                    "grossProfit": statement.get('grossProfit'),
                    "netIncome": statement.get('netIncome')
                }
            elif statement_type == 'balance':
                formatted_statement['data'] = {
                    "totalAssets": statement.get('totalAssets'),
                    "totalLiabilities": statement.get('totalLiabilities'),
                    "totalEquity": statement.get('totalStockholdersEquity')
                }
            elif statement_type == 'cash':
                formatted_statement['data'] = {
                    "cashFlowOperations": statement.get('cashFlowsFromOperatingActivities'),
                    "cashFlowInvesting": statement.get('cashFlowsFromInvestingActivities'),
                    "cashFlowFinancing": statement.get('cashFlowsFromFinancingActivities')
                }
            
            formatted_statements.append(formatted_statement)

        # Return a successful response wrapper
        return {
            "symbol": symbol.upper(),
            "statement_type": statement_type,
            "period": period,
            "data": formatted_statements
        }

    except requests.exceptions.Timeout:
        return create_error_response("Request timed out.", symbol)
    except requests.exceptions.HTTPError as e:
        return create_error_response(f"HTTP error {e.response.status_code}: {e.response.text}", symbol)
    except requests.exceptions.RequestException as e:
        return create_error_response(f"Network error: {str(e)}", symbol)
    except Exception as e:
        return create_error_response(f"An unexpected error occurred: {str(e)}", symbol)


def main():
    """Main CLI entry point for FMP wrapper."""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python fmp_data.py <command> <args>",
            "commands": ["fundamentals <symbol> <type> [period=annual] [limit=1]"]
        }, indent=2))
        sys.exit(1)

    command = sys.argv[1]

    if command == "fundamentals":
        if len(sys.argv) < 4:
            print(json.dumps(create_error_response(
                "Usage: python fmp_data.py fundamentals <symbol> <type> [period=annual] [limit=1]",
                symbol=sys.argv[2] if len(sys.argv) > 2 else "N/A"
            ), indent=2))
            sys.exit(1)

        symbol = sys.argv[2]
        statement_type = sys.argv[3].lower()
        period = sys.argv[4].lower() if len(sys.argv) > 4 else 'annual'
        limit = int(sys.argv[5]) if len(sys.argv) > 5 and sys.argv[5].isdigit() else 1
        
        result = get_fundamentals(symbol, statement_type, period, limit)
        print(json.dumps(result, indent=2))

    else:
        print(json.dumps(create_error_response(f"Unknown command: {command}")))
        sys.exit(1)


if __name__ == "__main__":
    main()
