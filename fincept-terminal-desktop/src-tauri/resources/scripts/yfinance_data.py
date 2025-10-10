"""
YFinance Data Fetcher
Fetches real-time stock quotes and historical data using yfinance
Returns JSON output for Rust integration
"""

import sys
import json
import yfinance as yf
import pandas as pd
from datetime import datetime

def get_quote(symbol):
    """Fetch real-time quote for a single symbol"""
    try:
        ticker = yf.Ticker(symbol)
        info = ticker.info
        hist = ticker.history(period="1d")

        if hist.empty:
            return None

        current_price = hist['Close'].iloc[-1]
        previous_close = info.get('previousClose', current_price)
        change = current_price - previous_close
        change_percent = (change / previous_close) * 100 if previous_close else 0

        quote_data = {
            "symbol": symbol,
            "price": round(float(current_price), 2),
            "change": round(float(change), 2),
            "change_percent": round(float(change_percent), 2),
            "volume": int(hist['Volume'].iloc[-1]) if not hist['Volume'].empty else None,
            "high": round(float(hist['High'].iloc[-1]), 2) if not hist['High'].empty else None,
            "low": round(float(hist['Low'].iloc[-1]), 2) if not hist['Low'].empty else None,
            "open": round(float(hist['Open'].iloc[-1]), 2) if not hist['Open'].empty else None,
            "previous_close": round(float(previous_close), 2),
            "timestamp": int(datetime.now().timestamp())
        }

        return quote_data
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_historical(symbol, start_date, end_date):
    """Fetch historical data for a symbol"""
    try:
        ticker = yf.Ticker(symbol)
        hist = ticker.history(start=start_date, end=end_date)

        if hist.empty:
            return None

        historical_data = []
        for index, row in hist.iterrows():
            historical_data.append({
                "symbol": symbol,
                "timestamp": int(index.timestamp()),
                "open": round(float(row['Open']), 2),
                "high": round(float(row['High']), 2),
                "low": round(float(row['Low']), 2),
                "close": round(float(row['Close']), 2),
                "volume": int(row['Volume']),
                "adj_close": round(float(row['Close']), 2)
            })

        return historical_data
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_info(symbol):
    """Fetch company info for a symbol"""
    try:
        ticker = yf.Ticker(symbol)
        info = ticker.info

        # Extract comprehensive information - many more fields available from yfinance
        info_data = {
            "symbol": symbol,
            "company_name": info.get('longName', info.get('shortName', 'N/A')),
            "sector": info.get('sector', 'N/A'),
            "industry": info.get('industry', 'N/A'),
            "market_cap": info.get('marketCap'),
            "pe_ratio": info.get('trailingPE'),
            "forward_pe": info.get('forwardPE'),
            "dividend_yield": info.get('dividendYield'),
            "beta": info.get('beta'),
            "fifty_two_week_high": info.get('fiftyTwoWeekHigh'),
            "fifty_two_week_low": info.get('fiftyTwoWeekLow'),
            "average_volume": info.get('averageVolume'),
            "description": info.get('longBusinessSummary', 'N/A'),
            "website": info.get('website', 'N/A'),
            "country": info.get('country', 'N/A'),
            "currency": info.get('currency', 'USD'),
            "exchange": info.get('exchange', 'N/A'),
            "employees": info.get('fullTimeEmployees'),
            # Additional comprehensive metrics
            "current_price": info.get('currentPrice'),
            "target_high_price": info.get('targetHighPrice'),
            "target_low_price": info.get('targetLowPrice'),
            "target_mean_price": info.get('targetMeanPrice'),
            "recommendation_mean": info.get('recommendationMean'),
            "recommendation_key": info.get('recommendationKey'),
            "number_of_analyst_opinions": info.get('numberOfAnalystOpinions'),
            "total_cash": info.get('totalCash'),
            "total_debt": info.get('totalDebt'),
            "total_revenue": info.get('totalRevenue'),
            "revenue_per_share": info.get('revenuePerShare'),
            "return_on_assets": info.get('returnOnAssets'),
            "return_on_equity": info.get('returnOnEquity'),
            "gross_profits": info.get('grossProfits'),
            "free_cashflow": info.get('freeCashflow'),
            "operating_cashflow": info.get('operatingCashflow'),
            "earnings_growth": info.get('earningsGrowth'),
            "revenue_growth": info.get('revenueGrowth'),
            "gross_margins": info.get('grossMargins'),
            "operating_margins": info.get('operatingMargins'),
            "ebitda_margins": info.get('ebitdaMargins'),
            "profit_margins": info.get('profitMargins'),
            "book_value": info.get('bookValue'),
            "price_to_book": info.get('priceToBook'),
            "enterprise_value": info.get('enterpriseValue'),
            "enterprise_to_revenue": info.get('enterpriseToRevenue'),
            "enterprise_to_ebitda": info.get('enterpriseToEbitda'),
            "shares_outstanding": info.get('sharesOutstanding'),
            "float_shares": info.get('floatShares'),
            "held_percent_insiders": info.get('heldPercentInsiders'),
            "held_percent_institutions": info.get('heldPercentInstitutions'),
            "short_ratio": info.get('shortRatio'),
            "short_percent_of_float": info.get('shortPercentOfFloat'),
            "peg_ratio": info.get('trailingPegRatio'),
            "timestamp": int(datetime.now().timestamp())
        }

        return info_data
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_financials(symbol):
    """Fetch financial statements for a symbol"""
    try:
        ticker = yf.Ticker(symbol)

        # Get income statement, balance sheet, and cash flow
        income_stmt = ticker.financials
        balance_sheet = ticker.balance_sheet
        cash_flow = ticker.cashflow

        def dataframe_to_dict(df):
            """Convert pandas DataFrame to dict with cleaned values"""
            if df is None or df.empty:
                return {}
            result = {}
            for col in df.columns:
                result[str(col)] = {}
                for idx in df.index:
                    value = df.loc[idx, col]
                    if pd.notna(value):
                        result[str(col)][str(idx)] = float(value) if isinstance(value, (int, float)) else str(value)
            return result

        financials_data = {
            "symbol": symbol,
            "income_statement": dataframe_to_dict(income_stmt),
            "balance_sheet": dataframe_to_dict(balance_sheet),
            "cash_flow": dataframe_to_dict(cash_flow),
            "timestamp": int(datetime.now().timestamp())
        }

        return financials_data
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_batch_quotes(symbols):
    """Fetch quotes for multiple symbols at once"""
    results = []
    for symbol in symbols:
        quote = get_quote(symbol)
        if quote:
            results.append(quote)
    return results

def main():
    if len(sys.argv) < 2:
        print(json.dumps({"error": "Usage: python yfinance_data.py <command> <args>"}))
        sys.exit(1)

    command = sys.argv[1]

    if command == "quote":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: python yfinance_data.py quote <symbol>"}))
            sys.exit(1)

        symbol = sys.argv[2]
        result = get_quote(symbol)
        print(json.dumps(result, indent=2))

    elif command == "batch_quotes":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: python yfinance_data.py batch_quotes <symbol1> <symbol2> ..."}))
            sys.exit(1)

        symbols = sys.argv[2:]
        result = get_batch_quotes(symbols)
        print(json.dumps(result, indent=2))

    elif command == "historical":
        if len(sys.argv) < 5:
            print(json.dumps({"error": "Usage: python yfinance_data.py historical <symbol> <start_date> <end_date>"}))
            sys.exit(1)

        symbol = sys.argv[2]
        start_date = sys.argv[3]
        end_date = sys.argv[4]
        result = get_historical(symbol, start_date, end_date)
        print(json.dumps(result, indent=2))

    elif command == "info":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: python yfinance_data.py info <symbol>"}))
            sys.exit(1)

        symbol = sys.argv[2]
        result = get_info(symbol)
        print(json.dumps(result, indent=2))

    elif command == "financials":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: python yfinance_data.py financials <symbol>"}))
            sys.exit(1)

        symbol = sys.argv[2]
        result = get_financials(symbol)
        print(json.dumps(result, indent=2))

    else:
        print(json.dumps({"error": f"Unknown command: {command}"}))
        sys.exit(1)

if __name__ == "__main__":
    main()
