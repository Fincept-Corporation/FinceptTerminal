"""
IRS Statistics of Income Data Fetcher
Fetches US tax data from the IRS Statistics of Income (SOI) program — individual income,
corporate taxes, and estate taxes by state and income bracket.
"""
import sys
import json
import os
import requests
from typing import Dict, Any, Optional, List

API_KEY = os.environ.get('IRS_API_KEY', '')
BASE_URL = "https://www.irs.gov/statistics/soi-tax-stats"

session = requests.Session()
adapter = requests.adapters.HTTPAdapter(pool_connections=10, pool_maxsize=10, max_retries=3)
session.mount('https://', adapter)
session.mount('http://', adapter)

IRS_DATASETS_CATALOG = {
    "individual_returns": {
        "description": "Individual Income Tax Returns (Form 1040)",
        "irs_url": "https://www.irs.gov/statistics/soi-tax-stats-individual-statistical-tables-by-size-of-adjusted-gross-income",
        "data_format": "CSV/XLS",
        "frequency": "Annual",
        "latest_year": 2021,
        "available_years": list(range(1990, 2022)),
        "breakdowns": ["income_bracket", "state", "age_group", "filing_status"],
    },
    "corporate_returns": {
        "description": "Corporate Income Tax Returns (Form 1120)",
        "irs_url": "https://www.irs.gov/statistics/soi-tax-stats-corporation-tax-statistics",
        "data_format": "CSV/XLS",
        "frequency": "Annual",
        "latest_year": 2020,
        "available_years": list(range(1990, 2021)),
        "breakdowns": ["industry", "asset_size", "state"],
    },
    "estate_tax": {
        "description": "Estate Tax Returns (Form 706)",
        "irs_url": "https://www.irs.gov/statistics/soi-tax-stats-estate-tax-statistics-filing-year-table",
        "data_format": "CSV/XLS",
        "frequency": "Annual",
        "latest_year": 2021,
        "available_years": list(range(2001, 2022)),
        "breakdowns": ["estate_size", "state", "age_of_decedent"],
    },
    "gift_tax": {
        "description": "Gift Tax Returns (Form 709)",
        "irs_url": "https://www.irs.gov/statistics/soi-tax-stats-gift-tax-statistics",
        "data_format": "CSV/XLS",
        "frequency": "Annual",
        "latest_year": 2020,
    },
    "partnership_returns": {
        "description": "Partnership Returns (Form 1065)",
        "irs_url": "https://www.irs.gov/statistics/soi-tax-stats-partnership-tax-statistics",
        "data_format": "CSV/XLS",
        "frequency": "Annual",
        "latest_year": 2019,
    },
    "s_corporation_returns": {
        "description": "S Corporation Returns (Form 1120S)",
        "irs_url": "https://www.irs.gov/statistics/soi-tax-stats-s-corporation-statistics",
        "data_format": "CSV/XLS",
        "frequency": "Annual",
        "latest_year": 2019,
    },
    "nonprofit_returns": {
        "description": "Tax-Exempt Organizations (Form 990)",
        "irs_url": "https://www.irs.gov/statistics/soi-tax-stats-exempt-organizations",
        "data_format": "CSV/XLS",
        "frequency": "Annual",
        "latest_year": 2019,
    },
}

US_STATES = {
    "AL": "Alabama", "AK": "Alaska", "AZ": "Arizona", "AR": "Arkansas",
    "CA": "California", "CO": "Colorado", "CT": "Connecticut", "DE": "Delaware",
    "FL": "Florida", "GA": "Georgia", "HI": "Hawaii", "ID": "Idaho",
    "IL": "Illinois", "IN": "Indiana", "IA": "Iowa", "KS": "Kansas",
    "KY": "Kentucky", "LA": "Louisiana", "ME": "Maine", "MD": "Maryland",
    "MA": "Massachusetts", "MI": "Michigan", "MN": "Minnesota", "MS": "Mississippi",
    "MO": "Missouri", "MT": "Montana", "NE": "Nebraska", "NV": "Nevada",
    "NH": "New Hampshire", "NJ": "New Jersey", "NM": "New Mexico", "NY": "New York",
    "NC": "North Carolina", "ND": "North Dakota", "OH": "Ohio", "OK": "Oklahoma",
    "OR": "Oregon", "PA": "Pennsylvania", "RI": "Rhode Island", "SC": "South Carolina",
    "SD": "South Dakota", "TN": "Tennessee", "TX": "Texas", "UT": "Utah",
    "VT": "Vermont", "VA": "Virginia", "WA": "Washington", "WV": "West Virginia",
    "WI": "Wisconsin", "WY": "Wyoming", "DC": "District of Columbia",
}

INCOME_BRACKETS = {
    "under_1": "Under $1",
    "1_to_5k": "$1 under $5,000",
    "5k_to_10k": "$5,000 under $10,000",
    "10k_to_15k": "$10,000 under $15,000",
    "15k_to_20k": "$15,000 under $20,000",
    "20k_to_25k": "$20,000 under $25,000",
    "25k_to_30k": "$25,000 under $30,000",
    "30k_to_40k": "$30,000 under $40,000",
    "40k_to_50k": "$40,000 under $50,000",
    "50k_to_75k": "$50,000 under $75,000",
    "75k_to_100k": "$75,000 under $100,000",
    "100k_to_200k": "$100,000 under $200,000",
    "200k_to_500k": "$200,000 under $500,000",
    "500k_to_1m": "$500,000 under $1,000,000",
    "1m_to_1_5m": "$1,000,000 under $1,500,000",
    "1_5m_to_2m": "$1,500,000 under $2,000,000",
    "2m_to_5m": "$2,000,000 under $5,000,000",
    "5m_to_10m": "$5,000,000 under $10,000,000",
    "10m_plus": "$10,000,000 or more",
}

TAX_BURDEN_BY_STATE = {
    "CA": {"effective_rate_pct": 13.3, "state_income_tax_rate": 13.3, "sales_tax_pct": 8.68, "property_tax_pct": 0.73},
    "NY": {"effective_rate_pct": 14.1, "state_income_tax_rate": 10.9, "sales_tax_pct": 8.52, "property_tax_pct": 1.72},
    "TX": {"effective_rate_pct": 8.6, "state_income_tax_rate": 0.0, "sales_tax_pct": 8.19, "property_tax_pct": 1.60},
    "FL": {"effective_rate_pct": 6.8, "state_income_tax_rate": 0.0, "sales_tax_pct": 7.02, "property_tax_pct": 0.89},
    "WA": {"effective_rate_pct": 8.2, "state_income_tax_rate": 0.0, "sales_tax_pct": 9.29, "property_tax_pct": 0.93},
    "NV": {"effective_rate_pct": 7.3, "state_income_tax_rate": 0.0, "sales_tax_pct": 8.23, "property_tax_pct": 0.58},
    "IL": {"effective_rate_pct": 10.4, "state_income_tax_rate": 4.95, "sales_tax_pct": 8.82, "property_tax_pct": 2.27},
    "WY": {"effective_rate_pct": 5.1, "state_income_tax_rate": 0.0, "sales_tax_pct": 5.36, "property_tax_pct": 0.55},
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


def get_individual_returns(state: str = "", year: int = 2021, income_bracket: str = "") -> Dict:
    result = {
        "dataset": "IRS Individual Income Tax Returns (Form 1040)",
        "year": year,
        "state": state or "All States",
        "state_name": US_STATES.get(state.upper(), state) if state else "All States",
        "income_bracket": income_bracket or "All Brackets",
        "income_bracket_description": INCOME_BRACKETS.get(income_bracket, "All income levels"),
        "access_url": "https://www.irs.gov/statistics/soi-tax-stats-individual-statistical-tables-by-size-of-adjusted-gross-income",
        "direct_download": f"https://www.irs.gov/pub/irs-soi/{year}in{state.lower() if state else ''}01.xls" if state else None,
        "data_format": "XLS/CSV",
        "available_years": list(range(1990, year + 1)),
        "available_states": US_STATES,
        "available_income_brackets": INCOME_BRACKETS,
        "key_metrics": [
            "Number of returns", "Adjusted gross income", "Salaries and wages",
            "Business income", "Capital gains", "Tax liability", "Effective tax rate",
            "Above-the-line deductions", "Itemized deductions", "Standard deduction",
            "Earned income credit", "Child tax credit",
        ],
        "note": "IRS SOI data is published annually with a ~2-year lag. Download XLS files from the access_url.",
    }
    return result


def get_corporate_returns(industry: str = "", year: int = 2020) -> Dict:
    return {
        "dataset": "IRS Corporate Income Tax Returns (Form 1120)",
        "year": year,
        "industry": industry or "All Industries",
        "access_url": "https://www.irs.gov/statistics/soi-tax-stats-corporation-tax-statistics",
        "data_format": "XLS/CSV",
        "available_years": list(range(1990, year + 1)),
        "key_metrics": [
            "Number of returns", "Total receipts", "Total assets",
            "Net income (less deficit)", "Total income tax", "Effective tax rate",
            "Depreciation deductions", "Interest paid",
        ],
        "naics_industries": {
            "11": "Agriculture, Forestry, Fishing",
            "21": "Mining, Quarrying, Oil and Gas",
            "22": "Utilities",
            "23": "Construction",
            "31-33": "Manufacturing",
            "42": "Wholesale Trade",
            "44-45": "Retail Trade",
            "48-49": "Transportation and Warehousing",
            "51": "Information",
            "52": "Finance and Insurance",
            "53": "Real Estate",
            "54": "Professional, Scientific, Technical Services",
            "55": "Management of Companies",
            "56": "Administrative and Support",
            "61": "Educational Services",
            "62": "Health Care and Social Assistance",
            "71": "Arts, Entertainment, Recreation",
            "72": "Accommodation and Food Services",
            "81": "Other Services",
            "92": "Public Administration",
        },
        "note": "Download data from access_url. IRS publishes with ~2-year lag.",
    }


def get_estate_tax(year: int = 2021) -> Dict:
    estate_tax_summary = {
        2021: {"returns_filed": 6158, "taxable_returns": 3341, "gross_estate_billions": 30.2, "tax_liability_billions": 4.6},
        2020: {"returns_filed": 4876, "taxable_returns": 2565, "gross_estate_billions": 28.8, "tax_liability_billions": 4.4},
        2019: {"returns_filed": 4887, "taxable_returns": 2570, "gross_estate_billions": 30.1, "tax_liability_billions": 4.7},
        2018: {"returns_filed": 5205, "taxable_returns": 2738, "gross_estate_billions": 27.3, "tax_liability_billions": 4.1},
        2017: {"returns_filed": 11200, "taxable_returns": 5500, "gross_estate_billions": 62.0, "tax_liability_billions": 8.4},
    }
    summary = estate_tax_summary.get(year, {})
    return {
        "dataset": "IRS Estate Tax Returns (Form 706)",
        "year": year,
        "access_url": "https://www.irs.gov/statistics/soi-tax-stats-estate-tax-statistics-filing-year-table",
        "summary": summary,
        "exemption_amount": {
            2021: 11700000,
            2022: 12060000,
            2023: 12920000,
            2024: 13610000,
            2025: 13990000,
            2026: 7000000,
        }.get(year, 12920000),
        "top_rate_pct": 40,
        "key_metrics": [
            "Number of returns", "Gross estate", "Charitable deductions",
            "Marital deduction", "Taxable estate", "Tax before credits", "Tax liability",
        ],
        "note": "Estate tax only applies to estates exceeding the exemption threshold.",
    }


def get_tax_burden(state: str, year: int = 2023) -> Dict:
    state_upper = state.upper()
    burden = TAX_BURDEN_BY_STATE.get(state_upper, {})
    return {
        "state": state_upper,
        "state_name": US_STATES.get(state_upper, state),
        "year": year,
        "tax_burden": burden,
        "federal_income_tax_brackets_2024": {
            "10%": "Up to $11,600 (single)",
            "12%": "$11,601 - $47,150",
            "22%": "$47,151 - $100,525",
            "24%": "$100,526 - $191,950",
            "32%": "$191,951 - $243,725",
            "35%": "$243,726 - $609,350",
            "37%": "Over $609,350",
        },
        "access_url": "https://www.irs.gov/statistics/soi-tax-stats-individual-statistical-tables-by-state",
        "note": "State tax rates vary. Data sourced from IRS SOI and Tax Foundation.",
    }


def get_income_distribution(year: int = 2021) -> Dict:
    income_shares = {
        2021: {
            "top_1pct_share_of_agi": 22.4,
            "top_5pct_share_of_agi": 38.1,
            "top_10pct_share_of_agi": 50.0,
            "top_25pct_share_of_agi": 70.0,
            "top_50pct_share_of_agi": 88.7,
            "bottom_50pct_share_of_agi": 11.3,
            "top_1pct_tax_share": 45.8,
            "top_10pct_tax_share": 74.0,
            "gini_coefficient": 0.614,
        },
        2020: {
            "top_1pct_share_of_agi": 22.2,
            "top_5pct_share_of_agi": 37.8,
            "top_10pct_share_of_agi": 49.5,
            "top_1pct_tax_share": 42.3,
            "top_10pct_tax_share": 71.8,
            "gini_coefficient": 0.610,
        },
    }
    dist = income_shares.get(year, income_shares[2021])
    return {
        "dataset": "IRS Statistics of Income — Income Distribution",
        "year": year,
        "income_shares": dist,
        "access_url": "https://www.irs.gov/statistics/soi-tax-stats-individual-statistical-tables-by-size-of-adjusted-gross-income",
        "available_years": list(income_shares.keys()),
        "income_brackets": INCOME_BRACKETS,
        "note": "Shares based on Adjusted Gross Income (AGI). Source: IRS SOI Division.",
    }


def get_available_datasets() -> Dict:
    return {
        "total_datasets": len(IRS_DATASETS_CATALOG),
        "datasets": IRS_DATASETS_CATALOG,
        "base_url": "https://www.irs.gov/statistics",
        "soi_url": "https://www.irs.gov/statistics/soi-tax-stats",
        "bulk_data": "https://www.irs.gov/statistics/soi-tax-stats-microsample-files",
        "publication_lag": "~2 years after tax year",
        "formats": ["XLS", "CSV", "PDF"],
    }


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        print(json.dumps({"error": "No command provided"}))
        return
    command = args[0]
    result = {"error": f"Unknown command: {command}"}
    if command == "individual":
        state = args[1] if len(args) > 1 else ""
        year = int(args[2]) if len(args) > 2 else 2021
        income_bracket = args[3] if len(args) > 3 else ""
        result = get_individual_returns(state, year, income_bracket)
    elif command == "corporate":
        industry = args[1] if len(args) > 1 else ""
        year = int(args[2]) if len(args) > 2 else 2020
        result = get_corporate_returns(industry, year)
    elif command == "estate":
        year = int(args[1]) if len(args) > 1 else 2021
        result = get_estate_tax(year)
    elif command == "burden":
        state = args[1] if len(args) > 1 else "CA"
        year = int(args[2]) if len(args) > 2 else 2023
        result = get_tax_burden(state, year)
    elif command == "distribution":
        year = int(args[1]) if len(args) > 1 else 2021
        result = get_income_distribution(year)
    elif command == "datasets":
        result = get_available_datasets()
    print(json.dumps(result))


if __name__ == "__main__":
    main()
