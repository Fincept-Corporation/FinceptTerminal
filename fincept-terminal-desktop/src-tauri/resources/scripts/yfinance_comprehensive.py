"""
YFinance Comprehensive Data Fetcher
Fetches all types of data from Yahoo Finance using yfinance
Supports: quotes, historical data, financials, options, dividends, splits, etc.
Returns JSON output for Rust/Node integration
"""

import sys
import json
import yfinance as yf
import pandas as pd
from datetime import datetime

def get_historical_data(symbol, period='1mo', interval='1d', start=None, end=None,
                       prepost=False, auto_adjust=True, actions=True):
    """Fetch historical OHLCV data"""
    try:
        ticker = yf.Ticker(symbol)

        if start and end:
            hist = ticker.history(start=start, end=end, interval=interval,
                                 prepost=prepost, auto_adjust=auto_adjust, actions=actions)
        else:
            hist = ticker.history(period=period, interval=interval,
                                 prepost=prepost, auto_adjust=auto_adjust, actions=actions)

        if hist.empty:
            return {"error": "No data found", "symbol": symbol}

        data = []
        for index, row in hist.iterrows():
            item = {
                "timestamp": int(index.timestamp() * 1000),
                "datetime": index.isoformat(),
                "open": round(float(row['Open']), 2) if 'Open' in row and pd.notna(row['Open']) else None,
                "high": round(float(row['High']), 2) if 'High' in row and pd.notna(row['High']) else None,
                "low": round(float(row['Low']), 2) if 'Low' in row and pd.notna(row['Low']) else None,
                "close": round(float(row['Close']), 2) if 'Close' in row and pd.notna(row['Close']) else None,
                "volume": int(row['Volume']) if 'Volume' in row and pd.notna(row['Volume']) else None,
            }
            if not auto_adjust and 'Adj Close' in row:
                item["adjClose"] = round(float(row['Adj Close']), 2)
            if 'Dividends' in row and pd.notna(row['Dividends']) and row['Dividends'] > 0:
                item["dividend"] = round(float(row['Dividends']), 4)
            if 'Stock Splits' in row and pd.notna(row['Stock Splits']) and row['Stock Splits'] > 0:
                item["split"] = float(row['Stock Splits'])
            data.append(item)

        return data
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_quote(symbol):
    """Fetch real-time quote"""
    try:
        ticker = yf.Ticker(symbol)
        info = ticker.info

        return {
            "symbol": symbol,
            "price": info.get('currentPrice') or info.get('regularMarketPrice'),
            "change": info.get('regularMarketChange'),
            "changePercent": info.get('regularMarketChangePercent'),
            "open": info.get('regularMarketOpen') or info.get('open'),
            "high": info.get('dayHigh') or info.get('regularMarketDayHigh'),
            "low": info.get('dayLow') or info.get('regularMarketDayLow'),
            "previousClose": info.get('previousClose') or info.get('regularMarketPreviousClose'),
            "volume": info.get('volume') or info.get('regularMarketVolume'),
            "marketCap": info.get('marketCap'),
            "timestamp": int(datetime.now().timestamp() * 1000),
        }
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_info(symbol):
    """Fetch comprehensive company information"""
    try:
        ticker = yf.Ticker(symbol)
        info = ticker.info

        return {
            "symbol": symbol,
            "shortName": info.get('shortName'),
            "longName": info.get('longName'),
            "sector": info.get('sector'),
            "industry": info.get('industry'),
            "fullTimeEmployees": info.get('fullTimeEmployees'),
            "website": info.get('website'),
            "summary": info.get('longBusinessSummary'),
            "city": info.get('city'),
            "state": info.get('state'),
            "country": info.get('country'),
            "marketCap": info.get('marketCap'),
            "beta": info.get('beta'),
            "trailingPE": info.get('trailingPE'),
            "forwardPE": info.get('forwardPE'),
            "dividendYield": info.get('dividendYield'),
            "fiftyTwoWeekHigh": info.get('fiftyTwoWeekHigh'),
            "fiftyTwoWeekLow": info.get('fiftyTwoWeekLow'),
            "fiftyDayAverage": info.get('fiftyDayAverage'),
            "twoHundredDayAverage": info.get('twoHundredDayAverage'),
            "timestamp": int(datetime.now().timestamp() * 1000),
        }
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_financials(symbol, frequency='annual'):
    """Fetch income statement"""
    try:
        ticker = yf.Ticker(symbol)
        financials = ticker.quarterly_financials if frequency == 'quarterly' else ticker.financials

        if financials is None or financials.empty:
            return {"error": "No financial data found", "symbol": symbol}

        data = []
        for col in financials.columns:
            period_data = {"period": col.isoformat()}
            for idx in financials.index:
                value = financials.loc[idx, col]
                if pd.notna(value):
                    key = str(idx).replace(' ', '_').lower()
                    period_data[key] = float(value)
            data.append(period_data)

        return data
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_balance_sheet(symbol, frequency='annual'):
    """Fetch balance sheet"""
    try:
        ticker = yf.Ticker(symbol)
        balance_sheet = ticker.quarterly_balance_sheet if frequency == 'quarterly' else ticker.balance_sheet

        if balance_sheet is None or balance_sheet.empty:
            return {"error": "No balance sheet data found", "symbol": symbol}

        data = []
        for col in balance_sheet.columns:
            period_data = {"period": col.isoformat()}
            for idx in balance_sheet.index:
                value = balance_sheet.loc[idx, col]
                if pd.notna(value):
                    key = str(idx).replace(' ', '_').lower()
                    period_data[key] = float(value)
            data.append(period_data)

        return data
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_cash_flow(symbol, frequency='annual'):
    """Fetch cash flow statement"""
    try:
        ticker = yf.Ticker(symbol)
        cash_flow = ticker.quarterly_cashflow if frequency == 'quarterly' else ticker.cashflow

        if cash_flow is None or cash_flow.empty:
            return {"error": "No cash flow data found", "symbol": symbol}

        data = []
        for col in cash_flow.columns:
            period_data = {"period": col.isoformat()}
            for idx in cash_flow.index:
                value = cash_flow.loc[idx, col]
                if pd.notna(value):
                    key = str(idx).replace(' ', '_').lower()
                    period_data[key] = float(value)
            data.append(period_data)

        return data
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_earnings(symbol):
    """Fetch earnings data"""
    try:
        ticker = yf.Ticker(symbol)
        earnings = ticker.quarterly_earnings

        if earnings is None or earnings.empty:
            return {"error": "No earnings data found", "symbol": symbol}

        data = []
        for index, row in earnings.iterrows():
            data.append({
                "period": str(index),
                "revenue": float(row['Revenue']) if 'Revenue' in row and pd.notna(row['Revenue']) else None,
                "earnings": float(row['Earnings']) if 'Earnings' in row and pd.notna(row['Earnings']) else None,
            })

        return data
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_dividends(symbol):
    """Fetch dividend history"""
    try:
        ticker = yf.Ticker(symbol)
        dividends = ticker.dividends

        if dividends is None or dividends.empty:
            return []

        data = []
        for index, value in dividends.items():
            data.append({
                "date": index.isoformat().split('T')[0],
                "dividend": round(float(value), 4),
            })

        return data
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_splits(symbol):
    """Fetch stock split history"""
    try:
        ticker = yf.Ticker(symbol)
        splits = ticker.splits

        if splits is None or splits.empty:
            return []

        data = []
        for index, value in splits.items():
            ratio_parts = str(value).split(':') if ':' in str(value) else [str(value), '1']
            data.append({
                "date": index.isoformat().split('T')[0],
                "ratio": f"{value}:1",
                "numerator": float(value),
                "denominator": 1,
            })

        return data
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_actions(symbol):
    """Fetch all corporate actions (dividends + splits)"""
    try:
        ticker = yf.Ticker(symbol)
        actions = ticker.actions

        if actions is None or actions.empty:
            return []

        data = []
        for index, row in actions.iterrows():
            item = {"date": index.isoformat().split('T')[0]}
            if 'Dividends' in row and pd.notna(row['Dividends']) and row['Dividends'] > 0:
                item["type"] = "dividend"
                item["dividend"] = round(float(row['Dividends']), 4)
            if 'Stock Splits' in row and pd.notna(row['Stock Splits']) and row['Stock Splits'] > 0:
                item["type"] = "split"
                item["ratio"] = f"{row['Stock Splits']}:1"
                item["numerator"] = float(row['Stock Splits'])
                item["denominator"] = 1
            if item.get("type"):
                data.append(item)

        return data
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_recommendations(symbol):
    """Fetch analyst recommendations"""
    try:
        ticker = yf.Ticker(symbol)
        recommendations = ticker.recommendations

        if recommendations is None or recommendations.empty:
            return []

        data = []
        for index, row in recommendations.iterrows():
            data.append({
                "date": index.isoformat().split('T')[0],
                "firm": str(row['Firm']) if 'Firm' in row else None,
                "toGrade": str(row['To Grade']) if 'To Grade' in row else None,
                "fromGrade": str(row['From Grade']) if 'From Grade' in row else None,
                "action": str(row['Action']) if 'Action' in row else None,
            })

        return data[-10:]  # Return last 10 recommendations
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_options(symbol, expiration=None):
    """Fetch options chain"""
    try:
        ticker = yf.Ticker(symbol)

        # Get available expiration dates
        expirations = ticker.options
        if not expirations:
            return {"error": "No options data available", "symbol": symbol}

        # Use provided expiration or nearest one
        exp_date = expiration if expiration in expirations else expirations[0]

        # Get options chain
        opt = ticker.option_chain(exp_date)

        data = []

        # Process calls
        for _, row in opt.calls.iterrows():
            data.append({
                "type": "call",
                "expiration": exp_date,
                "strike": float(row['strike']),
                "lastPrice": round(float(row['lastPrice']), 2) if pd.notna(row['lastPrice']) else None,
                "bid": round(float(row['bid']), 2) if pd.notna(row['bid']) else None,
                "ask": round(float(row['ask']), 2) if pd.notna(row['ask']) else None,
                "volume": int(row['volume']) if pd.notna(row['volume']) else None,
                "openInterest": int(row['openInterest']) if pd.notna(row['openInterest']) else None,
                "impliedVolatility": round(float(row['impliedVolatility']), 4) if pd.notna(row['impliedVolatility']) else None,
            })

        # Process puts
        for _, row in opt.puts.iterrows():
            data.append({
                "type": "put",
                "expiration": exp_date,
                "strike": float(row['strike']),
                "lastPrice": round(float(row['lastPrice']), 2) if pd.notna(row['lastPrice']) else None,
                "bid": round(float(row['bid']), 2) if pd.notna(row['bid']) else None,
                "ask": round(float(row['ask']), 2) if pd.notna(row['ask']) else None,
                "volume": int(row['volume']) if pd.notna(row['volume']) else None,
                "openInterest": int(row['openInterest']) if pd.notna(row['openInterest']) else None,
                "impliedVolatility": round(float(row['impliedVolatility']), 4) if pd.notna(row['impliedVolatility']) else None,
            })

        return data
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_institutional_holders(symbol):
    """Fetch institutional holders"""
    try:
        ticker = yf.Ticker(symbol)
        holders = ticker.institutional_holders

        if holders is None or holders.empty:
            return []

        data = []
        for _, row in holders.iterrows():
            data.append({
                "holder": str(row['Holder']) if 'Holder' in row else None,
                "shares": int(row['Shares']) if 'Shares' in row and pd.notna(row['Shares']) else None,
                "dateReported": str(row['Date Reported']) if 'Date Reported' in row else None,
                "percentOut": round(float(row['% Out']) * 100, 2) if '% Out' in row and pd.notna(row['% Out']) else None,
                "value": int(row['Value']) if 'Value' in row and pd.notna(row['Value']) else None,
            })

        return data
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_major_holders(symbol):
    """Fetch major holders breakdown"""
    try:
        ticker = yf.Ticker(symbol)
        holders = ticker.major_holders

        if holders is None or holders.empty:
            return {"error": "No major holders data found", "symbol": symbol}

        data = {}
        for _, row in holders.iterrows():
            key = str(row[1]).replace(' ', '_').replace('%', 'percent').lower()
            value = str(row[0]).replace('%', '')
            try:
                data[key] = float(value)
            except:
                data[key] = value

        return data
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def get_sustainability(symbol):
    """Fetch ESG/sustainability scores"""
    try:
        ticker = yf.Ticker(symbol)
        sustainability = ticker.sustainability

        if sustainability is None or sustainability.empty:
            return {"error": "No sustainability data found", "symbol": symbol}

        data = {}
        for idx in sustainability.index:
            value = sustainability.loc[idx].iloc[0]
            if pd.notna(value):
                key = str(idx).replace(' ', '_').lower()
                data[key] = float(value) if isinstance(value, (int, float)) else str(value)

        return data
    except Exception as e:
        return {"error": str(e), "symbol": symbol}

def main(args=None):
    """Main entry point"""
    if args is None:
        args = sys.argv[1:]

    if len(args) < 2:
        return json.dumps({"error": "Usage: python yfinance_comprehensive.py <symbol> <operation> [args...]"})

    symbol = args[0]
    operation = args[1]
    result = None

    try:
        if operation == "history":
            params = {}
            for i in range(2, len(args), 2):
                if i + 1 < len(args):
                    key = args[i].replace('--', '')
                    value = args[i + 1]
                    # Convert boolean strings
                    if value.lower() in ['true', 'false']:
                        value = value.lower() == 'true'
                    params[key] = value
            result = get_historical_data(symbol, **params)

        elif operation == "quote":
            result = get_quote(symbol)

        elif operation == "info":
            result = get_info(symbol)

        elif operation == "financials":
            frequency = args[2] if len(args) > 2 else 'annual'
            result = get_financials(symbol, frequency)

        elif operation == "balanceSheet":
            frequency = args[2] if len(args) > 2 else 'annual'
            result = get_balance_sheet(symbol, frequency)

        elif operation == "cashFlow":
            frequency = args[2] if len(args) > 2 else 'annual'
            result = get_cash_flow(symbol, frequency)

        elif operation == "earnings":
            result = get_earnings(symbol)

        elif operation == "dividends":
            result = get_dividends(symbol)

        elif operation == "splits":
            result = get_splits(symbol)

        elif operation == "actions":
            result = get_actions(symbol)

        elif operation == "recommendations":
            result = get_recommendations(symbol)

        elif operation == "options":
            expiration = args[2] if len(args) > 2 else None
            result = get_options(symbol, expiration)

        elif operation == "institutionalHolders":
            result = get_institutional_holders(symbol)

        elif operation == "majorHolders":
            result = get_major_holders(symbol)

        elif operation == "sustainability":
            result = get_sustainability(symbol)

        else:
            result = {"error": f"Unknown operation: {operation}"}

    except Exception as e:
        result = {"error": str(e), "symbol": symbol, "operation": operation}

    output = json.dumps(result, indent=2)
    print(output)
    return output

if __name__ == "__main__":
    main()
