import os
import requests
from dotenv import load_dotenv

load_dotenv()

ALPHA_VANTAGE_API_KEY = os.getenv("ALPHA_VANTAGE_API_KEY")
ALPHA_VANTAGE_URL = "https://www.alphavantage.co/query"

def fetch_stock_data(symbol):
    """
    Fetch stock data for the given symbol using Alpha Vantage API.

    :param symbol: Stock ticker symbol (e.g., 'AAPL', 'MSFT').
    :return: Formatted stock data or an error message.
    """
    if not ALPHA_VANTAGE_API_KEY or ALPHA_VANTAGE_API_KEY.strip() == "":
        return "Alpha Vantage API key is missing or empty. Please set ALPHA_VANTAGE_API_KEY in the .env file."

    params = {
        "function": "TIME_SERIES_INTRADAY",
        "symbol": symbol,
        "interval": "5min",  # Fetch data every 5 minutes
        "apikey": ALPHA_VANTAGE_API_KEY,
    }

    try:
        response = requests.get(ALPHA_VANTAGE_URL, params=params)
        response.raise_for_status()  # Raise exception for HTTP errors

        data = response.json()

        # Check for errors in the response
        if "Error Message" in data:
            return f"Error: {data['Error Message']}"

        if "Time Series (5min)" not in data:
            return "No time series data available. Please check the symbol or API key."

        # Extract and format the most recent data
        latest_time = sorted(data["Time Series (5min)"].keys())[-1]
        stock_data = data["Time Series (5min)"][latest_time]

        return {
            "symbol": symbol.upper(),
            "time": latest_time,
            "open": stock_data["1. open"],
            "high": stock_data["2. high"],
            "low": stock_data["3. low"],
            "close": stock_data["4. close"],
            "volume": stock_data["5. volume"],
        }

    except requests.exceptions.RequestException as e:
        return f"Error fetching stock data: {e}"


from fincept_terminal.themes import console
from rich.table import Table

def display_stock_data():
    """Prompt user for a stock symbol and display its data."""
    from rich.prompt import Prompt
    symbol = Prompt.ask("Enter the stock ticker symbol (e.g., AAPL, MSFT)")

    console.print(f"\n[bold cyan]Fetching data for {symbol.upper()}...[/bold cyan]")
    stock_data = fetch_stock_data(symbol)

    if isinstance(stock_data, str):  # Handle error messages as strings
        console.print(f"[bold red]{stock_data}[/bold red]")
        return

    # Display the stock data in a table
    table = Table(title=f"Stock Data: {stock_data['symbol']}", header_style="bold green")
    table.add_column("Time", justify="left", style="cyan", no_wrap=True)
    table.add_column("Open", justify="right", style="white", no_wrap=True)
    table.add_column("High", justify="right", style="white", no_wrap=True)
    table.add_column("Low", justify="right", style="white", no_wrap=True)
    table.add_column("Close", justify="right", style="white", no_wrap=True)
    table.add_column("Volume", justify="right", style="yellow", no_wrap=True)

    table.add_row(
        stock_data["time"],
        stock_data["open"],
        stock_data["high"],
        stock_data["low"],
        stock_data["close"],
        stock_data["volume"],
    )

    console.print(table)
