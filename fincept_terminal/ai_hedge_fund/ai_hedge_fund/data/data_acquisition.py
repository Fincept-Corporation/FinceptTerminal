import logging
import yfinance as yf
import pandas as pd

def get_historical_data(ticker: str, start_date: str, end_date: str) -> pd.DataFrame:
    logging.getLogger(__name__).info("Fetching historical data for %s", ticker)
    return yf.Ticker(ticker).history(start=start_date, end=end_date)

