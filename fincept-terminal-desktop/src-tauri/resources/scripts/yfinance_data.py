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
            "timestamp": int(datetime.now().timestamp()),
            "exchange": info.get('exchange', '')
        }

        return quote_data
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_historical(symbol, start_date, end_date, interval='1d'):
    """Fetch historical data for a symbol with specified interval

    Args:
        symbol: Stock symbol (e.g., 'AAPL')
        start_date: Start date (YYYY-MM-DD)
        end_date: End date (YYYY-MM-DD)
        interval: Data interval - valid values: 1m, 2m, 5m, 15m, 30m, 60m, 90m, 1h, 1d, 5d, 1wk, 1mo, 3mo
                  Note: Intraday data (< 1d) is only available for last 60 days
    """
    try:
        ticker = yf.Ticker(symbol)
        hist = ticker.history(start=start_date, end=end_date, interval=interval)

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

def get_company_profile(symbol):
    """Get company profile data formatted for peer comparison"""
    try:
        ticker = yf.Ticker(symbol)
        info = ticker.info

        if not info or 'symbol' not in info:
            return {"error": f"No data found for symbol: {symbol}"}

        # Format data to match FMP structure
        profile = {
            "symbol": info.get("symbol", symbol),
            "companyName": info.get("longName", info.get("shortName", "")),
            "sector": info.get("sector", ""),
            "industry": info.get("industry", ""),
            "website": info.get("website", ""),
            "description": info.get("longBusinessSummary", ""),
            "exchange": info.get("exchange", ""),
            "country": info.get("country", ""),
            "city": info.get("city", ""),
            "address": info.get("address1", ""),
            "phone": info.get("phone", ""),
            "marketCap": info.get("marketCap", 0),
            "employees": info.get("fullTimeEmployees", 0),
            "currency": info.get("currency", "USD"),
            "beta": info.get("beta", 0),
            "price": info.get("currentPrice", info.get("regularMarketPrice", 0)),
            "changes": info.get("regularMarketChangePercent", 0),
        }

        return profile
    except Exception as e:
        return {"error": str(e)}

def get_financial_ratios(symbol):
    """Get financial ratios formatted for peer comparison"""
    try:
        ticker = yf.Ticker(symbol)
        info = ticker.info

        if not info or 'symbol' not in info:
            return {"error": f"No data found for symbol: {symbol}"}

        # Calculate free cash flow per share
        free_cashflow = info.get("freeCashflow", 0)
        shares_outstanding = info.get("sharesOutstanding", 1)
        fcf_per_share = free_cashflow / shares_outstanding if shares_outstanding else 0

        ratios = {
            "symbol": symbol,
            "peRatio": info.get("trailingPE", 0),
            "forwardPE": info.get("forwardPE", 0),
            "priceToBook": info.get("priceToBook", 0),
            "priceToSales": info.get("priceToSalesTrailing12Months", 0),
            "pegRatio": info.get("trailingPegRatio", 0),
            "debtToEquity": info.get("debtToEquity", 0),
            "returnOnEquity": info.get("returnOnEquity", 0),
            "returnOnAssets": info.get("returnOnAssets", 0),
            "profitMargin": info.get("profitMargins", 0),
            "operatingMargin": info.get("operatingMargins", 0),
            "grossMargin": info.get("grossMargins", 0),
            "currentRatio": info.get("currentRatio", 0),
            "quickRatio": info.get("quickRatio", 0),
            "dividendYield": info.get("dividendYield", 0),
            "revenuePerShare": info.get("revenuePerShare", 0),
            "bookValuePerShare": info.get("bookValue", 0),
            "freeCashFlowPerShare": fcf_per_share,
        }

        return ratios
    except Exception as e:
        return {"error": str(e)}

def get_multiple_profiles(symbols):
    """Get company profiles for multiple symbols"""
    results = []
    for symbol in symbols:
        profile = get_company_profile(symbol)
        if profile and "error" not in profile:
            results.append(profile)
    return results

def get_multiple_ratios(symbols):
    """Get financial ratios for multiple symbols"""
    results = []
    for symbol in symbols:
        ratios = get_financial_ratios(symbol)
        if ratios and "error" not in ratios:
            results.append(ratios)
    return results

def search_symbols(query, limit=50):
    """
    Search for stock symbols using yfinance
    Returns a list of matching symbols with company info
    """
    try:
        import yfinance as yf

        # Common exchanges and suffixes to try
        exchanges = ['', '.NS', '.BO', '.L', '.TO', '.AX', '.HK', '.SS', '.SZ']
        results = []
        query_upper = query.upper()

        # Try exact match first
        for exchange in exchanges:
            symbol_with_exchange = query_upper + exchange
            try:
                ticker = yf.Ticker(symbol_with_exchange)
                info = ticker.info

                # Check if ticker is valid by verifying it has a name
                if info.get('longName') or info.get('shortName'):
                    results.append({
                        'symbol': symbol_with_exchange,
                        'name': info.get('longName', info.get('shortName', 'N/A')),
                        'exchange': info.get('exchange', 'N/A'),
                        'type': info.get('quoteType', 'EQUITY'),
                        'currency': info.get('currency', 'USD'),
                        'sector': info.get('sector', 'N/A'),
                        'industry': info.get('industry', 'N/A')
                    })
                    if len(results) >= limit:
                        break
            except:
                continue

        # If no exact matches, try common US tickers that start with the query
        if len(results) == 0:
            # List of popular tickers for quick matching (you can expand this)
            popular_tickers = [
                'AAPL', 'MSFT', 'GOOGL', 'GOOG', 'AMZN', 'TSLA', 'META', 'NVDA',
                'BRK-B', 'BRK-A', 'V', 'JNJ', 'WMT', 'JPM', 'MA', 'PG', 'UNH',
                'DIS', 'HD', 'PYPL', 'BAC', 'NFLX', 'ADBE', 'CRM', 'CMCSA',
                'XOM', 'VZ', 'INTC', 'ABT', 'KO', 'NKE', 'PFE', 'MRK', 'PEP',
                'TMO', 'CSCO', 'COST', 'AVGO', 'TXN', 'ACN', 'DHR', 'NEE', 'CVX'
            ]

            matching_tickers = [t for t in popular_tickers if t.startswith(query_upper)]

            for symbol in matching_tickers[:limit]:
                try:
                    ticker = yf.Ticker(symbol)
                    info = ticker.info

                    if info.get('longName') or info.get('shortName'):
                        results.append({
                            'symbol': symbol,
                            'name': info.get('longName', info.get('shortName', 'N/A')),
                            'exchange': info.get('exchange', 'N/A'),
                            'type': info.get('quoteType', 'EQUITY'),
                            'currency': info.get('currency', 'USD'),
                            'sector': info.get('sector', 'N/A'),
                            'industry': info.get('industry', 'N/A')
                        })
                except:
                    continue

        return {"results": results, "query": query, "count": len(results)}

    except Exception as e:
        return {"error": str(e), "query": query, "results": []}

def main(args=None):
    # Support both worker pool (args parameter) and subprocess/CLI (sys.argv)
    if args is None:
        args = sys.argv[1:]

    if len(args) < 1:
        return json.dumps({"error": "Usage: python yfinance_data.py <command> <args>"})

    command = args[0]

    if command == "quote":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py quote <symbol>"}
        else:
            symbol = args[1]
            result = get_quote(symbol)

    elif command == "batch_quotes":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py batch_quotes <symbol1> <symbol2> ..."}
        else:
            symbols = args[1:]
            result = get_batch_quotes(symbols)

    elif command == "historical":
        if len(args) < 4:
            result = {"error": "Usage: python yfinance_data.py historical <symbol> <start_date> <end_date> [interval]"}
        else:
            symbol = args[1]
            start_date = args[2]
            end_date = args[3]
            interval = args[4] if len(args) > 4 else '1d'
            result = get_historical(symbol, start_date, end_date, interval)

    elif command == "info":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py info <symbol>"}
        else:
            symbol = args[1]
            result = get_info(symbol)

    elif command == "financials":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py financials <symbol>"}
        else:
            symbol = args[1]
            result = get_financials(symbol)

    elif command == "company_profile":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py company_profile <symbol>"}
        else:
            symbol = args[1]
            result = get_company_profile(symbol)

    elif command == "financial_ratios":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py financial_ratios <symbol>"}
        else:
            symbol = args[1]
            result = get_financial_ratios(symbol)

    elif command == "multiple_profiles":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py multiple_profiles <symbol1,symbol2,...>"}
        else:
            symbols = args[1].split(",")
            result = get_multiple_profiles(symbols)

    elif command == "multiple_ratios":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py multiple_ratios <symbol1,symbol2,...>"}
        else:
            symbols = args[1].split(",")
            result = get_multiple_ratios(symbols)

    elif command == "search":
        if len(args) < 2:
            result = {"error": "Usage: python yfinance_data.py search <query> [limit]"}
        else:
            query = args[1]
            limit = int(args[2]) if len(args) > 2 else 50
            result = search_symbols(query, limit)

    else:
        result = {"error": f"Unknown command: {command}"}

    # Return JSON for worker pool, print for subprocess/CLI
    output = json.dumps(result, indent=2)
    print(output)
    return output

if __name__ == "__main__":
    main()
