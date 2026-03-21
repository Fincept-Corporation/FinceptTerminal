"""
OSCAR (Observing Systems Capability Analysis and Review Tool) Data Fetcher
Comprehensive wrapper for WMO OSCAR API providing access to satellite instruments,
satellites, and Earth observation variables data

API Documentation:
- Base URL: https://space.oscar.wmo.int/api/v1
- Authentication: No API key required (public API)
- Rate limits: None specified, but be reasonable with requests
- License: WMO data (public meteorological data)

Available Data Categories:
- Instruments: Satellite instruments and sensors with technical specifications
- Satellites: Satellite platforms and their operational details
- Variables: Earth observation variables and measurements

Data Coverage:
- 1200+ instruments across various categories
- Satellite platforms from multiple space agencies
- Atmospheric, oceanic, and terrestrial observation variables
- Historical and planned satellite missions
"""

import sys
import json
import os
import requests
from datetime import datetime, timedelta
from typing import Dict, Any, Optional, List, Union
from urllib.parse import urlencode


class OSCARError:
    """Error handling wrapper for OSCAR API responses"""
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


class OSCARWrapper:
    """Comprehensive OSCAR API wrapper with fault tolerance"""

    def __init__(self):
        self.base_url = "https://space.oscar.wmo.int/api/v1"
        self.session = requests.Session()
        self.session.headers.update({
            'User-Agent': 'Fincept-Terminal/1.0',
            'Accept': 'application/json',
            'Accept-Encoding': 'gzip'
        })

    def _make_request(self, endpoint: str, params: Dict[str, Any]) -> Dict[str, Any]:
        """Centralized request handler with comprehensive error handling"""
        try:
            url = f"{self.base_url}{endpoint}"

            # Remove None values from params
            clean_params = {k: v for k, v in params.items() if v is not None}

            # Handle filterValue serialization for complex objects
            if 'filterValue' in clean_params and isinstance(clean_params['filterValue'], dict):
                clean_params['filterValue'] = json.dumps(clean_params['filterValue'])

            response = self.session.get(url, params=clean_params, timeout=60)
            response.raise_for_status()

            # Handle JSON response
            try:
                data = response.json()

                # Check for API errors in response
                if 'error' in data:
                    error_msg = data.get('message', 'Unknown API error')
                    return OSCARError(endpoint, error_msg, response.status_code).to_dict()

                return {
                    "success": True,
                    "endpoint": endpoint,
                    "data": data,
                    "parameters": clean_params,
                    "timestamp": int(datetime.now().timestamp())
                }

            except json.JSONDecodeError as e:
                return OSCARError(endpoint, f"JSON decode error: {str(e)}", response.status_code).to_dict()

        except requests.exceptions.RequestException as e:
            return OSCARError(endpoint, f"Network error: {str(e)}").to_dict()
        except Exception as e:
            return OSCARError(endpoint, f"Unexpected error: {str(e)}").to_dict()

    # ==================== INSTRUMENTS ENDPOINTS ====================

    def get_instruments(self, page: int = 1, sort: str = "id", order: str = "asc",
                       filter_on: str = None, filter_value: Union[str, Dict] = None) -> Dict[str, Any]:
        """Fetch list of satellite instruments with pagination and filtering"""
        try:
            params = {
                'page': page,
                'sort': sort,
                'order': order
            }

            if filter_on:
                params['filterOn'] = filter_on
            if filter_value:
                params['filterValue'] = filter_value

            result = self._make_request('/instruments', params)
            return result

        except Exception as e:
            return OSCARError('get_instruments', str(e)).to_dict()

    def get_instrument_by_slug(self, slug: str) -> Dict[str, Any]:
        """Get detailed information for a specific instrument by slug"""
        try:
            if not slug:
                return OSCARError('get_instrument_by_slug', 'Instrument slug is required').to_dict()

            result = self._make_request(f'/instruments/{slug}', {})
            return result

        except Exception as e:
            return OSCARError('get_instrument_by_slug', str(e)).to_dict()

    def search_instruments_by_type(self, instrument_type: str, page: int = 1) -> Dict[str, Any]:
        """Search instruments by type (e.g., 'Moderate-resolution optical imager')"""
        try:
            if not instrument_type:
                return OSCARError('search_instruments_by_type', 'Instrument type is required').to_dict()

            result = self.get_instruments(
                page=page,
                filter_on="instrumenttype",
                filter_value=instrument_type
            )

            if result.get("success"):
                result["search_type"] = "instrument_type"
                result["search_value"] = instrument_type

            return result

        except Exception as e:
            return OSCARError('search_instruments_by_type', str(e)).to_dict()

    def search_instruments_by_agency(self, agency: str, page: int = 1) -> Dict[str, Any]:
        """Search instruments by providing agency (e.g., 'NASA', 'ESA', 'NOAA')"""
        try:
            if not agency:
                return OSCARError('search_instruments_by_agency', 'Agency is required').to_dict()

            result = self.get_instruments(
                page=page,
                filter_on="providing_agency",
                filter_value=agency
            )

            if result.get("success"):
                result["search_type"] = "agency"
                result["search_value"] = agency

            return result

        except Exception as e:
            return OSCARError('search_instruments_by_agency', str(e)).to_dict()

    def search_instruments_by_year_range(self, start_year: int, end_year: int, page: int = 1) -> Dict[str, Any]:
        """Search instruments by operational year range"""
        try:
            if not start_year or not end_year:
                return OSCARError('search_instruments_by_year_range', 'Both start_year and end_year are required').to_dict()

            filter_value = {"min": start_year, "max": end_year}

            result = self.get_instruments(
                page=page,
                filter_on="year",
                filter_value=filter_value
            )

            if result.get("success"):
                result["search_type"] = "year_range"
                result["year_range"] = {"start": start_year, "end": end_year}

            return result

        except Exception as e:
            return OSCARError('search_instruments_by_year_range', str(e)).to_dict()

    def get_instrument_classifications(self) -> Dict[str, Any]:
        """Get unique instrument classifications from available instruments"""
        try:
            # Get first page to extract classifications
            result = self.get_instruments(page=1, sort="id", order="asc")

            if not result.get("success"):
                return result

            instruments = result.get("data", {}).get("_embedded", {}).get("instruments", [])
            classifications = set()

            for instrument in instruments:
                if "classification" in instrument:
                    for classification in instrument["classification"]:
                        classifications.add(classification)

            return {
                "success": True,
                "endpoint": "get_instrument_classifications",
                "data": sorted(list(classifications)),
                "total_classifications": len(classifications),
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return OSCARError('get_instrument_classifications', str(e)).to_dict()

    # ==================== SATELLITES ENDPOINTS ====================

    def get_satellites(self, page: int = 1, sort: str = "id", order: str = "asc",
                      filter_on: str = None, filter_value: Union[str, Dict] = None) -> Dict[str, Any]:
        """Fetch list of satellites with pagination and filtering"""
        try:
            params = {
                'page': page,
                'sort': sort,
                'order': order
            }

            if filter_on:
                params['filterOn'] = filter_on
            if filter_value:
                params['filterValue'] = filter_value

            result = self._make_request('/satellites', params)
            return result

        except Exception as e:
            return OSCARError('get_satellites', str(e)).to_dict()

    def get_satellite_by_slug(self, slug: str) -> Dict[str, Any]:
        """Get detailed information for a specific satellite by slug"""
        try:
            if not slug:
                return OSCARError('get_satellite_by_slug', 'Satellite slug is required').to_dict()

            result = self._make_request(f'/satellites/{slug}', {})
            return result

        except Exception as e:
            return OSCARError('get_satellite_by_slug', str(e)).to_dict()

    def search_satellites_by_orbit(self, orbit_type: str, page: int = 1) -> Dict[str, Any]:
        """Search satellites by orbit type (e.g., 'Sunsynchronous orbit')"""
        try:
            if not orbit_type:
                return OSCARError('search_satellites_by_orbit', 'Orbit type is required').to_dict()

            result = self.get_satellites(
                page=page,
                filter_on="orbit",
                filter_value=orbit_type
            )

            if result.get("success"):
                result["search_type"] = "orbit_type"
                result["search_value"] = orbit_type

            return result

        except Exception as e:
            return OSCARError('search_satellites_by_orbit', str(e)).to_dict()

    def search_satellites_by_agency(self, agency: str, page: int = 1) -> Dict[str, Any]:
        """Search satellites by agency"""
        try:
            if not agency:
                return OSCARError('search_satellites_by_agency', 'Agency is required').to_dict()

            result = self.get_satellites(
                page=page,
                filter_on="agency",
                filter_value=agency
            )

            if result.get("success"):
                result["search_type"] = "agency"
                result["search_value"] = agency

            return result

        except Exception as e:
            return OSCARError('search_satellites_by_agency', str(e)).to_dict()

    # ==================== VARIABLES ENDPOINTS ====================

    def get_variables(self, page: int = 1, sort: str = "id", order: str = "asc",
                     filter_on: str = None, filter_value: Union[str, Dict] = None) -> Dict[str, Any]:
        """Fetch list of Earth observation variables with pagination and filtering"""
        try:
            params = {
                'page': page,
                'sort': sort,
                'order': order
            }

            if filter_on:
                params['filterOn'] = filter_on
            if filter_value:
                params['filterValue'] = filter_value

            result = self._make_request('/variables', params)
            return result

        except Exception as e:
            return OSCARError('get_variables', str(e)).to_dict()

    def get_variable_by_slug(self, slug: str) -> Dict[str, Any]:
        """Get detailed information for a specific variable by slug"""
        try:
            if not slug:
                return OSCARError('get_variable_by_slug', 'Variable slug is required').to_dict()

            result = self._make_request(f'/variables/{slug}', {})
            return result

        except Exception as e:
            return OSCARError('get_variable_by_slug', str(e)).to_dict()

    def search_variables_by_instrument(self, instrument: str, page: int = 1) -> Dict[str, Any]:
        """Search variables measured by specific instrument"""
        try:
            if not instrument:
                return OSCARError('search_variables_by_instrument', 'Instrument is required').to_dict()

            result = self.get_variables(
                page=page,
                filter_on="instrument",
                filter_value=instrument
            )

            if result.get("success"):
                result["search_type"] = "instrument"
                result["search_value"] = instrument

            return result

        except Exception as e:
            return OSCARError('search_variables_by_instrument', str(e)).to_dict()

    def get_ecv_variables(self, page: int = 1) -> Dict[str, Any]:
        """Get Essential Climate Variables (ECV) from the variables list"""
        try:
            # Get first page to check for ECV variables
            result = self.get_variables(page=page, sort="id", order="asc")

            if not result.get("success"):
                return result

            variables = result.get("data", {}).get("_embedded", {}).get("variables", [])
            ecv_variables = []

            for variable in variables:
                if variable.get("ecv_variable"):
                    ecv_variables.append(variable)

            return {
                "success": True,
                "endpoint": "get_ecv_variables",
                "data": ecv_variables,
                "total_ecv_variables": len(ecv_variables),
                "page": page,
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return OSCARError('get_ecv_variables', str(e)).to_dict()

    # ==================== CONVENIENCE METHODS ====================

    def get_space_agencies(self) -> Dict[str, Any]:
        """Get list of space agencies from instruments data"""
        try:
            # Get instruments to extract agencies
            result = self.get_instruments(page=1, sort="id", order="asc")

            if not result.get("success"):
                return result

            instruments = result.get("data", {}).get("_embedded", {}).get("instruments", [])
            agencies = set()

            for instrument in instruments:
                if "providing-agency" in instrument:
                    agencies.add(instrument["providing-agency"])

            return {
                "success": True,
                "endpoint": "get_space_agencies",
                "data": sorted(list(agencies)),
                "total_agencies": len(agencies),
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return OSCARError('get_space_agencies', str(e)).to_dict()

    def get_instrument_types(self) -> Dict[str, Any]:
        """Get list of instrument types"""
        try:
            # Get instruments to extract types
            result = self.get_instruments(page=1, sort="id", order="asc")

            if not result.get("success"):
                return result

            instruments = result.get("data", {}).get("_embedded", {}).get("instruments", [])
            types = set()

            for instrument in instruments:
                if "instrumenttype" in instrument:
                    types.add(instrument["instrumenttype"])

            return {
                "success": True,
                "endpoint": "get_instrument_types",
                "data": sorted(list(types)),
                "total_types": len(types),
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return OSCARError('get_instrument_types', str(e)).to_dict()

    def search_weather_instruments(self, page: int = 1) -> Dict[str, Any]:
        """Get instruments commonly used for weather monitoring"""
        try:
            # Weather-related instrument types
            weather_types = [
                "Moderate-resolution optical imager",
                "Cross-nadir infrared sounder, possibly including VIS channels",
                "Microwave cross-track scanning radiometer",
                "Microwave conical scanning radiometer",
                "Radar altimeter",
                "Scatterometer"
            ]

            all_weather_instruments = []

            for instrument_type in weather_types:
                result = self.search_instruments_by_type(instrument_type, page=1)
                if result.get("success"):
                    instruments = result.get("data", {}).get("_embedded", {}).get("instruments", [])
                    all_weather_instruments.extend(instruments)

            return {
                "success": True,
                "endpoint": "search_weather_instruments",
                "data": all_weather_instruments,
                "total_weather_instruments": len(all_weather_instruments),
                "searched_types": weather_types,
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return OSCARError('search_weather_instruments', str(e)).to_dict()

    def get_climate_monitoring_instruments(self, page: int = 1) -> Dict[str, Any]:
        """Get instruments important for climate monitoring"""
        try:
            # Get ECV variables first
            ecv_result = self.get_ecv_variables(page=1)

            if not ecv_result.get("success"):
                return ecv_result

            # Extract unique instruments from ECV variables
            climate_instruments = []
            processed_slugs = set()

            # This is a simplified approach - in practice you might need to query
            # instruments that measure ECV variables more specifically
            result = self.get_instruments(page=page, sort="id", order="asc")

            if result.get("success"):
                instruments = result.get("data", {}).get("_embedded", {}).get("instruments", [])

                for instrument in instruments:
                    # Include instruments commonly used for climate monitoring
                    if any(keyword in instrument.get("instrumenttype", "").lower()
                          for keyword in ["radiometer", "spectrometer", "sounder", "altimeter"]):
                        climate_instruments.append(instrument)

            return {
                "success": True,
                "endpoint": "get_climate_monitoring_instruments",
                "data": climate_instruments,
                "total_climate_instruments": len(climate_instruments),
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return OSCARError('get_climate_monitoring_instruments', str(e)).to_dict()

    def get_overview_statistics(self) -> Dict[str, Any]:
        """Get overview statistics of the OSCAR database"""
        try:
            # Get basic counts from each category
            instruments_result = self.get_instruments(page=1)
            satellites_result = self.get_satellites(page=1)
            variables_result = self.get_variables(page=1)

            total_instruments = 0
            total_satellites = 0
            total_variables = 0

            if instruments_result.get("success"):
                page_info = instruments_result.get("data", {}).get("page", {})
                total_instruments = page_info.get("totalElements", 0)

            if satellites_result.get("success"):
                page_info = satellites_result.get("data", {}).get("page", {})
                total_satellites = page_info.get("totalElements", 0)

            if variables_result.get("success"):
                page_info = variables_result.get("data", {}).get("page", {})
                total_variables = page_info.get("totalElements", 0)

            return {
                "success": True,
                "endpoint": "get_overview_statistics",
                "data": {
                    "total_instruments": total_instruments,
                    "total_satellites": total_satellites,
                    "total_variables": total_variables,
                    "database_summary": "WMO OSCAR space-based observing systems database"
                },
                "timestamp": int(datetime.now().timestamp())
            }

        except Exception as e:
            return OSCARError('get_overview_statistics', str(e)).to_dict()


def main():
    """CLI interface for OSCAR Data Fetcher"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python oscar_data.py <command> <args>",
            "available_commands": [
                "instruments [page] [options]",
                "instrument <slug>",
                "search_instruments_type <type> [page]",
                "search_instruments_agency <agency> [page]",
                "search_instruments_year <start_year> <end_year> [page]",
                "instrument_classifications",
                "satellites [page] [options]",
                "satellite <slug>",
                "search_satellites_orbit <orbit> [page]",
                "search_satellites_agency <agency> [page]",
                "variables [page] [options]",
                "variable <slug>",
                "search_variables_instrument <instrument> [page]",
                "ecv_variables [page]",
                "space_agencies",
                "instrument_types",
                "weather_instruments [page]",
                "climate_instruments [page]",
                "overview_statistics"
            ]
        }))
        sys.exit(1)

    command = sys.argv[1]
    wrapper = OSCARWrapper()

    try:
        if command == "instruments":
            page = int(sys.argv[2]) if len(sys.argv) > 2 else 1
            sort = sys.argv[3] if len(sys.argv) > 3 else "id"
            order = sys.argv[4] if len(sys.argv) > 4 else "asc"
            filter_on = None
            filter_value = None

            # Parse additional options
            i = 5
            while i < len(sys.argv):
                arg = sys.argv[i]
                if arg == '--filter-on' and i + 1 < len(sys.argv):
                    filter_on = sys.argv[i + 1]
                    i += 1
                elif arg == '--filter-value' and i + 1 < len(sys.argv):
                    filter_value = sys.argv[i + 1]
                    i += 1
                i += 1

            result = wrapper.get_instruments(
                page=page,
                sort=sort,
                order=order,
                filter_on=filter_on,
                filter_value=filter_value
            )
            print(json.dumps(result, indent=2))

        elif command == "instrument":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python oscar_data.py instrument <slug>"}))
                sys.exit(1)

            slug = sys.argv[2]
            result = wrapper.get_instrument_by_slug(slug)
            print(json.dumps(result, indent=2))

        elif command == "search_instruments_type":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python oscar_data.py search_instruments_type <type> [page]"}))
                sys.exit(1)

            instrument_type = sys.argv[2]
            page = int(sys.argv[3]) if len(sys.argv) > 3 else 1
            result = wrapper.search_instruments_by_type(instrument_type, page)
            print(json.dumps(result, indent=2))

        elif command == "search_instruments_agency":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python oscar_data.py search_instruments_agency <agency> [page]"}))
                sys.exit(1)

            agency = sys.argv[2]
            page = int(sys.argv[3]) if len(sys.argv) > 3 else 1
            result = wrapper.search_instruments_by_agency(agency, page)
            print(json.dumps(result, indent=2))

        elif command == "search_instruments_year":
            if len(sys.argv) < 4:
                print(json.dumps({"error": "Usage: python oscar_data.py search_instruments_year <start_year> <end_year> [page]"}))
                sys.exit(1)

            start_year = int(sys.argv[2])
            end_year = int(sys.argv[3])
            page = int(sys.argv[4]) if len(sys.argv) > 4 else 1
            result = wrapper.search_instruments_by_year_range(start_year, end_year, page)
            print(json.dumps(result, indent=2))

        elif command == "instrument_classifications":
            result = wrapper.get_instrument_classifications()
            print(json.dumps(result, indent=2))

        elif command == "satellites":
            page = int(sys.argv[2]) if len(sys.argv) > 2 else 1
            sort = sys.argv[3] if len(sys.argv) > 3 else "id"
            order = sys.argv[4] if len(sys.argv) > 4 else "asc"
            filter_on = None
            filter_value = None

            # Parse additional options
            i = 5
            while i < len(sys.argv):
                arg = sys.argv[i]
                if arg == '--filter-on' and i + 1 < len(sys.argv):
                    filter_on = sys.argv[i + 1]
                    i += 1
                elif arg == '--filter-value' and i + 1 < len(sys.argv):
                    filter_value = sys.argv[i + 1]
                    i += 1
                i += 1

            result = wrapper.get_satellites(
                page=page,
                sort=sort,
                order=order,
                filter_on=filter_on,
                filter_value=filter_value
            )
            print(json.dumps(result, indent=2))

        elif command == "satellite":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python oscar_data.py satellite <slug>"}))
                sys.exit(1)

            slug = sys.argv[2]
            result = wrapper.get_satellite_by_slug(slug)
            print(json.dumps(result, indent=2))

        elif command == "search_satellites_orbit":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python oscar_data.py search_satellites_orbit <orbit> [page]"}))
                sys.exit(1)

            orbit_type = sys.argv[2]
            page = int(sys.argv[3]) if len(sys.argv) > 3 else 1
            result = wrapper.search_satellites_by_orbit(orbit_type, page)
            print(json.dumps(result, indent=2))

        elif command == "search_satellites_agency":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python oscar_data.py search_satellites_agency <agency> [page]"}))
                sys.exit(1)

            agency = sys.argv[2]
            page = int(sys.argv[3]) if len(sys.argv) > 3 else 1
            result = wrapper.search_satellites_by_agency(agency, page)
            print(json.dumps(result, indent=2))

        elif command == "variables":
            page = int(sys.argv[2]) if len(sys.argv) > 2 else 1
            sort = sys.argv[3] if len(sys.argv) > 3 else "id"
            order = sys.argv[4] if len(sys.argv) > 4 else "asc"
            filter_on = None
            filter_value = None

            # Parse additional options
            i = 5
            while i < len(sys.argv):
                arg = sys.argv[i]
                if arg == '--filter-on' and i + 1 < len(sys.argv):
                    filter_on = sys.argv[i + 1]
                    i += 1
                elif arg == '--filter-value' and i + 1 < len(sys.argv):
                    filter_value = sys.argv[i + 1]
                    i += 1
                i += 1

            result = wrapper.get_variables(
                page=page,
                sort=sort,
                order=order,
                filter_on=filter_on,
                filter_value=filter_value
            )
            print(json.dumps(result, indent=2))

        elif command == "variable":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python oscar_data.py variable <slug>"}))
                sys.exit(1)

            slug = sys.argv[2]
            result = wrapper.get_variable_by_slug(slug)
            print(json.dumps(result, indent=2))

        elif command == "search_variables_instrument":
            if len(sys.argv) < 3:
                print(json.dumps({"error": "Usage: python oscar_data.py search_variables_instrument <instrument> [page]"}))
                sys.exit(1)

            instrument = sys.argv[2]
            page = int(sys.argv[3]) if len(sys.argv) > 3 else 1
            result = wrapper.search_variables_by_instrument(instrument, page)
            print(json.dumps(result, indent=2))

        elif command == "ecv_variables":
            page = int(sys.argv[2]) if len(sys.argv) > 2 else 1
            result = wrapper.get_ecv_variables(page)
            print(json.dumps(result, indent=2))

        elif command == "space_agencies":
            result = wrapper.get_space_agencies()
            print(json.dumps(result, indent=2))

        elif command == "instrument_types":
            result = wrapper.get_instrument_types()
            print(json.dumps(result, indent=2))

        elif command == "weather_instruments":
            page = int(sys.argv[2]) if len(sys.argv) > 2 else 1
            result = wrapper.search_weather_instruments(page)
            print(json.dumps(result, indent=2))

        elif command == "climate_instruments":
            page = int(sys.argv[2]) if len(sys.argv) > 2 else 1
            result = wrapper.get_climate_monitoring_instruments(page)
            print(json.dumps(result, indent=2))

        elif command == "overview_statistics":
            result = wrapper.get_overview_statistics()
            print(json.dumps(result, indent=2))

        else:
            print(json.dumps({
                "error": f"Unknown command: {command}",
                "available_commands": [
                    "instruments [page] [options]",
                    "instrument <slug>",
                    "search_instruments_type <type> [page]",
                    "search_instruments_agency <agency> [page]",
                    "search_instruments_year <start_year> <end_year> [page]",
                    "instrument_classifications",
                    "satellites [page] [options]",
                    "satellite <slug>",
                    "search_satellites_orbit <orbit> [page]",
                    "search_satellites_agency <agency> [page]",
                    "variables [page] [options]",
                    "variable <slug>",
                    "search_variables_instrument <instrument> [page]",
                    "ecv_variables [page]",
                    "space_agencies",
                    "instrument_types",
                    "weather_instruments [page]",
                    "climate_instruments [page]",
                    "overview_statistics"
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