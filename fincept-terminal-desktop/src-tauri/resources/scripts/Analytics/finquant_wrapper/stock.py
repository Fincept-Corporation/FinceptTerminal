import pandas as pd
import numpy as np
from typing import Dict, Optional, Union
import json
from finquant.stock import Stock

def create_stock(
    ticker: str,
    start_date: str,
    end_date: Optional[str] = None,
    data: Optional[Union[pd.DataFrame, Dict]] = None
) -> Dict:
    if data is not None:
        if isinstance(data, dict):
            data = pd.DataFrame(data)
        stock = Stock(investmentinfo={'Name': ticker}, data=data)
    else:
        import yfinance as yf
        df = yf.download(ticker, start=start_date, end=end_date, progress=False)
        if df.empty:
            return {
                'success': False,
                'error': 'No data available for ticker {}'.format(ticker)
            }
        stock = Stock(investmentinfo={'Name': ticker}, data=df)

    return {
        'success': True,
        'ticker': ticker,
        'start_date': start_date,
        'end_date': end_date,
        'data_points': len(stock.data) if hasattr(stock, 'data') else 0
    }

def get_stock_properties(
    ticker: str,
    start_date: str,
    end_date: Optional[str] = None,
    data: Optional[Union[pd.DataFrame, Dict]] = None,
    freq: int = 252
) -> Dict:
    if data is not None:
        if isinstance(data, dict):
            data = pd.DataFrame(data)
        stock = Stock(investmentinfo={'Name': ticker}, data=data)
    else:
        import yfinance as yf
        df = yf.download(ticker, start=start_date, end=end_date, progress=False)
        if df.empty:
            return {
                'success': False,
                'error': 'No data available for ticker {}'.format(ticker)
            }
        stock = Stock(investmentinfo={'Name': ticker}, data=df)

    return {
        'ticker': ticker,
        'expected_return': float(stock.comp_expected_return(freq=freq).values[0]),
        'volatility': float(stock.comp_volatility(freq=freq)),
        'data_points': len(stock.data) if hasattr(stock, 'data') else 0
    }

def calculate_stock_metrics(
    ticker: str,
    start_date: str,
    end_date: Optional[str] = None,
    data: Optional[Union[pd.DataFrame, Dict]] = None,
    market_data: Optional[Union[pd.Series, Dict]] = None,
    freq: int = 252
) -> Dict:
    if data is not None:
        if isinstance(data, dict):
            data = pd.DataFrame(data)
        stock = Stock(investmentinfo={'Name': ticker}, data=data)
    else:
        import yfinance as yf
        df = yf.download(ticker, start=start_date, end=end_date, progress=False)
        if df.empty:
            return {
                'success': False,
                'error': 'No data available for ticker {}'.format(ticker)
            }
        stock = Stock(investmentinfo={'Name': ticker}, data=df)

    metrics = {
        'ticker': ticker,
        'expected_return': float(stock.comp_expected_return(freq=freq).values[0]),
        'volatility': float(stock.comp_volatility(freq=freq)),
    }

    if market_data is not None:
        if isinstance(market_data, dict):
            market_data = pd.Series(market_data)

        daily_returns = stock.comp_daily_returns()
        beta = stock.comp_beta(market_data)
        metrics['beta'] = float(beta)

        rsquared = stock.comp_rsquared(market_data)
        metrics['r_squared'] = float(rsquared)

    return metrics

def calculate_stock_beta(
    ticker: str,
    market_ticker: str,
    start_date: str,
    end_date: Optional[str] = None,
    stock_data: Optional[Union[pd.DataFrame, Dict]] = None,
    market_data: Optional[Union[pd.Series, Dict]] = None
) -> Dict:
    if stock_data is not None:
        if isinstance(stock_data, dict):
            stock_data = pd.DataFrame(stock_data)
        stock = Stock(investmentinfo={'Name': ticker}, data=stock_data)
    else:
        import yfinance as yf
        df = yf.download(ticker, start=start_date, end=end_date, progress=False)
        if df.empty:
            return {
                'success': False,
                'error': 'No data available for ticker {}'.format(ticker)
            }
        stock = Stock(investmentinfo={'Name': ticker}, data=df)

    if market_data is not None:
        if isinstance(market_data, dict):
            market_data = pd.Series(market_data)
    else:
        import yfinance as yf
        market_df = yf.download(market_ticker, start=start_date, end=end_date, progress=False)
        if market_df.empty:
            return {
                'success': False,
                'error': 'No data available for market ticker {}'.format(market_ticker)
            }
        from finquant.returns import daily_returns
        market_data = daily_returns(market_df).iloc[:, 0]

    daily_ret = stock.comp_daily_returns()
    beta = stock.comp_beta(market_data)
    rsquared = stock.comp_rsquared(market_data)

    return {
        'ticker': ticker,
        'market_ticker': market_ticker,
        'beta': float(beta),
        'r_squared': float(rsquared)
    }

def calculate_stock_returns(
    ticker: str,
    start_date: str,
    end_date: Optional[str] = None,
    data: Optional[Union[pd.DataFrame, Dict]] = None
) -> Dict:
    if data is not None:
        if isinstance(data, dict):
            data = pd.DataFrame(data)
        stock = Stock(investmentinfo={'Name': ticker}, data=data)
    else:
        import yfinance as yf
        df = yf.download(ticker, start=start_date, end=end_date, progress=False)
        if df.empty:
            return {
                'success': False,
                'error': 'No data available for ticker {}'.format(ticker)
            }
        stock = Stock(investmentinfo={'Name': ticker}, data=df)

    daily_ret = stock.comp_daily_returns()

    return {
        'ticker': ticker,
        'daily_returns': daily_ret.to_dict(),
        'index': daily_ret.index.astype(str).tolist()
    }

def main():
    print("Testing FinQuant Stock Wrapper")

    ticker = 'AAPL'
    start_date = '2023-01-01'
    end_date = '2023-12-31'

    print("\n1. Testing get_stock_properties...")
    try:
        result = get_stock_properties(ticker, start_date, end_date)
        print("Ticker: {}".format(result['ticker']))
        print("Expected Return: {:.4f}".format(result['expected_return']))
        print("Volatility: {:.4f}".format(result['volatility']))
        print("Data points: {}".format(result['data_points']))
        print("Test 1: PASSED")
    except Exception as e:
        print("Test 1: FAILED - {}".format(str(e)))
        return

    print("\n2. Testing calculate_stock_beta...")
    try:
        result = calculate_stock_beta(ticker, 'SPY', start_date, end_date)
        print("Ticker: {}".format(result['ticker']))
        print("Market: {}".format(result['market_ticker']))
        print("Beta: {:.4f}".format(result['beta']))
        print("R-squared: {:.4f}".format(result['r_squared']))
        print("Test 2: PASSED")
    except Exception as e:
        print("Test 2: FAILED - {}".format(str(e)))
        return

    print("\nAll tests: PASSED")

if __name__ == "__main__":
    main()
