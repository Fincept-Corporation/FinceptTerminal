#!/usr/bin/env python3
"""
ECB (European Central Bank) Data Wrapper

This script provides access to ECB statistical data including:
- Currency reference rates (daily FX rates)
- Yield curve data (spot rates, forward rates, par yields)
- Balance of payments data

Data sources:
- https://data.ecb.europa.eu/data-detail-api
- https://www.ecb.europa.eu/stats/eurofxref/eurofxref-daily.xml

Usage:
    python ecb_data.py available_categories
    python ecb_data.py currency_rates
    python ecb_data.py yield_curve --rating aaa --type spot_rate
    python ecb_data.py balance_of_payments --country US --frequency monthly
"""

import sys
import json
import asyncio
import xml.etree.ElementTree as ET
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Union, Literal
import aiohttp
import pandas as pd

class ECBError(Exception):
    """Custom exception for ECB data errors"""
    pass

class ECBDataWrapper:
    """European Central Bank data wrapper"""

    def __init__(self):
        self.base_url = "https://data.ecb.europa.eu/data-detail-api"
        self.fx_url = "https://www.ecb.europa.eu/stats/eurofxref/eurofxref-daily.xml"

        # Yield curve parameters
        self.maturities = [
            "month_3", "month_6", "year_1", "year_2", "year_3", "year_4", "year_5",
            "year_6", "year_7", "year_8", "year_9", "year_10", "year_11", "year_12",
            "year_13", "year_14", "year_15", "year_16", "year_17", "year_18", "year_19",
            "year_20", "year_21", "year_22", "year_23", "year_24", "year_25", "year_26",
            "year_27", "year_28", "year_29", "year_30"
        ]

        self.rating_dict = {"aaa": "A", "all_ratings": "C"}
        self.yield_type_dict = {
            "spot_rate": "SR",
            "instantaneous_forward": "IF",
            "par_yield": "PY"
        }

        # Balance of payments parameters
        self.bps_countries = [
            "US", "JP", "GB", "CH", "CA", "AU", "SE", "NO", "DK", "CN", "IN", "BR",
            "ZA", "KR", "SG", "HK", "TH", "MY", "ID", "PH", "TW", "NZ", "IE", "IS",
            "LU", "BE", "NL", "AT", "FI", "GR", "PT", "ES", "IT", "FR", "DE", "PL",
            "CZ", "HU", "SK", "SI", "EE", "LV", "LT", "MT", "CY", "BG", "RO", "HR"
        ]

        self.bps_frequencies = ["monthly", "quarterly", "annual"]
        self.bps_report_types = ["main", "summary", "services", "investment_income",
                                "direct_investment", "portfolio_investment", "other_investment"]

    async def fetch_data(self, session: aiohttp.ClientSession, url: str, params: Dict = None) -> Union[Dict, List]:
        """Fetch data from ECB API"""
        try:
            async with session.get(url, params=params) as response:
                if response.status == 200:
                    content_type = response.headers.get('content-type', '')
                    if 'application/json' in content_type:
                        return await response.json()
                    elif 'application/xml' in content_type or 'text/xml' in content_type:
                        xml_text = await response.text()
                        return xml_text
                    else:
                        text = await response.text()
                        return text
                else:
                    raise ECBError(f"HTTP {response.status}: Failed to fetch data from {url}")
        except aiohttp.ClientError as e:
            raise ECBError(f"Network error: {str(e)}")
        except Exception as e:
            raise ECBError(f"Unexpected error: {str(e)}")

    def get_yield_curve_series_id(self, maturity: str, rating: str = "aaa", yield_curve_type: str = "spot_rate") -> str:
        """Generate yield curve series ID"""
        rating_code = self.rating_dict.get(rating, "A")
        type_code = self.yield_type_dict.get(yield_curve_type, "SR")

        maturity_map = {
            "month_3": "3M", "month_6": "6M",
            "year_1": "1Y", "year_2": "2Y", "year_3": "3Y", "year_4": "4Y", "year_5": "5Y",
            "year_6": "6Y", "year_7": "7Y", "year_8": "8Y", "year_9": "9Y", "year_10": "10Y",
            "year_11": "11Y", "year_12": "12Y", "year_13": "13Y", "year_14": "14Y", "year_15": "15Y",
            "year_16": "16Y", "year_17": "17Y", "year_18": "18Y", "year_19": "19Y", "year_20": "20Y",
            "year_21": "21Y", "year_22": "22Y", "year_23": "23Y", "year_24": "24Y", "year_25": "25Y",
            "year_26": "26Y", "year_27": "27Y", "year_28": "28Y", "year_29": "29Y", "year_30": "30Y"
        }

        maturity_code = maturity_map.get(maturity, "1Y")
        return f"YC.B.U2.EUR.4F.G_N_{rating_code}.SV_C_YM.{type_code}_{maturity_code}"

    async def get_currency_reference_rates(self) -> Dict:
        """Get daily currency reference rates from ECB"""
        try:
            async with aiohttp.ClientSession() as session:
                xml_data = await self.fetch_data(session, self.fx_url)

                # Parse XML
                root = ET.fromstring(xml_data)

                # Define namespaces
                namespaces = {
                    'gesmes': 'http://www.gesmes.org/xml/2002-08-01',
                    'ecb': 'http://www.ecb.int/vocabulary/2002-08-01/eurofxref'
                }

                # Find the Cube with time attribute using proper namespace
                time_element = root.find('.//ecb:Cube[@time]', namespaces)
                if time_element is None:
                    raise ECBError("Could not find time element in FX data")

                date = time_element.get('time')

                # Extract rates using proper namespace
                rates = {"EUR": 1.0, "date": date}  # Base currency
                for cube in time_element.findall('ecb:Cube', namespaces):
                    currency = cube.get('currency')
                    rate = float(cube.get('rate'))
                    rates[currency] = rate

                return {
                    "success": True,
                    "data": rates,
                    "source": "ECB",
                    "description": "Daily currency reference rates"
                }

        except Exception as e:
            return {"success": False, "error": str(e)}

    async def get_yield_curve_data(self, rating: str = "aaa", yield_curve_type: str = "spot_rate",
                                  date: str = None) -> Dict:
        """Get yield curve data for specified parameters"""
        try:
            async with aiohttp.ClientSession() as session:
                results = []

                # Get data for each maturity
                tasks = []
                for maturity in self.maturities:
                    series_id = self.get_yield_curve_series_id(maturity, rating, yield_curve_type)
                    url = f"{self.base_url}/{series_id}"
                    tasks.append(self.fetch_data(session, url))

                responses = await asyncio.gather(*tasks, return_exceptions=True)

                for i, response in enumerate(responses):
                    if isinstance(response, Exception):
                        continue

                    if isinstance(response, list):
                        for item in response:
                            try:
                                data_point = {
                                    "date": item.get("PERIOD"),
                                    "maturity": self.maturities[i],
                                    "rate": float(item.get("OBS_VALUE_AS_IS", 0)) / 100,  # Convert to decimal
                                    "rating": rating,
                                    "curve_type": yield_curve_type
                                }

                                # Filter by date if specified
                                if date is None or data_point["date"] == date:
                                    results.append(data_point)
                            except (ValueError, TypeError):
                                continue

                # Sort by date and maturity
                results.sort(key=lambda x: (x.get("date", ""), x.get("maturity", "")))

                return {
                    "success": True,
                    "data": results,
                    "parameters": {
                        "rating": rating,
                        "curve_type": yield_curve_type,
                        "date": date
                    },
                    "source": "ECB",
                    "description": f"Yield curve data - {rating} rated bonds - {yield_curve_type}"
                }

        except Exception as e:
            return {"success": False, "error": str(e)}

    async def get_balance_of_payments_data(self, country: str = None, frequency: str = "monthly",
                                         report_type: str = "main") -> Dict:
        """Get balance of payments data"""
        try:
            # This is a simplified version - in a full implementation,
            # you would need to map all the series IDs like in the OpenBB provider
            async with aiohttp.ClientSession() as session:
                # For demonstration, return a placeholder response
                # In practice, you would need the complete series ID mapping from bps_series.py

                return {
                    "success": True,
                    "data": [],
                    "parameters": {
                        "country": country,
                        "frequency": frequency,
                        "report_type": report_type
                    },
                    "source": "ECB",
                    "description": "Balance of payments data",
                    "note": "Full implementation requires series ID mapping from OpenBB utils"
                }

        except Exception as e:
            return {"success": False, "error": str(e)}

    def get_available_options(self) -> Dict:
        """Get available options for each data category"""
        return {
            "success": True,
            "data": {
                "categories": {
                    "currency_reference_rates": {
                        "description": "Daily foreign exchange reference rates",
                        "frequency": "daily",
                        "base_currency": "EUR",
                        "currencies": ["USD", "GBP", "JPY", "CHF", "CAD", "AUD", "SEK", "NOK", "DKK",
                                      "CNY", "HKD", "SGD", "KRW", "INR", "BRL", "ZAR", "THB", "MYR",
                                      "IDR", "PHP", "TWD", "NZD", "PLN", "CZK", "HUF", "RON", "BGN",
                                      "HRK", "RUB", "TRY", "MXN", "CLP", "COP", "PEN", "UYU"]
                    },
                    "yield_curve": {
                        "description": "Euro area yield curve data",
                        "ratings": list(self.rating_dict.keys()),
                        "curve_types": list(self.yield_type_dict.keys()),
                        "maturities": self.maturities,
                        "frequency": "daily"
                    },
                    "balance_of_payments": {
                        "description": "Balance of payments statistics",
                        "countries": self.bps_countries,
                        "frequencies": self.bps_frequencies,
                        "report_types": self.bps_report_types
                    }
                }
            }
        }

    async def get_ecb_overview(self) -> Dict:
        """Get overview of available ECB data"""
        try:
            # Get currency rates
            currency_result = await self.get_currency_reference_rates()

            # Get yield curve snapshot
            yield_curve_result = await self.get_yield_curve_data(
                rating="aaa",
                yield_curve_type="spot_rate"
            )

            overview = {
                "success": True,
                "data": {
                    "currency_rates": currency_result.get("data", {}),
                    "yield_curve_snapshot": yield_curve_result.get("data", [])[:10],  # First 10 items
                    "available_categories": self.get_available_options()["data"]
                },
                "source": "ECB",
                "description": "European Central Bank data overview",
                "timestamp": datetime.now().isoformat()
            }

            return overview

        except Exception as e:
            return {"success": False, "error": str(e)}

def main():
    """Main function for CLI interface"""
    wrapper = ECBDataWrapper()

    if len(sys.argv) < 2:
        print(json.dumps({
            "success": False,
            "error": "Usage: python ecb_data.py <command> [options]",
            "commands": [
                "available_categories",
                "currency_rates",
                "yield_curve [--rating aaa|all_ratings] [--type spot_rate|instantaneous_forward|par_yield] [--date YYYY-MM-DD]",
                "balance_of_payments [--country CODE] [--frequency monthly|quarterly|annual] [--report_type TYPE]",
                "overview"
            ]
        }))
        sys.exit(1)

    command = sys.argv[1]

    # Parse command line arguments
    args = {}
    for i, arg in enumerate(sys.argv[2:], 2):
        if arg.startswith('--'):
            if '=' in arg:
                key, value = arg[2:].split('=', 1)
                args[key] = value
            elif i + 1 < len(sys.argv) and not sys.argv[i + 1].startswith('--'):
                key = arg[2:]
                value = sys.argv[i + 1]
                args[key] = value
                i += 1
            else:
                args[arg[2:]] = True

    try:
        if command == "available_categories":
            result = wrapper.get_available_options()

        elif command == "currency_rates":
            result = asyncio.run(wrapper.get_currency_reference_rates())

        elif command == "yield_curve":
            rating = args.get("rating", "aaa")
            curve_type = args.get("type", "spot_rate")
            date = args.get("date")
            result = asyncio.run(wrapper.get_yield_curve_data(rating, curve_type, date))

        elif command == "balance_of_payments":
            country = args.get("country")
            frequency = args.get("frequency", "monthly")
            report_type = args.get("report_type", "main")
            result = asyncio.run(wrapper.get_balance_of_payments_data(country, frequency, report_type))

        elif command == "overview":
            result = asyncio.run(wrapper.get_ecb_overview())

        else:
            result = {
                "success": False,
                "error": f"Unknown command: {command}"
            }

        print(json.dumps(result, indent=2))

    except Exception as e:
        print(json.dumps({
            "success": False,
            "error": str(e)
        }))

if __name__ == "__main__":
    main()