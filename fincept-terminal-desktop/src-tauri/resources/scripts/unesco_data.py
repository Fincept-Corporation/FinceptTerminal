"""
UNESCO Institute for Statistics (UIS) Data Fetcher
Comprehensive wrapper for UNESCO UIS Data API providing access to
global education, science, culture, and demographic statistics

API Documentation:
- Base URL: https://api.uis.unesco.org
- Authentication: No API key required (public API)
- Rate limits: None specified, but be reasonable with requests
- License: Creative Commons Attribution-ShareAlike 4.0 International

Available Data Categories:
- Education: Enrollment, completion rates, literacy, teaching staff
- Science Technology & Innovation: R&D expenditure, researchers, patents
- Culture: Heritage sites, cultural employment, cultural participation
- Demographic & Socio-Economic: Population, economy, social indicators

Data Coverage:
- 200+ countries and regions
- Time series data from 1970s to present
- Multiple disaggregation levels (by sex, age, region, etc.)
- 100,000+ indicators
"""

import sys
import json
import os
import requests
from datetime import datetime, timedelta
from typing import Dict, Any, Optional, List, Union
from urllib.parse import urlencode
import zipfile
import io


class UISError:
    """Error handling wrapper for UNESCO UIS API responses"""
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


class UISWrapper:
    """Comprehensive UNESCO UIS API wrapper with fault tolerance"""

    def __init__(self):
        self.base_url = "https://api.uis.unesco.org/api/public"
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': 'Fincept-Terminal/1.0',
            'Accept': 'application/json',
            'Accept-Encoding': 'gzip'
        })

    def _make_request(self, endpoint: str, params: Dict[str, Any]) -> Dict[str, Any]:
        """Centralized request handler with comprehensive error handling"""
        try:
            url = f"{self.base_url}{endpoint}?{urlencode(params, doseq=True)}"

            response = self.session.get(url, timeout=60)
            response.raise_for_status()

            # Handle JSON response
            try:
                data = response.json()

                # Check for API errors in response
                if 'error' in data:
                    error_msg = data.get('message', 'Unknown API error')
                    return UISError(endpoint, error_msg, response.status_code).to_dict()

                return {
                    "success": True,
                    "endpoint": endpoint,
                    "data": data,
                    "parameters": params,
                    "timestamp": int(datetime.now().timestamp())
                }

            except json.JSONDecodeError as e:
                return UISError(endpoint, f"JSON decode error: {str(e)}", response.status_code).to_dict()

        except requests.exceptions.RequestException as e:
            return UISError(endpoint, f"Network error: {str(e)}").to_dict()
        except Exception as e:
            return UISError(endpoint, f"Unexpected error: {str(e)}").to_dict()

    def _make_export_request(self, endpoint: str, params: Dict[str, Any]) -> Dict[str, Any]:
        """Handle export requests that return ZIP files"""
        try:
            url = f"{self.base_url}{endpoint}?{urlencode(params, doseq=True)}"

            response = self.session.get(url, timeout=120)
            response.raise_for_status()

            # Check if response is a ZIP file
            if response.headers.get('content-type', '').startswith('application/zip'):
                # Process ZIP file
                zip_content = io.BytesIO(response.content)

                with zipfile.ZipFile(zip_content) as zip_file:
                    file_list = zip_file.namelist()
                    extracted_files = {}

                    for file_name in file_list:
                        try:
                            with zip_file.open(file_name) as file:
                                if file_name.endswith('.csv') or file_name.endswith('.md'):
                                    content = file.read().decode('utf-8')
                                    extracted_files[file_name] = content
                        except Exception as e:
                            extracted_files[file_name] = f"Error extracting file: {str(e)}"

                return {
                    "success": True,
                    "endpoint": endpoint,
                    "data": {
                        "type": "export_zip",
                        "files": extracted_files,
                        "file_count": len(file_list)
                    },
                    "parameters": params,
                    "timestamp": int(datetime.now().timestamp())
                }

            # Handle other export formats (CSV, Excel)
            else:
                content_type = response.headers.get('content-type', '')
                content = response.content.decode('utf-8', errors='ignore')

                return {
                    "success": True,
                    "endpoint": endpoint,
                    "data": {
                        "type": "export_file",
                        "content_type": content_type,
                        "content": content
                    },
                    "parameters": params,
                    "timestamp": int(datetime.now().timestamp())
                }

        except requests.exceptions.RequestException as e:
            return UISError(endpoint, f"Network error: {str(e)}").to_dict()
        except Exception as e:
            return UISError(endpoint, f"Unexpected error: {str(e)}").to_dict()

    # ==================== DATA ENDPOINTS ====================

    def get_indicator_data(self, indicators: List[str] = None, geo_units: List[str] = None,
                           geo_unit_type: str = None, start_year: int = None,
                           end_year: int = None, footnotes: bool = False,
                           indicator_metadata: bool = False, version: str = None) -> Dict[str, Any]:
        """Fetch indicator data records based on filter parameters"""
        try:
            params = {}

            if indicators:
                params['indicator'] = indicators
            if geo_units:
                params['geoUnit'] = geo_units
            if geo_unit_type:
                params['geoUnitType'] = geo_unit_type
            if start_year:
                params['start'] = start_year
            if end_year:
                params['end'] = end_year
            if footnotes:
                params['footnotes'] = footnotes
            if indicator_metadata:
                params['indicatorMetadata'] = indicator_metadata
            if version:
                params['version'] = version

            result = self._make_request('/data/indicators', params)
            return result

        except Exception as e:
            return UISError('get_indicator_data', str(e)).to_dict()

    def export_indicator_data(self, format_type: str, indicators: List[str] = None,
                            geo_units: List[str] = None, geo_unit_type: str = None,
                            start_year: int = None, end_year: int = None,
                            footnotes: bool = False, indicator_metadata: bool = False,
                            version: str = None) -> Dict[str, Any]:
        """Export indicator data in CSV, Excel, or JSON format"""
        try:
            if format_type not in ['csv', 'excel', 'json']:
                return UISError('export_indicator_data', f"Invalid format: {format_type}. Must be csv, excel, or json").to_dict()

            params = {'format': format_type}

            if indicators:
                params['indicator'] = indicators
            if geo_units:
                params['geoUnit'] = geo_units
            if geo_unit_type:
                params['geoUnitType'] = geo_unit_type
            if start_year:
                params['start'] = start_year
            if end_year:
                params['end'] = end_year
            if footnotes:
                params['footnotes'] = footnotes
            if indicator_metadata:
                params['indicatorMetadata'] = indicator_metadata
            if version:
                params['version'] = version

            result = self._make_export_request('/data/indicators/export', params)
            return result

        except Exception as e:
            return UISError('export_indicator_data', str(e)).to_dict()

    # ==================== DEFINITIONS ENDPOINTS ====================

    def list_geo_units(self, version: str = None) -> Dict[str, Any]:
        """Get list of all available geographic units (countries and regions)"""
        try:
            params = {}
            if version:
                params['version'] = version

            result = self._make_request('/definitions/geounits', params)
            return result

        except Exception as e:
            return UISError('list_geo_units', str(e)).to_dict()

    def export_geo_units(self, format_type: str, version: str = None) -> Dict[str, Any]:
        """Export list of geo units in CSV or Excel format"""
        try:
            if format_type not in ['csv', 'excel']:
                return UISError('export_geo_units', f"Invalid format: {format_type}. Must be csv or excel").to_dict()

            params = {'format': format_type}
            if version:
                params['version'] = version

            result = self._make_export_request('/definitions/geounits/export', params)
            return result

        except Exception as e:
            return UISError('export_geo_units', str(e)).to_dict()

    def list_indicators(self, glossary_terms: bool = False, disaggregations: bool = False,
                       version: str = None) -> Dict[str, Any]:
        """Get list of all available indicators with optional metadata"""
        try:
            params = {}
            if glossary_terms:
                params['glossaryTerms'] = glossary_terms
            if disaggregations:
                params['disaggregations'] = disaggregations
            if version:
                params['version'] = version

            result = self._make_request('/definitions/indicators', params)
            return result

        except Exception as e:
            return UISError('list_indicators', str(e)).to_dict()

    def export_indicators(self, format_type: str, glossary_terms: bool = False,
                         disaggregations: bool = False, version: str = None) -> Dict[str, Any]:
        """Export list of indicators in CSV or Excel format"""
        try:
            if format_type not in ['csv', 'excel']:
                return UISError('export_indicators', f"Invalid format: {format_type}. Must be csv or excel").to_dict()

            params = {'format': format_type}
            if glossary_terms:
                params['glossaryTerms'] = glossary_terms
            if disaggregations:
                params['disaggregations'] = disaggregations
            if version:
                params['version'] = version

            result = self._make_export_request('/definitions/indicators/export', params)
            return result

        except Exception as e:
            return UISError('export_indicators', str(e)).to_dict()

    # ==================== VERSION ENDPOINTS ====================

    def get_default_version(self) -> Dict[str, Any]:
        """Get the current default data version"""
        try:
            result = self._make_request('/versions/default', {})
            return result

        except Exception as e:
            return UISError('get_default_version', str(e)).to_dict()

    def list_versions(self) -> Dict[str, Any]:
        """Get list of all published data versions"""
        try:
            result = self._make_request('/versions', {})
            return result

        except Exception as e:
            return UISError('list_versions', str(e)).to_dict()

    # ==================== CONVENIENCE METHODS ====================

    def get_education_overview(self, country_codes: List[str] = None,
                             start_year: int = None, end_year: int = None) -> Dict[str, Any]:
        """Get comprehensive education data overview for countries"""
        try:
            # Key education indicators
            education_indicators = [
                'ROFST.1',      # Gross enrolment ratio, primary
                'NERA.1',       # Net enrolment rate, primary
                'CR.1',         # Completion rate, primary
                'SE.XPD.TOTL.GD.ZS',  # Government expenditure on education (% of GDP)
                'UIS.LIT.AG25T64',   # Literacy rate, 25-64 years
                'STABBRV.2',    # Pupil-teacher ratio, primary
                'ENRL.TCHL.ADV.ZS', # Trained teachers (% of total teachers)
                'SCH.PRM',      # Number of primary schools
                'EDUDATA.NY'    # Education data availability
            ]

            result = self.get_indicator_data(
                indicators=education_indicators,
                geo_units=country_codes or [],
                start_year=start_year,
                end_year=end_year,
                indicator_metadata=True,
                footnotes=True
            )

            if result.get("success"):
                result["overview_type"] = "education_overview"
                result["indicators_count"] = len(education_indicators)
                if country_codes:
                    result["countries"] = country_codes

            return result

        except Exception as e:
            return UISError('get_education_overview', str(e)).to_dict()

    def get_global_education_trends(self, indicator_code: str = None,
                                    start_year: int = 2000, end_year: int = None) -> Dict[str, Any]:
        """Get global education trends for a specific indicator"""
        try:
            if not indicator_code:
                indicator_code = 'ROFST.1'  # Default to primary gross enrolment ratio

            # Get all national data for the indicator
            result = self.get_indicator_data(
                indicators=[indicator_code],
                geo_unit_type='NATIONAL',
                start_year=start_year,
                end_year=end_year or datetime.now().year,
                indicator_metadata=True
            )

            if result.get("success"):
                result["trend_analysis"] = {
                    "indicator": indicator_code,
                    "time_period": f"{start_year}-{end_year or datetime.now().year}",
                    "data_type": "national_trends"
                }

            return result

        except Exception as e:
            return UISError('get_global_education_trends', str(e)).to_dict()

    def get_country_comparison(self, indicator_code: str, country_codes: List[str],
                             start_year: int = None, end_year: int = None) -> Dict[str, Any]:
        """Compare specific indicators across multiple countries"""
        try:
            if not indicator_code:
                return UISError('get_country_comparison', 'Indicator code is required').to_dict()

            if not country_codes:
                return UISError('get_country_comparison', 'At least one country code is required').to_dict()

            result = self.get_indicator_data(
                indicators=[indicator_code],
                geo_units=country_codes,
                start_year=start_year,
                end_year=end_year,
                indicator_metadata=True,
                footnotes=True
            )

            if result.get("success"):
                result["comparison_type"] = "country_comparison"
                result["indicator"] = indicator_code
                result["countries"] = country_codes

            return result

        except Exception as e:
            return UISError('get_country_comparison', str(e)).to_dict()

    def search_indicators_by_theme(self, theme: str, limit: int = 50) -> Dict[str, Any]:
        """Search indicators by theme (EDUCATION, SCIENCE_TECHNOLOGY_INNOVATION, CULTURE, DEMOGRAPHIC_SOCIOECONOMIC)"""
        try:
            # Get all indicators with metadata
            all_indicators_result = self.list_indicators(
                glossary_terms=True,
                disaggregations=True
            )

            if not all_indicators_result.get("success"):
                return all_indicators_result

            # Filter indicators by theme
            indicators = all_indicators_result.get("data", [])
            filtered_indicators = []

            for indicator in indicators:
                if indicator.get("theme") == theme:
                    filtered_indicators.append(indicator)
                    if len(filtered_indicators) >= limit:
                        break

            return {
                "success": True,
                "endpoint": "search_indicators_by_theme",
                "data": filtered_indicators,
                "theme": theme,
                "total_found": len(filtered_indicators),
                "limit": limit,
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return UISError('search_indicators_by_theme', str(e)).to_dict()

    def get_regional_education_data(self, region_id: str = None,
                                   indicator_codes: List[str] = None,
                                   start_year: int = None, end_year: int = None) -> Dict[str, Any]:
        """Get education data for specific regions"""
        try:
            if not region_id:
                return UISError('get_regional_education_data', 'Region ID is required').to_dict()

            # Default to key education indicators if none provided
            if not indicator_codes:
                indicator_codes = ['ROFST.1', 'NERA.1', 'CR.1']

            result = self.get_indicator_data(
                indicators=indicator_codes,
                geo_units=[region_id],
                start_year=start_year,
                end_year=end_year,
                indicator_metadata=True
            )

            if result.get("success"):
                result["region"] = region_id
                result["data_type"] = "regional_education"

            return result

        except Exception as e:
            return UISError('get_regional_education_data', str(e)).to_dict()

    def get_science_technology_data(self, country_codes: List[str] = None,
                                   start_year: int = None, end_year: int = None) -> Dict[str, Any]:
        """Get Science, Technology & Innovation indicators"""
        try:
            # Key STI indicators
            sti_indicators = [
                'GERD.GD.ZS',      # R&D expenditure (% of GDP)
                'RE.R&D.GD.ZS',    # Researchers per million inhabitants
                'PAT.RES.PROD',   # Patent applications, resident
                'PAT.NRES.PROD',  # Patent applications, non-resident
                'ST.JRN.ART.SC',  # Scientific and technical journal articles
                'HIGH.TECH.EXP.ZS', # High-technology exports (% of manufactured exports)
                'ICT.EXP.ZS'      # ICT service exports (% of service exports)
            ]

            result = self.get_indicator_data(
                indicators=sti_indicators,
                geo_units=country_codes or [],
                start_year=start_year,
                end_year=end_year,
                indicator_metadata=True
            )

            if result.get("success"):
                result["data_category"] = "science_technology_innovation"
                result["indicators_count"] = len(sti_indicators)

            return result

        except Exception as e:
            return UISError('get_science_technology_data', str(e)).to_dict()

    def get_culture_data(self, country_codes: List[str] = None,
                        start_year: int = None, end_year: int = None) -> Dict[str, Any]:
        """Get Cultural indicators"""
        try:
            # Key culture indicators
            culture_indicators = [
                'CULT.HRST.PT',    # Cultural employment (% of total employment)
                'CULT.GOV.EXP.ZS', # Government cultural expenditure (% of total government expenditure)
                'CULT.HER.SITE',   # World Heritage properties (number)
                'CULT.MEMB.VIS',  # Museum visits (number)
                'CULT.MEMB.INST', # Museums (number)
                'CULT.HER.SITE.PROT', # Protected natural heritage (number)
                'CULT.TV.PROD'   # TV content production (hours)
            ]

            result = self.get_indicator_data(
                indicators=culture_indicators,
                geo_units=country_codes or [],
                start_year=start_year,
                end_year=end_year,
                indicator_metadata=True
            )

            if result.get("success"):
                result["data_category"] = "culture"
                result["indicators_count"] = len(culture_indicators)

            return result

        except Exception as e:
            return UISError('get_culture_data', str(e)).to_dict()

    def export_country_dataset(self, country_code: str, format_type: str = 'excel',
                              include_metadata: bool = True) -> Dict[str, Any]:
        """Export comprehensive dataset for a single country"""
        try:
            # Get all available indicators for the country
            indicators_result = self.list_indicators(glossary_terms=True, disaggregations=True)

            if not indicators_result.get("success"):
                return indicators_result

            # Extract indicator codes (limit to prevent too large exports)
            indicators = indicators_result.get("data", [])[:500]  # Limit to 500 indicators
            indicator_codes = [ind.get("indicatorCode") for ind in indicators if ind.get("indicatorCode")]

            # Export data
            result = self.export_indicator_data(
                format_type=format_type,
                indicators=indicator_codes,
                geo_units=[country_code],
                footnotes=True,
                indicator_metadata=include_metadata
            )

            if result.get("success"):
                result["export_info"] = {
                    "country": country_code,
                    "format": format_type,
                    "indicators_count": len(indicator_codes),
                    "include_metadata": include_metadata
                }

            return result

        except Exception as e:
            return UISError('export_country_dataset', str(e)).to_dict()


def main():
    """CLI interface for UNESCO UIS Data Fetcher"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python unesco_data.py <command> <args>",
            "available_commands": [
                "indicator_data [indicators...] [geo_units...] [options]",
                "export_indicator_data <format> [indicators...] [geo_units...] [options]",
                "list_geo_units [version]",
                "export_geo_units <format> [version]",
                "list_indicators [options]",
                "export_indicators <format> [options]",
                "get_default_version",
                "list_versions",
                "education_overview [country_codes...] [start_year] [end_year]",
                "global_trends <indicator_code> [start_year] [end_year]",
                "country_comparison <indicator_code> <country_codes...> [start_year] [end_year]",
                "search_by_theme <theme> [limit]",
                "regional_data <region_id> [indicators...] [start_year] [end_year]",
                "sti_data [country_codes...] [start_year] [end_year]",
                "culture_data [country_codes...] [start_year] [end_year]",
                "export_country_dataset <country_code> <format> [include_metadata]"
            ],
            "themes": ["EDUCATION", "SCIENCE_TECHNOLOGY_INNOVATION", "CULTURE", "DEMOGRAPHIC_SOCIOECONOMIC"],
            "formats": ["csv", "excel", "json"]
        }))
        sys.exit(1)

    command = sys.argv[1]
    wrapper = UISWrapper()

    try:
        if command == "indicator_data":
            indicators = []
            geo_units = []
            geo_unit_type = None
            start_year = None
            end_year = None
            footnotes = False
            indicator_metadata = False
            version = None

            i = 2
            while i < len(sys.argv):
                arg = sys.argv[i]
                if arg.startswith('--'):
                    if arg == '--geo-unit-type' and i + 1 < len(sys.argv):
                        geo_unit_type = sys.argv[i + 1]
                        i += 1
                    elif arg == '--start-year' and i + 1 < len(sys.argv):
                        start_year = int(sys.argv[i + 1])
                        i += 1
                    elif arg == '--end-year' and i + 1 < len(sys.argv):
                        end_year = int(sys.argv[i + 1])
                        i += 1
                    elif arg == '--footnotes':
                        footnotes = True
                    elif arg == '--metadata':
                        indicator_metadata = True
                    elif arg == '--version' and i + 1 < len(sys.argv):
                        version = sys.argv[i + 1]
                        i += 1
                else:
                    # Assume it's an indicator code or geo unit code
                    if arg.startswith('ROFST') or arg.startswith('NERA') or arg.startswith('CR.') or arg.startswith('UIS.') or arg.startswith('SE.') or arg.startswith('ST.') or arg.startswith('CULT.') or arg.startswith('GERD') or arg.startswith('PAT.') or arg.startswith('HIGH.') or arg.startswith('ICT.'):
                        indicators.append(arg)
                    else:
                        geo_units.append(arg)
                i += 1

            result = wrapper.get_indicator_data(
                indicators=indicators if indicators else None,
                geo_units=geo_units if geo_units else None,
                geo_unit_type=geo_unit_type,
                start_year=start_year,
                end_year=end_year,
                footnotes=footnotes,
                indicator_metadata=indicator_metadata,
                version=version
            )
            print(json.dumps(result, indent=2))

        elif command == "export_indicator_data":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python unesco_data.py export_indicator_data <format> [options]"}))
                sys.exit(1)

            format_type = sys.argv[2]
            indicators = []
            geo_units = []
            geo_unit_type = None
            start_year = None
            end_year = None
            footnotes = False
            indicator_metadata = False
            version = None

            i = 3
            while i < len(sys.argv):
                arg = sys.argv[i]
                if arg.startswith('--'):
                    if arg == '--geo-unit-type' and i + 1 < len(sys.argv):
                        geo_unit_type = sys.argv[i + 1]
                        i += 1
                    elif arg == '--start-year' and i + 1 < len(sys.argv):
                        start_year = int(sys.argv[i + 1])
                        i += 1
                    elif arg == '--end-year' and i + 1 < len(sys.argv):
                        end_year = int(sys.argv[i + 1])
                        i += 1
                    elif arg == '--footnotes':
                        footnotes = True
                    elif arg == '--metadata':
                        indicator_metadata = True
                    elif arg == '--version' and i + 1 < len(sys.argv):
                        version = sys.argv[i + 1]
                        i += 1
                else:
                    if arg.startswith('ROFST') or arg.startswith('NERA') or arg.startswith('CR.'):
                        indicators.append(arg)
                    else:
                        geo_units.append(arg)
                i += 1

            result = wrapper.export_indicator_data(
                format_type=format_type,
                indicators=indicators if indicators else None,
                geo_units=geo_units if geo_units else None,
                geo_unit_type=geo_unit_type,
                start_year=start_year,
                end_year=end_year,
                footnotes=footnotes,
                indicator_metadata=indicator_metadata,
                version=version
            )
            print(json.dumps(result, indent=2))

        elif command == "list_geo_units":
            version = sys.argv[2] if len(sys.argv) > 2 else None
            result = wrapper.list_geo_units(version=version)
            print(json.dumps(result, indent=2))

        elif command == "export_geo_units":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python unesco_data.py export_geo_units <format> [version]"}))
                sys.exit(1)

            format_type = sys.argv[2]
            version = sys.argv[3] if len(sys.argv) > 3 else None
            result = wrapper.export_geo_units(format_type=format_type, version=version)
            print(json.dumps(result, indent=2))

        elif command == "list_indicators":
            glossary_terms = False
            disaggregations = False
            version = None

            i = 2
            while i < len(sys.argv):
                arg = sys.argv[i]
                if arg == '--glossary':
                    glossary_terms = True
                elif arg == '--disaggregations':
                    disaggregations = True
                elif arg == '--version' and i + 1 < len(sys.argv):
                    version = sys.argv[i + 1]
                    i += 1
                i += 1

            result = wrapper.list_indicators(
                glossary_terms=glossary_terms,
                disaggregations=disaggregations,
                version=version
            )
            print(json.dumps(result, indent=2))

        elif command == "export_indicators":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python unesco_data.py export_indicators <format> [options]"}))
                sys.exit(1)

            format_type = sys.argv[2]
            glossary_terms = False
            disaggregations = False
            version = None

            i = 3
            while i < len(sys.argv):
                arg = sys.argv[i]
                if arg == '--glossary':
                    glossary_terms = True
                elif arg == '--disaggregations':
                    disaggregations = True
                elif arg == '--version' and i + 1 < len(sys.argv):
                    version = sys.argv[i + 1]
                    i += 1
                i += 1

            result = wrapper.export_indicators(
                format_type=format_type,
                glossary_terms=glossary_terms,
                disaggregations=disaggregations,
                version=version
            )
            print(json.dumps(result, indent=2))

        elif command == "get_default_version":
            result = wrapper.get_default_version()
            print(json.dumps(result, indent=2))

        elif command == "list_versions":
            result = wrapper.list_versions()
            print(json.dumps(result, indent=2))

        elif command == "education_overview":
            country_codes = []
            start_year = None
            end_year = None

            i = 2
            while i < len(sys.argv):
                arg = sys.argv[i]
                if arg.startswith('--'):
                    if arg == '--start-year' and i + 1 < len(sys.argv):
                        start_year = int(sys.argv[i + 1])
                        i += 1
                    elif arg == '--end-year' and i + 1 < len(sys.argv):
                        end_year = int(sys.argv[i + 1])
                        i += 1
                else:
                    country_codes.append(arg)
                i += 1

            result = wrapper.get_education_overview(
                country_codes=country_codes if country_codes else None,
                start_year=start_year,
                end_year=end_year
            )
            print(json.dumps(result, indent=2))

        elif command == "global_trends":
            indicator_code = sys.argv[2] if len(sys.argv) > 2 else None
            start_year = None
            end_year = None

            i = 3
            while i < len(sys.argv):
                arg = sys.argv[i]
                if arg == '--start-year' and i + 1 < len(sys.argv):
                    start_year = int(sys.argv[i + 1])
                    i += 1
                elif arg == '--end-year' and i + 1 < len(sys.argv):
                    end_year = int(sys.argv[i + 1])
                    i += 1
                i += 1

            result = wrapper.get_global_education_trends(
                indicator_code=indicator_code,
                start_year=start_year,
                end_year=end_year
            )
            print(json.dumps(result, indent=2))

        elif command == "country_comparison":
            if len(sys.argv) < 4:
                print(json.dumps({"error": "Usage: python unesco_data.py country_comparison <indicator_code> <country_codes...> [options]"}))
                sys.exit(1)

            indicator_code = sys.argv[2]
            country_codes = sys.argv[3:] if len(sys.argv) > 3 else []
            start_year = None
            end_year = None

            i = 4
            while i < len(sys.argv):
                arg = sys.argv[i]
                if arg == '--start-year' and i + 1 < len(sys.argv):
                    start_year = int(sys.argv[i + 1])
                    i += 1
                elif arg == '--end-year' and i + 1 < len(sys.argv):
                    end_year = int(sys.argv[i + 1])
                    i += 1
                i += 1

            result = wrapper.get_country_comparison(
                indicator_code=indicator_code,
                country_codes=country_codes,
                start_year=start_year,
                end_year=end_year
            )
            print(json.dumps(result, indent=2))

        elif command == "search_by_theme":
            theme = sys.argv[2] if len(sys.argv) > 2 else None
            limit = int(sys.argv[3]) if len(sys.argv) > 3 else 50

            if not theme:
                print(json.dumps({"error": "Theme is required. Available themes: EDUCATION, SCIENCE_TECHNOLOGY_INNOVATION, CULTURE, DEMOGRAPHIC_SOCIOECONOMIC"}))
                sys.exit(1)

            result = wrapper.search_indicators_by_theme(theme=theme, limit=limit)
            print(json.dumps(result, indent=2))

        elif command == "regional_data":
            region_id = sys.argv[2] if len(sys.argv) > 2 else None
            indicator_codes = []
            start_year = None
            end_year = None

            i = 3
            while i < len(sys.argv):
                arg = sys.argv[i]
                if arg.startswith('--'):
                    if arg == '--start-year' and i + 1 < len(sys.argv):
                        start_year = int(sys.argv[i + 1])
                        i += 1
                    elif arg == '--end-year' and i + 1 < len(sys.argv):
                        end_year = int(sys.argv[i + 1])
                        i += 1
                else:
                    if arg.startswith('ROFST') or arg.startswith('NERA') or arg.startswith('CR.'):
                        indicator_codes.append(arg)
                i += 1

            if not region_id:
                print(json.dumps({"error": "Region ID is required"}))
                sys.exit(1)

            result = wrapper.get_regional_education_data(
                region_id=region_id,
                indicator_codes=indicator_codes if indicator_codes else None,
                start_year=start_year,
                end_year=end_year
            )
            print(json.dumps(result, indent=2))

        elif command == "sti_data":
            country_codes = []
            start_year = None
            end_year = None

            i = 2
            while i < len(sys.argv):
                arg = sys.argv[i]
                if arg.startswith('--'):
                    if arg == '--start-year' and i + 1 < len(sys.argv):
                        start_year = int(sys.argv[i + 1])
                        i += 1
                    elif arg == '--end-year' and i + 1 < len(sys.argv):
                        end_year = int(sys.argv[i + 1])
                        i += 1
                else:
                    country_codes.append(arg)
                i += 1

            result = wrapper.get_science_technology_data(
                country_codes=country_codes if country_codes else None,
                start_year=start_year,
                end_year=end_year
            )
            print(json.dumps(result, indent=2))

        elif command == "culture_data":
            country_codes = []
            start_year = None
            end_year = None

            i = 2
            while i < len(sys.argv):
                arg = sys.argv[i]
                if arg.startswith('--'):
                    if arg == '--start-year' and i + 1 < len(sys.argv):
                        start_year = int(sys.argv[i + 1])
                        i += 1
                    elif arg == '--end-year' and i + 1 < len(sys.argv):
                        end_year = int(sys.argv[i + 1])
                        i += 1
                else:
                    country_codes.append(arg)
                i += 1

            result = wrapper.get_culture_data(
                country_codes=country_codes if country_codes else None,
                start_year=start_year,
                end_year=end_year
            )
            print(json.dumps(result, indent=2))

        elif command == "export_country_dataset":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python unesco_data.py export_country_dataset <country_code> <format> [include_metadata]"}))
                sys.exit(1)

            country_code = sys.argv[2]
            format_type = sys.argv[3]
            include_metadata = '--metadata' in sys.argv[4:] if len(sys.argv) > 4 else False

            result = wrapper.export_country_dataset(
                country_code=country_code,
                format_type=format_type,
                include_metadata=include_metadata
            )
            print(json.dumps(result, indent=2))

        else:
            print(json.dumps({
                "error": f"Unknown command: {command}",
                "available_commands": [
                    "indicator_data [indicators...] [geo_units...] [options]",
                    "export_indicator_data <format> [indicators...] [geo_units...] [options]",
                    "list_geo_units [version]",
                    "export_geo_units <format> [version]",
                    "list_indicators [options]",
                    "export_indicators <format> [options]",
                    "get_default_version",
                    "list_versions",
                    "education_overview [country_codes...] [start_year] [end_year]",
                    "global_trends <indicator_code> [start_year] [end_year]",
                    "country_comparison <indicator_code> <country_codes...> [options]",
                    "search_by_theme <theme> [limit]",
                    "regional_data <region_id> [indicators...] [start_year] [end_year]",
                    "sti_data [country_codes...] [start_year] [end_year]",
                    "culture_data [country_codes...] [start_year] [end_year]",
                    "export_country_dataset <country_code> <format> [include_metadata]"
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