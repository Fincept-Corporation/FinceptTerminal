import requests
import pandas as pd
from themes import console

def fetch_equities_data():
    equities_url = "https://fincept.share.zrok.io/FinanceDB/equities/data"
    
    try:
        response = requests.get(equities_url)
        response.raise_for_status()
        return pd.DataFrame(response.json())
    except requests.exceptions.RequestException as e:
        console.print(f"[bold red]Error fetching equities data: {e}[/bold red]", justify="left")
        return pd.DataFrame()
