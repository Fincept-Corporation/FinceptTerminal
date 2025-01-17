# data.py
import requests
import pandas as pd
from time import sleep

# Updated mapping of continents to countries with no 'Others' category
continent_countries = {
    "Asia": [
        "Afghanistan", "Bangladesh", "Cambodia", "China", "India", "Indonesia", "Japan", "Kazakhstan", 
        "Kyrgyzstan", "Malaysia", "Mongolia", "Myanmar", "Philippines", "Singapore", "South Korea", "Taiwan", 
        "Thailand", "Vietnam"
    ],
    "Europe": [
        "Austria", "Azerbaijan", "Belgium", "Cyprus", "Czech Republic", "Denmark", "Estonia", "Finland", 
        "France", "Germany", "Greece", "Hungary", "Iceland", "Ireland", "Italy", "Latvia", "Lithuania", 
        "Luxembourg", "Malta", "Monaco", "Netherlands", "Norway", "Poland", "Portugal", "Romania", "Russia", 
        "Slovakia", "Slovenia", "Spain", "Sweden", "Switzerland", "United Kingdom"
    ],
    "Africa": [
        "Botswana", "Egypt", "Gabon", "Ghana", "Ivory Coast", "Kenya", "Morocco", "Mozambique", "Namibia", 
        "Nigeria", "South Africa", "Zambia"
    ],
    "North America": [
        "Bahamas", "Barbados", "Belize", "Bermuda", "Canada", "Cayman Islands", "Costa Rica", "Mexico", 
        "Panama", "United States"
    ],
    "South America": [
        "Argentina", "Brazil", "Chile", "Colombia", "Peru", "Uruguay"
    ],
    "Oceania": [
        "Australia", "Fiji", "New Zealand", "Papua New Guinea"
    ],
    "Middle East": [
        "Israel", "Jordan", "Saudi Arabia", "United Arab Emirates", "Qatar"
    ]
}

def fetch_equities_data(retries=3, delay=2):
    equities_url = "https://fincept.share.zrok.io/FinanceDB/equities/data"
    for attempt in range(retries):
        try:
            response = requests.get(equities_url)
            response.raise_for_status()
            return pd.DataFrame(response.json())
        except requests.exceptions.RequestException:
            if attempt < retries - 1:
                # Wait for the specified delay before retrying
                sleep(delay)
            else:
                # If all retries are exhausted, return an empty DataFrame silently
                return pd.DataFrame()


def get_countries_by_continent(continent):
    """
    Returns the list of countries for the given continent.
    """
    return continent_countries.get(continent, [])
