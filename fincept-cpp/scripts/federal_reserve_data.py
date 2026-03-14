"""
Federal Reserve Data Fetcher
Modular, fault-tolerant wrapper for Federal Reserve Economic Data
Based on OpenBB Federal Reserve provider with comprehensive endpoints
Each endpoint works independently with isolated error handling
"""

import sys
import json
import requests
import pandas as pd
from datetime import datetime, timedelta
from typing import Dict, Any, Optional, List, Union
from io import BytesIO

# Federal Reserve API Endpoints
FED_BASE_URL = "https://www.federalreserve.gov"
NY_FED_API_URL = "https://markets.newyorkfed.org/api"

# Maturity constants for Treasury data
TREASURY_MATURITIES = [
    "month_1", "month_3", "month_6", "year_1", "year_2",
    "year_3", "year_5", "year_7", "year_10", "year_20", "year_30"
]

# Money supply measures
MONEY_MEASURES = {
    "M1": "M1",
    "M2": "M2",
    "MCU": "currency",
    "MDD": "demand_deposits",
    "MMFGB": "retail_money_market_funds",
    "MDL": "other_liquid_deposits",
    "MDTS": "small_denomination_time_deposits"
}

# Central Bank Holdings types
HOLDING_TYPES = [
    "all_agency", "agency_debts", "mbs", "cmbs",
    "all_treasury", "bills", "notesbonds", "frn", "tips"
]

class FederalReserveError:
    """Error handling wrapper for Federal Reserve API responses"""
    def __init__(self, endpoint: str, error: str, status_code: Optional[int] = None):
        self.endpoint = endpoint
        self.error = error
        self.status_code = status_code
        self.timestamp = int(datetime.now().timestamp())

    def to_dict(self) -> Dict[str, Any]:
        return {
            "success": False,
            "error": self.error,
            "endpoint": self.endpoint,
            "status_code": self.status_code,
            "timestamp": self.timestamp
        }

class FederalReserveWrapper:
    """Modular Federal Reserve API wrapper with fault tolerance"""

    def __init__(self):
        self.base_url = FED_BASE_URL
        self.ny_fed_url = NY_FED_API_URL
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': 'Fincept-Terminal/1.0'
        })

    def _make_request(self, url: str, params: Optional[Dict] = None, timeout: int = 30) -> Dict[str, Any]:
        """Centralized request handler with comprehensive error handling"""
        try:
            response = self.session.get(url, params=params, timeout=timeout)
            response.raise_for_status()

            # Check if response is JSON or CSV
            content_type = response.headers.get('content-type', '')
            if 'json' in content_type.lower():
                return {"success": True, "data": response.json(), "format": "json"}
            else:
                return {"success": True, "data": response.content, "format": "binary"}

        except requests.exceptions.Timeout:
            return {"error": "Request timeout", "timeout": True, "status_code": None}
        except requests.exceptions.ConnectionError:
            return {"error": "Connection error", "connection_error": True, "status_code": None}
        except requests.exceptions.HTTPError as e:
            if response.status_code == 404:
                return {"error": "Data not found", "not_found": True, "status_code": response.status_code}
            else:
                return {"error": f"HTTP error: {e}", "http_error": True, "status_code": response.status_code}
        except requests.exceptions.RequestException as e:
            return {"error": f"Request error: {e}", "request_error": True, "status_code": None}
        except Exception as e:
            return {"error": f"Unexpected error: {e}", "general_error": True, "status_code": None}

    # ===== FEDERAL FUNDS RATE ENDPOINT =====

    def get_federal_funds_rate(self, start_date: Optional[str] = None, end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get Federal Funds Rate data from NY Fed"""
        try:
            # Set default dates
            if not start_date:
                start_date = "2016-03-01"
            if not end_date:
                end_date = datetime.now().strftime("%Y-%m-%d")

            url = f"{self.ny_fed_url}/rates/unsecured/effr/search.json"
            params = {
                "startDate": start_date,
                "endDate": end_date
            }

            result = self._make_request(url, params)

            if "error" in result:
                return FederalReserveError('federal_funds_rate', result['error'], result.get('status_code')).to_dict()

            if result.get("format") == "json":
                data = result.get("data", {})
                ref_rates = data.get("refRates", [])

                if not ref_rates:
                    return FederalReserveError('federal_funds_rate', 'No federal funds rate data found').to_dict()

                # Process the data
                processed_data = []
                for rate_data in ref_rates:
                    processed_data.append({
                        "date": rate_data.get("effectiveDate"),
                        "rate": float(rate_data.get("percentRate", 0)) / 100 if rate_data.get("percentRate") else None,
                        "target_range_upper": float(rate_data.get("targetRateTo", 0)) / 100 if rate_data.get("targetRateTo") else None,
                        "target_range_lower": float(rate_data.get("targetRateFrom", 0)) / 100 if rate_data.get("targetRateFrom") else None,
                        "percentile_1": float(rate_data.get("percentPercentile1", 0)) / 100 if rate_data.get("percentPercentile1") else None,
                        "percentile_25": float(rate_data.get("percentPercentile25", 0)) / 100 if rate_data.get("percentPercentile25") else None,
                        "percentile_75": float(rate_data.get("percentPercentile75", 0)) / 100 if rate_data.get("percentPercentile75") else None,
                        "percentile_99": float(rate_data.get("percentPercentile99", 0)) / 100 if rate_data.get("percentPercentile99") else None,
                        "volume": float(rate_data.get("volumeInBillions", 0)) if rate_data.get("volumeInBillions") else None,
                        "intraday_low": float(rate_data.get("intraDayLow", 0)) / 100 if rate_data.get("intraDayLow") else None,
                        "intraday_high": float(rate_data.get("intraDayHigh", 0)) / 100 if rate_data.get("intraDayHigh") else None,
                        "standard_deviation": float(rate_data.get("stdDeviation", 0)) / 100 if rate_data.get("stdDeviation") else None,
                        "revision_indicator": rate_data.get("revisionIndicator")
                    })

                return {
                    "success": True,
                    "endpoint": "federal_funds_rate",
                    "data": processed_data,
                    "parameters": {
                        "start_date": start_date,
                        "end_date": end_date
                    },
                    "total_records": len(processed_data),
                    "timestamp": int(datetime.now().timestamp())
                }

            else:
                return FederalReserveError('federal_funds_rate', 'Invalid response format').to_dict()

        except Exception as e:
            return FederalReserveError('federal_funds_rate', str(e)).to_dict()

    # ===== SOFR RATE ENDPOINT =====

    def get_sofr_rate(self, start_date: Optional[str] = None, end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get Secured Overnight Financing Rate (SOFR) data"""
        try:
            # Set default dates
            if not start_date:
                start_date = "2018-04-02"  # SOFR start date
            if not end_date:
                end_date = datetime.now().strftime("%Y-%m-%d")

            url = f"{self.ny_fed_url}/rates/secured/sofr/search.json"
            params = {
                "startDate": start_date,
                "endDate": end_date
            }

            result = self._make_request(url, params)

            if "error" in result:
                return FederalReserveError('sofr_rate', result['error'], result.get('status_code')).to_dict()

            if result.get("format") == "json":
                data = result.get("data", {})
                ref_rates = data.get("refRates", [])

                if not ref_rates:
                    return FederalReserveError('sofr_rate', 'No SOFR data found').to_dict()

                # Process the data
                processed_data = []
                for rate_data in ref_rates:
                    def safe_float_convert(value):
                        """Safely convert to float, handling 'NA' and None values"""
                        if not value or value == "NA" or value == "''":
                            return None
                        try:
                            return float(value) / 100 if float(value) != 0 else 0
                        except (ValueError, TypeError):
                            return None

                    def safe_volume_convert(value):
                        """Safely convert volume to float"""
                        if not value or value == "NA" or value == "''":
                            return None
                        try:
                            return float(value)
                        except (ValueError, TypeError):
                            return None

                    processed_data.append({
                        "date": rate_data.get("effectiveDate"),
                        "rate": safe_float_convert(rate_data.get("percentRate")),
                        "percentile_1": safe_float_convert(rate_data.get("percentPercentile1")),
                        "percentile_25": safe_float_convert(rate_data.get("percentPercentile25")),
                        "percentile_75": safe_float_convert(rate_data.get("percentPercentile75")),
                        "percentile_99": safe_float_convert(rate_data.get("percentPercentile99")),
                        "volume": safe_volume_convert(rate_data.get("volumeInBillions"))
                    })

                return {
                    "success": True,
                    "endpoint": "sofr_rate",
                    "data": processed_data,
                    "parameters": {
                        "start_date": start_date,
                        "end_date": end_date
                    },
                    "total_records": len(processed_data),
                    "timestamp": int(datetime.now().timestamp())
                }

            else:
                return FederalReserveError('sofr_rate', 'Invalid response format').to_dict()

        except Exception as e:
            return FederalReserveError('sofr_rate', str(e)).to_dict()

    # ===== TREASURY RATES ENDPOINT =====

    def get_treasury_rates(self, start_date: Optional[str] = None, end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get Treasury rates data (H.15 release)"""
        try:
            # Set default dates
            if not start_date:
                start_date = (datetime.now() - timedelta(days=365)).strftime("%Y-%m-%d")
            if not end_date:
                end_date = datetime.now().strftime("%Y-%m-%d")

            url = (
                f"{self.base_url}/datadownload/Output.aspx?"
                "rel=H15&series=bf17364827e38702b42a58cf8eaa3f78&lastobs=&"
                "from=&to=&filetype=csv&label=include&layout=seriescolumn&type=package"
            )

            result = self._make_request(url)

            if "error" in result:
                return FederalReserveError('treasury_rates', result['error'], result.get('status_code')).to_dict()

            if result.get("format") == "binary":
                # Parse CSV data
                df = pd.read_csv(BytesIO(result["data"]), header=5, index_col=None, parse_dates=True)
                df.columns = ["date"] + TREASURY_MATURITIES
                df = df.set_index("date").replace("ND", pd.NA)
                df = df.dropna(axis=0, how="all").reset_index()

                # Filter by date range
                df["date"] = pd.to_datetime(df["date"])
                start_dt = pd.to_datetime(start_date)
                end_dt = pd.to_datetime(end_date)
                df = df[(df["date"] >= start_dt) & (df["date"] <= end_dt)]

                # Convert percentages to decimal
                for maturity in TREASURY_MATURITIES:
                    df[maturity] = pd.to_numeric(df[maturity], errors='coerce') / 100

                # Convert to list of dictionaries
                df = df.fillna("N/A").replace("N/A", None)
                # Convert Timestamp objects to strings
                df["date"] = df["date"].dt.strftime("%Y-%m-%d")
                processed_data = df.to_dict(orient="records")

                return {
                    "success": True,
                    "endpoint": "treasury_rates",
                    "data": processed_data,
                    "parameters": {
                        "start_date": start_date,
                        "end_date": end_date
                    },
                    "total_records": len(processed_data),
                    "timestamp": int(datetime.now().timestamp())
                }

            else:
                return FederalReserveError('treasury_rates', 'Invalid response format').to_dict()

        except Exception as e:
            return FederalReserveError('treasury_rates', str(e)).to_dict()

    # ===== YIELD CURVE ENDPOINT =====

    def get_yield_curve(self, date: Optional[str] = None) -> Dict[str, Any]:
        """Get yield curve data for specific date(s)"""
        try:
            if not date:
                # Default to latest available date
                date = datetime.now().strftime("%Y-%m-%d")

            url = (
                f"{self.base_url}/datadownload/Output.aspx?"
                "rel=H15&series=bf17364827e38702b42a58cf8eaa3f78&lastobs=&"
                "from=&to=&filetype=csv&label=include&layout=seriescolumn&type=package"
            )

            result = self._make_request(url)

            if "error" in result:
                return FederalReserveError('yield_curve', result['error'], result.get('status_code')).to_dict()

            if result.get("format") == "binary":
                # Parse CSV data
                df = pd.read_csv(BytesIO(result["data"]), header=5, index_col=None, parse_dates=True)
                df.columns = ["date"] + TREASURY_MATURITIES
                df = df.set_index("date").replace("ND", pd.NA)
                df = df.dropna(axis=0, how="all")

                # Handle multiple dates
                dates = [d.strip() for d in date.split(",")]
                df.index = pd.to_datetime(df.index)

                # Find nearest dates in data
                nearest_dates = []
                for target_date in dates:
                    try:
                        target_dt = pd.to_datetime(target_date)
                        nearest = df.index.asof(target_dt)
                        if nearest is not pd.NaT:
                            nearest_dates.append(nearest)
                    except:
                        continue

                if not nearest_dates:
                    return FederalReserveError('yield_curve', 'No valid dates found').to_dict()

                df = df[df.index.isin(nearest_dates)]
                df = df.fillna("N/A").replace("N/A", None)

                # Flatten the data
                flattened_data = df.reset_index().melt(
                    id_vars="date", var_name="maturity", value_name="rate"
                )
                flattened_data = flattened_data.sort_values(["date", "maturity"])
                flattened_data["rate"] = pd.to_numeric(flattened_data["rate"], errors='coerce') / 100
                flattened_data["date"] = flattened_data["date"].dt.strftime("%Y-%m-%d")

                processed_data = flattened_data.to_dict(orient="records")

                return {
                    "success": True,
                    "endpoint": "yield_curve",
                    "data": processed_data,
                    "parameters": {
                        "date": date
                    },
                    "total_records": len(processed_data),
                    "timestamp": int(datetime.now().timestamp())
                }

            else:
                return FederalReserveError('yield_curve', 'Invalid response format').to_dict()

        except Exception as e:
            return FederalReserveError('yield_curve', str(e)).to_dict()

    # ===== MONEY MEASURES ENDPOINT =====

    def get_money_measures(self, start_date: Optional[str] = None, end_date: Optional[str] = None, adjusted: bool = False) -> Dict[str, Any]:
        """Get Money Supply Measures (M1, M2) data"""
        try:
            # Set default dates
            if not start_date:
                start_date = (datetime.now() - timedelta(days=10*365)).strftime("%Y-%m-%d")
            if not end_date:
                end_date = datetime.now().strftime("%Y-%m-%d")

            url = (
                f"{self.base_url}/datadownload/Output.aspx?rel=H6&series=798e2796917702a5f8423426ba7e6b42"
                "&lastobs=&from=&to=&filetype=csv&label=include&layout=seriescolumn&type=package"
            )

            result = self._make_request(url)

            if "error" in result:
                return FederalReserveError('money_measures', result['error'], result.get('status_code')).to_dict()

            if result.get("format") == "binary":
                # Parse CSV data
                df = pd.read_csv(BytesIO(result["data"]), header=5, index_col=None, parse_dates=True)

                # Select relevant columns
                suffix = "_N" if adjusted else ""
                columns_to_get = ["Time Period"] + [col + f"{suffix}.M" for col in MONEY_MEASURES.keys()]
                df = df[columns_to_get]
                df.columns = ["month"] + list(MONEY_MEASURES.values())
                df = df.replace("ND", None)
                df["month"] = pd.to_datetime(df["month"])

                # Filter by date range
                start_dt = pd.to_datetime(start_date)
                end_dt = pd.to_datetime(end_date)
                df = df[(df["month"] >= start_dt) & (df["month"] <= end_dt)]
                df = df.set_index("month")

                # Convert numeric values
                df = df.applymap(lambda x: float(x) if x != "-" and x is not None else x)
                df = df.reset_index(drop=False)

                # Convert to list of dictionaries
                # Convert Timestamp objects to strings
                df["month"] = df["month"].dt.strftime("%Y-%m-%d")
                processed_data = df.to_dict(orient="records")

                return {
                    "success": True,
                    "endpoint": "money_measures",
                    "data": processed_data,
                    "parameters": {
                        "start_date": start_date,
                        "end_date": end_date,
                        "adjusted": adjusted
                    },
                    "total_records": len(processed_data),
                    "timestamp": int(datetime.now().timestamp())
                }

            else:
                return FederalReserveError('money_measures', 'Invalid response format').to_dict()

        except Exception as e:
            return FederalReserveError('money_measures', str(e)).to_dict()

    # ===== CENTRAL BANK HOLDINGS ENDPOINT =====

    def get_central_bank_holdings(self, holding_type: str = "all_treasury", summary: bool = False,
                                 date: Optional[str] = None) -> Dict[str, Any]:
        """Get Federal Reserve Central Bank Holdings (SOMA) data"""
        try:
            if holding_type not in HOLDING_TYPES:
                return FederalReserveError('central_bank_holdings', f'Invalid holding type: {holding_type}').to_dict()

            # For now, return a simplified implementation
            # Full implementation would require the complex NY Fed API from OpenBB
            if summary:
                # Return summary data structure
                return {
                    "success": True,
                    "endpoint": "central_bank_holdings",
                    "data": {
                        "message": "Central bank holdings summary - requires NY Fed API implementation",
                        "holding_type": holding_type,
                        "note": "This endpoint requires the full NY Fed SOMA API implementation"
                    },
                    "parameters": {
                        "holding_type": holding_type,
                        "summary": summary,
                        "date": date
                    },
                    "timestamp": int(datetime.now().timestamp())
                }
            else:
                return {
                    "success": True,
                    "endpoint": "central_bank_holdings",
                    "data": {
                        "message": "Central bank holdings detailed data - requires NY Fed API implementation",
                        "holding_type": holding_type,
                        "note": "This endpoint requires the full NY Fed SOMA API implementation"
                    },
                    "parameters": {
                        "holding_type": holding_type,
                        "summary": summary,
                        "date": date
                    },
                    "timestamp": int(datetime.now().timestamp())
                }

        except Exception as e:
            return FederalReserveError('central_bank_holdings', str(e)).to_dict()

    # ===== OVERNIGHT BANK FUNDING RATE ENDPOINT =====

    def get_overnight_bank_funding_rate(self, start_date: Optional[str] = None, end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get Overnight Bank Funding Rate data"""
        try:
            # Set default dates
            if not start_date:
                start_date = (datetime.now() - timedelta(days=365)).strftime("%Y-%m-%d")
            if not end_date:
                end_date = datetime.now().strftime("%Y-%m-%d")

            # This would use the NY Fed API - simplified implementation for now
            return {
                "success": True,
                "endpoint": "overnight_bank_funding_rate",
                "data": {
                    "message": "Overnight Bank Funding Rate - requires NY Fed API implementation",
                    "note": "This endpoint requires the full NY Fed API implementation"
                },
                "parameters": {
                    "start_date": start_date,
                    "end_date": end_date
                },
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return FederalReserveError('overnight_bank_funding_rate', str(e)).to_dict()

    # ===== COMPOSITE METHODS =====

    def get_comprehensive_monetary_data(self, start_date: Optional[str] = None, end_date: Optional[str] = None) -> Dict[str, Any]:
        """Get comprehensive monetary data from multiple endpoints"""
        result = {
            "success": True,
            "start_date": start_date,
            "end_date": end_date,
            "timestamp": int(datetime.now().timestamp()),
            "endpoints": {},
            "failed_endpoints": []
        }

        # Define endpoints to try
        endpoints = [
            ('federal_funds_rate', lambda: self.get_federal_funds_rate(start_date, end_date)),
            ('sofr_rate', lambda: self.get_sofr_rate(start_date, end_date)),
            ('treasury_rates', lambda: self.get_treasury_rates(start_date, end_date)),
            ('money_measures', lambda: self.get_money_measures(start_date, end_date))
        ]

        overall_success = False

        for endpoint_name, endpoint_func in endpoints:
            try:
                endpoint_result = endpoint_func()
                result["endpoints"][endpoint_name] = endpoint_result

                if endpoint_result.get("success"):
                    overall_success = True
                else:
                    result["failed_endpoints"].append({
                        "endpoint": endpoint_name,
                        "error": endpoint_result.get("error", "Unknown error")
                    })

            except Exception as e:
                result["failed_endpoints"].append({
                    "endpoint": endpoint_name,
                    "error": str(e)
                })

        result["success"] = overall_success
        return result

    def get_market_overview(self) -> Dict[str, Any]:
        """Get current market overview with key rates"""
        result = {
            "success": True,
            "timestamp": int(datetime.now().timestamp()),
            "endpoints": {},
            "failed_endpoints": []
        }

        # Get recent data for key rates
        end_date = datetime.now().strftime("%Y-%m-%d")
        start_date = (datetime.now() - timedelta(days=7)).strftime("%Y-%m-%d")

        endpoints = [
            ('federal_funds_rate', lambda: self.get_federal_funds_rate(start_date, end_date)),
            ('sofr_rate', lambda: self.get_sofr_rate(start_date, end_date)),
            ('treasury_rates', lambda: self.get_treasury_rates(start_date, end_date))
        ]

        overall_success = False

        for endpoint_name, endpoint_func in endpoints:
            try:
                endpoint_result = endpoint_func()
                result["endpoints"][endpoint_name] = endpoint_result

                if endpoint_result.get("success"):
                    overall_success = True
                else:
                    result["failed_endpoints"].append({
                        "endpoint": endpoint_name,
                        "error": endpoint_result.get("error", "Unknown error")
                    })

            except Exception as e:
                result["failed_endpoints"].append({
                    "endpoint": endpoint_name,
                    "error": str(e)
                })

        result["success"] = overall_success
        return result

# ===== CLI INTERFACE =====

def main():
    """CLI interface for Federal Reserve Data Fetcher"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python federal_reserve_data.py <command> <args>",
            "available_commands": [
                "federal_funds_rate [start_date] [end_date]",
                "sofr_rate [start_date] [end_date]",
                "treasury_rates [start_date] [end_date]",
                "yield_curve [date]",
                "money_measures [start_date] [end_date] [adjusted]",
                "central_bank_holdings [holding_type] [summary] [date]",
                "overnight_bank_funding_rate [start_date] [end_date]",
                "comprehensive_monetary_data [start_date] [end_date]",
                "market_overview"
            ],
            "note": "No API key required - Federal Reserve data is publicly available"
        }))
        sys.exit(1)

    command = sys.argv[1]
    wrapper = FederalReserveWrapper()

    try:
        if command == "federal_funds_rate":
            start_date = sys.argv[2] if len(sys.argv) > 2 else None
            end_date = sys.argv[3] if len(sys.argv) > 3 else None
            result = wrapper.get_federal_funds_rate(start_date, end_date)
            print(json.dumps(result, indent=2))

        elif command == "sofr_rate":
            start_date = sys.argv[2] if len(sys.argv) > 2 else None
            end_date = sys.argv[3] if len(sys.argv) > 3 else None
            result = wrapper.get_sofr_rate(start_date, end_date)
            print(json.dumps(result, indent=2))

        elif command == "treasury_rates":
            start_date = sys.argv[2] if len(sys.argv) > 2 else None
            end_date = sys.argv[3] if len(sys.argv) > 3 else None
            result = wrapper.get_treasury_rates(start_date, end_date)
            print(json.dumps(result, indent=2))

        elif command == "yield_curve":
            date = sys.argv[2] if len(sys.argv) > 2 else None
            result = wrapper.get_yield_curve(date)
            print(json.dumps(result, indent=2))

        elif command == "money_measures":
            start_date = sys.argv[2] if len(sys.argv) > 2 else None
            end_date = sys.argv[3] if len(sys.argv) > 3 else None
            adjusted = sys.argv[4].lower() == "true" if len(sys.argv) > 4 else False
            result = wrapper.get_money_measures(start_date, end_date, adjusted)
            print(json.dumps(result, indent=2))

        elif command == "central_bank_holdings":
            holding_type = sys.argv[2] if len(sys.argv) > 2 else "all_treasury"
            summary = sys.argv[3].lower() == "true" if len(sys.argv) > 3 else False
            date = sys.argv[4] if len(sys.argv) > 4 else None
            result = wrapper.get_central_bank_holdings(holding_type, summary, date)
            print(json.dumps(result, indent=2))

        elif command == "overnight_bank_funding_rate":
            start_date = sys.argv[2] if len(sys.argv) > 2 else None
            end_date = sys.argv[3] if len(sys.argv) > 3 else None
            result = wrapper.get_overnight_bank_funding_rate(start_date, end_date)
            print(json.dumps(result, indent=2))

        elif command == "comprehensive_monetary_data":
            start_date = sys.argv[2] if len(sys.argv) > 2 else None
            end_date = sys.argv[3] if len(sys.argv) > 3 else None
            result = wrapper.get_comprehensive_monetary_data(start_date, end_date)
            print(json.dumps(result, indent=2))

        elif command == "market_overview":
            result = wrapper.get_market_overview()
            print(json.dumps(result, indent=2))

        else:
            print(json.dumps({
                "error": f"Unknown command: {command}",
                "available_commands": [
                    "federal_funds_rate [start_date] [end_date]",
                    "sofr_rate [start_date] [end_date]",
                    "treasury_rates [start_date] [end_date]",
                    "yield_curve [date]",
                    "money_measures [start_date] [end_date] [adjusted]",
                    "central_bank_holdings [holding_type] [summary] [date]",
                    "overnight_bank_funding_rate [start_date] [end_date]",
                    "comprehensive_monetary_data [start_date] [end_date]",
                    "market_overview"
                ]
            }))
            sys.exit(1)

    except KeyboardInterrupt:
        print(json.dumps({"error": "Operation cancelled by user"}))
        sys.exit(1)
    except Exception as e:
        print(json.dumps({"error": f"Unexpected error: {str(e)}"}))
        sys.exit(1)

if __name__ == "__main__":
    main()