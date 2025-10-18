# IMF (International Monetary Fund) Data Wrapper
# Modular, fault-tolerant design - each endpoint works independently

import sys
import json
import requests
import pandas as pd
from typing import Dict, Any, List, Optional, Union
from datetime import datetime
import traceback
from io import StringIO


class IMFError:
    """Custom error class for IMF API errors"""
    def __init__(self, endpoint: str, error: str, status_code: Optional[int] = None):
        self.endpoint = endpoint
        self.error = error
        self.status_code = status_code
        self.timestamp = int(datetime.now().timestamp())

    def to_dict(self) -> Dict[str, Any]:
        return {
            "endpoint": self.endpoint,
            "error": self.error,
            "status_code": self.status_code,
            "timestamp": self.timestamp,
            "type": "IMFError"
        }


class IMFDataWrapper:
    """Modular IMF data wrapper with fault-tolerant endpoints"""

    def __init__(self):
        self.base_url = "http://dataservices.imf.org/REST/SDMX_JSON.svc/"
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': 'Fincept-Terminal/1.0'
        })

        # Country mappings (simplified version based on OpenBB patterns)
        self.country_to_code = {
            "united_states": "US", "usa": "US",
            "united_kingdom": "GB", "uk": "GB", "great_britain": "GB",
            "china": "CN", "japan": "JP", "germany": "DE", "france": "FR",
            "india": "IN", "italy": "IT", "canada": "CA", "south_korea": "KR",
            "russia": "RU", "brazil": "BR", "australia": "AU", "spain": "ES",
            "mexico": "MX", "indonesia": "ID", "netherlands": "NL", "saudi_arabia": "SA",
            "turkey": "TR", "switzerland": "CH", "poland": "PL", "sweden": "SE",
            "belgium": "BE", "argentina": "AR", "ireland": "IE", "austria": "AT",
            "norway": "NO", "israel": "IL", "united_arab_emirates": "AE", "uae": "AE",
            "egypt": "EG", "south_africa": "ZA", "denmark": "DK", "singapore": "SG",
            "malaysia": "MY", "philippines": "PH", "thailand": "TH", "nigeria": "NG",
            "pakistan": "PK", "chile": "CL", "finland": "FI", "romania": "RO",
            "czech_republic": "CZ", "portugal": "PT", "iraq": "IQ", "peru": "PE",
            "greece": "GR", "new_zealand": "NZ", "qatar": "QA", "algeria": "DZ",
            "hungary": "HU", "kazakhstan": "KZ", "kuwait": "KW", "morocco": "MA",
            "ukraine": "UA", "slovakia": "SK", "ecuador": "EC", "vietnam": "VN",
            "bangladesh": "BD", "angola": "AO", "azerbaijan": "AZ", "czechia": "CZ",
            "kenya": "KE", "omani": "OM", "azerbaijan": "AZ", "az": "AZ",
            "sri_lanka": "LK", "luxembourg": "LU", "panama": "PA", "uruguay": "UY",
            "myanmar": "MM", "burma": "MM", "costa_rica": "CR", "lithuania": "LT",
            "slovenia": "SI", "belarus": "BY", "uzbekistan": "UZ", "bulgaria": "BG",
            "croatia": "HR", "lebanon": "LB", "guatemala": "GT", "tanzania": "TZ",
            "ethiopia": "ET", "ghana": "GH", "ivory_coast": "CI", "côte_d'ivoire": "CI",
            "dominican_republic": "DO", "austria": "AT", "serbia": "RS", "ecuador": "EC",
            "bolivia": "BO", "uzbekistan": "UZ", "cameroon": "CM", "turkmenistan": "TM",
            "yemen": "YE", "paraguay": "PY", "senegal": "SN", "zambia": "ZM",
            "papua_new_guinea": "PG", "libya": "LY", "honduras": "HN", "congo": "CG",
            "bulgaria": "BG", "congo": "CD", "niger": "NE", "mozambique": "MZ",
            "benin": "BJ", "guinea": "GN", "kyrgyzstan": "KG", "zimbabwe": "ZW",
            "tunisia": "TN", "somalia": "SO", "mali": "ML", "nicaragua": "NI",
            "madagascar": "MG", "cameroon": "CM", "angola": "AO", "mali": "ML",
            "cambodia": "KH", "nepal": "NP", "jordan": "JO", "laos": "LA",
            "honduras": "HN", "georgia": "GE", "papua_new_guinea": "PG", "cambodia": "KH",
            "jordan": "JO", "laos": "LA", "congo": "CG", "somalia": "SO",
            "mali": "ML", "nicaragua": "NI", "kyrgyzstan": "KG", "madagascar": "MG",
            "north_macedonia": "MK", "macedonia": "MK", "botswana": "BW", "albania": "AL",
            "namibia": "NA", "gabon": "GA", "lesotho": "LS", "burkina_faso": "BF",
            "mongolia": "MN", "armenia": "AM", "fiji": "FJ", "haiti": "HT",
            "brunei": "BN", "montenegro": "ME", "suriname": "SR", "bhutan": "BT",
            "guyana": "GY", "south_sudan": "SS", "eritrea": "ER", "gambia": "GM",
            "djibouti": "DJ", "timor_leste": "TL", "east_timor": "TL", "seychelles": "SC",
            "antigua_and_barbuda": "AG", "belize": "BZ", "grenada": "GD", "st_vincent_and_the_grenadines": "VC",
            "st_kitts_and_nevis": "KN", "dominica": "DM", "samoa": "WS", "vanuatu": "VU",
            "sao_tome_and_principe": "ST", "comoros": "KM", "tonga": "TO", "micronesia": "FM",
            "palau": "PW", "marshall_islands": "MH", "kiribati": "KI", "tuvalu": "TV",
            "nauru": "NR"
        }

        # Economic indicator presets
        self.irfcl_presets = {
            "irfcl_top_lines": "RAF_USD,RAFA_USD,RAFAFX_USD,RAOFA_USD,RAPFA_USD,RAFAIMF_USD,RAFASDR_USD,RAFAGOLD_USD,RACFA_USD,RAMDCD_USD,RAMFIFC_USD,RAMSR_USD",
            "reserve_assets": "RAF_USD,RAFA_USD,RAFAFX_USD,RAOFA_USD,RAPFA_USD,RAFAIMF_USD,RAFASDR_USD,RAFAGOLD_USD",
            "gold_reserves": "RAFAGOLD_USD,RAFAGOLDV_OZT",
            "derivative_assets": "RAMFDA_USD"
        }

        # FSI presets
        self.fsi_presets = [
            "fsi_core", "fsi_core_underlying", "fsi_other",
            "fsi_encouraged_set", "fsi_balance_sheets", "fsi_all"
        ]

        # Trade indicators
        self.trade_indicators = {
            "exports": "TXG_FOB_USD",
            "imports": "TMG_CIF_USD",
            "balance": "TBG_USD",
            "all": "TXG_FOB_USD+TMG_CIF_USD+TBG_USD"
        }

        # Frequency mappings
        self.frequency_map = {
            "annual": "A", "yearly": "A", "a": "A",
            "quarter": "Q", "quarterly": "Q", "q": "Q",
            "month": "M", "monthly": "M", "m": "M"
        }

        # Sector mappings for IRFCL
        self.sector_map = {
            "government": "S1311",
            "central_bank": "S121",
            "monetary_authorities": "S1X",
            "all": ""
        }

        # Trade indicator titles
        self.trade_titles = {
            "TXG_FOB_USD": "Goods, Value of Exports, Free on board (FOB), US Dollars",
            "TMG_CIF_USD": "Goods, Value of Imports, Cost, Insurance, Freight (CIF), US Dollars",
            "TBG_USD": "Goods, Value of Trade Balance, US Dollars"
        }

    def _normalize_country(self, country: str) -> str:
        """Normalize country name to ISO code"""
        if not country:
            return ""

        country_lower = country.lower().strip().replace(" ", "_")

        # Direct mapping
        if country_lower in self.country_to_code:
            return self.country_to_code[country_lower]

        # Already 2-letter code?
        if len(country) == 2 and country.isupper():
            return country

        # Check if country name contains key words
        for mapped_name, code in self.country_to_code.items():
            if mapped_name in country_lower or country_lower in mapped_name:
                return code

        return country.upper()  # fallback

    def _make_request(self, url: str) -> Dict[str, Any]:
        """Make HTTP request with error handling"""
        try:
            response = self.session.get(url, timeout=30)
            response.raise_for_status()
            return response.json()
        except requests.exceptions.RequestException as e:
            raise Exception(f"HTTP request failed: {str(e)}")
        except json.JSONDecodeError as e:
            raise Exception(f"JSON decode error: {str(e)}")

    def _adjust_date_by_frequency(self, date_str: str, frequency: str, is_start: bool = True) -> str:
        """Adjust date based on frequency like OpenBB does"""
        if not date_str:
            return ""

        try:
            date = pd.to_datetime(date_str)
            freq = self.frequency_map.get(frequency.lower(), "Q")

            if freq == "Q":
                if is_start:
                    date = date.to_period('Q').start_time
                else:
                    date = date.to_period('Q').end_time
            elif freq == "A":
                if is_start:
                    date = date.to_period('A').start_time
                else:
                    date = date.to_period('A').end_time
            else:  # Monthly
                if is_start:
                    date = date.to_period('M').start_time
                else:
                    date = date.to_period('M').end_time

            return date.strftime("%Y-%m-%d")
        except:
            return date_str

    def get_economic_indicators(self, countries: Optional[str] = None, symbols: Optional[str] = None,
                              frequency: Optional[str] = "quarter", start_date: Optional[str] = None,
                              end_date: Optional[str] = None, sector: Optional[str] = "monetary_authorities") -> Dict[str, Any]:
        """Get economic indicators data (IRFCL and FSI)"""
        try:
            # Handle parameters
            if not countries:
                countries = "all"
            if not symbols:
                symbols = "irfcl_top_lines"

            # Normalize countries
            if countries.lower() != "all":
                country_list = [c.strip() for c in countries.split(",")]
                normalized_countries = "+".join([self._normalize_country(c) for c in country_list if self._normalize_country(c)])
            else:
                normalized_countries = ""

            # Handle symbols/presets
            if symbols in self.irfcl_presets:
                indicator_symbols = self.irfcl_presets[symbols].replace(",", "+")
            elif symbols in self.fsi_presets:
                indicator_symbols = symbols  # FSI symbols handled differently
            else:
                symbol_list = [s.strip().upper() for s in symbols.split(",")]
                indicator_symbols = "+".join(symbol_list)

            # Handle frequency
            freq_code = self.frequency_map.get(frequency.lower(), "Q")

            # Handle sector
            sector_code = self.sector_map.get(sector.lower(), "")

            # Adjust dates
            if start_date:
                start_date = self._adjust_date_by_frequency(start_date, frequency, True)
            if end_date:
                end_date = self._adjust_date_by_frequency(end_date, frequency, False)

            # Build URL
            date_range = f"?startPeriod={start_date}&endPeriod={end_date}" if start_date and end_date else ""

            # IRFCL Data URL
            if symbols in self.irfcl_presets or not any(p in symbols for p in self.fsi_presets):
                url = f"{self.base_url}CompactData/IRFCL/{freq_code}.{normalized_countries}.{indicator_symbols}.{sector_code}{date_range}"
            else:
                # FSI data would need different handling - simplified for now
                url = f"{self.base_url}CompactData/FSI/{freq_code}.{normalized_countries}.{indicator_symbols}{date_range}"

            # Make request
            response_data = self._make_request(url)

            # Check for API errors
            if "ErrorDetails" in response_data:
                error_msg = response_data["ErrorDetails"].get("Message", "Unknown IMF API error")
                return {"error": IMFError("economic_indicators", error_msg).to_dict()}

            # Process response data
            series_data = response_data.get("CompactData", {}).get("DataSet", {}).get("Series", [])

            if not series_data:
                return {"error": IMFError("economic_indicators", "No data found for the specified parameters").to_dict()}

            # Handle single series vs multiple series
            if isinstance(series_data, dict):
                series_data = [series_data]

            processed_data = []
            for series in series_data:
                if "Obs" not in series:
                    continue

                # Extract metadata
                metadata = {k.replace("@", "").lower(): v for k, v in series.items() if k != "Obs"}
                indicator = metadata.get("indicator", "")
                country_code = metadata.get("ref_area", "")

                # Get observations
                observations = series["Obs"]
                if isinstance(observations, dict):
                    observations = [observations]

                for obs in observations:
                    date_str = obs.get("@TIME_PERIOD", "")
                    value = obs.get("@OBS_VALUE")

                    if value is not None:
                        try:
                            value = float(value)
                        except:
                            value = None

                    # Find country name
                    country_name = country_code
                    for name, code in self.country_to_code.items():
                        if code == country_code:
                            country_name = name.replace("_", " ").title()
                            break

                    data_point = {
                        "date": date_str,
                        "symbol": indicator,
                        "country": country_name,
                        "country_code": country_code,
                        "value": value,
                        "frequency": frequency,
                        "sector": sector
                    }

                    # Add additional metadata
                    if metadata.get("unit_mult"):
                        data_point["scale"] = metadata["unit_mult"]
                    if metadata.get("ref_sector"):
                        data_point["reference_sector"] = metadata["ref_sector"]

                    processed_data.append(data_point)

            return {
                "success": True,
                "data": processed_data,
                "parameters": {
                    "countries": countries,
                    "symbols": symbols,
                    "frequency": frequency,
                    "start_date": start_date,
                    "end_date": end_date,
                    "sector": sector
                }
            }

        except Exception as e:
            return {"error": IMFError("economic_indicators", str(e)).to_dict()}

    def get_direction_of_trade(self, countries: Optional[str] = None, counterparts: Optional[str] = None,
                             direction: Optional[str] = "all", frequency: Optional[str] = "quarter",
                             start_date: Optional[str] = None, end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get direction of trade data (exports, imports, balance)"""
        try:
            if not countries:
                countries = "all"
            if not counterparts:
                counterparts = "all"
            if not direction:
                direction = "all"

            # Validate parameters
            if countries.lower() == "all" and counterparts.lower() == "all":
                return {"error": IMFError("direction_of_trade", "Both country and counterpart cannot be 'all'").to_dict()}

            # Normalize countries
            if countries.lower() != "all":
                country_list = [c.strip() for c in countries.split(",")]
                normalized_countries = "+".join([self._normalize_country(c) for c in country_list if self._normalize_country(c)])
            else:
                normalized_countries = ""

            if counterparts.lower() != "all":
                counterpart_list = [c.strip() for c in counterparts.split(",")]
                normalized_counterparts = "+".join([self._normalize_country(c) for c in counterpart_list if self._normalize_country(c)])
            else:
                normalized_counterparts = ""

            # Get indicator code
            indicator_code = self.trade_indicators.get(direction.lower(), "TXG_FOB_USD+TMG_CIF_USD+TBG_USD")

            # Handle frequency
            freq_code = self.frequency_map.get(frequency.lower(), "Q")

            # Adjust dates
            if start_date:
                start_date = self._adjust_date_by_frequency(start_date, frequency, True)
            if end_date:
                end_date = self._adjust_date_by_frequency(end_date, frequency, False)

            # Build URL
            date_range = f"?startPeriod={start_date}&endPeriod={end_date}" if start_date and end_date else ""
            url = f"{self.base_url}CompactData/DOT/{freq_code}.{normalized_countries}.{indicator_code}.{normalized_counterparts}{date_range}"

            # Make request
            response_data = self._make_request(url)

            # Check for API errors
            if "ErrorDetails" in response_data:
                error_msg = response_data["ErrorDetails"].get("Message", "Unknown IMF API error")
                return {"error": IMFError("direction_of_trade", error_msg).to_dict()}

            # Process response data
            series_data = response_data.get("CompactData", {}).get("DataSet", {}).get("Series", [])

            if not series_data:
                return {"error": IMFError("direction_of_trade", "No trade data found for the specified parameters").to_dict()}

            # Handle single series vs multiple series
            if isinstance(series_data, dict):
                series_data = [series_data]

            processed_data = []
            for series in series_data:
                if "Obs" not in series:
                    continue

                # Extract metadata
                metadata = {k.replace("@", "").lower(): v for k, v in series.items() if k != "Obs"}
                indicator = metadata.get("indicator", "")
                country_code = metadata.get("ref_area", "")
                counterpart_code = metadata.get("counterpart_area", "")

                # Get observations
                observations = series["Obs"]
                if isinstance(observations, dict):
                    observations = [observations]

                for obs in observations:
                    date_str = obs.get("@TIME_PERIOD", "")
                    value = obs.get("@OBS_VALUE")

                    if value is not None:
                        try:
                            value = float(value)
                        except:
                            value = None

                    if value is None:
                        continue

                    # Find country names
                    country_name = country_code
                    counterpart_name = counterpart_code

                    for name, code in self.country_to_code.items():
                        if code == country_code:
                            country_name = name.replace("_", " ").title()
                        if code == counterpart_code:
                            counterpart_name = name.replace("_", " ").title()

                    data_point = {
                        "date": date_str,
                        "symbol": indicator,
                        "country": country_name,
                        "country_code": country_code,
                        "counterpart": counterpart_name,
                        "counterpart_code": counterpart_code,
                        "value": value,
                        "frequency": frequency,
                        "direction": direction,
                        "title": self.trade_titles.get(indicator, indicator)
                    }

                    # Add additional metadata
                    if metadata.get("unit_mult"):
                        data_point["scale"] = metadata["unit_mult"]

                    processed_data.append(data_point)

            return {
                "success": True,
                "data": processed_data,
                "parameters": {
                    "countries": countries,
                    "counterparts": counterparts,
                    "direction": direction,
                    "frequency": frequency,
                    "start_date": start_date,
                    "end_date": end_date
                }
            }

        except Exception as e:
            return {"error": IMFError("direction_of_trade", str(e)).to_dict()}

    def get_available_indicators(self, query: Optional[str] = None) -> Dict[str, Any]:
        """Get list of available IMF indicators"""
        try:
            # Return a curated list of common IMF indicators since we don't have the full symbols file
            indicators = [
                # IRFCL (International Reserves & Foreign Currency Liquidity)
                {"symbol": "RAF_USD", "name": "Total Reserves", "dataset": "IRFCL", "description": "Total reserves excluding gold"},
                {"symbol": "RAFA_USD", "name": "Foreign Exchange Reserves", "dataset": "IRFCL", "description": "Foreign exchange reserves"},
                {"symbol": "RAFAGOLD_USD", "name": "Gold Reserves", "dataset": "IRFCL", "description": "Gold reserves"},
                {"symbol": "RAFAIMF_USD", "name": "IMF Reserves", "dataset": "IRFCL", "description": "Reserves position in the IMF"},
                {"symbol": "RAFASDR_USD", "name": "SDR Holdings", "dataset": "IRFCL", "description": "Special Drawing Rights"},
                {"symbol": "RAMFDA_USD", "name": "Derivative Assets", "dataset": "IRFCL", "description": "Net derivative assets"},

                # FSI (Financial Soundness Indicators) - Core
                {"symbol": "FSI_CAPR", "name": "Capital Adequacy Ratio", "dataset": "FSI", "description": "Regulatory capital to risk-weighted assets"},
                {"symbol": "FSI_NPL", "name": "Non-Performing Loans", "dataset": "FSI", "description": "Non-performing loans to total gross loans"},
                {"symbol": "FSI_ROA", "name": "Return on Assets", "dataset": "FSI", "description": "Return on assets"},
                {"symbol": "FSI_ROE", "name": "Return on Equity", "dataset": "FSI", "description": "Return on equity"},

                # DOT (Direction of Trade)
                {"symbol": "TXG_FOB_USD", "name": "Exports", "dataset": "DOT", "description": "Goods, Value of Exports, Free on board (FOB)"},
                {"symbol": "TMG_CIF_USD", "name": "Imports", "dataset": "DOT", "description": "Goods, Value of Imports, Cost, Insurance, Freight (CIF)"},
                {"symbol": "TBG_USD", "name": "Trade Balance", "dataset": "DOT", "description": "Goods, Value of Trade Balance"},
            ]

            # Filter by query if provided
            if query:
                query_terms = [term.strip().lower() for term in query.split(";")]
                filtered_indicators = []
                for indicator in indicators:
                    indicator_text = f"{indicator['symbol']} {indicator['name']} {indicator['description']}".lower()
                    if all(term in indicator_text for term in query_terms):
                        filtered_indicators.append(indicator)
                indicators = filtered_indicators

            return {
                "success": True,
                "data": indicators,
                "count": len(indicators),
                "parameters": {"query": query}
            }

        except Exception as e:
            return {"error": IMFError("available_indicators", str(e)).to_dict()}

    def get_comprehensive_economic_data(self, country: str, start_date: Optional[str] = None,
                                      end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get comprehensive economic data for a country"""
        try:
            if not country:
                return {"error": IMFError("comprehensive_economic_data", "Country parameter is required").to_dict()}

            # Get multiple data types
            results = {}

            # 1. Get top line reserves data
            reserves_result = self.get_economic_indicators(
                countries=country,
                symbols="irfcl_top_lines",
                frequency="quarter",
                start_date=start_date,
                end_date=end_date
            )
            results["reserves"] = reserves_result

            # 2. Get trade data
            trade_result = self.get_direction_of_trade(
                countries=country,
                counterparts="all",
                direction="all",
                frequency="quarter",
                start_date=start_date,
                end_date=end_date
            )
            results["trade"] = trade_result

            # 3. Get available indicators
            indicators_result = self.get_available_indicators()
            results["available_indicators"] = indicators_result

            # Check if we have any successful data
            has_data = any(
                result.get("success") and result.get("data")
                for result in results.values()
            )

            if not has_data:
                return {"error": IMFError("comprehensive_economic_data", "No data found for the specified country").to_dict()}

            return {
                "success": True,
                "data": results,
                "parameters": {
                    "country": country,
                    "start_date": start_date,
                    "end_date": end_date
                }
            }

        except Exception as e:
            return {"error": IMFError("comprehensive_economic_data", str(e)).to_dict()}


def main():
    """Main function for CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python imf_data.py <command> [args...]",
            "commands": [
                "economic_indicators [countries] [symbols] [frequency] [start_date] [end_date] [sector]",
                "direction_of_trade [countries] [counterparts] [direction] [frequency] [start_date] [end_date]",
                "available_indicators [query]",
                "comprehensive_economic_data [country] [start_date] [end_date]"
            ]
        }))
        sys.exit(1)

    command = sys.argv[1]
    wrapper = IMFDataWrapper()

    try:
        if command == "economic_indicators":
            countries = sys.argv[2] if len(sys.argv) > 2 else None
            symbols = sys.argv[3] if len(sys.argv) > 3 else None
            frequency = sys.argv[4] if len(sys.argv) > 4 else "quarter"
            start_date = sys.argv[5] if len(sys.argv) > 5 else None
            end_date = sys.argv[6] if len(sys.argv) > 6 else None
            sector = sys.argv[7] if len(sys.argv) > 7 else "monetary_authorities"

            result = wrapper.get_economic_indicators(countries, symbols, frequency, start_date, end_date, sector)

        elif command == "direction_of_trade":
            countries = sys.argv[2] if len(sys.argv) > 2 else None
            counterparts = sys.argv[3] if len(sys.argv) > 3 else None
            direction = sys.argv[4] if len(sys.argv) > 4 else "all"
            frequency = sys.argv[5] if len(sys.argv) > 5 else "quarter"
            start_date = sys.argv[6] if len(sys.argv) > 6 else None
            end_date = sys.argv[7] if len(sys.argv) > 7 else None

            result = wrapper.get_direction_of_trade(countries, counterparts, direction, frequency, start_date, end_date)

        elif command == "available_indicators":
            query = sys.argv[2] if len(sys.argv) > 2 else None
            result = wrapper.get_available_indicators(query)

        elif command == "comprehensive_economic_data":
            country = sys.argv[2] if len(sys.argv) > 2 else None
            start_date = sys.argv[3] if len(sys.argv) > 3 else None
            end_date = sys.argv[4] if len(sys.argv) > 4 else None

            result = wrapper.get_comprehensive_economic_data(country, start_date, end_date)

        else:
            result = {"error": IMFError(command, f"Unknown command: {command}").to_dict()}

        print(json.dumps(result, indent=2))

    except Exception as e:
        print(json.dumps({"error": IMFError(command, str(e)).to_dict()}, indent=2))


if __name__ == "__main__":
    main()