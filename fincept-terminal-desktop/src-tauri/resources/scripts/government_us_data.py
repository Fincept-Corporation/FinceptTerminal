"""
US Government Data Fetcher
Modular, fault-tolerant wrapper for US Treasury data based on OpenBB government_us provider
Sources: TreasuryDirect.gov official APIs
Each endpoint works independently with isolated error handling
"""

import sys
import json
import requests
import pandas as pd
from datetime import datetime, timedelta, date
from typing import Dict, Any, Optional, List, Union
from io import StringIO
import re

try:
    from random_user_agent.user_agent import UserAgent

    def get_random_agent() -> str:
        """Generate a random user agent for a request."""
        user_agent_rotator = UserAgent(limit=100)
        user_agent = user_agent_rotator.get_random_user_agent()
        return user_agent
except ImportError:
    def get_random_agent() -> str:
        """Fallback user agent if random_user_agent not available."""
        return 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36'

class GovernmentUSError:
    """Error handling wrapper for government data responses"""
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

class GovernmentUSWrapper:
    """Modular US Government data wrapper with fault tolerance"""

    def __init__(self):
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36'
        })
        self.treasury_base_url = "https://www.treasurydirect.gov"
        self.treasury_price_url = "https://treasurydirect.gov/GA-FI/FedInvest/securityPriceDetail"

    def _make_request(self, url: str, method: str = 'GET', headers: Optional[Dict] = None,
                     data: Optional[str] = None, params: Optional[Dict] = None) -> Dict[str, Any]:
        """Make HTTP request with comprehensive error handling"""
        try:
            response = self.session.request(
                method=method,
                url=url,
                headers=headers,
                data=data,
                params=params,
                timeout=30
            )
            response.raise_for_status()
            return {"success": True, "data": response}
        except requests.exceptions.Timeout:
            return {"error": "Request timeout", "timeout": True, "status_code": None}
        except requests.exceptions.ConnectionError:
            return {"error": "Connection error", "connection_error": True, "status_code": None}
        except requests.exceptions.HTTPError as e:
            return {"error": f"HTTP error: {e}", "http_error": True, "status_code": response.status_code}
        except requests.exceptions.RequestException as e:
            return {"error": f"Request error: {e}", "request_error": True, "status_code": None}
        except Exception as e:
            return {"error": f"Unexpected error: {e}", "general_error": True, "status_code": None}

    def _parse_csv_response(self, content: str) -> pd.DataFrame:
        """Parse CSV response from Treasury Direct using pandas (like OpenBB)"""
        try:
            if not content:
                raise ValueError("Empty content")

            # Use pandas to read CSV exactly like OpenBB does
            results = pd.read_csv(StringIO(content), header=0)

            # Set column names to match OpenBB exactly
            results.columns = pd.Index([
                "cusip",
                "security_type",
                "rate",
                "maturity_date",
                "call_date",
                "bid",
                "offer",
                "eod_price",
            ])

            return results

        except Exception as e:
            raise ValueError(f"Failed to parse CSV response: {str(e)}")

    # ===== TREASURY PRICES ENDPOINT =====

    def get_treasury_prices(self, target_date: Optional[str] = None,
                           cusip: Optional[str] = None,
                           security_type: Optional[str] = None) -> Dict[str, Any]:
        """
        Get US Treasury prices for a specific date
        Args:
            target_date: Date in YYYY-MM-DD format (default: last business day)
            cusip: Filter by CUSIP
            security_type: Filter by security type (bill, note, bond, tips, frn)
        """
        try:
            # Set default date to last business day if not provided (exact OpenBB logic)
            if not target_date:
                today = date.today()
                last_bd = (
                    today - timedelta(today.weekday() - 4) if today.weekday() > 4 else today
                )
                date_obj = last_bd
            else:
                date_obj = datetime.strptime(target_date, '%Y-%m-%d').date()

            # Use exact OpenBB headers
            HEADERS = {
                "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8",
                "Accept-Language": "en-CA,en-US;q=0.7,en;q=0.3",
                "Accept-Encoding": "gzip, deflate, br",
                "Referer": "https://treasurydirect.gov/",
                "Content-Type": "application/x-www-form-urlencoded",
                "Origin": "https://treasurydirect.gov",
                "User-Agent": get_random_agent(),
            }

            # Exact OpenBB payload format
            payload = (
                f"priceDateDay={date_obj.day}"
                f"&priceDateMonth={date_obj.month}"
                f"&priceDateYear={date_obj.year}"
                "&fileType=csv"
                "&csv=CSV+FORMAT"
            )

            result = self._make_request(
                url=self.treasury_price_url,
                method='POST',
                headers=HEADERS,
                data=payload
            )

            if "error" in result:
                return GovernmentUSError('treasury_prices', result['error']).to_dict()

            response = result['data']
            if response.status_code != 200:
                return GovernmentUSError('treasury_prices', f'HTTP {response.status_code}', response.status_code).to_dict()

            # OpenBB checks for ISO-8859-1 encoding specifically
            if response.encoding != "ISO-8859-1":
                return GovernmentUSError('treasury_prices', f'Expected ISO-8859-1 encoding but got: {response.encoding}').to_dict()

            content = response.content.decode("utf-8")

            # Parse CSV data using OpenBB method
            try:
                results = self._parse_csv_response(content)

                # Add date column (OpenBB logic)
                results["date"] = date_obj.strftime("%Y-%m-%d")

                # Process date columns exactly like OpenBB
                for col in ["maturity_date", "call_date"]:
                    results[col] = (
                        pd.to_datetime(results[col], format="%m/%d/%Y").dt.strftime("%Y-%m-%d")
                    ).fillna("-").replace("-", None)

                # Apply filters exactly like OpenBB
                if security_type is not None:
                    results = results[
                        results["security_type"].str.contains(security_type, case=False)
                    ]
                if cusip is not None:
                    results = results[results["cusip"] == cusip]

                # Convert to list of dictionaries
                standardized_data = []
                for _, row in results.iterrows():
                    standardized_row = {
                        "cusip": row.get('cusip', ''),
                        "security_type": row.get('security_type', ''),
                        "rate": float(row.get('rate', 0)) if pd.notna(row.get('rate')) else None,
                        "maturity_date": row.get('maturity_date'),
                        "call_date": row.get('call_date'),
                        "bid": float(row.get('bid', 0)) if pd.notna(row.get('bid')) else None,
                        "offer": float(row.get('offer', 0)) if pd.notna(row.get('offer')) else None,
                        "eod_price": float(row.get('eod_price', 0)) if pd.notna(row.get('eod_price')) else None,
                        "date": row.get('date', target_date)
                    }
                    standardized_data.append(standardized_row)

                return {
                    "success": True,
                    "endpoint": "treasury_prices",
                    "date": date_obj.strftime('%Y-%m-%d'),
                    "total_records": len(standardized_data),
                    "filters": {
                        "cusip": cusip,
                        "security_type": security_type
                    },
                    "data": standardized_data,
                    "timestamp": int(datetime.now().timestamp())
                }

            except Exception as e:
                return GovernmentUSError('treasury_prices', f'Failed to parse data: {str(e)}').to_dict()

        except Exception as e:
            return GovernmentUSError('treasury_prices', str(e)).to_dict()

    def _parse_date(self, date_str: Optional[str]) -> Optional[str]:
        """Parse date string to YYYY-MM-DD format"""
        if not date_str or date_str in ['-', '', 'N/A']:
            return None
        try:
            # Try different date formats
            for fmt in ['%m/%d/%Y', '%Y-%m-%d', '%d-%b-%Y']:
                try:
                    parsed = datetime.strptime(date_str, fmt)
                    return parsed.strftime('%Y-%m-%d')
                except ValueError:
                    continue
            return None
        except:
            return None

    # ===== TREASURY AUCTIONS ENDPOINT =====

    def get_treasury_auctions(self, start_date: Optional[str] = None,
                             end_date: Optional[str] = None,
                             security_type: Optional[str] = None,
                             page_size: int = 100,
                             page_num: int = 1) -> Dict[str, Any]:
        """
        Get US Treasury auction data
        Args:
            start_date: Start date in MM/DD/YYYY format
            end_date: End date in MM/DD/YYYY format
            security_type: Security type (Bill, Note, Bond, CMB, TIPS, FRN)
            page_size: Number of results per page (max 100)
            page_num: Page number
        """
        try:
            # Security type mapping
            security_mapping = {
                "bill": "Bill",
                "note": "Note",
                "bond": "Bond",
                "cmb": "CMB",
                "tips": "TIPS",
                "frn": "FRN"
            }

            # Format dates
            formatted_start_date = None
            formatted_end_date = None

            if start_date:
                try:
                    # Convert YYYY-MM-DD to MM/DD/YYYY if needed
                    if '-' in start_date:
                        date_obj = datetime.strptime(start_date, '%Y-%m-%d')
                        formatted_start_date = date_obj.strftime('%m/%d/%Y')
                    else:
                        formatted_start_date = start_date
                except ValueError:
                    return GovernmentUSError('treasury_auctions', 'Invalid start_date format. Use YYYY-MM-DD or MM/DD/YYYY').to_dict()

            if end_date:
                try:
                    if '-' in end_date:
                        date_obj = datetime.strptime(end_date, '%Y-%m-%d')
                        formatted_end_date = date_obj.strftime('%m/%d/%Y')
                    else:
                        formatted_end_date = end_date
                except ValueError:
                    return GovernmentUSError('treasury_auctions', 'Invalid end_date format. Use YYYY-MM-DD or MM/DD/YYYY').to_dict()

            # Map security type
            mapped_security_type = security_mapping.get(security_type.lower(), security_type) if security_type else None

            # Build query parameters
            params = {
                "pagesize": min(page_size, 100),  # Max 100 per page
                "pagenum": page_num,
                "format": "json"
            }

            if formatted_start_date:
                params["startDate"] = formatted_start_date
            if formatted_end_date:
                params["endDate"] = formatted_end_date
            if mapped_security_type:
                params["type"] = mapped_security_type

            # Make request
            url = f"{self.treasury_base_url}/TA_WS/securities/search"
            result = self._make_request(url=url, params=params)

            if "error" in result:
                return GovernmentUSError('treasury_auctions', result['error']).to_dict()

            response = result['data']
            if response.status_code != 200:
                return GovernmentUSError('treasury_auctions', f'HTTP {response.status_code}', response.status_code).to_dict()

            try:
                json_data = response.json()

                if not isinstance(json_data, list):
                    return GovernmentUSError('treasury_auctions', 'Invalid JSON response format').to_dict()

                # Convert to DataFrame and process exactly like OpenBB
                data = pd.DataFrame(json_data)
                results = (
                    data.fillna("N/A").replace("", None).replace("N/A", None).to_dict("records")
                )

                # Apply OpenBB's percentage normalization logic
                processed_data = []
                for item in results:
                    processed_item = {}
                    for k, v in item.items():
                        # OpenBB's exact percentage normalization logic
                        if (
                            k.endswith("Rate")
                            or k.endswith("Yield")
                            or k.endswith("Margin")
                            or k.endswith("Percentage")
                            or "spread" in k.lower()
                        ):
                            if v not in ["Yes", "No", None, ""]:
                                processed_item[k] = float(v) / 100 if v else None
                            else:
                                processed_item[k] = v if v in ["Yes", "No"] else None
                        elif v in ['', 'N/A', None]:
                            processed_item[k] = None
                        elif k.endswith(('Amount', 'Accepted', 'Tendered', 'Award', 'Holdings')):
                            try:
                                processed_item[k] = int(float(v)) if v else None
                            except (ValueError, TypeError):
                                processed_item[k] = None
                        elif k.endswith(('Price', 'Ratio', 'Factor')):
                            try:
                                processed_item[k] = float(v) if v else None
                            except (ValueError, TypeError):
                                processed_item[k] = None
                        else:
                            processed_item[k] = v

                    processed_data.append(processed_item)

                return {
                    "success": True,
                    "endpoint": "treasury_auctions",
                    "page_num": page_num,
                    "page_size": min(page_size, 100),
                    "total_records": len(processed_data),
                    "filters": {
                        "start_date": start_date,
                        "end_date": end_date,
                        "security_type": security_type
                    },
                    "data": processed_data,
                    "timestamp": int(datetime.now().timestamp())
                }

            except json.JSONDecodeError as e:
                return GovernmentUSError('treasury_auctions', f'Failed to parse JSON response: {str(e)}').to_dict()
            except Exception as e:
                return GovernmentUSError('treasury_auctions', f'Failed to process data: {str(e)}').to_dict()

        except Exception as e:
            return GovernmentUSError('treasury_auctions', str(e)).to_dict()

    # ===== COMPOSITE METHODS =====

    def get_comprehensive_treasury_data(self, target_date: Optional[str] = None,
                                       security_type: Optional[str] = None) -> Dict[str, Any]:
        """Get comprehensive treasury data from multiple endpoints"""
        result = {
            "success": True,
            "target_date": target_date or "latest",
            "security_type": security_type,
            "timestamp": int(datetime.now().timestamp()),
            "endpoints": {},
            "failed_endpoints": []
        }

        # Define endpoints to try
        endpoints = [
            ('treasury_prices', lambda: self.get_treasury_prices(target_date=target_date, security_type=security_type)),
            ('treasury_auctions', lambda: self.get_treasury_auctions(security_type=security_type, page_size=10))
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

    def get_treasury_summary(self, target_date: Optional[str] = None) -> Dict[str, Any]:
        """Get a summary of treasury data with key metrics"""
        try:
            prices_result = self.get_treasury_prices(target_date=target_date)

            if not prices_result.get("success"):
                return GovernmentUSError('treasury_summary', f'Failed to get prices: {prices_result.get("error")}').to_dict()

            data = prices_result.get("data", [])

            # Calculate summary statistics
            summary = {
                "success": True,
                "endpoint": "treasury_summary",
                "date": target_date or prices_result.get("date"),
                "total_securities": len(data),
                "security_types": {},
                "yield_summary": {},
                "price_summary": {},
                "timestamp": int(datetime.now().timestamp())
            }

            # Group by security type
            security_counts = {}
            all_rates = []
            all_prices = []

            for item in data:
                sec_type = item.get('security_type', 'Unknown')
                security_counts[sec_type] = security_counts.get(sec_type, 0) + 1

                rate = item.get('rate')
                if rate is not None:
                    all_rates.append(rate)

                price = item.get('eod_price')
                if price is not None:
                    all_prices.append(price)

            summary["security_types"] = security_counts

            # Calculate yield statistics
            if all_rates:
                summary["yield_summary"] = {
                    "min_rate": min(all_rates),
                    "max_rate": max(all_rates),
                    "avg_rate": sum(all_rates) / len(all_rates),
                    "total_with_rates": len(all_rates)
                }

            # Calculate price statistics
            if all_prices:
                summary["price_summary"] = {
                    "min_price": min(all_prices),
                    "max_price": max(all_prices),
                    "avg_price": sum(all_prices) / len(all_prices),
                    "total_with_prices": len(all_prices)
                }

            return summary

        except Exception as e:
            return GovernmentUSError('treasury_summary', str(e)).to_dict()

# ===== CLI INTERFACE =====

def main():
    if len(sys.argv) < 2:
        print(json.dumps({
            "success": False,
            "error": "Usage: python government_us_data.py <command> <args>",
            "available_commands": [
                "treasury_prices [date] [cusip] [security_type]",
                "treasury_auctions [start_date] [end_date] [security_type] [page_size] [page_num]",
                "comprehensive [date] [security_type]",
                "summary [date]"
            ]
        }))
        sys.exit(1)

    command = sys.argv[1]
    wrapper = GovernmentUSWrapper()

    try:
        if command == "treasury_prices":
            date_arg = sys.argv[2] if len(sys.argv) > 2 else None
            cusip_arg = sys.argv[3] if len(sys.argv) > 3 else None
            security_type_arg = sys.argv[4] if len(sys.argv) > 4 else None

            result = wrapper.get_treasury_prices(
                target_date=date_arg,
                cusip=cusip_arg,
                security_type=security_type_arg
            )
            print(json.dumps(result, indent=2))

        elif command == "treasury_auctions":
            start_date_arg = sys.argv[2] if len(sys.argv) > 2 else None
            end_date_arg = sys.argv[3] if len(sys.argv) > 3 else None
            security_type_arg = sys.argv[4] if len(sys.argv) > 4 else None
            page_size_arg = int(sys.argv[5]) if len(sys.argv) > 5 else 100
            page_num_arg = int(sys.argv[6]) if len(sys.argv) > 6 else 1

            result = wrapper.get_treasury_auctions(
                start_date=start_date_arg,
                end_date=end_date_arg,
                security_type=security_type_arg,
                page_size=page_size_arg,
                page_num=page_num_arg
            )
            print(json.dumps(result, indent=2))

        elif command == "comprehensive":
            date_arg = sys.argv[2] if len(sys.argv) > 2 else None
            security_type_arg = sys.argv[3] if len(sys.argv) > 3 else None

            result = wrapper.get_comprehensive_treasury_data(
                target_date=date_arg,
                security_type=security_type_arg
            )
            print(json.dumps(result, indent=2))

        elif command == "summary":
            date_arg = sys.argv[2] if len(sys.argv) > 2 else None

            result = wrapper.get_treasury_summary(target_date=date_arg)
            print(json.dumps(result, indent=2))

        else:
            print(json.dumps({
                "success": False,
                "error": f"Unknown command: {command}",
                "available_commands": [
                    "treasury_prices [date] [cusip] [security_type]",
                    "treasury_auctions [start_date] [end_date] [security_type] [page_size] [page_num]",
                    "comprehensive [date] [security_type]",
                    "summary [date]"
                ]
            }))
            sys.exit(1)

    except KeyboardInterrupt:
        print(json.dumps({"success": False, "error": "Operation cancelled by user"}))
        sys.exit(1)
    except Exception as e:
        print(json.dumps({"success": False, "error": f"Unexpected error: {str(e)}"}))
        sys.exit(1)

if __name__ == "__main__":
    main()