from .themes import console
from .utilities import display_fii_dii_table
from .data import fetch_equities_data  
import requests
import pandas as pd 
import urllib.parse

def fetch_sectors_by_country(country):
    """
    Fetch available sectors for the selected country using the provided API.
    """
    url = f"https://fincept.share.zrok.io/FinanceDB/equities/sectors_and_industries_and_stocks?filter_column=country&filter_value={country}"
    
    try:
        response = requests.get(url)
        response.raise_for_status()
        data = response.json()
        return data.get("sectors", [])
    except requests.exceptions.RequestException as e:
        console.print(f"[bold red]Error fetching sectors for {country}: {e}[/bold red]")
        return []

def fetch_industries_by_sector(country, sector):
    """
    Fetch available industries for the selected sector in the given country.
    """
    url = f"https://fincept.share.zrok.io/FinanceDB/equities/sectors_and_industries_and_stocks?filter_column=country&filter_value={country}&sector={sector.replace(' ', '%20')}"
    
    try:
        response = requests.get(url)
        response.raise_for_status()
        data = response.json()
        return data.get("industries", [])
    except requests.exceptions.RequestException as e:
        console.print(f"[bold red]Error fetching industries for {sector} in {country}: {e}[/bold red]")
        return []

def fetch_stocks_by_industry(country, sector, industry):
    """
    Fetch available stocks for the selected industry in the given sector and country.
    """
    # URL encode the sector and industry to handle special characters
    sector_encoded = urllib.parse.quote(sector)
    industry_encoded = urllib.parse.quote(industry)
    
    url = f"https://fincept.share.zrok.io/FinanceDB/equities/sectors_and_industries_and_stocks?filter_column=country&filter_value={country}&sector={sector_encoded}&industry={industry_encoded}"

    try:
        response = requests.get(url)
        response.raise_for_status()
        stock_data = response.json()

        if not stock_data:  # Check if the response is empty
            console.print(f"[bold red]No stocks found for {industry} in {sector}, {country}.[/bold red]")
            return pd.DataFrame()

        return pd.DataFrame(stock_data)
    except requests.exceptions.RequestException as e:
        console.print(f"[bold red]Error fetching stocks for {industry} in {sector}, {country}: {e}[/bold red]")
        return pd.DataFrame()  # Return an empty DataFrame on failure


def display_fii_dii_data():
    fii_dii_url = "https://fincept.share.zrok.io/IndiaStockExchange/fii_dii_data/data"
    
    try:
        response = requests.get(fii_dii_url)
        response.raise_for_status()
        fii_dii_data = response.json()

        display_fii_dii_table(fii_dii_data)

    except requests.exceptions.RequestException as e:
        console.print(f"[bold red]Error fetching FII/DII data: {e}[/bold red]", justify="left")

def fetch_equities_by_country(country):
    """
    Fetch stock data filtered by country using the provided API.
    """
    url = f"https://fincept.share.zrok.io/FinanceDB/equities/filter?column=country&filter_value={country}"
    
    try:
        response = requests.get(url)
        response.raise_for_status()  # Raise an error for bad HTTP responses
        return pd.DataFrame(response.json())  # Convert JSON response to DataFrame
    except requests.exceptions.RequestException as e:
        console.print(f"[bold red]Error fetching stock data for {country}: {e}[/bold red]")
        return pd.DataFrame()  # Return empty DataFrame on failure