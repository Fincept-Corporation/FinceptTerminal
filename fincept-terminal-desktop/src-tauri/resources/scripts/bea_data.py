"""
BEA (Bureau of Economic Analysis) Data Fetcher
Comprehensive wrapper for BEA Data Retrieval API providing access to
National, Regional, Industry and International economic data

API Documentation:
- Base URL: https://apps.bea.gov/api/data/
- Authentication: API key required
- Rate limits: None specified but be reasonable with requests
- Registration: https://www.bea.gov/data/api/register

Supported Datasets:
- NIPA: National Income and Product Accounts
- NIUnderlyingDetail: NIPA Underlying Detail
- FixedAssets: Fixed Assets
- MNE: Multinational Enterprises
- GDPbyIndustry: GDP by Industry
- ITA: International Transactions
- IIP: International Investment Position
- InputOutput: Input-Output Accounts
- UnderlyingGDPbyIndustry: GDP by Industry - Underlying Detail
- IntlServTrade: International Services Trade
- Regional: Regional Economic Accounts
"""

import sys
import json
import os
import requests
from datetime import datetime, timedelta
from typing import Dict, Any, Optional, List, Union
from urllib.parse import urlencode


class BEAError:
    """Error handling wrapper for BEA API responses"""
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


class BEAWrapper:
    """Comprehensive BEA API wrapper with fault tolerance"""

    def __init__(self, api_key: Optional[str] = None):
        self.api_key = api_key or os.environ.get('BEA_API_KEY', '')
        self.base_url = "https://apps.bea.gov/api/data/"
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': 'Fincept-Terminal/1.0'
        })

    def _make_request(self, method: str, params: Dict[str, Any]) -> Dict[str, Any]:
        """Centralized request handler with comprehensive error handling"""
        try:
            # Add API key to all requests
            params['UserID'] = self.api_key
            params['method'] = method
            params['resultformat'] = 'JSON'

            # Add Year parameter if not specified (default to most recent)
            if 'Year' not in params and method.startswith('GetData'):
                current_year = datetime.now().year
                params['Year'] = str(current_year)

            url = f"{self.base_url}?{urlencode(params)}"

            response = self.session.get(url, timeout=30)
            response.raise_for_status()

            data = response.json()

            # Check for BEA API errors
            if 'BEAAPI' in data:
                if 'Error' in data['BEAAPI']:
                    error_desc = data['BEAAPI']['Error'].get('ErrorDesc', 'Unknown BEA API error')
                    return BEAError(method, error_desc).to_dict()

                # Extract actual data
                results = data['BEAAPI'].get('Results', {})

                # Handle different response structures
                if method == 'GetDatasetList':
                    return {
                        "success": True,
                        "endpoint": method,
                        "data": results.get('Dataset', []),
                        "timestamp": int(datetime.now().timestamp())
                    }

                elif method == 'GetParameterList':
                    return {
                        "success": True,
                        "endpoint": method,
                        "data": results.get('Parameter', []),
                        "dataset_name": params.get('DatasetName', ''),
                        "timestamp": int(datetime.now().timestamp())
                    }

                elif method in ['GetParameterValues', 'GetParameterValuesFiltered']:
                    return {
                        "success": True,
                        "endpoint": method,
                        "data": results.get('ParamValue', []),
                        "parameter": params.get('ParameterName', ''),
                        "dataset_name": params.get('DatasetName', ''),
                        "timestamp": int(datetime.now().timestamp())
                    }

                else:  # GetData methods
                    return {
                        "success": True,
                        "endpoint": method,
                        "data": results.get('Data', []),
                        "dataset_name": params.get('DatasetName', ''),
                        "parameters": {
                            k: v for k, v in params.items()
                            if k not in ['UserID', 'method', 'resultformat']
                        },
                        "notes": results.get('Notes', []),
                        "statistics": results.get('Stat', []),
                        "dimensions": results.get('Dimensions', []),
                        "timestamp": int(datetime.now().timestamp())
                    }

            return BEAError(method, "Unexpected response format").to_dict()

        except requests.exceptions.RequestException as e:
            return BEAError(method, f"Network error: {str(e)}").to_dict()
        except json.JSONDecodeError as e:
            return BEAError(method, f"JSON decode error: {str(e)}").to_dict()
        except Exception as e:
            return BEAError(method, f"Unexpected error: {str(e)}").to_dict()

    # ==================== METADATA ENDPOINTS ====================

    def get_dataset_list(self) -> Dict[str, Any]:
        """Get list of all available datasets"""
        try:
            result = self._make_request('GetDatasetList', {})

            if result.get("success"):
                # Add descriptions for major datasets
                dataset_descriptions = {
                    "NIPA": "National Income and Product Accounts",
                    "NIUnderlyingDetail": "NIPA Underlying Detail",
                    "FixedAssets": "Fixed Assets",
                    "MNE": "Multinational Enterprises",
                    "GDPbyIndustry": "GDP by Industry",
                    "ITA": "International Transactions",
                    "IIP": "International Investment Position",
                    "InputOutput": "Input-Output Accounts",
                    "UnderlyingGDPbyIndustry": "GDP by Industry - Underlying Detail",
                    "IntlServTrade": "International Services Trade",
                    "Regional": "Regional Economic Accounts"
                }

                # Enhance dataset information
                for dataset in result.get("data", []):
                    dataset_name = dataset.get("DatasetName", "")
                    if dataset_name in dataset_descriptions:
                        dataset["Description"] = dataset_descriptions[dataset_name]

            return result

        except Exception as e:
            return BEAError('GetDatasetList', str(e)).to_dict()

    def get_parameter_list(self, dataset_name: str) -> Dict[str, Any]:
        """Get list of parameters for a specific dataset"""
        try:
            if not dataset_name:
                return BEAError('GetParameterList', 'DatasetName is required').to_dict()

            params = {'DatasetName': dataset_name}
            result = self._make_request('GetParameterList', params)

            return result

        except Exception as e:
            return BEAError('GetParameterList', str(e)).to_dict()

    def get_parameter_values(self, dataset_name: str, parameter_name: str) -> Dict[str, Any]:
        """Get all possible values for a specific parameter"""
        try:
            if not dataset_name or not parameter_name:
                return BEAError('GetParameterValues', 'DatasetName and ParameterName are required').to_dict()

            params = {
                'DatasetName': dataset_name,
                'ParameterName': parameter_name
            }
            result = self._make_request('GetParameterValues', params)

            return result

        except Exception as e:
            return BEAError('GetParameterValues', str(e)).to_dict()

    def get_parameter_values_filtered(self, dataset_name: str, parameter_name: str,
                                    target_parameter: str) -> Dict[str, Any]:
        """Get filtered parameter values based on another parameter"""
        try:
            if not dataset_name or not parameter_name or not target_parameter:
                return BEAError('GetParameterValuesFiltered', 'DatasetName, ParameterName, and TargetParameter are required').to_dict()

            params = {
                'DatasetName': dataset_name,
                'ParameterName': parameter_name,
                'TargetParameter': target_parameter
            }
            result = self._make_request('GetParameterValuesFiltered', params)

            return result

        except Exception as e:
            return BEAError('GetParameterValuesFiltered', str(e)).to_dict()

    # ==================== DATA RETRIEVAL ENDPOINTS ====================

    def get_nipa_data(self, table_name: str, frequency: str = 'A', year: str = None,
                     year_range: str = None) -> Dict[str, Any]:
        """Get National Income and Product Accounts data"""
        try:
            if not table_name:
                return BEAError('NIPA', 'TableName is required').to_dict()

            params = {
                'DatasetName': 'NIPA',
                'TableName': table_name,
                'Frequency': frequency
            }

            if year:
                params['Year'] = year
            elif year_range:
                params['Year'] = year_range
            elif frequency == 'Q':
                # Default to recent quarters for quarterly data
                params['Year'] = f"{datetime.now().year-1}Q1,{datetime.now().year}Q4"

            result = self._make_request('GetData', params)
            return result

        except Exception as e:
            return BEAError('NIPA', str(e)).to_dict()

    def get_ni_underlying_detail(self, table_name: str, frequency: str = 'A',
                                year: str = None) -> Dict[str, Any]:
        """Get NIPA Underlying Detail data"""
        try:
            if not table_name:
                return BEAError('NIUnderlyingDetail', 'TableName is required').to_dict()

            params = {
                'DatasetName': 'NIUnderlyingDetail',
                'TableName': table_name,
                'Frequency': frequency
            }

            if year:
                params['Year'] = year

            result = self._make_request('GetData', params)
            return result

        except Exception as e:
            return BEAError('NIUnderlyingDetail', str(e)).to_dict()

    def get_fixed_assets(self, table_name: str, year: str = None) -> Dict[str, Any]:
        """Get Fixed Assets data"""
        try:
            if not table_name:
                return BEAError('FixedAssets', 'TableName is required').to_dict()

            params = {'DatasetName': 'FixedAssets', 'TableName': table_name}

            if year:
                params['Year'] = year

            result = self._make_request('GetData', params)
            return result

        except Exception as e:
            return BEAError('FixedAssets', str(e)).to_dict()

    def get_mne_data(self, series_id: str = None, direction: str = 'Outward',
                    classification: str = 'Country', year: str = None,
                    country: str = None, industry: str = None,
                    state: str = None, ownership_level: str = None,
                    nonbank_affiliates_only: str = None,
                    get_footnotes: str = 'No') -> Dict[str, Any]:
        """Get Multinational Enterprises data"""
        try:
            params = {'DatasetName': 'MNE'}

            # Required parameters
            if direction:
                params['DirectionOfInvestment'] = direction
            else:
                return BEAError('MNE', 'DirectionOfInvestment is required').to_dict()

            # Optional parameters
            if series_id:
                params['SeriesID'] = series_id
            if classification:
                params['Classification'] = classification
            if year:
                params['Year'] = year
            if country:
                params['Country'] = country
            if industry:
                params['Industry'] = industry
            if state:
                params['State'] = state
            if ownership_level:
                params['OwnershipLevel'] = ownership_level
            if nonbank_affiliates_only:
                params['NonBankAffiliatesOnly'] = nonbank_affiliates_only
            if get_footnotes:
                params['GetFootnotes'] = get_footnotes

            result = self._make_request('GetData', params)
            return result

        except Exception as e:
            return BEAError('MNE', str(e)).to_dict()

    def get_gdp_by_industry(self, table_id: str, year: str = None,
                           frequency: str = 'A', industry: str = 'ALL') -> Dict[str, Any]:
        """Get GDP by Industry data"""
        try:
            if not table_id:
                return BEAError('GDPbyIndustry', 'TableID is required').to_dict()

            params = {
                'DatasetName': 'GDPbyIndustry',
                'TableID': table_id,
                'Frequency': frequency,
                'Year': year,
                'Industry': industry
            }

            result = self._make_request('GetData', params)
            return result

        except Exception as e:
            return BEAError('GDPbyIndustry', str(e)).to_dict()

    def get_international_transactions(self, indicator: str = None, area_or_country: str = 'AllCountries',
                                     frequency: str = 'A', year: str = None) -> Dict[str, Any]:
        """Get International Transactions Accounts data"""
        try:
            params = {
                'DatasetName': 'ITA',
                'AreaOrCountry': area_or_country,
                'Frequency': frequency,
                'Year': year
            }

            if indicator:
                params['Indicator'] = indicator

            result = self._make_request('GetData', params)
            return result

        except Exception as e:
            return BEAError('ITA', str(e)).to_dict()

    def get_international_investment_position(self, type_of_investment: str = None,
                                            component: str = None, frequency: str = 'A',
                                            year: str = None) -> Dict[str, Any]:
        """Get International Investment Position data"""
        try:
            params = {
                'DatasetName': 'IIP',
                'Frequency': frequency,
                'Year': year
            }

            if type_of_investment:
                params['TypeOfInvestment'] = type_of_investment
            if component:
                params['Component'] = component

            result = self._make_request('GetData', params)
            return result

        except Exception as e:
            return BEAError('IIP', str(e)).to_dict()

    def get_input_output(self, table_id: str, year: str = None) -> Dict[str, Any]:
        """Get Input-Output Accounts data"""
        try:
            if not table_id:
                return BEAError('InputOutput', 'TableID is required').to_dict()

            params = {'DatasetName': 'InputOutput', 'TableID': table_id}

            if year:
                params['Year'] = year

            result = self._make_request('GetData', params)
            return result

        except Exception as e:
            return BEAError('InputOutput', str(e)).to_dict()

    def get_underlying_gdp_by_industry(self, table_id: str, year: str = None,
                                    frequency: str = 'A', industry: str = 'ALL') -> Dict[str, Any]:
        """Get GDP by Industry - Underlying Detail data"""
        try:
            if not table_id:
                return BEAError('UnderlyingGDPbyIndustry', 'TableID is required').to_dict()

            params = {
                'DatasetName': 'UnderlyingGDPbyIndustry',
                'TableID': table_id,
                'Frequency': frequency,
                'Year': year,
                'Industry': industry
            }

            result = self._make_request('GetData', params)
            return result

        except Exception as e:
            return BEAError('UnderlyingGDPbyIndustry', str(e)).to_dict()

    def get_international_services_trade(self, type_of_service: str = None,
                                       trade_direction: str = None,
                                       affiliation: str = None,
                                       area_or_country: str = 'AllCountries',
                                       year: str = None) -> Dict[str, Any]:
        """Get International Services Trade data"""
        try:
            params = {
                'DatasetName': 'IntlServTrade',
                'AreaOrCountry': area_or_country,
                'Year': year
            }

            if type_of_service:
                params['TypeOfService'] = type_of_service
            if trade_direction:
                params['TradeDirection'] = trade_direction
            if affiliation:
                params['Affiliation'] = affiliation

            result = self._make_request('GetData', params)
            return result

        except Exception as e:
            return BEAError('IntlServTrade', str(e)).to_dict()

    def get_regional_data(self, table_name: str, line_code: str = 'ALL',
                         geo_fips: str = 'STATE', year: str = None) -> Dict[str, Any]:
        """Get Regional Economic Accounts data"""
        try:
            if not table_name:
                return BEAError('Regional', 'TableName is required').to_dict()

            params = {
                'DatasetName': 'Regional',
                'TableName': table_name,
                'LineCode': line_code,
                'GeoFIPS': geo_fips
            }

            if year:
                params['Year'] = year

            result = self._make_request('GetData', params)
            return result

        except Exception as e:
            return BEAError('Regional', str(e)).to_dict()

    # ==================== COMPOSITE METHODS ====================

    def get_economic_overview(self, year: str = None) -> Dict[str, Any]:
        """Get comprehensive economic overview from multiple datasets"""
        result = {
            "success": True,
            "overview_type": "economic_overview",
            "year": year or str(datetime.now().year),
            "timestamp": int(datetime.now().timestamp()),
            "datasets": {},
            "failed_datasets": []
        }

        # Define datasets to include in overview
        overview_datasets = [
            ('NIPA GDP', lambda: self.get_nipa_data('T10101', 'Q', year)),
            ('GDP by Industry', lambda: self.get_gdp_by_industry('1', year, 'A')),
            ('International Transactions', lambda: self.get_international_transactions('BalGds', 'AllCountries', 'A', year)),
            ('Regional Data', lambda: self.get_regional_data('SAINC1', '1', 'STATE', year))
        ]

        overall_success = False

        for dataset_name, dataset_func in overview_datasets:
            try:
                dataset_result = dataset_func()
                result["datasets"][dataset_name] = dataset_result

                if dataset_result.get("success"):
                    overall_success = True
                else:
                    result["failed_datasets"].append({
                        "dataset": dataset_name,
                        "error": dataset_result.get("error", "Unknown error")
                    })

            except Exception as e:
                result["failed_datasets"].append({
                    "dataset": dataset_name,
                    "error": str(e)
                })

        result["success"] = overall_success
        return result

    def get_regional_snapshot(self, geo_fips: str = 'USA', year: str = None) -> Dict[str, Any]:
        """Get comprehensive regional economic snapshot"""
        result = {
            "success": True,
            "snapshot_type": "regional_snapshot",
            "geo_fips": geo_fips,
            "year": year or str(datetime.now().year),
            "timestamp": int(datetime.now().timestamp()),
            "datasets": {},
            "failed_datasets": []
        }

        # Define regional datasets to include
        regional_datasets = [
            ('Personal Income', lambda: self.get_regional_data('SAINC1', '1', geo_fips, year)),
            ('GDP by State', lambda: self.get_regional_data('SAGDP2N', '2', geo_fips, year)),
            ('Real GDP', lambda: self.get_regional_data('SAGDP9N', '2', geo_fips, year))
        ]

        overall_success = False

        for dataset_name, dataset_func in regional_datasets:
            try:
                dataset_result = dataset_func()
                result["datasets"][dataset_name] = dataset_result

                if dataset_result.get("success"):
                    overall_success = True
                else:
                    result["failed_datasets"].append({
                        "dataset": dataset_name,
                        "error": dataset_result.get("error", "Unknown error")
                    })

            except Exception as e:
                result["failed_datasets"].append({
                    "dataset": dataset_name,
                    "error": str(e)
                })

        result["success"] = overall_success
        return result


def main():
    """CLI interface for BEA Data Fetcher"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python bea_data.py <command> <args>",
            "available_commands": [
                "dataset_list",
                "parameter_list <dataset_name>",
                "parameter_values <dataset_name> <parameter_name>",
                "parameter_values_filtered <dataset_name> <parameter_name> <target_parameter>",
                "nipa <table_name> [frequency] [year]",
                "ni_underlying <table_name> [frequency] [year]",
                "fixed_assets <table_name> [year]",
                "mne <direction> [classification] [year] [country] [industry] [state] [ownership_level] [nonbank_affiliates_only] [get_footnotes]",
                "gdp_by_industry <table_id> [year] [frequency] [industry]",
                "international_transactions [indicator] [area_or_country] [frequency] [year]",
                "international_investment [type_of_investment] [component] [frequency] [year]",
                "input_output <table_id> [year]",
                "underlying_gdp_industry <table_id> [year] [frequency] [industry]",
                "international_services [type_of_service] [trade_direction] [affiliation] [area_or_country] [year]",
                "regional <table_name> [line_code] [geo_fips] [year]",
                "economic_overview [year]",
                "regional_snapshot [geo_fips] [year]"
            ]
        }))
        sys.exit(1)

    command = sys.argv[1]
    wrapper = BEAWrapper()

    try:
        if command == "dataset_list":
            result = wrapper.get_dataset_list()
            print(json.dumps(result, indent=2))

        elif command == "parameter_list":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python bea_data.py parameter_list <dataset_name>"}))
                sys.exit(1)

            dataset_name = sys.argv[2]
            result = wrapper.get_parameter_list(dataset_name)
            print(json.dumps(result, indent=2))

        elif command == "parameter_values":
            if len(sys.argv) < 4:
                print(json.dumps({"error": "Usage: python bea_data.py parameter_values <dataset_name> <parameter_name>"}))
                sys.exit(1)

            dataset_name = sys.argv[2]
            parameter_name = sys.argv[3]
            result = wrapper.get_parameter_values(dataset_name, parameter_name)
            print(json.dumps(result, indent=2))

        elif command == "parameter_values_filtered":
            if len(sys.argv) < 5:
                print(json.dumps({"error": "Usage: python bea_data.py parameter_values_filtered <dataset_name> <parameter_name> <target_parameter>"}))
                sys.exit(1)

            dataset_name = sys.argv[2]
            parameter_name = sys.argv[3]
            target_parameter = sys.argv[4]
            result = wrapper.get_parameter_values_filtered(dataset_name, parameter_name, target_parameter)
            print(json.dumps(result, indent=2))

        elif command == "nipa":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python bea_data.py nipa <table_name> [frequency] [year]"}))
                sys.exit(1)

            table_name = sys.argv[2]
            frequency = sys.argv[3] if len(sys.argv) > 3 else 'A'
            year = sys.argv[4] if len(sys.argv) > 4 else None
            result = wrapper.get_nipa_data(table_name, frequency, year)
            print(json.dumps(result, indent=2))

        elif command == "ni_underlying":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python bea_data.py ni_underlying <table_name> [frequency] [year]"}))
                sys.exit(1)

            table_name = sys.argv[2]
            frequency = sys.argv[3] if len(sys.argv) > 3 else 'A'
            year = sys.argv[4] if len(sys.argv) > 4 else None
            result = wrapper.get_ni_underlying_detail(table_name, frequency, year)
            print(json.dumps(result, indent=2))

        elif command == "fixed_assets":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python bea_data.py fixed_assets <table_name> [year]"}))
                sys.exit(1)

            table_name = sys.argv[2]
            year = sys.argv[3] if len(sys.argv) > 3 else None
            result = wrapper.get_fixed_assets(table_name, year)
            print(json.dumps(result, indent=2))

        elif command == "mne":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python bea_data.py mne <direction> [classification] [year] [country] [industry] [state] [ownership_level] [nonbank_affiliates_only] [get_footnotes]"}))
                sys.exit(1)

            direction = sys.argv[2]
            classification = sys.argv[3] if len(sys.argv) > 3 else 'Country'
            year = sys.argv[4] if len(sys.argv) > 4 else None
            country = sys.argv[5] if len(sys.argv) > 5 else None
            industry = sys.argv[6] if len(sys.argv) > 6 else None
            state = sys.argv[7] if len(sys.argv) > 7 else None
            ownership_level = sys.argv[8] if len(sys.argv) > 8 else None
            nonbank_affiliates_only = sys.argv[9] if len(sys.argv) > 9 else None
            get_footnotes = sys.argv[10] if len(sys.argv) > 10 else 'No'
            result = wrapper.get_mne_data(None, direction, classification, year, country, industry, state, ownership_level, nonbank_affiliates_only, get_footnotes)
            print(json.dumps(result, indent=2))

        elif command == "gdp_by_industry":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python bea_data.py gdp_by_industry <table_id> [year] [frequency] [industry]"}))
                sys.exit(1)

            table_id = sys.argv[2]
            year = sys.argv[3] if len(sys.argv) > 3 else None
            frequency = sys.argv[4] if len(sys.argv) > 4 else 'A'
            industry = sys.argv[5] if len(sys.argv) > 5 else 'ALL'
            result = wrapper.get_gdp_by_industry(table_id, year, frequency, industry)
            print(json.dumps(result, indent=2))

        elif command == "international_transactions":
            indicator = sys.argv[2] if len(sys.argv) > 2 else None
            area_or_country = sys.argv[3] if len(sys.argv) > 3 else 'AllCountries'
            frequency = sys.argv[4] if len(sys.argv) > 4 else 'A'
            year = sys.argv[5] if len(sys.argv) > 5 else None
            result = wrapper.get_international_transactions(indicator, area_or_country, frequency, year)
            print(json.dumps(result, indent=2))

        elif command == "international_investment":
            type_of_investment = sys.argv[2] if len(sys.argv) > 2 else None
            component = sys.argv[3] if len(sys.argv) > 3 else None
            frequency = sys.argv[4] if len(sys.argv) > 4 else 'A'
            year = sys.argv[5] if len(sys.argv) > 5 else None
            result = wrapper.get_international_investment_position(type_of_investment, component, frequency, year)
            print(json.dumps(result, indent=2))

        elif command == "input_output":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python bea_data.py input_output <table_id> [year]"}))
                sys.exit(1)

            table_id = sys.argv[2]
            year = sys.argv[3] if len(sys.argv) > 3 else None
            result = wrapper.get_input_output(table_id, year)
            print(json.dumps(result, indent=2))

        elif command == "underlying_gdp_industry":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python bea_data.py underlying_gdp_industry <table_id> [year] [frequency] [industry]"}))
                sys.exit(1)

            table_id = sys.argv[2]
            year = sys.argv[3] if len(sys.argv) > 3 else None
            frequency = sys.argv[4] if len(sys.argv) > 4 else 'A'
            industry = sys.argv[5] if len(sys.argv) > 5 else 'ALL'
            result = wrapper.get_underlying_gdp_by_industry(table_id, year, frequency, industry)
            print(json.dumps(result, indent=2))

        elif command == "international_services":
            type_of_service = sys.argv[2] if len(sys.argv) > 2 else None
            trade_direction = sys.argv[3] if len(sys.argv) > 3 else None
            affiliation = sys.argv[4] if len(sys.argv) > 4 else None
            area_or_country = sys.argv[5] if len(sys.argv) > 5 else 'AllCountries'
            year = sys.argv[6] if len(sys.argv) > 6 else None
            result = wrapper.get_international_services_trade(type_of_service, trade_direction, affiliation, area_or_country, year)
            print(json.dumps(result, indent=2))

        elif command == "regional":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python bea_data.py regional <table_name> [line_code] [geo_fips] [year]"}))
                sys.exit(1)

            table_name = sys.argv[2]
            line_code = sys.argv[3] if len(sys.argv) > 3 else 'ALL'
            geo_fips = sys.argv[4] if len(sys.argv) > 4 else 'STATE'
            year = sys.argv[5] if len(sys.argv) > 5 else None
            result = wrapper.get_regional_data(table_name, line_code, geo_fips, year)
            print(json.dumps(result, indent=2))

        elif command == "economic_overview":
            year = sys.argv[2] if len(sys.argv) > 2 else None
            result = wrapper.get_economic_overview(year)
            print(json.dumps(result, indent=2))

        elif command == "regional_snapshot":
            geo_fips = sys.argv[2] if len(sys.argv) > 2 else 'USA'
            year = sys.argv[3] if len(sys.argv) > 3 else None
            result = wrapper.get_regional_snapshot(geo_fips, year)
            print(json.dumps(result, indent=2))

        else:
            print(json.dumps({
                "error": f"Unknown command: {command}",
                "available_commands": [
                    "dataset_list",
                    "parameter_list <dataset_name>",
                    "parameter_values <dataset_name> <parameter_name>",
                    "parameter_values_filtered <dataset_name> <parameter_name> <target_parameter>",
                    "nipa <table_name> [frequency] [year]",
                    "ni_underlying <table_name> [frequency] [year]",
                    "fixed_assets <table_name> [year]",
                    "mne <direction> [classification] [year] [country] [industry] [state] [ownership_level] [nonbank_affiliates_only] [get_footnotes]",
                    "gdp_by_industry <table_id> [year] [frequency] [industry]",
                    "international_transactions [indicator] [area_or_country] [frequency] [year]",
                    "international_investment [type_of_investment] [component] [frequency] [year]",
                    "input_output <table_id> [year]",
                    "underlying_gdp_industry <table_id> [year] [frequency] [industry]",
                    "international_services [type_of_service] [trade_direction] [affiliation] [area_or_country] [year]",
                    "regional <table_name> [line_code] [geo_fips] [year]",
                    "economic_overview [year]",
                    "regional_snapshot [geo_fips] [year]"
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