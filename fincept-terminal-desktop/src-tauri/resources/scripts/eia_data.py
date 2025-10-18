# EIA (U.S. Energy Information Administration) Data Wrapper
# Provides access to Weekly Petroleum Status Report and Short Term Energy Outlook data

import sys
import json
import asyncio
from typing import Dict, List, Optional, Union, Any, Literal
from datetime import datetime, date
from io import BytesIO
import warnings

import requests
import pandas as pd
from numpy import nan

# Constants
BASE_API_URL = "https://api.eia.gov/v2/"
USER_AGENTS = [
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36",
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36",
]

# Petroleum Status Report categories and tables
WPSR_CATEGORY_CHOICES = [
    "balance_sheet",
    "inputs_and_production",
    "refiner_blender_net_production",
    "crude_petroleum_stocks",
    "gasoline_fuel_stocks",
    "total_gasoline_by_sub_padd",
    "distillate_fuel_oil_stocks",
    "imports",
    "imports_by_country",
    "weekly_estimates",
    "spot_prices_crude_gas_heating",
    "spot_prices_diesel_jet_fuel_propane",
    "retail_prices",
]

WPSR_FILE_MAP = {
    "balance_sheet": "https://ir.eia.gov/wpsr/psw01.xls",
    "inputs_and_production": "https://ir.eia.gov/wpsr/psw02.xls",
    "refiner_blender_net_production": "https://ir.eia.gov/wpsr/psw03.xls",
    "crude_petroleum_stocks": "https://ir.eia.gov/wpsr/psw04.xls",
    "gasoline_fuel_stocks": "https://ir.eia.gov/wpsr/psw05.xls",
    "total_gasoline_by_sub_padd": "https://ir.eia.gov/wpsr/psw05a.xls",
    "distillate_fuel_oil_stocks": "https://ir.eia.gov/wpsr/psw06.xls",
    "imports": "https://ir.eia.gov/wpsr/psw07.xls",
    "imports_by_country": "https://ir.eia.gov/wpsr/psw08.xls",
    "weekly_estimates": "https://ir.eia.gov/wpsr/psw09.xls",
    "spot_prices_crude_gas_heating": "https://ir.eia.gov/wpsr/psw11.xls",
    "spot_prices_diesel_jet_fuel_propane": "https://ir.eia.gov/wpsr/psw12.xls",
    "retail_prices": "https://ir.eia.gov/wpsr/psw14.xls",
}

# STEO Table mappings (subset for commonly used tables)
STEO_TABLE_NAMES = {
    "01": "US Energy Markets Summary",
    "02": "Nominal Energy Prices",
    "04a": "US Petroleum and Other Liquid Fuels Supply, Consumption, and Inventories",
    "05a": "US Natural Gas Supply, Consumption, and Inventories",
    "07a": "US Electricity Industry Overview",
    "09a": "US Macroeconomic Indicators and CO2 Emissions",
}

STEO_SYMBOLS_MAP = {
    "01": ["COPRPUS", "NGPRPUS", "WTIPUUS", "NGHHUUS", "ESTXPUS", "RETCBUS", "TETCFUEL"],
    "02": ["WTIPUUS", "BREPUUS", "RAIMUUS", "RACPUUS", "NGHHUUS", "CLEUDUS"],
    "04a": ["COPRPUS", "PASUPPLY", "PATCPUSX", "NGTCPUS", "EOTCPUS", "PAIMPORT", "PASXPUS"],
    "05a": ["NGMPPUS", "NGSUPP", "NGPRPUS", "NGNWPUS", "NGSFPUS", "NGIMPUS_LNG", "NGEXPUS_LNG"],
    "07a": ["ELSUTWH", "EPEOTWH", "INEOTWH", "CMEOTWH", "ELNITWH", "SODTP_US"],
    "09a": ["GDPQXUS", "CONSRUS", "I87RXUS", "YD87OUS", "EMNFPUS", "TETCCO2"],
}

class EIAError(Exception):
    """Custom exception for EIA API errors"""
    pass

class EIADataFetcher:
    """Fault-tolerant EIA data fetcher"""

    def __init__(self, api_key: Optional[str] = None):
        self.api_key = api_key
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': USER_AGENTS[0],
            'Accept': 'application/json,application/vnd.ms-excel,application/vnd.openxmlformats-officedocument.spreadsheetml.sheet',
            'Accept-Language': 'en-US,en;q=0.5',
            'Accept-Encoding': 'gzip, deflate',
            'Connection': 'keep-alive',
        })

    def _get_random_user_agent(self) -> str:
        """Get a random user agent"""
        import random
        return random.choice(USER_AGENTS)

    def _make_request(self, url: str, timeout: int = 60) -> Optional[Union[Dict, bytes]]:
        """Make HTTP request with error handling"""
        try:
            # Rotate user agent
            self.session.headers['User-Agent'] = self._get_random_user_agent()

            response = self.session.get(url, timeout=timeout)
            response.raise_for_status()

            # Check content type
            content_type = response.headers.get('content-type', '').lower()
            if 'application/json' in content_type:
                return response.json()
            elif 'excel' in content_type or 'spreadsheet' in content_type:
                return response.content
            else:
                return response.content

        except requests.exceptions.RequestException as e:
            raise EIAError(f"HTTP request failed for {url}: {str(e)}")
        except Exception as e:
            raise EIAError(f"Unexpected error fetching {url}: {str(e)}")

    def _parse_petroleum_excel(self, excel_data: bytes, category: str, tables: List[str]) -> List[Dict]:
        """Parse petroleum status report Excel data"""
        try:
            # Read all sheets
            xls = pd.ExcelFile(BytesIO(excel_data))
            sheet_names = xls.sheet_names
            results = []

            for sheet_name in sheet_names:
                df = pd.read_excel(xls, sheet_name=sheet_name)

                # Skip empty sheets
                if df.empty or len(df) < 2:
                    continue

                # Find date column (could be various names)
                date_col = None
                for col in df.columns:
                    if any(keyword in str(col).lower() for keyword in ['date', 'week', 'period']):
                        date_col = col
                        break

                if date_col is None:
                    # Try first column as date
                    date_col = df.columns[0]

                # Clean and convert date column
                df[date_col] = pd.to_datetime(df[date_col], errors='coerce')
                df = df.dropna(subset=[date_col])
                df[date_col] = df[date_col].dt.date

                # Melt the dataframe to long format
                id_vars = [date_col]
                value_vars = [col for col in df.columns if col != date_col]

                if len(value_vars) > 0:
                    melted_df = df.melt(id_vars=id_vars, value_vars=value_vars,
                                      var_name='symbol', value_name='value')
                    melted_df = melted_df.dropna(subset=['value'])

                    # Add results
                    for _, row in melted_df.iterrows():
                        results.append({
                            "date": row[date_col].strftime("%Y-%m-%d"),
                            "category": category,
                            "table": sheet_name,
                            "symbol": str(row['symbol']),
                            "value": float(row['value']) if pd.notna(row['value']) else None,
                            "source": "petroleum_status_report"
                        })

            return results

        except Exception as e:
            raise EIAError(f"Error parsing petroleum Excel data for {category}: {str(e)}")

    def _parse_steo_data(self, api_data: Dict, table: str) -> List[Dict]:
        """Parse STEO API data"""
        try:
            results = []
            data = api_data.get('response', {}).get('data', [])

            for item in data:
                results.append({
                    "date": item.get('period', ''),
                    "symbol": item.get('seriesId', ''),
                    "title": item.get('seriesDescription', ''),
                    "value": item.get('value', None),
                    "units": item.get('unitsofmeasure', ''),
                    "table": f"STEO-{table}: {STEO_TABLE_NAMES.get(table, table)}",
                    "source": "short_term_energy_outlook"
                })

            return results

        except Exception as e:
            raise EIAError(f"Error parsing STEO data for table {table}: {str(e)}")

    def get_petroleum_status_report(
        self,
        category: str = "balance_sheet",
        tables: Optional[List[str]] = None,
        start_date: Optional[str] = None,
        end_date: Optional[str] = None,
        use_cache: bool = True
    ) -> Dict[str, Any]:
        """Get petroleum status report data"""
        try:
            if category not in WPSR_CATEGORY_CHOICES:
                raise EIAError(f"Invalid category: {category}. Valid choices: {WPSR_CATEGORY_CHOICES}")

            url = WPSR_FILE_MAP.get(category)
            if not url:
                raise EIAError(f"No URL found for category: {category}")

            # Get Excel data
            excel_data = self._make_request(url)
            if not excel_data:
                raise EIAError(f"No data received from {url}")

            # Determine tables to fetch
            if not tables:
                tables = ["all"]  # Default to get all tables

            # Parse the data
            results = self._parse_petroleum_excel(excel_data, category, tables)

            # Filter by date range if provided
            if start_date:
                start_date_obj = datetime.strptime(start_date, '%Y-%m-%d').date()
                results = [r for r in results if datetime.strptime(r['date'], '%Y-%m-%d').date() >= start_date_obj]

            if end_date:
                end_date_obj = datetime.strptime(end_date, '%Y-%m-%d').date()
                results = [r for r in results if datetime.strptime(r['date'], '%Y-%m-%d').date() <= end_date_obj]

            return {
                "success": True,
                "category": category,
                "tables": tables,
                "data": results,
                "count": len(results),
                "url": url
            }

        except EIAError:
            raise
        except Exception as e:
            return {
                "success": False,
                "error": f"Error fetching petroleum status report: {str(e)}",
                "category": category
            }

    def get_short_term_energy_outlook(
        self,
        table: str = "01",
        symbols: Optional[List[str]] = None,
        frequency: Literal["month", "quarter", "annual"] = "month",
        start_date: Optional[str] = None,
        end_date: Optional[str] = None
    ) -> Dict[str, Any]:
        """Get short term energy outlook data"""
        try:
            if not self.api_key:
                raise EIAError("API key required for STEO data")

            if table not in STEO_TABLE_NAMES:
                raise EIAError(f"Invalid table: {table}. Valid choices: {list(STEO_TABLE_NAMES.keys())}")

            # Get symbols for the table
            if not symbols:
                symbols = STEO_SYMBOLS_MAP.get(table, [])

            if not symbols:
                raise EIAError(f"No symbols available for table: {table}")

            # Build API URL
            frequency_map = {
                "month": "monthly",
                "quarter": "quarterly",
                "annual": "annual"
            }

            base_url = f"{BASE_API_URL}steo/data/?api_key={self.api_key}&frequency={frequency_map[frequency]}&data[0]=value"

            # Add symbols to URL (limit to 10 per request)
            results = []
            for i in range(0, len(symbols), 10):
                symbol_chunk = symbols[i:i+10]
                url_symbols = ""
                for symbol in symbol_chunk:
                    url_symbols += f"&facets[seriesId][]={symbol}"

                # Add date filters
                if start_date:
                    if frequency == "monthly":
                        url_date = f"&start={datetime.strptime(start_date, '%Y-%m-%d').strftime('%Y-%m')}"
                    elif frequency == "quarterly":
                        quarter = (datetime.strptime(start_date, '%Y-%m-%d').month - 1) // 3 + 1
                        url_date = f"&start={datetime.strptime(start_date, '%Y-%m-%d').year}-Q{quarter}"
                    else:  # annual
                        url_date = f"&start={datetime.strptime(start_date, '%Y-%m-%d').year}"
                    url_symbols += url_date

                if end_date:
                    if frequency == "monthly":
                        url_date = f"&end={datetime.strptime(end_date, '%Y-%m-%d').strftime('%Y-%m')}"
                    elif frequency == "quarterly":
                        quarter = (datetime.strptime(end_date, '%Y-%m-%d').month - 1) // 3 + 1
                        url_date = f"&end={datetime.strptime(end_date, '%Y-%m-%d').year}-Q{quarter}"
                    else:  # annual
                        url_date = f"&end={datetime.strptime(end_date, '%Y-%m-%d').year}"
                    url_symbols += url_date

                url = f"{base_url}{url_symbols}&offset=0&length=5000"

                # Make API request
                api_data = self._make_request(url)
                if api_data and isinstance(api_data, dict):
                    chunk_results = self._parse_steo_data(api_data, table)
                    results.extend(chunk_results)

            # Sort results by date
            results.sort(key=lambda x: x['date'])

            return {
                "success": True,
                "table": table,
                "table_name": STEO_TABLE_NAMES[table],
                "frequency": frequency,
                "symbols": symbols,
                "data": results,
                "count": len(results)
            }

        except EIAError:
            raise
        except Exception as e:
            return {
                "success": False,
                "error": f"Error fetching STEO data: {str(e)}",
                "table": table
            }

    def get_available_categories(self) -> Dict[str, Any]:
        """Get available petroleum status report categories"""
        return {
            "success": True,
            "categories": WPSR_CATEGORY_CHOICES,
            "total_categories": len(WPSR_CATEGORY_CHOICES),
            "file_urls": WPSR_FILE_MAP
        }

    def get_available_steo_tables(self) -> Dict[str, Any]:
        """Get available STEO tables"""
        return {
            "success": True,
            "tables": STEO_TABLE_NAMES,
            "symbols_map": STEO_SYMBOLS_MAP,
            "total_tables": len(STEO_TABLE_NAMES)
        }

    def get_energy_overview(self, limit: Optional[int] = None) -> Dict[str, Any]:
        """Get comprehensive energy overview"""
        results = []
        errors = []

        # Get petroleum data (no API key required)
        try:
            petroleum_data = self.get_petroleum_status_report("balance_sheet", ["stocks", "supply"])
            results.append(("petroleum_balance", petroleum_data))
        except Exception as e:
            errors.append(("petroleum_balance", str(e)))

        # Get STEO data (API key required)
        if self.api_key:
            try:
                steo_data = self.get_short_term_energy_outlook("01", frequency="month")
                results.append(("energy_markets_summary", steo_data))
            except Exception as e:
                errors.append(("energy_markets_summary", str(e)))

            try:
                natural_gas_data = self.get_short_term_energy_outlook("05a", frequency="month")
                results.append(("natural_gas", natural_gas_data))
            except Exception as e:
                errors.append(("natural_gas", str(e)))

        return {
            "success": len(results) > 0,
            "results": results,
            "errors": errors,
            "total_requests": len(results) + len(errors),
            "successful_fetches": len(results),
            "failed_fetches": len(errors)
        }

def main():
    """CLI interface for EIA data wrapper"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "success": False,
            "error": "Usage: python eia_data.py <command> [args...]"
        }))
        sys.exit(1)

    command = sys.argv[1]
    api_key = None  # In production, this should come from environment variables or config

    try:
        fetcher = EIADataFetcher(api_key=api_key)

        if command == "get_petroleum":
            # Usage: get_petroleum <category> [tables_csv] [start_date] [end_date]
            if len(sys.argv) < 3:
                print(json.dumps({
                    "success": False,
                    "error": "Usage: python eia_data.py get_petroleum <category> [tables_csv] [start_date] [end_date]"
                }))
                sys.exit(1)

            category = sys.argv[2]
            tables = sys.argv[3].split(',') if len(sys.argv) > 3 else None
            start_date = sys.argv[4] if len(sys.argv) > 4 else None
            end_date = sys.argv[5] if len(sys.argv) > 5 else None

            result = fetcher.get_petroleum_status_report(category, tables, start_date, end_date)
            print(json.dumps(result, indent=2))

        elif command == "get_steo":
            # Usage: get_steo <table> [symbols_csv] [frequency] [start_date] [end_date]
            if len(sys.argv) < 3:
                print(json.dumps({
                    "success": False,
                    "error": "Usage: python eia_data.py get_steo <table> [symbols_csv] [frequency] [start_date] [end_date]"
                }))
                sys.exit(1)

            table = sys.argv[2]
            symbols = sys.argv[3].split(',') if len(sys.argv) > 3 and sys.argv[3] else None
            frequency = sys.argv[4] if len(sys.argv) > 4 else "month"
            start_date = sys.argv[5] if len(sys.argv) > 5 else None
            end_date = sys.argv[6] if len(sys.argv) > 6 else None

            result = fetcher.get_short_term_energy_outlook(table, symbols, frequency, start_date, end_date)
            print(json.dumps(result, indent=2))

        elif command == "available_categories":
            result = fetcher.get_available_categories()
            print(json.dumps(result, indent=2))

        elif command == "available_steo_tables":
            result = fetcher.get_available_steo_tables()
            print(json.dumps(result, indent=2))

        elif command == "energy_overview":
            limit = int(sys.argv[2]) if len(sys.argv) > 2 else None
            result = fetcher.get_energy_overview(limit)
            print(json.dumps(result, indent=2))

        elif command == "get_crude_stocks":
            # Convenience method for crude stocks
            result = fetcher.get_petroleum_status_report("crude_petroleum_stocks", ["stocks"])
            print(json.dumps(result, indent=2))

        elif command == "get_gasoline_stocks":
            # Convenience method for gasoline stocks
            result = fetcher.get_petroleum_status_report("gasoline_fuel_stocks", ["stocks"])
            print(json.dumps(result, indent=2))

        elif command == "get_petroleum_imports":
            # Convenience method for petroleum imports
            start_date = sys.argv[2] if len(sys.argv) > 2 else None
            end_date = sys.argv[3] if len(sys.argv) > 3 else None
            result = fetcher.get_petroleum_status_report("imports", ["imports"], start_date, end_date)
            print(json.dumps(result, indent=2))

        elif command == "get_spot_prices":
            # Convenience method for spot prices
            result = fetcher.get_petroleum_status_report("spot_prices_crude_gas_heating", ["crude", "conventional_gas"])
            print(json.dumps(result, indent=2))

        elif command == "get_energy_markets_summary":
            # Convenience method for energy markets summary
            result = fetcher.get_short_term_energy_outlook("01", frequency="month")
            print(json.dumps(result, indent=2))

        elif command == "get_natural_gas_summary":
            # Convenience method for natural gas summary
            result = fetcher.get_short_term_energy_outlook("05a", frequency="month")
            print(json.dumps(result, indent=2))

        else:
            print(json.dumps({
                "success": False,
                "error": f"Unknown command: {command}. Available commands: get_petroleum, get_steo, available_categories, available_steo_tables, energy_overview, get_crude_stocks, get_gasoline_stocks, get_petroleum_imports, get_spot_prices, get_energy_markets_summary, get_natural_gas_summary"
            }))
            sys.exit(1)

    except Exception as e:
        print(json.dumps({
            "success": False,
            "error": f"Command execution failed: {str(e)}"
        }))
        sys.exit(1)

if __name__ == "__main__":
    main()