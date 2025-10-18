# CFTC (Commodity Futures Trading Commission) Data Wrapper
# Modular, fault-tolerant design - each endpoint works independently
# Focus on Commitment of Traders (COT) reports for market sentiment analysis

import sys
import json
import requests
import pandas as pd
from typing import Dict, Any, List, Optional, Union
from datetime import datetime, timedelta
import traceback
import os


class CFTCError:
    """Custom error class for CFTC API errors"""
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
            "type": "CFTCError"
        }


class CFTCDataWrapper:
    """Modular CFTC data wrapper with fault-tolerant endpoints"""

    def __init__(self, app_token: Optional[str] = None):
        self.base_url = "https://publicreporting.cftc.gov"
        self.app_token = app_token or os.environ.get('CFTC_APP_TOKEN', '')
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': 'Fincept Terminal - financial analysis tool (contact@fincept.com)',
            'Accept': 'application/json'
        })

        # Report type mappings
        self.reports_dict = {
            "legacy_futures_only": "6dca-aqww",
            "legacy_combined": "jun7-fc8e",
            "disaggregated_futures_only": "72hh-3qpy",
            "disaggregated_combined": "kh3c-gbw2",
            "tff_futures_only": "gpe5-46if",
            "tff_combined": "yw9f-hn96",
            "supplemental": "4zgm-a668",
        }

        # Report type descriptions
        self.report_descriptions = {
            "legacy": "Legacy reports with commercial, non-commercial and non-reportable classifications",
            "disaggregated": "Disaggregated reports with Producer/Merchant, Swap Dealers, Managed Money classifications",
            "financial": "Traders in Financial Futures (TFF) reports for financial contracts",
            "supplemental": "Supplemental reports for additional market information"
        }

        # Sample COT contract codes (curated from the full list)
        self.cot_codes = {
            # Agricultural
            "corn": "002602",
            "wheat": "001602",
            "soybeans": "005602",
            "cotton": "033661",
            "cocoa": "073732",
            "coffee": "083731",
            "sugar": "080732",
            "live_cattle": "057642",
            "lean_hogs": "054642",

            # Energy
            "crude_oil": "067651",
            "natural_gas": "02365B",
            "gasoline": "111659",
            "heating_oil": "022651",

            # Metals
            "gold": "088691",
            "silver": "084691",
            "copper": "085692",
            "platinum": "076651",
            "palladium": "075651",

            # Financial
            "euro": "099741",
            "jpy": "097741",
            "british_pound": "096742",
            "swiss_franc": "092741",
            "canadian_dollar": "090741",
            "australian_dollar": "232741",

            # Index Futures
            "s&p_500": "13874A",
            "nasdaq_100": "209742",
            "dow_jones": "124603",
            "nikkei": "240741",
            "vix": "1170E1",

            # Interest Rates
            "treasury_bonds": "020601",
            "treasury_notes_2y": "042601",
            "treasury_notes_5y": "044601",
            "treasury_notes_10y": "043602",
            "fed_funds": "045601",

            # Crypto
            "bitcoin": "133741",
            "ether": "146021",

            # Other
            "us_dollar_index": "098662"
        }

        # Common market codes and names for search
        self.market_mappings = {
            "gold": ["GOLD", "088691"],
            "silver": ["SILVER", "084691"],
            "crude": ["CRUDE OIL", "067651"],
            "wti": ["WTI-PHYSICAL", "067651"],
            "brent": ["BRENT LAST DAY", "06765T"],
            "natural_gas": ["HENRY HUB", "02365B"],
            "corn": ["CORN", "002602"],
            "wheat": ["WHEAT-SRW", "001602"],
            "s&p": ["E-MINI S&P 500", "13874A"],
            "sp500": ["E-MINI S&P 500", "13874A"],
            "nasdaq": ["NASDAQ MINI", "209742"],
            "bitcoin": ["BITCOIN", "133741"],
            "btc": ["BITCOIN", "133741"],
            "ether": ["ETHER CASH SETTLED", "146021"],
            "eth": ["ETHER CASH SETTLED", "146021"],
            "euro": ["EURO FX", "099741"],
            "yen": ["JAPANESE YEN", "097741"],
            "pound": ["BRITISH POUND", "096742"],
            "vix": ["VIX FUTURES", "1170E1"],
            "treasury": ["UST BOND", "020601"],
            "dollar": ["USD INDEX", "098662"]
        }

    def _make_request(self, url: str) -> Dict[str, Any]:
        """Make HTTP request with proper error handling"""
        try:
            # Add app token if available
            if self.app_token:
                separator = "&" if "?" in url else "?"
                url = f"{url}{separator}$$app_token={self.app_token}"

            response = self.session.get(url, timeout=30)
            response.raise_for_status()

            data = response.json()

            # Check for API errors
            if isinstance(data, dict) and "error" in data:
                raise Exception(f"CFTC API error: {data['error']}")

            return data

        except requests.exceptions.RequestException as e:
            raise Exception(f"HTTP request failed: {str(e)}")
        except json.JSONDecodeError as e:
            raise Exception(f"JSON decode error: {str(e)}")

    def _format_date(self, date_str: str) -> str:
        """Format date string to CFTC format (YYYY-MM-DD)"""
        try:
            if not date_str:
                return ""

            # Handle different date formats
            if '-' in date_str:
                # Already in YYYY-MM-DD format
                return date_str
            elif len(date_str) == 8 and date_str.isdigit():
                # Convert YYYYMMDD to YYYY-MM-DD
                return f"{date_str[:4]}-{date_str[4:6]}-{date_str[6:8]}"
            else:
                # Try to parse and format
                date_obj = pd.to_datetime(date_str)
                return date_obj.strftime('%Y-%m-%d')

        except:
            return date_str

    def _build_search_query(self, identifier: str) -> str:
        """Build search query for CFTC identifier"""
        if not identifier or identifier.lower() == "all":
            return ""

        # Check if identifier is in our mappings
        identifier_lower = identifier.lower()
        if identifier_lower in self.market_mappings:
            # Use the exact market name from mappings
            return self.market_mappings[identifier_lower][0]

        # Check if identifier is a 6-digit code
        if identifier.isdigit() and len(identifier) == 6:
            return identifier

        # Otherwise use as wildcard search
        return f"%{identifier}%"

    # COMMITMENT OF TRADERS (COT) ENDPOINTS

    def get_cot_data(self, identifier: str = "all", report_type: str = "legacy",
                     futures_only: bool = False, start_date: Optional[str] = None,
                     end_date: Optional[str] = None, limit: Optional[int] = 1000) -> Dict[str, Any]:
        """Get Commitment of Traders (COT) data"""
        try:
            # Default dates
            if not start_date:
                # Default to 1 year ago for specific codes, last week for general
                if identifier.lower() != "all" and identifier and identifier.replace('-', '').isdigit():
                    start_date = (datetime.now() - timedelta(days=365)).strftime("%Y-%m-%d")
                else:
                    start_date = (datetime.now() - timedelta(days=7)).strftime("%Y-%m-%d")

            if not end_date:
                end_date = datetime.now().strftime("%Y-%m-%d")

            # Format dates
            start_formatted = self._format_date(start_date)
            end_formatted = self._format_date(end_date)

            # Build report type
            report_type_key = report_type.lower()
            if report_type_key == "financial":
                report_type_key = "tff"

            # Add futures_only/combined suffix
            if report_type_key != "supplemental":
                if futures_only:
                    report_type_key += "_futures_only"
                else:
                    report_type_key += "_combined"

            # Validate report type
            if report_type_key not in self.reports_dict:
                return {"error": CFTCError("cot_data", f"Invalid report type: {report_type}").to_dict()}

            # Build base URL
            resource_id = self.reports_dict[report_type_key]
            date_range = f"Report_Date_as_YYYY_MM_DD between '{start_formatted}' AND '{end_formatted}'"

            base_url = f"{self.base_url}/resource/{resource_id}.json?$limit={limit}&$where={date_range}&$order=Report_Date_as_YYYY_MM_DD ASC"

            # Add search filter if identifier is provided
            search_query = self._build_search_query(identifier)
            if search_query and search_query != "all":
                base_url += f" AND (UPPER(contract_market_name) like UPPER('{search_query}') OR UPPER(commodity) like UPPER('{search_query}') OR UPPER(cftc_contract_market_code) like UPPER('{search_query}') OR UPPER(commodity_group_name) like UPPER('{search_query}') OR UPPER(commodity_subgroup_name) like UPPER('{search_query}'))"

            # Make request
            data = self._make_request(base_url)

            if not data:
                return {"error": CFTCError("cot_data", f"No COT data found for identifier: {identifier}").to_dict()}

            # Process and clean data
            processed_data = []
            for record in data:
                # Clean up record
                clean_record = {}
                for key, value in record.items():
                    # Convert keys to lowercase
                    clean_key = key.lower().replace("__", "_")

                    # Handle different data types
                    if isinstance(value, str):
                        if clean_key == "report_date_as_yyyy_mm_dd":
                            # Clean up date
                            clean_record[clean_key] = value.split("T")[0] if "T" in value else value
                        elif clean_key.startswith("pct_"):
                            # Convert percentage strings to decimal
                            try:
                                clean_record[clean_key] = float(value) / 100
                            except:
                                clean_record[clean_key] = value
                        else:
                            clean_record[clean_key] = value
                    elif value is not None:
                        clean_record[clean_key] = value

                processed_data.append(clean_record)

            return {
                "success": True,
                "data": processed_data,
                "parameters": {
                    "identifier": identifier,
                    "report_type": report_type,
                    "futures_only": futures_only,
                    "start_date": start_formatted,
                    "end_date": end_formatted,
                    "limit": limit
                }
            }

        except Exception as e:
            return {"error": CFTCError("cot_data", str(e)).to_dict()}

    def search_cot_markets(self, query: str) -> Dict[str, Any]:
        """Search for available COT markets"""
        try:
            if not query:
                return {"error": CFTCError("cot_search", "Search query is required").to_dict()}

            query_lower = query.lower()
            results = []

            # Search through our market mappings
            for key, (name, code) in self.market_mappings.items():
                if (query_lower in key.lower() or
                    query_lower in name.lower() or
                    query_lower == code.lower() or
                    name.lower().startswith(query_lower) or
                    key.lower().startswith(query_lower)):

                    # Try to categorize the market
                    category = "other"
                    if code.startswith("0"):
                        category = "commodities"
                    elif code.startswith("1"):
                        category = "financial"
                    elif code.startswith("2"):
                        category = "energy"
                    elif code.startswith("3"):
                        category = "metals"

                    results.append({
                        "search_key": key,
                        "cftc_code": code,
                        "market_name": name,
                        "category": category,
                        "description": f"{name} ({code})"
                    })

            if not results:
                return {"error": CFTCError("cot_search", f"No markets found for query: {query}").to_dict()}

            return {
                "success": True,
                "data": results,
                "parameters": {"query": query}
            }

        except Exception as e:
            return {"error": CFTCError("cot_search", str(e)).to_dict()}

    def get_available_report_types(self) -> Dict[str, Any]:
        """Get information about available COT report types"""
        try:
            report_info = {}

            for report_type, description in self.report_descriptions.items():
                report_info[report_type] = {
                    "description": description,
                    "has_futures_only_option": report_type != "supplemental"
                }

            return {
                "success": True,
                "data": report_info,
                "parameters": {}
            }

        except Exception as e:
            return {"error": CFTCError("available_report_types", str(e)).to_dict()}

    # MARKET SENTIMENT ANALYSIS ENDPOINTS

    def analyze_market_sentiment(self, identifier: str, report_type: str = "disaggregated") -> Dict[str, Any]:
        """Analyze market sentiment from COT data"""
        try:
            # Get recent COT data
            cot_result = self.get_cot_data(
                identifier=identifier,
                report_type=report_type,
                start_date=(datetime.now() - timedelta(days=90)).strftime("%Y-%m-%d"),
                limit=20
            )

            if not cot_result.get("success"):
                return cot_result

            cot_data = cot_result["data"]
            if not cot_data:
                return {"error": CFTCError("market_sentiment", f"No COT data available for sentiment analysis: {identifier}").to_dict()}

            # Sort by date (most recent first)
            cot_data.sort(key=lambda x: x.get("report_date_as_yyyy_mm_dd", ""), reverse=True)

            # Get most recent data
            latest_data = cot_data[0]
            previous_data = cot_data[1] if len(cot_data) > 1 else None

            # Calculate sentiment metrics
            sentiment_analysis = {
                "latest_report": latest_data.get("report_date_as_yyyy_mm_dd"),
                "market_name": latest_data.get("market_and_exchange_names", ""),
                "open_interest": latest_data.get("open_interest_all", 0),
                "change_in_oi": 0,
                "commercial_positions": {
                    "long": latest_data.get("comm_long_all", 0),
                    "short": latest_data.get("comm_short_all", 0),
                    "net": latest_data.get("comm_long_all", 0) - latest_data.get("comm_short_all", 0)
                },
                "non_commercial_positions": {
                    "long": latest_data.get("noncomm_long_all", 0),
                    "short": latest_data.get("noncomm_short_all", 0),
                    "net": latest_data.get("noncomm_long_all", 0) - latest_data.get("noncomm_short_all", 0)
                },
                "non_reportable_positions": {
                    "long": latest_data.get("nonreportable_long_all", 0),
                    "short": latest_data.get("nonreportable_short_all", 0)
                }
            }

            # Calculate week-over-week changes if previous data exists
            if previous_data:
                sentiment_analysis["change_in_oi"] = latest_data.get("open_interest_all", 0) - previous_data.get("open_interest_all", 0)
                sentiment_analysis["commercial_positions"]["long_change"] = latest_data.get("comm_long_all", 0) - previous_data.get("comm_long_all", 0)
                sentiment_analysis["commercial_positions"]["short_change"] = latest_data.get("comm_short_all", 0) - previous_data.get("comm_short_all", 0)
                sentiment_analysis["non_commercial_positions"]["long_change"] = latest_data.get("noncomm_long_all", 0) - previous_data.get("noncomm_long_all", 0)
                sentiment_analysis["non_commercial_positions"]["short_change"] = latest_data.get("noncomm_short_all", 0) - previous_data.get("noncomm_short_all", 0)

            # Calculate sentiment scores
            total_oi = sentiment_analysis["open_interest"]
            if total_oi > 0:
                sentiment_analysis["commercial_long_pct"] = (sentiment_analysis["commercial_positions"]["long"] / total_oi) * 100
                sentiment_analysis["non_commercial_long_pct"] = (sentiment_analysis["non_commercial_positions"]["long"] / total_oi) * 100
                sentiment_analysis["non_reportable_long_pct"] = (sentiment_analysis["non_reportable_positions"]["long"] / total_oi) * 100

            # Determine overall sentiment
            net_commercial = sentiment_analysis["commercial_positions"]["net"]
            net_non_commercial = sentiment_analysis["non_commercial_positions"]["net"]
            oi_change_pct = (sentiment_analysis["change_in_oi"] / total_oi * 100) if total_oi > 0 else 0

            sentiment_analysis["overall_sentiment"] = {
                "commercial_bias": "bullish" if net_commercial > 0 else "bearish",
                "non_commercial_bias": "bullish" if net_non_commercial > 0 else "bearish",
                "oi_trend": "increasing" if oi_change_pct > 0 else "decreasing",
                "activity_level": "high" if abs(oi_change_pct) > 5 else "moderate" if abs(oi_change_pct) > 1 else "low"
            }

            return {
                "success": True,
                "data": sentiment_analysis,
                "parameters": {
                    "identifier": identifier,
                    "report_type": report_type
                }
            }

        except Exception as e:
            return {"error": CFTCError("market_sentiment", str(e)).to_dict()}

    def get_position_summary(self, identifier: str, report_type: str = "disaggregated") -> Dict[str, Any]:
        """Get summary of current positions for a market"""
        try:
            # Get latest COT data
            cot_result = self.get_cot_data(
                identifier=identifier,
                report_type=report_type,
                start_date=(datetime.now() - timedelta(days=30)).strftime("%Y-%m-%d"),
                limit=5
            )

            if not cot_result.get("success"):
                return cot_result

            cot_data = cot_result["data"]
            if not cot_data:
                return {"error": CFTCError("position_summary", f"No position data available: {identifier}").to_dict()}

            # Get most recent data
            latest = cot_data[0]

            # Build position summary
            summary = {
                "report_date": latest.get("report_date_as_yyyy_mm_dd"),
                "market_name": latest.get("market_and_exchange_names", ""),
                "open_interest": latest.get("open_interest_all", 0),
                "positions": {
                    "commercial": {
                        "long": latest.get("comm_long_all", 0),
                        "short": latest.get("comm_short_all", 0),
                        "net": latest.get("comm_long_all", 0) - latest.get("comm_short_all", 0),
                        "pct_of_oi": {
                            "long": (latest.get("comm_long_all", 0) / latest.get("open_interest_all", 1)) * 100,
                            "short": (latest.get("comm_short_all", 0) / latest.get("open_interest_all", 1)) * 100
                        }
                    },
                    "non_commercial": {
                        "long": latest.get("noncomm_long_all", 0),
                        "short": latest.get("noncomm_short_all", 0),
                        "net": latest.get("noncomm_long_all", 0) - latest.get("noncomm_short_all", 0),
                        "pct_of_oi": {
                            "long": (latest.get("noncomm_long_all", 0) / latest.get("open_interest_all", 1)) * 100,
                            "short": (latest.get("noncomm_short_all", 0) / latest.get("open_interest_all", 1)) * 100
                        }
                    },
                    "non_reportable": {
                        "long": latest.get("nonreportable_long_all", 0),
                        "short": latest.get("nonreportable_short_all", 0),
                        "pct_of_oi": {
                            "long": (latest.get("nonreportable_long_all", 0) / latest.get("open_interest_all", 1)) * 100,
                            "short": (latest.get("nonreportable_short_all", 0) / latest.get("open_interest_all", 1)) * 100
                        }
                    }
                }
            }

            return {
                "success": True,
                "data": summary,
                "parameters": {
                    "identifier": identifier,
                    "report_type": report_type
                }
            }

        except Exception as e:
            return {"error": CFTCError("position_summary", str(e)).to_dict()}

    # COMPREHENSIVE DATA ENDPOINTS

    def get_comprehensive_cot_overview(self, identifiers: List[str] = None, report_type: str = "disaggregated") -> Dict[str, Any]:
        """Get comprehensive COT overview for multiple markets"""
        try:
            if not identifiers:
                # Default to major markets
                identifiers = ["gold", "crude_oil", "euro", "s&p_500", "bitcoin"]

            results = {}

            for identifier in identifiers:
                # Get position summary
                summary_result = self.get_position_summary(identifier, report_type)
                results[identifier] = summary_result

                # Get sentiment analysis
                sentiment_result = self.analyze_market_sentiment(identifier, report_type)
                results[f"{identifier}_sentiment"] = sentiment_result

            # Check if we have any successful data
            has_data = any(
                result.get("success") and result.get("data")
                for result in results.values()
            )

            if not has_data:
                return {"error": CFTCError("comprehensive_cot_overview", "No COT data found for any markets").to_dict()}

            return {
                "success": True,
                "data": results,
                "parameters": {
                    "identifiers": identifiers,
                    "report_type": report_type
                }
            }

        except Exception as e:
            return {"error": CFTCError("comprehensive_cot_overview", str(e)).to_dict()}

    def get_cot_historical_trend(self, identifier: str, report_type: str = "disaggregated",
                                period: int = 52) -> Dict[str, Any]:
        """Get historical COT trend data for analysis"""
        try:
            # Get historical data
            cot_result = self.get_cot_data(
                identifier=identifier,
                report_type=report_type,
                start_date=(datetime.now() - timedelta(weeks=period)).strftime("%Y-%m-%d"),
                limit=period
            )

            if not cot_result.get("success"):
                return cot_result

            cot_data = cot_result["data"]
            if not cot_data:
                return {"error": CFTCError("cot_historical_trend", f"No historical COT data: {identifier}").to_dict()}

            # Process trend data
            trend_data = []
            for record in cot_data:
                trend_point = {
                    "date": record.get("report_date_as_yyyy_mm_dd"),
                    "open_interest": record.get("open_interest_all", 0),
                    "commercial_long": record.get("comm_long_all", 0),
                    "commercial_short": record.get("comm_short_all", 0),
                    "commercial_net": record.get("comm_long_all", 0) - record.get("comm_short_all", 0),
                    "non_commercial_long": record.get("noncomm_long_all", 0),
                    "non_commercial_short": record.get("noncomm_short_all", 0),
                    "non_commercial_net": record.get("noncomm_long_all", 0) - record.get("noncomm_short_all", 0)
                }
                trend_data.append(trend_point)

            # Sort by date
            trend_data.sort(key=lambda x: x["date"])

            return {
                "success": True,
                "data": trend_data,
                "parameters": {
                    "identifier": identifier,
                    "report_type": report_type,
                    "period": period
                }
            }

        except Exception as e:
            return {"error": CFTCError("cot_historical_trend", str(e)).to_dict()}


def main():
    """Main function for CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python cftc_data.py <command> [args...]",
            "commands": [
                "cot_data [identifier] [report_type] [futures_only] [start_date] [end_date] [limit]",
                "search_cot_markets [query]",
                "available_report_types",
                "market_sentiment [identifier] [report_type]",
                "position_summary [identifier] [report_type]",
                "comprehensive_cot_overview [identifiers] [report_type]",
                "cot_historical_trend [identifier] [report_type] [period]"
            ],
            "note": "CFTC app token optional but recommended for rate limiting. Set CFTC_APP_TOKEN environment variable.",
            "identifier_examples": ["all", "gold", "crude_oil", "euro", "bitcoin", "002602", "088691"]
        }))
        sys.exit(1)

    command = sys.argv[1]
    wrapper = CFTCDataWrapper()

    try:
        if command == "cot_data":
            identifier = sys.argv[2] if len(sys.argv) > 2 else "all"
            report_type = sys.argv[3] if len(sys.argv) > 3 else "legacy"
            futures_only = sys.argv[4].lower() == "true" if len(sys.argv) > 4 else False
            start_date = sys.argv[5] if len(sys.argv) > 5 else None
            end_date = sys.argv[6] if len(sys.argv) > 6 else None
            limit = int(sys.argv[7]) if len(sys.argv) > 7 else 1000
            result = wrapper.get_cot_data(identifier, report_type, futures_only, start_date, end_date, limit)

        elif command == "search_cot_markets":
            query = sys.argv[2] if len(sys.argv) > 2 else None
            result = wrapper.search_cot_markets(query)

        elif command == "available_report_types":
            result = wrapper.get_available_report_types()

        elif command == "market_sentiment":
            identifier = sys.argv[2] if len(sys.argv) > 2 else None
            report_type = sys.argv[3] if len(sys.argv) > 3 else "disaggregated"
            result = wrapper.analyze_market_sentiment(identifier, report_type)

        elif command == "position_summary":
            identifier = sys.argv[2] if len(sys.argv) > 2 else None
            report_type = sys.argv[3] if len(sys.argv) > 3 else "disaggregated"
            result = wrapper.get_position_summary(identifier, report_type)

        elif command == "comprehensive_cot_overview":
            identifiers_str = sys.argv[2] if len(sys.argv) > 2 else None
            identifiers = identifiers_str.split(',') if identifiers_str else None
            report_type = sys.argv[3] if len(sys.argv) > 3 else "disaggregated"
            result = wrapper.get_comprehensive_cot_overview(identifiers, report_type)

        elif command == "cot_historical_trend":
            identifier = sys.argv[2] if len(sys.argv) > 2 else None
            report_type = sys.argv[3] if len(sys.argv) > 3 else "disaggregated"
            period = int(sys.argv[4]) if len(sys.argv) > 4 else 52
            result = wrapper.get_cot_historical_trend(identifier, report_type, period)

        else:
            result = {"error": CFTCError(command, f"Unknown command: {command}").to_dict()}

        print(json.dumps(result, indent=2))

    except Exception as e:
        print(json.dumps({"error": CFTCError(command, str(e)).to_dict()}, indent=2))


if __name__ == "__main__":
    main()