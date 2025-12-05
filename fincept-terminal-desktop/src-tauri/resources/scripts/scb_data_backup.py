"""
SCB (Statistics Sweden) Data Wrapper
Comprehensive wrapper for SCB PxWebApi providing access to Swedish statistical data

API Documentation:
- Base URL: https://api.scb.se/OV0104/v1/doris/sv/ssd/
- Authentication: None required (public API)
- Rate limits: Be reasonable with requests
- Documentation: https://www.scb.se/vara-tjanster/oppna-data/pxwebapi/api-for-statistikdatabasen/

Supported Categories:
- Population (BE): Population statistics, demographics, projections
- Labor Market (AM): Employment, wages, labor surveys
- Economy (NR): National accounts, regional accounts, health accounts
- Environment (MI): Emissions, waste, land use, environmental indicators
- Energy (EN): Energy production, consumption, prices
- Education (UF): Students, schools, research, training
- Prices (PR): Consumer price index, producer price index
- Healthcare (HS): Health statistics, causes of death
- And many more categories covering all aspects of Swedish society
"""

import sys
import json
import requests
import os
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Union, Any
from urllib.parse import urlencode

# SCB API Configuration
BASE_URL = "https://api.scb.se/OV0104/v1/doris/sv/ssd/"
TIMEOUT = 30
RATE_LIMIT_DELAY = 0.1  # 100ms delay between requests

# Main SCB Categories
SCB_CATEGORIES = {
    "AA": {"name": "Ämnesövergripande statistik", "description": "Cross-cutting statistics"},
    "AM": {"name": "Arbetsmarknad", "description": "Labor market statistics"},
    "BE": {"name": "Befolkning", "description": "Population statistics"},
    "BO": {"name": "Boende, byggande och bebyggelse", "description": "Housing and construction"},
    "EN": {"name": "Energi", "description": "Energy statistics"},
    "FM": {"name": "Finansmarknad", "description": "Financial market statistics"},
    "HA": {"name": "Handel med varor och tjänster", "description": "Trade in goods and services"},
    "HE": {"name": "Hushållens ekonomi", "description": "Household economics"},
    "HS": {"name": "Hälso- och sjukvård", "description": "Health and medical care"},
    "JO": {"name": "Jord- och skogsbruk, fiske", "description": "Agriculture, forestry, fishing"},
    "KU": {"name": "Kultur och fritid", "description": "Culture and leisure"},
    "LE": {"name": "Levnadsförhållanden", "description": "Living conditions"},
    "ME": {"name": "Demokrati", "description": "Democracy statistics"},
    "MI": {"name": "Miljö", "description": "Environment statistics"},
    "NR": {"name": "Nationalräkenskaper", "description": "National accounts"},
    "NV": {"name": "Näringsverksamhet", "description": "Business activities"},
    "OE": {"name": "Offentlig ekonomi", "description": "Public economy"},
    "OV": {"name": "Övrigt", "description": "Other statistics"},
    "PR": {"name": "Priser och konsumtion", "description": "Prices and consumption"},
    "SO": {"name": "Socialtjänst", "description": "Social services"},
    "TK": {"name": "Transporter och kommunikationer", "description": "Transport and communications"},
    "UF": {"name": "Utbildning och forskning", "description": "Education and research"}
}

# Popular SCB Tables for quick access
POPULAR_TABLES = {
    "population": {
        "id": "BE/BE0101/BE0101A/BefolkningNy",
        "name": "Population by year",
        "description": "Swedish population statistics"
    },
    "gdp": {
        "id": "NR/NR0103/NR0103A/BNPBrott",
        "name": "GDP by quarter",
        "description": "Swedish Gross Domestic Product"
    },
    "employment": {
        "id": "AM/AM0201/AntalSysselsatta",
        "name": "Employment by sector",
        "description": "Number of employed by sector"
    },
    "unemployment": {
        "id": "AM/AM0401/AntalArbetslosa",
        "name": "Unemployment statistics",
        "description": "Unemployment by region and age"
    },
    "cpi": {
        "id": "PR/PR0101/KPIFaststallelse",
        "name": "Consumer Price Index",
        "description": "Swedish Consumer Price Index"
    },
    "births": {
        "id": "BE/BE0101/BE0101G/Fodda",
        "name": "Live births",
        "description": "Number of live births by month"
    },
    "deaths": {
        "id": "BE/BE0101/BE0101G/Doda",
        "name": "Deaths",
        "description": "Number of deaths by month"
    },
    "immigration": {
        "id": "BE/BE0101/BE0101G/Inflyttningar",
        "name": "Immigration",
        "description": "Number of immigrants by month"
    }
}


class SCBError:
    """Custom error class for SCB API errors"""
    def __init__(self, endpoint: str, error: str, status_code: Optional[int] = None):
        self.endpoint = endpoint
        self.error = error
        self.status_code = status_code
        self.timestamp = int(datetime.now().timestamp()

    def to_dict(self) -> Dict[str, Any]:
        return {
            "error": True,
            "endpoint": self.endpoint,
            "message": self.error,
            "status_code": self.status_code,
            "timestamp": self.timestamp,
            "source": "Statistics Sweden (SCB) API"
        }


class SCBDataAPI:
    """SCB Data API wrapper for Swedish statistical data"""

    def __init__(self):
        self.base_url = BASE_URL
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': 'Fincept-Terminal/1.0',
            'Accept': 'application/json'
        })

    def _make_request(self, url: str, method: str = "GET",
                     data: Optional[Dict] = None) -> Dict[str, Any]:
        """Make HTTP request with error handling"""
        try:
            if method.upper() == "GET":
                response = self.session.get(url, timeout=TIMEOUT)
            elif method.upper() == "POST":
                response = self.session.post(url, json=data, timeout=TIMEOUT)
            else:
                return SCBError(url, f"Unsupported method: {method}").to_dict()

            if response.status_code == 200:
                try:
                    result = response.json()
                    return {"success": True, "data": result}
                except json.JSONDecodeError:
                    return SCBError(url, "Invalid JSON response", response.status_code).to_dict()
            else:
                return SCBError(url, f"HTTP {response.status_code}: {response.text}", response.status_code).to_dict()

        except requests.exceptions.RequestException as e:
            return SCBError(url, f"Network error: {str(e)}", getattr(e.response, 'status_code', None).to_dict()
        except Exception as e:
            return SCBError(url, f"Unexpected error: {str(e)}").to_dict()

    def get_categories(self) -> Dict[str, Any]:
        """Get all main SCB categories"""
        try:
            result = self._make_request(self.base_url)

            if not result.get("success"):
                return result

            categories_data = result.get("data", [])
            enhanced_categories = []

            for category in categories_data:
                category_id = category.get("id")
                if category_id in SCB_CATEGORIES:
                    category["description"] = SCB_CATEGORIES[category_id]["description"]
                    category["english_name"] = SCB_CATEGORIES[category_id]["name"]
                enhanced_categories.append(category)

            return {
                "success": True,
                "data": enhanced_categories,
                "total_categories": len(enhanced_categories),
                "metadata": {
                    "source": "Statistics Sweden (SCB) API",
                    "endpoint": "main_categories",
                    "last_updated": datetime.now().isoformat()
                }
            }

        except Exception as e:
            return SCBError("get_categories", str(e).to_dict()

    def get_subcategories(self, category_id: str) -> Dict[str, Any]:
        """Get subcategories for a specific category"""
        try:
            if category_id not in SCB_CATEGORIES:
                available_categories = ", ".join(SCB_CATEGORIES.keys()
                return SCBError("get_subcategories", f"Invalid category '{category_id}'. Available: {available_categories}").to_dict()

            url = f"{self.base_url}{category_id}"
            result = self._make_request(url)

            if not result.get("success"):
                return result

            subcategories = result.get("data", [])

            return {
                "success": True,
                "data": {
                    "category_id": category_id,
                    "category_name": SCB_CATEGORIES[category_id]["name"],
                    "subcategories": subcategories,
                    "total_subcategories": len(subcategories)
                },
                "metadata": {
                    "source": "Statistics Sweden (SCB) API",
                    "category": category_id,
                    "last_updated": datetime.now().isoformat()
                }
            }

        except Exception as e:
            return SCBError("get_subcategories", str(e).to_dict()

    def get_table_metadata(self, table_path: str) -> Dict[str, Any]:
        """Get metadata for a specific table"""
        try:
            url = f"{self.base_url}{table_path}"
            result = self._make_request(url)

            if not result.get("success"):
                return result

            return {
                "success": True,
                "data": result.get("data", {}),
                "metadata": {
                    "table_path": table_path,
                    "source": "Statistics Sweden (SCB) API",
                    "last_updated": datetime.now().isoformat()
                }
            }

        except Exception as e:
            return SCBError("get_table_metadata", str(e).to_dict()

    def get_table_data(self, table_path: str, query: Dict[str, Any]) -> Dict[str, Any]:
        """Get data from a specific table with query parameters"""
        try:
            url = f"{self.base_url}{table_path}"

            # Format query according to SCB API specification
            scb_query = {
                "query": query,
                "response": {
                    "format": "json"
                }
            }

            result = self._make_request(url, "POST", scb_query)

            if not result.get("success"):
                return result

            scb_data = result.get("data", {})

            # Process the response according to SCB API format
            processed_data = self._process_scb_response(scb_data, table_path, query)

            return {
                "success": True,
                "data": processed_data,
                "metadata": {
                    "table_path": table_path,
                    "query": query,
                    "source": "Statistics Sweden (SCB) API",
                    "last_updated": datetime.now().isoformat()
                }
            }

        except Exception as e:
            return SCBError("get_table_data", str(e).to_dict()

    def _process_scb_response(self, scb_data: Dict[str, Any], table_path: str, query: Dict[str, Any]) -> Dict[str, Any]:
        """Process SCB API response into standardized format"""
        try:
            if "data" not in scb_data:
                return {"columns": [], "data": [], "error": "No data field in SCB response"}

            columns = scb_data.get("columns", [])
            data = scb_data.get("data", [])

            # Convert SCB column format to standard format
            processed_columns = []
            for col in columns:
                processed_columns.append({
                    "code": col.get("code", ""),
                    "text": col.get("text", ""),
                    "type": col.get("type", "")
                })

            # Process data rows
            processed_data = []
            for row in data:
                row_dict = {}
                for i, value in enumerate(row.get("values", []):
                    if i < len(processed_columns):
                        column_name = processed_columns[i]["code"]
                        row_dict[column_name] = value
                processed_data.append(row_dict)

            return {
                "columns": processed_columns,
                "data": processed_data,
                "total_records": len(processed_data),
                "table_path": table_path,
                "query_used": query
            }

        except Exception as e:
            return {"error": f"Error processing SCB response: {str(e)}"}

    def get_population_data(self, years: Optional[List[str]] = None) -> Dict[str, Any]:
        """Get Swedish population data"""
        try:
            if not years:
                # Default to last 5 years
                current_year = datetime.now().year
                years = [str(year) for year in range(current_year - 4, current_year + 1)]

            query = [
                {
                    "code": "ContentsCode",
                    "selection": {
                        "filter": "item",
                        "values": ["BE0101N1"]  # Population
                    }
                },
                {
                    "code": "Tid",
                    "selection": {
                        "filter": "item",
                        "values": years
                    }
                }
            ]

            result = self.get_table_data("BE/BE0101/BE0101A/BefolkningNy", query)

            if result.get("success"):
                result["data"]["description"] = "Swedish population statistics by year"
                result["data"]["years"] = years

            return result

        except Exception as e:
            return SCBError("get_population_data", str(e).to_dict()

    def get_gdp_data(self, quarters: Optional[List[str]] = None) -> Dict[str, Any]:
        """Get Swedish GDP data"""
        try:
            if not quarters:
                # Default to last 8 quarters
                current_year = datetime.now().year
                quarters = []
                for year in range(current_year - 2, current_year + 1):
                    for q in ["K1", "K2", "K3", "K4"]:
                        quarters.append(f"{year}{q}")

            query = [
                {
                    "code": "ContentsCode",
                    "selection": {
                        "filter": "item",
                        "values": ["BNPGDP"]  # GDP
                    }
                },
                {
                    "code": "Tid",
                    "selection": {
                        "filter": "item",
                        "values": quarters
                    }
                }
            ]

            result = self.get_table_data("NR/NR0103/NR0103A/BNPBrott", query)

            if result.get("success"):
                result["data"]["description"] = "Swedish GDP by quarter"
                result["data"]["quarters"] = quarters

            return result

        except Exception as e:
            return SCBError("get_gdp_data", str(e).to_dict()

    def get_employment_data(self, regions: Optional[List[str]] = None, years: Optional[List[str]] = None) -> Dict[str, Any]:
        """Get Swedish employment data"""
        try:
            if not years:
                current_year = datetime.now().year
                years = [str(year) for year in range(current_year - 4, current_year + 1)]

            if not regions:
                regions = ["00"]  # Whole country

            query = [
                {
                    "code": "Region",
                    "selection": {
                        "filter": "item",
                        "values": regions
                    }
                },
                {
                    "code": "Tid",
                    "selection": {
                        "filter": "item",
                        "values": years
                    }
                }
            ]

            result = self.get_table_data("AM/AM0201/AntalSysselsatta", query)

            if result.get("success"):
                result["data"]["description"] = "Swedish employment by region and year"
                result["data"]["regions"] = regions
                result["data"]["years"] = years

            return result

        except Exception as e:
            return SCBError("get_employment_data", str(e).to_dict()

    def get_cpi_data(self, months: Optional[List[str]] = None) -> Dict[str, Any]:
        """Get Swedish Consumer Price Index data"""
        try:
            if not months:
                # Default to last 24 months
                current_date = datetime.now()
                months = []
                for i in range(24):
                    month_date = current_date - timedelta(days=30 * i)
                    months.append(month_date.strftime("%Y%m")

            query = [
                {
                    "code": "ContentsCode",
                    "selection": {
                        "filter": "item",
                        "values": ["KPI"]  # CPI
                    }
                },
                {
                    "code": "Tid",
                    "selection": {
                        "filter": "item",
                        "values": months
                    }
                }
            ]

            result = self.get_table_data("PR/PR0101/KPIFaststallelse", query)

            if result.get("success"):
                result["data"]["description"] = "Swedish Consumer Price Index"
                result["data"]["months"] = months

            return result

        except Exception as e:
            return SCBError("get_cpi_data", str(e).to_dict()

    def get_demographic_data(self, start_year: Optional[str] = None, end_year: Optional[str] = None) -> Dict[str, Any]:
        """Get comprehensive demographic overview"""
        try:
            current_year = datetime.now().year
            if not start_year:
                start_year = str(current_year - 10)
            if not end_year:
                end_year = str(current_year)

            years = [str(year) for year in range(int(start_year), int(end_year) + 1)]

            # Get multiple demographic datasets sequentially
            results = [
                self.get_population_data(years),
                self.get_cpi_data(None),  # Recent CPI data
                self.get_employment_data(["00"], years)  # Whole country employment
            ]

            demographic_data = {}
            failed_requests = []

            dataset_names = ["population", "cpi", "employment"]
            for i, result in enumerate(results):
                if isinstance(result, Exception):
                    failed_requests.append({
                        "dataset": dataset_names[i],
                        "error": str(result)
                    })
                elif isinstance(result, dict):
                    if result.get("success"):
                        demographic_data[dataset_names[i]] = result.get("data", {})
                    else:
                        failed_requests.append({
                            "dataset": dataset_names[i],
                            "error": result.get("error", "Unknown error")
                        })

            return {
                "success": len(demographic_data) > 0,
                "data": {
                    "demographic_overview": demographic_data,
                    "year_range": f"{start_year}-{end_year}",
                    "summary": {
                        "successful_datasets": list(demographic_data.keys(),
                        "failed_datasets": failed_requests,
                        "total_data_points": sum(
                            data.get("total_records", 0)
                            for data in demographic_data.values()
                        )
                    }
                },
                "metadata": {
                    "source": "Statistics Sweden (SCB) API",
                    "overview_type": "demographic_overview",
                    "year_range": f"{start_year}-{end_year}",
                    "last_updated": datetime.now().isoformat()
                }
            }

        except Exception as e:
            return SCBError("get_demographic_data", str(e).to_dict()

    def search_tables(self, query: str, category: Optional[str] = None) -> Dict[str, Any]:
        """Search for tables matching query"""
        try:
            # This is a simplified search - in a real implementation, you might
            # want to search through available tables and metadata

            search_results = []

            # Check popular tables first
            for table_key, table_info in POPULAR_TABLES.items():
                if query.lower() in table_info["name"].lower() or \
                   query.lower() in table_info["description"].lower():
                    search_results.append({
                        "id": table_info["id"],
                        "name": table_info["name"],
                        "description": table_info["description"],
                        "type": "popular_table",
                        "category": "misc"
                    })

            return {
                "success": True,
                "data": {
                    "query": query,
                    "results": search_results,
                    "total_results": len(search_results)
                },
                "metadata": {
                    "source": "Statistics Sweden (SCB) API",
                    "search_type": "table_search",
                    "last_updated": datetime.now().isoformat()
                }
            }

        except Exception as e:
            return SCBError("search_tables", str(e).to_dict()

    def get_popular_tables(self) -> Dict[str, Any]:
        """Get list of popular SCB tables"""
        try:
            popular_list = []
            for key, info in POPULAR_TABLES.items():
                popular_list.append({
                    "key": key,
                    "id": info["id"],
                    "name": info["name"],
                    "description": info["description"]
                })

            return {
                "success": True,
                "data": {
                    "popular_tables": popular_list,
                    "total_tables": len(popular_list)
                },
                "metadata": {
                    "source": "Statistics Sweden (SCB) API",
                    "last_updated": datetime.now().isoformat()
                }
            }

        except Exception as e:
            return SCBError("get_popular_tables", str(e).to_dict()

    def get_category_info(self) -> Dict[str, Any]:
        """Get detailed information about all SCB categories"""
        try:
            categories_info = []
            for code, info in SCB_CATEGORIES.items():
                categories_info.append({
                    "code": code,
                    "name": info["name"],
                    "description": info["description"],
                    "swedish_name": info["name"],
                    "english_description": info["description"]
                })

            return {
                "success": True,
                "data": {
                    "categories": categories_info,
                    "total_categories": len(categories_info)
                },
                "metadata": {
                    "source": "Statistics Sweden (SCB) API",
                    "last_updated": datetime.now().isoformat()
                }
            }

        except Exception as e:
            return SCBError("get_category_info", str(e).to_dict()


def main():
    """Main function for CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps(SCBError("cli", "Usage: python scb_data.py <command> [args...]").to_dict())
        sys.exit(1)

    command = sys.argv[1]

    # Create API instance
    api = SCBDataAPI()

    # Map commands to functions
    if command == "get_categories":
        result = api.get_categories()

    elif command == "get_subcategories":
        if len(sys.argv) < 3:
            print(json.dumps(SCBError("cli", "Usage: python scb_data.py get_subcategories <category_id>").to_dict())
            sys.exit(1)
        category_id = sys.argv[2]
        result = api.get_subcategories(category_id)

    elif command == "get_table_metadata":
        if len(sys.argv) < 3:
            print(json.dumps(SCBError("cli", "Usage: python scb_data.py get_table_metadata <table_path>").to_dict())
            sys.exit(1)
        table_path = sys.argv[2]
        result = api.get_table_metadata(table_path)

    elif command == "get_population":
        years = sys.argv[2].split(",") if len(sys.argv) > 2 else None
        result = api.get_population_data(years)

    elif command == "get_gdp":
        quarters = sys.argv[2].split(",") if len(sys.argv) > 2 else None
        result = api.get_gdp_data(quarters)

    elif command == "get_employment":
        regions = sys.argv[2].split(",") if len(sys.argv) > 2 else None
        years = sys.argv[3].split(",") if len(sys.argv) > 3 else None
        result = api.get_employment_data(regions, years)

    elif command == "get_cpi":
        months = sys.argv[2].split(",") if len(sys.argv) > 2 else None
        result = api.get_cpi_data(months)

    elif command == "get_demographic_overview":
        start_year = sys.argv[2] if len(sys.argv) > 2 else None
        end_year = sys.argv[3] if len(sys.argv) > 3 else None
        result = api.get_demographic_data(start_year, end_year)

    elif command == "search_tables":
        query = sys.argv[2] if len(sys.argv) > 2 else ""
        category = sys.argv[3] if len(sys.argv) > 3 else None
        result = api.search_tables(query, category)

    elif command == "get_popular_tables":
        result = api.get_popular_tables()

    elif command == "get_category_info":
        result = api.get_category_info()

    else:
        available_commands = [
            "get_categories",
            "get_subcategories <category_id>",
            "get_table_metadata <table_path>",
            "get_population [years]",
            "get_gdp [quarters]",
            "get_employment [regions] [years]",
            "get_cpi [months]",
            "get_demographic_overview [start_year] [end_year]",
            "search_tables <query> [category]",
            "get_popular_tables",
            "get_category_info"
        ]
        print(json.dumps({
            "error": f"Unknown command: {command}",
            "available_commands": available_commands
        })
        sys.exit(1)

    print(json.dumps(result, indent=2, default=str)


if __name__ == "__main__":
    main(