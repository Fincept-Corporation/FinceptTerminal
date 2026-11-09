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

import os
import sys
import json
import math
import asyncio
import xml.etree.ElementTree as ET
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Union, Literal
import aiohttp
import pandas as pd

try:
    import fincept_json
except ImportError:  # scripts dir not on sys.path (unusual launcher)
    sys.path.append(os.path.dirname(os.path.abspath(__file__)))
    import fincept_json

# aiohttp defaults to aiodns when it is installed, and aiodns times out on many
# Windows setups ("Timeout while contacting DNS servers"). The threaded resolver
# uses the OS resolver instead (same fix as bis_data.py).
if sys.platform == "win32":
    import aiohttp.connector as _aiohttp_connector
    _aiohttp_connector.DefaultResolver = aiohttp.ThreadedResolver

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
        """Get one euro-area yield curve (all maturities): the latest one, or the one in force
        on `date` (YYYY-MM-DD) — the last observation on or before it, since the ECB publishes
        curves on TARGET business days only."""
        try:
            async with aiohttp.ClientSession() as session:
                # Get data for each maturity
                tasks = []
                for maturity in self.maturities:
                    series_id = self.get_yield_curve_series_id(maturity, rating, yield_curve_type)
                    url = f"{self.base_url}/{series_id}"
                    tasks.append(self.fetch_data(session, url))

                responses = await asyncio.gather(*tasks, return_exceptions=True)

            # maturity -> {YYYY-MM-DD: rate}. Each series is the full daily history and carries
            # empty rows for weekends / holidays; those are skipped. They used to be read as 0 %
            # (about 30 % of all rows), and with no date the whole history since 2004 was returned.
            series = {}
            for maturity, response in zip(self.maturities, responses):
                if not isinstance(response, list):
                    continue
                points = {}
                for item in response:
                    try:
                        day = str(item.get("PERIOD") or "")[:10]
                        value = item.get("OBS_VALUE_AS_IS")
                        if len(day) != 10 or value in (None, ""):
                            continue
                        rate = round(float(value) / 100, 12)  # percent -> decimal
                    except (AttributeError, TypeError, ValueError):
                        continue
                    if math.isfinite(rate):
                        points[day] = rate
                if points:
                    series[maturity] = points

            if not series:
                return {"success": False, "error": "ECB returned no yield curve observations"}

            all_days = sorted({day for points in series.values() for day in points})
            if date:
                eligible = [day for day in all_days if day <= date]
                if not eligible:
                    return {"success": False,
                            "error": f"No ECB yield curve on or before {date} (the series starts {all_days[0]})"}
                as_of = eligible[-1]
            else:
                as_of = all_days[-1]

            results = [{"date": as_of, "maturity": m, "rate": series[m][as_of], "rating": rating,
                        "curve_type": yield_curve_type}
                       for m in self.maturities if as_of in series.get(m, {})]
            missing = [m for m in self.maturities if as_of not in series.get(m, {})]

            out = {
                "success": True,
                "data": results,
                "as_of": as_of,
                "parameters": {
                    "rating": rating,
                    "curve_type": yield_curve_type,
                    "date": date
                },
                "source": "ECB",
                "description": f"Yield curve data - {rating} rated bonds - {yield_curve_type}"
            }
            if missing:
                out["missing_maturities"] = missing
            return out

        except Exception as e:
            return {"success": False, "error": str(e)}

    async def get_balance_of_payments_data(self, country: str = None, frequency: str = "monthly",
                                         report_type: str = "main") -> Dict:
        """Balance of payments — not implemented.

        It needs the ECB BPS series-key mapping (OpenBB's bps_series.py), which was never
        ported. The old body answered success with an empty list, so a caller saw "no data"
        rather than "not available".
        """
        return {
            "success": False,
            "error": "balance_of_payments is not implemented: the ECB balance-of-payments series "
                     "mapping has not been ported to this connector",
            "parameters": {
                "country": country,
                "frequency": frequency,
                "report_type": report_type
            }
        }

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
                        "available": False,
                        "note": "Not implemented yet: the ECB series mapping has not been ported",
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

            # Get yield curve snapshot (the latest AAA spot curve; this used to be the first ten
            # rows of the full history, i.e. the curve of 6 Sep 2004)
            yield_curve_result = await self.get_yield_curve_data(
                rating="aaa",
                yield_curve_type="spot_rate"
            )

            errors = {name: part.get("error") for name, part in
                      (("currency_rates", currency_result), ("yield_curve", yield_curve_result))
                      if not part.get("success")}
            if len(errors) == 2:
                return {"success": False, "error": "; ".join(f"{k}: {v}" for k, v in errors.items())}

            overview = {
                "success": True,
                "data": {
                    "currency_rates": currency_result.get("data", {}),
                    "yield_curve_snapshot": yield_curve_result.get("data", []),
                    "yield_curve_as_of": yield_curve_result.get("as_of"),
                    "available_categories": self.get_available_options()["data"]
                },
                "source": "ECB",
                "description": "European Central Bank data overview",
                "timestamp": datetime.now().isoformat()
            }
            if errors:
                overview["errors"] = errors

            return overview

        except Exception as e:
            return {"success": False, "error": str(e)}

def _parse_options(tokens):
    """`--name value` / `--name=value` tokens -> {name: value}. A bare word or a flag without a
    value is an error rather than being dropped."""
    options = {}
    i = 0
    while i < len(tokens):
        token = tokens[i]
        if not token.startswith("--") or len(token) < 3:
            raise ValueError(f"Unexpected argument '{token}': options are written --name value")
        if "=" in token:
            name, value = token[2:].split("=", 1)
        elif i + 1 < len(tokens) and not tokens[i + 1].startswith("--"):
            name, value = token[2:], tokens[i + 1]
            i += 1
        else:
            raise ValueError(f"Option {token} needs a value")
        options[name] = value
        i += 1
    return options


def _reject_unknown(options, known, command):
    unknown = sorted(set(options) - set(known))
    if unknown:
        valid = ", ".join("--" + k for k in known) if known else "none"
        raise ValueError(f"{command}: unknown option(s) {', '.join('--' + u for u in unknown)} (valid: {valid})")


def _option(options, name, default, allowed, command, upper=False):
    """--name's value (default when absent), which must be one of `allowed`."""
    value = options.get(name)
    if value is None:
        return default
    value = value.strip().upper() if upper else value.strip().lower()
    if value not in allowed:
        raise ValueError(f"{command}: --{name} must be one of {', '.join(allowed)} (got '{value}')")
    return value


def main(args=None):

    if args is None:
        args = sys.argv[1:]
    """Main function for CLI interface"""
    wrapper = ECBDataWrapper()

    if len(args) + 1 < 2:
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

    command = args[0]

    try:
        # --key value / --key=value. The old loop rebound `args` to {} and then tested
        # `i + 1 < len(args) + 1`, so `--rating all_ratings` became rating=True with the value
        # dropped: every documented space-separated call silently fell back to the AAA spot
        # curve and --date never matched. Unknown options and stray words are errors now.
        options = _parse_options(args[1:])

        if command == "available_categories":
            _reject_unknown(options, (), command)
            result = wrapper.get_available_options()

        elif command == "currency_rates":
            _reject_unknown(options, (), command)
            result = asyncio.run(wrapper.get_currency_reference_rates())

        elif command == "yield_curve":
            _reject_unknown(options, ("rating", "type", "date"), command)
            rating = _option(options, "rating", "aaa", wrapper.rating_dict, command)
            curve_type = _option(options, "type", "spot_rate", wrapper.yield_type_dict, command)
            date = options.get("date")
            if date is not None:
                try:
                    date = datetime.strptime(date.strip(), "%Y-%m-%d").strftime("%Y-%m-%d")
                except ValueError:
                    raise ValueError(f"{command}: --date must be YYYY-MM-DD (got '{date}')")
            result = asyncio.run(wrapper.get_yield_curve_data(rating, curve_type, date))

        elif command == "balance_of_payments":
            _reject_unknown(options, ("country", "frequency", "report_type"), command)
            country = _option(options, "country", None, wrapper.bps_countries, command, upper=True)
            frequency = _option(options, "frequency", "monthly", wrapper.bps_frequencies, command)
            report_type = _option(options, "report_type", "main", wrapper.bps_report_types, command)
            result = asyncio.run(wrapper.get_balance_of_payments_data(country, frequency, report_type))

        elif command == "overview":
            _reject_unknown(options, (), command)
            result = asyncio.run(wrapper.get_ecb_overview())

        else:
            result = {
                "success": False,
                "error": f"Unknown command: {command}"
            }

        print(fincept_json.dumps(result, indent=2, ensure_ascii=True))

    except Exception as e:
        print(json.dumps({
            "success": False,
            "error": str(e)
        }))

if __name__ == "__main__":
    main()