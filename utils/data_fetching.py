import pandas as pd
import yfinance as yf
import requests
from difflib import get_close_matches

# Load the updated equities.csv file
equities_df = pd.read_csv(r'C:\Projects\FinceptInvestments\fincept\data\updated_equities.csv')

def fetch_fii_dii_data():
    fii_dii_url = "https://fincept.share.zrok.io/IndiaStockExchange/fii_dii_data/data"
    try:
        response = requests.get(fii_dii_url)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.RequestException as e:
        return None

def match_symbol(input_name, df):
    symbols = df['symbol'].tolist()
    names = df['name'].tolist()

    closest_symbol = get_close_matches(input_name.upper(), symbols, n=1)
    if closest_symbol:
        return closest_symbol[0]

    closest_name = get_close_matches(input_name.lower(), [name.lower() for name in names], n=1)
    if closest_name:
        symbol = df[df['name'].str.lower() == closest_name[0]].iloc[0]['symbol']
        return symbol

    return None

def fetch_detailed_data(symbol):
    stock = yf.Ticker(symbol)
    return stock.info
