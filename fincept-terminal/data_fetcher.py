from themes import console
from utilities import display_fii_dii_table
from data import fetch_equities_data  
import requests

def display_fii_dii_data():
    fii_dii_url = "https://fincept.share.zrok.io/IndiaStockExchange/fii_dii_data/data"
    
    try:
        response = requests.get(fii_dii_url)
        response.raise_for_status()
        fii_dii_data = response.json()

        display_fii_dii_table(fii_dii_data)

    except requests.exceptions.RequestException as e:
        console.print(f"[bold red]Error fetching FII/DII data: {e}[/bold red]", justify="left")
