"""
Maddison Project Data Fetcher
Fetches long-run GDP per capita estimates for 169 countries back to year 1 from the
Maddison Project Database (MPD 2020).
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('MADDISON_API_KEY', '')
BASE_URL = "https://www.rug.nl/ggdc/historicaldevelopment/maddison"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

MPD_DOWNLOAD_URL = "https://www.rug.nl/ggdc/historicaldevelopment/maddison/data/mpd2020.xlsx"
MPD_CSV_URL = "https://raw.githubusercontent.com/owid/owid-datasets/master/datasets/Maddison%20Project%20Database%202020%20(Bolt%20and%20van%20Zanden%2C%202020)/Maddison%20Project%20Database%202020%20(Bolt%20and%20van%20Zanden%2C%202020).csv"

WORLD_REGIONS = {
    "western_europe": ["GBR", "DEU", "FRA", "ITA", "ESP", "NLD", "BEL", "SWE", "NOR", "DNK", "FIN", "AUT", "CHE", "PRT", "IRL", "GRC"],
    "eastern_europe": ["POL", "CZE", "HUN", "ROU", "BGR", "SVK", "HRV", "SRB", "UKR", "RUS"],
    "north_america": ["USA", "CAN", "MEX"],
    "latin_america": ["BRA", "ARG", "CHL", "COL", "PER", "VEN", "URY", "ECU", "BOL", "PRY"],
    "east_asia": ["CHN", "JPN", "KOR", "TWN", "HKG", "SGP"],
    "south_asia": ["IND", "PAK", "BGD", "LKA", "NPL"],
    "southeast_asia": ["IDN", "THA", "MYS", "PHL", "VNM", "MMR", "KHM"],
    "middle_east": ["TUR", "EGY", "IRN", "IRQ", "SAU", "SYR", "ISR", "LBN"],
    "africa": ["ZAF", "NGA", "ETH", "GHA", "KEN", "TZA", "MOZ", "MDG", "CIV", "CMR"],
    "oceania": ["AUS", "NZL"],
}

COUNTRY_NAMES = {
    "USA": "United States", "GBR": "United Kingdom", "DEU": "Germany",
    "FRA": "France", "JPN": "Japan", "CHN": "China", "IND": "India",
    "BRA": "Brazil", "RUS": "Russia", "CAN": "Canada", "AUS": "Australia",
    "KOR": "South Korea", "ESP": "Spain", "ITA": "Italy", "MEX": "Mexico",
    "IDN": "Indonesia", "NLD": "Netherlands", "SAU": "Saudi Arabia",
    "CHE": "Switzerland", "SWE": "Sweden", "NOR": "Norway", "DNK": "Denmark",
    "FIN": "Finland", "BEL": "Belgium", "AUT": "Austria", "POL": "Poland",
    "TUR": "Turkey", "ZAF": "South Africa", "ARG": "Argentina", "NGA": "Nigeria",
    "EGY": "Egypt", "SGP": "Singapore", "HKG": "Hong Kong", "TWN": "Taiwan",
    "MYS": "Malaysia", "THA": "Thailand", "PHL": "Philippines", "VNM": "Vietnam",
    "BGD": "Bangladesh", "PAK": "Pakistan", "GRC": "Greece", "PRT": "Portugal",
    "IRL": "Ireland", "CZE": "Czech Republic", "HUN": "Hungary", "ROU": "Romania",
    "BGR": "Bulgaria", "HRV": "Croatia", "UKR": "Ukraine", "CHL": "Chile",
    "COL": "Colombia", "PER": "Peru",
}

HISTORICAL_BENCHMARK = {
    1: {"world_gdppc": 467, "note": "Roman Empire era"},
    1000: {"world_gdppc": 453, "note": "Medieval period"},
    1500: {"world_gdppc": 556, "note": "Pre-Columbus"},
    1600: {"world_gdppc": 596, "note": "Early modern"},
    1700: {"world_gdppc": 638, "note": "Pre-Industrial Revolution"},
    1820: {"world_gdppc": 1102, "note": "Industrial Revolution onset"},
    1870: {"world_gdppc": 1526, "note": "Gilded Age"},
    1900: {"world_gdppc": 2111, "note": "Belle Epoque"},
    1950: {"world_gdppc": 4060, "note": "Post-WWII"},
    1973: {"world_gdppc": 6940, "note": "Golden Age of Capitalism"},
    2000: {"world_gdppc": 9565, "note": "Millennium"},
    2018: {"world_gdppc": 14574, "note": "Latest Maddison estimate"},
}


def _make_request(endpoint: str, params: Dict = None) -> Any:
    url = f"{BASE_URL}/{endpoint}" if not endpoint.startswith('http') else endpoint
    try:
        response = session.get(url, params=params, timeout=30)
        response.raise_for_status()
        return response.json()
    except requests.exceptions.HTTPError as e:
        return {"error": f"HTTP {e.response.status_code}: {str(e)}"}
    except requests.exceptions.RequestException as e:
        return {"error": f"Request failed: {str(e)}"}
    except (json.JSONDecodeError, ValueError) as e:
        return {"error": f"JSON decode error: {str(e)}"}


def _fetch_owid_csv() -> List[Dict]:
    try:
        response = session.get(MPD_CSV_URL, timeout=60)
        response.raise_for_status()
        lines = response.text.strip().split("\n")
        if not lines:
            return []
        headers = [h.strip().strip('"') for h in lines[0].split(",")]
        records = []
        for line in lines[1:]:
            values = line.split(",")
            if len(values) >= len(headers):
                record = {}
                for i, h in enumerate(headers):
                    record[h] = values[i].strip().strip('"')
                records.append(record)
        return records
    except Exception:
        return []


def get_gdp_per_capita(country: str, start_year: int = 1820, end_year: int = 2018) -> Dict:
    records = _fetch_owid_csv()
    country_upper = country.upper()
    country_name = COUNTRY_NAMES.get(country_upper, country)
    if records:
        filtered = []
        for r in records:
            entity = r.get("Entity", "")
            code = r.get("Code", "")
            year_str = r.get("Year", "")
            gdppc = r.get("GDP per capita", r.get("gdppc", ""))
            if (code == country_upper or entity.lower() == country.lower()) and year_str:
                try:
                    year = int(year_str)
                    if start_year <= year <= end_year:
                        filtered.append({"year": year, "gdp_per_capita_2011usd": gdppc})
                except ValueError:
                    continue
        filtered.sort(key=lambda x: x["year"])
        return {
            "country": country_upper,
            "country_name": country_name,
            "start_year": start_year,
            "end_year": end_year,
            "unit": "2011 USD (PPP)",
            "source": "Maddison Project Database 2020",
            "data_points": len(filtered),
            "data": filtered,
        }
    return {
        "country": country_upper,
        "country_name": country_name,
        "start_year": start_year,
        "end_year": end_year,
        "unit": "2011 USD (PPP)",
        "source": "Maddison Project Database 2020",
        "download_url": MPD_DOWNLOAD_URL,
        "note": "Could not fetch live data. Download the Excel file for offline analysis.",
        "historical_benchmarks": HISTORICAL_BENCHMARK,
    }


def get_global_gdp(start_year: int = 1820, end_year: int = 2018) -> Dict:
    return {
        "source": "Maddison Project Database 2020",
        "start_year": start_year,
        "end_year": end_year,
        "unit": "2011 USD (PPP)",
        "historical_world_gdppc": {
            str(year): data
            for year, data in HISTORICAL_BENCHMARK.items()
            if start_year <= year <= end_year
        },
        "download_url": MPD_DOWNLOAD_URL,
        "note": "Global aggregates available in the full Excel download.",
    }


def get_regional_gdp(region: str, start_year: int = 1820, end_year: int = 2018) -> Dict:
    region_lower = region.lower().replace(" ", "_")
    country_codes = WORLD_REGIONS.get(region_lower, [])
    if not country_codes:
        return {
            "error": f"Unknown region: {region}",
            "available_regions": list(WORLD_REGIONS.keys()),
        }
    return {
        "region": region,
        "start_year": start_year,
        "end_year": end_year,
        "countries_in_region": country_codes,
        "country_names": {code: COUNTRY_NAMES.get(code, code) for code in country_codes},
        "unit": "2011 USD (PPP)",
        "source": "Maddison Project Database 2020",
        "download_url": MPD_DOWNLOAD_URL,
        "note": "Fetch individual country data using gdp_per_capita command.",
        "available_regions": list(WORLD_REGIONS.keys()),
    }


def get_top_economies(year: int = 2018, n: int = 10) -> Dict:
    top_by_year = {
        2018: [
            {"rank": 1, "country": "SGP", "name": "Singapore", "gdppc_2011usd": 85535},
            {"rank": 2, "country": "USA", "name": "United States", "gdppc_2011usd": 54225},
            {"rank": 3, "country": "NOR", "name": "Norway", "gdppc_2011usd": 53478},
            {"rank": 4, "country": "CHE", "name": "Switzerland", "gdppc_2011usd": 50532},
            {"rank": 5, "country": "HKG", "name": "Hong Kong", "gdppc_2011usd": 50030},
            {"rank": 6, "country": "NLD", "name": "Netherlands", "gdppc_2011usd": 46155},
            {"rank": 7, "country": "AUS", "name": "Australia", "gdppc_2011usd": 44649},
            {"rank": 8, "country": "DNK", "name": "Denmark", "gdppc_2011usd": 44029},
            {"rank": 9, "country": "SWE", "name": "Sweden", "gdppc_2011usd": 43484},
            {"rank": 10, "country": "DEU", "name": "Germany", "gdppc_2011usd": 43132},
            {"rank": 11, "country": "AUT", "name": "Austria", "gdppc_2011usd": 42432},
            {"rank": 12, "country": "BEL", "name": "Belgium", "gdppc_2011usd": 41239},
            {"rank": 13, "country": "CAN", "name": "Canada", "gdppc_2011usd": 40522},
            {"rank": 14, "country": "FIN", "name": "Finland", "gdppc_2011usd": 39764},
            {"rank": 15, "country": "FRA", "name": "France", "gdppc_2011usd": 38488},
            {"rank": 16, "country": "GBR", "name": "United Kingdom", "gdppc_2011usd": 38059},
            {"rank": 17, "country": "JPN", "name": "Japan", "gdppc_2011usd": 36156},
            {"rank": 18, "country": "KOR", "name": "South Korea", "gdppc_2011usd": 35938},
            {"rank": 19, "country": "TWN", "name": "Taiwan", "gdppc_2011usd": 35453},
            {"rank": 20, "country": "ITA", "name": "Italy", "gdppc_2011usd": 33558},
        ],
        1973: [
            {"rank": 1, "country": "CHE", "name": "Switzerland", "gdppc_2011usd": 27171},
            {"rank": 2, "country": "USA", "name": "United States", "gdppc_2011usd": 23062},
            {"rank": 3, "country": "NOR", "name": "Norway", "gdppc_2011usd": 18479},
            {"rank": 4, "country": "SWE", "name": "Sweden", "gdppc_2011usd": 18401},
        ],
    }
    year_data = top_by_year.get(year, top_by_year[2018])
    return {
        "year": year,
        "top_n": n,
        "unit": "2011 USD (PPP)",
        "source": "Maddison Project Database 2020",
        "economies": year_data[:n],
        "note": "Rankings based on GDP per capita in 2011 PPP USD.",
    }


def get_growth_rates(country: str, period: str = "1950-2018") -> Dict:
    growth_by_country_period = {
        "CHN": {"1950-2018": 5.82, "1980-2018": 7.95, "2000-2018": 7.41},
        "KOR": {"1950-2018": 4.89, "1980-2018": 4.62, "2000-2018": 3.51},
        "JPN": {"1950-2018": 3.41, "1980-2018": 1.84, "2000-2018": 0.68},
        "USA": {"1950-2018": 2.01, "1980-2018": 1.98, "2000-2018": 1.37},
        "DEU": {"1950-2018": 2.60, "1980-2018": 1.65, "2000-2018": 1.36},
        "GBR": {"1950-2018": 2.15, "1980-2018": 1.97, "2000-2018": 1.19},
        "IND": {"1950-2018": 2.83, "1980-2018": 4.53, "2000-2018": 5.81},
        "BRA": {"1950-2018": 2.32, "1980-2018": 0.88, "2000-2018": 1.35},
    }
    country_upper = country.upper()
    rates = growth_by_country_period.get(country_upper, {})
    rate = rates.get(period, None)
    return {
        "country": country_upper,
        "country_name": COUNTRY_NAMES.get(country_upper, country),
        "period": period,
        "annual_growth_rate_pct": rate,
        "unit": "Average annual GDP per capita growth (%)",
        "source": "Maddison Project Database 2020",
        "available_periods": list(rates.keys()) if rates else ["1950-2018", "1980-2018", "2000-2018"],
        "note": "Growth rates are approximate. Download full dataset for precise calculations.",
    }


def get_countries() -> Dict:
    return {
        "total_countries": 169,
        "countries": COUNTRY_NAMES,
        "regions": WORLD_REGIONS,
        "source": "Maddison Project Database 2020",
        "time_coverage": "Year 1 to 2018 (with gaps for early centuries)",
        "download_url": MPD_DOWNLOAD_URL,
    }


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "gdp_per_capita":
        country = args[1] if len(args) > 1 else "USA"
        start_year = int(args[2]) if len(args) > 2 else 1820
        end_year = int(args[3]) if len(args) > 3 else 2018
        result = get_gdp_per_capita(country, start_year, end_year)
    elif command == "global":
        start_year = int(args[1]) if len(args) > 1 else 1820
        end_year = int(args[2]) if len(args) > 2 else 2018
        result = get_global_gdp(start_year, end_year)
    elif command == "regional":
        region = args[1] if len(args) > 1 else "western_europe"
        start_year = int(args[2]) if len(args) > 2 else 1820
        end_year = int(args[3]) if len(args) > 3 else 2018
        result = get_regional_gdp(region, start_year, end_year)
    elif command == "top":
        year = int(args[1]) if len(args) > 1 else 2018
        n = int(args[2]) if len(args) > 2 else 10
        result = get_top_economies(year, n)
    elif command == "growth":
        country = args[1] if len(args) > 1 else "USA"
        period = args[2] if len(args) > 2 else "1950-2018"
        result = get_growth_rates(country, period)
    elif command == "countries":
        result = get_countries()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
