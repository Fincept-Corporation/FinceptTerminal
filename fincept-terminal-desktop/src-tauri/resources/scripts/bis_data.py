"""
BIS (Bank for International Settlements) SDMX API wrapper
Provides access to global economic and financial statistics data from BIS

API Documentation: https://www.bis.org/statistics/about_bis_stats_data_services.htm
Base URL: https://stats.bis.org/api/v1

Key Features:
- No API key required (public data)
- SDMX 2.1 format support
- Multiple response formats (XML, JSON, CSV)
- Comprehensive time series data
- Global economic and financial statistics
"""

import asyncio
import json
import sys
import re
import xml.etree.ElementTree as ET
from datetime import datetime, timedelta
from typing import Dict, Any, Optional, List, Union
from urllib.parse import urlencode, quote
import aiohttp
import html


class BISError(Exception):
    """Custom exception for BIS API errors"""
    def __init__(self, message: str, status_code: Optional[int] = None, endpoint: Optional[str] = None):
        super().__init__(message)
        self.status_code = status_code
        self.endpoint = endpoint


class BISAPI:
    """BIS SDMX API client with comprehensive coverage of all endpoints"""

    def __init__(self, base_url: str = "https://stats.bis.org/api/v1", timeout: int = 30):
        self.base_url = base_url.rstrip('/')
        self.timeout = aiohttp.ClientTimeout(total=timeout)
        self.session = None

        # Common BIS data flows (statistical domains)
        self.known_flows = {
            "WS_EER": "Effective exchange rates",
            "WS_CBPOL": "Central bank policy rates",
            "WS_DT1": "Debt securities",
            "WS_LTINT": "Long-term interest rates",
            "WS_STINT": "Short-term interest rates",
            "WS_MON": "Monetary aggregates",
            "WS_XRU": "Exchange rates",
            "WS_CRD": "Credit to the non-financial sector",
            "WS_HP": "House prices",
            "WS_REER": "Real effective exchange rates",
            "WS_CUST": "Customs and exchange controls",
            "WS_FDI": "Foreign direct investment",
            "WS_CUR": "Currency composition of official foreign exchange reserves"
        }

        # Random user agents for requests
        self.user_agents = [
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
            "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36",
            "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36"
        ]

    async def __aenter__(self):
        self.session = aiohttp.ClientSession(timeout=self.timeout)
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        if self.session:
            await self.session.close()

    def _get_headers(self, endpoint_type: str = "data") -> Dict[str, str]:
        """Get request headers with random user agent"""
        import random

        # Set appropriate Accept header based on endpoint type
        if endpoint_type == "structure":
            accept_header = "application/vnd.sdmx.structure+json;version=1.0.0,application/vnd.sdmx.structure+xml;version=2.1,application/xml"
        else:
            accept_header = "application/vnd.sdmx.data+json;version=1.0.0,application/vnd.sdmx.genericdata+xml;version=2.1,application/xml"

        return {
            "User-Agent": random.choice(self.user_agents),
            "Accept": accept_header,
            "Accept-Encoding": "gzip, deflate"
        }

    async def _make_request(self, endpoint: str, params: Optional[Dict[str, Any]] = None, endpoint_type: str = "data") -> Dict[str, Any]:
        """Make HTTP request with error handling"""
        if not self.session:
            raise BISError("Session not initialized. Use async with BISAPI()...")

        url = f"{self.base_url}/{endpoint}"
        headers = self._get_headers(endpoint_type)

        try:
            async with self.session.get(url, params=params, headers=headers) as response:
                if response.status == 200:
                    content_type = response.headers.get('content-type', '')

                    if 'application/json' in content_type or 'application/vnd.sdmx.data+json' in content_type:
                        data = await response.json()
                        return self._format_response(data, endpoint)
                    elif 'application/xml' in content_type or 'text/xml' in content_type:
                        text_data = await response.text()
                        return self._parse_xml_response(text_data, endpoint)
                    elif 'text/csv' in content_type:
                        text_data = await response.text()
                        return self._parse_csv_response(text_data, endpoint)
                    else:
                        # Try to parse as JSON first, then as text
                        try:
                            data = await response.json()
                            return self._format_response(data, endpoint)
                        except:
                            text_data = await response.text()
                            return {
                                "success": True,
                                "data": text_data,
                                "content_type": content_type,
                                "endpoint": endpoint,
                                "params": params
                            }
                else:
                    error_text = await response.text()
                    raise BISError(f"HTTP {response.status}: {error_text}", response.status, endpoint)

        except asyncio.TimeoutError:
            raise BISError(f"Request timeout after {self.timeout.total} seconds", None, endpoint)
        except aiohttp.ClientError as e:
            raise BISError(f"Network error: {str(e)}", None, endpoint)
        except Exception as e:
            raise BISError(f"Unexpected error: {str(e)}", None, endpoint)

    def _format_response(self, data: Dict[str, Any], endpoint: str) -> Dict[str, Any]:
        """Format API response with metadata"""
        return {
            "success": True,
            "data": data,
            "endpoint": endpoint,
            "timestamp": datetime.now().isoformat()
        }

    def _parse_xml_response(self, xml_data: str, endpoint: str) -> Dict[str, Any]:
        """Parse XML SDMX response"""
        try:
            root = ET.fromstring(xml_data)

            # Extract basic information
            result = {
                "success": True,
                "data": {
                    "xml_raw": xml_data,
                    "root_tag": root.tag,
                    "namespaces": dict(root.attrib.items()) if hasattr(root, 'attrib') else {}
                },
                "endpoint": endpoint,
                "format": "xml"
            }

            # Try to extract series data if present
            series = root.findall('.//{http://www.sdmx.org/resources/sdmxml/schemas/v2_1/data/generic}Series')
            if series:
                result["data"]["series_count"] = len(series)
                series_data = []
                for series_elem in series[:5]:  # Limit to first 5 for preview
                    series_info = {}
                    for child in series_elem:
                        if child.tag.endswith('SeriesKey'):
                            series_info["key"] = {v.attrib.get('id'): v.text for v in child}
                        elif child.tag.endswith('Obs'):
                            obs_info = {v.attrib.get('id'): v.text for v in child}
                            series_info.setdefault("observations", []).append(obs_info)
                    series_data.append(series_info)
                result["data"]["series_preview"] = series_data

            return result

        except ET.ParseError as e:
            return {
                "success": True,
                "data": {"xml_raw": xml_data, "parse_error": str(e)},
                "endpoint": endpoint,
                "format": "xml"
            }

    def _parse_csv_response(self, csv_data: str, endpoint: str) -> Dict[str, Any]:
        """Parse CSV response"""
        lines = csv_data.strip().split('\n')
        return {
            "success": True,
            "data": {
                "csv_raw": csv_data,
                "lines_count": len(lines),
                "header": lines[0] if lines else "",
                "preview": lines[:10]  # First 10 lines as preview
            },
            "endpoint": endpoint,
            "format": "csv"
        }

    # DATA QUERY ENDPOINTS

    async def get_data(
        self,
        flow: str,
        key: str = "all",
        start_period: Optional[str] = None,
        end_period: Optional[str] = None,
        first_n_observations: Optional[int] = None,
        last_n_observations: Optional[int] = None,
        detail: str = "full"
    ) -> Dict[str, Any]:
        """
        Get statistical data for a specific flow and key combination

        Args:
            flow: Statistical domain (e.g., 'WS_EER', 'BIS,WS_EER,1.0')
            key: Data key identifying series (use 'all' for all data)
            start_period: Start period (ISO 8601 or SDMX format)
            end_period: End period (ISO 8601 or SDMX format)
            first_n_observations: Return first N observations from oldest
            last_n_observations: Return last N observations from newest
            detail: Detail level ('full', 'dataonly', 'serieskeysonly', 'nodata')
        """
        params = {}
        if start_period:
            params["startPeriod"] = start_period
        if end_period:
            params["endPeriod"] = end_period
        if first_n_observations:
            params["firstNObservations"] = first_n_observations
        if last_n_observations:
            params["lastNObservations"] = last_n_observations
        if detail != "full":
            params["detail"] = detail

        # URL encode the flow and key properly
        encoded_flow = quote(flow, safe='')
        encoded_key = quote(key, safe='')

        endpoint = f"data/{encoded_flow}/{encoded_key}/all"
        return await self._make_request(endpoint, params)

    async def get_available_constraints(
        self,
        flow: str,
        key: str,
        component_id: str,
        mode: str = "exact",
        references: str = "none",
        start_period: Optional[str] = None,
        end_period: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Get information about data availability for specific constraints

        Args:
            flow: Statistical domain
            key: Data key
            component_id: Dimension ID for availability info
            mode: 'exact' for current selection, 'available' for remaining valid options
            references: References to include ('none', 'all', etc.)
            start_period: Start period filter
            end_period: End period filter
        """
        params = {"mode": mode, "references": references}
        if start_period:
            params["startPeriod"] = start_period
        if end_period:
            params["endPeriod"] = end_period

        encoded_flow = quote(flow, safe='')
        encoded_key = quote(key, safe='')

        endpoint = f"availableconstraint/{encoded_flow}/{encoded_key}/all/{component_id}"
        return await self._make_request(endpoint, params, "structure")

    # STRUCTURE QUERY ENDPOINTS

    async def get_data_structures(
        self,
        agency_id: str = "all",
        resource_id: str = "all",
        version: str = "all",
        references: str = "none",
        detail: str = "full"
    ) -> Dict[str, Any]:
        """
        Get data structure definitions

        Args:
            agency_id: Maintenance agency (e.g., 'BIS', 'all')
            resource_id: Resource ID or 'all'
            version: Version or 'latest' or 'all'
            references: References to include
            detail: Structure detail level
        """
        params = {"references": references, "detail": detail}

        agency_id = quote(agency_id, safe='')
        resource_id = quote(resource_id, safe='')
        version = quote(version, safe='')

        endpoint = f"datastructure/{agency_id}/{resource_id}/{version}"
        return await self._make_request(endpoint, params, "structure")

    async def get_dataflows(
        self,
        agency_id: str = "all",
        resource_id: str = "all",
        version: str = "all",
        references: str = "none",
        detail: str = "full"
    ) -> Dict[str, Any]:
        """Get dataflow definitions (available datasets)"""
        params = {"references": references, "detail": detail}

        agency_id = quote(agency_id, safe='')
        resource_id = quote(resource_id, safe='')
        version = quote(version, safe='')

        endpoint = f"dataflow/{agency_id}/{resource_id}/{version}"
        return await self._make_request(endpoint, params, "structure")

    async def get_categorisations(
        self,
        agency_id: str = "all",
        resource_id: str = "all",
        version: str = "all",
        references: str = "none",
        detail: str = "full"
    ) -> Dict[str, Any]:
        """Get categorisation definitions"""
        params = {"references": references, "detail": detail}

        agency_id = quote(agency_id, safe='')
        resource_id = quote(resource_id, safe='')
        version = quote(version, safe='')

        endpoint = f"categorisation/{agency_id}/{resource_id}/{version}"
        return await self._make_request(endpoint, params, "structure")

    async def get_content_constraints(
        self,
        agency_id: str = "all",
        resource_id: str = "all",
        version: str = "all",
        references: str = "none",
        detail: str = "full"
    ) -> Dict[str, Any]:
        """Get content constraint definitions"""
        params = {"references": references, "detail": detail}

        agency_id = quote(agency_id, safe='')
        resource_id = quote(resource_id, safe='')
        version = quote(version, safe='')

        endpoint = f"contentconstraint/{agency_id}/{resource_id}/{version}"
        return await self._make_request(endpoint, params, "structure")

    async def get_actual_constraints(
        self,
        agency_id: str = "all",
        resource_id: str = "all",
        version: str = "all",
        references: str = "none",
        detail: str = "full"
    ) -> Dict[str, Any]:
        """Get actual constraint definitions"""
        params = {"references": references, "detail": detail}

        agency_id = quote(agency_id, safe='')
        resource_id = quote(resource_id, safe='')
        version = quote(version, safe='')

        endpoint = f"actualconstraint/{agency_id}/{resource_id}/{version}"
        return await self._make_request(endpoint, params, "structure")

    async def get_allowed_constraints(
        self,
        agency_id: str = "all",
        resource_id: str = "all",
        version: str = "all",
        references: str = "none",
        detail: str = "full"
    ) -> Dict[str, Any]:
        """Get allowed constraint definitions"""
        params = {"references": references, "detail": detail}

        agency_id = quote(agency_id, safe='')
        resource_id = quote(resource_id, safe='')
        version = quote(version, safe='')

        endpoint = f"allowedconstraint/{agency_id}/{resource_id}/{version}"
        return await self._make_request(endpoint, params, "structure")

    async def get_structures(
        self,
        agency_id: str = "all",
        resource_id: str = "all",
        version: str = "all",
        references: str = "none",
        detail: str = "full"
    ) -> Dict[str, Any]:
        """Get general structure definitions"""
        params = {"references": references, "detail": detail}

        agency_id = quote(agency_id, safe='')
        resource_id = quote(resource_id, safe='')
        version = quote(version, safe='')

        endpoint = f"structure/{agency_id}/{resource_id}/{version}"
        return await self._make_request(endpoint, params, "structure")

    async def get_concept_schemes(
        self,
        agency_id: str = "all",
        resource_id: str = "all",
        version: str = "all",
        references: str = "none",
        detail: str = "full"
    ) -> Dict[str, Any]:
        """Get concept scheme definitions"""
        params = {"references": references, "detail": detail}

        agency_id = quote(agency_id, safe='')
        resource_id = quote(resource_id, safe='')
        version = quote(version, safe='')

        endpoint = f"conceptscheme/{agency_id}/{resource_id}/{version}"
        return await self._make_request(endpoint, params, "structure")

    async def get_codelists(
        self,
        agency_id: str = "all",
        resource_id: str = "all",
        version: str = "all",
        references: str = "none",
        detail: str = "full"
    ) -> Dict[str, Any]:
        """Get codelist definitions"""
        params = {"references": references, "detail": detail}

        agency_id = quote(agency_id, safe='')
        resource_id = quote(resource_id, safe='')
        version = quote(version, safe='')

        endpoint = f"codelist/{agency_id}/{resource_id}/{version}"
        return await self._make_request(endpoint, params, "structure")

    async def get_category_schemes(
        self,
        agency_id: str = "all",
        resource_id: str = "all",
        version: str = "all",
        references: str = "none",
        detail: str = "full"
    ) -> Dict[str, Any]:
        """Get category scheme definitions"""
        params = {"references": references, "detail": detail}

        agency_id = quote(agency_id, safe='')
        resource_id = quote(resource_id, safe='')
        version = quote(version, safe='')

        endpoint = f"categoryscheme/{agency_id}/{resource_id}/{version}"
        return await self._make_request(endpoint, params, "structure")

    async def get_hierarchical_codelists(
        self,
        agency_id: str = "all",
        resource_id: str = "all",
        version: str = "all",
        references: str = "none",
        detail: str = "full"
    ) -> Dict[str, Any]:
        """Get hierarchical codelist definitions"""
        params = {"references": references, "detail": detail}

        agency_id = quote(agency_id, safe='')
        resource_id = quote(resource_id, safe='')
        version = quote(version, safe='')

        endpoint = f"hierarchicalcodelist/{agency_id}/{resource_id}/{version}"
        return await self._make_request(endpoint, params, "structure")

    async def get_agency_schemes(
        self,
        agency_id: str = "all",
        resource_id: str = "all",
        version: str = "all",
        references: str = "none",
        detail: str = "full"
    ) -> Dict[str, Any]:
        """Get agency scheme definitions"""
        params = {"references": references, "detail": detail}

        agency_id = quote(agency_id, safe='')
        resource_id = quote(resource_id, safe='')
        version = quote(version, safe='')

        endpoint = f"agencyscheme/{agency_id}/{resource_id}/{version}"
        return await self._make_request(endpoint, params, "structure")

    # ITEM QUERY ENDPOINTS

    async def get_concepts(
        self,
        agency_id: str,
        resource_id: str,
        version: str,
        item_id: str = "all",
        references: str = "none",
        detail: str = "full"
    ) -> Dict[str, Any]:
        """Get specific concepts from concept schemes"""
        params = {"references": references, "detail": detail}

        agency_id = quote(agency_id, safe='')
        resource_id = quote(resource_id, safe='')
        version = quote(version, safe='')
        item_id = quote(item_id, safe='')

        endpoint = f"conceptscheme/{agency_id}/{resource_id}/{version}/{item_id}"
        return await self._make_request(endpoint, params, "structure")

    async def get_codes(
        self,
        agency_id: str,
        resource_id: str,
        version: str,
        item_id: str = "all",
        references: str = "none",
        detail: str = "full"
    ) -> Dict[str, Any]:
        """Get specific codes from codelists"""
        params = {"references": references, "detail": detail}

        agency_id = quote(agency_id, safe='')
        resource_id = quote(resource_id, safe='')
        version = quote(version, safe='')
        item_id = quote(item_id, safe='')

        endpoint = f"codelist/{agency_id}/{resource_id}/{version}/{item_id}"
        return await self._make_request(endpoint, params, "structure")

    async def get_categories(
        self,
        agency_id: str,
        resource_id: str,
        version: str,
        item_id: str = "all",
        references: str = "none",
        detail: str = "full"
    ) -> Dict[str, Any]:
        """Get specific categories from category schemes"""
        params = {"references": references, "detail": detail}

        agency_id = quote(agency_id, safe='')
        resource_id = quote(resource_id, safe='')
        version = quote(version, safe='')
        item_id = quote(item_id, safe='')

        endpoint = f"categoryscheme/{agency_id}/{resource_id}/{version}/{item_id}"
        return await self._make_request(endpoint, params, "structure")

    async def get_hierarchies(
        self,
        agency_id: str,
        resource_id: str,
        version: str,
        item_id: str = "all",
        references: str = "none",
        detail: str = "full"
    ) -> Dict[str, Any]:
        """Get specific hierarchies from hierarchical codelists"""
        params = {"references": references, "detail": detail}

        agency_id = quote(agency_id, safe='')
        resource_id = quote(resource_id, safe='')
        version = quote(version, safe='')
        item_id = quote(item_id, safe='')

        endpoint = f"hierarchicalcodelist/{agency_id}/{resource_id}/{version}/{item_id}"
        return await self._make_request(endpoint, params, "structure")

    # CONVENIENCE METHODS FOR COMMON DATA TYPES

    async def get_effective_exchange_rates(
        self,
        countries: Optional[List[str]] = None,
        start_period: Optional[str] = None,
        end_period: Optional[str] = None,
        detail: str = "dataonly"
    ) -> Dict[str, Any]:
        """
        Get effective exchange rates data (WS_EER)

        Args:
            countries: List of country codes (e.g., ['US', 'GB', 'JP'])
            start_period: Start period
            end_period: End period
            detail: Detail level
        """
        key = "all"
        if countries:
            # Build key with country codes
            # Format example: M.N.B.US+GB+JP
            key = f"M.N.B.{'+'.join(countries)}"

        return await self.get_data(
            flow="WS_EER",
            key=key,
            start_period=start_period,
            end_period=end_period,
            detail=detail
        )

    async def get_central_bank_policy_rates(
        self,
        countries: Optional[List[str]] = None,
        start_period: Optional[str] = None,
        end_period: Optional[str] = None,
        detail: str = "dataonly"
    ) -> Dict[str, Any]:
        """
        Get central bank policy rates (WS_CBPOL)

        Args:
            countries: List of country codes
            start_period: Start period
            end_period: End period
            detail: Detail level
        """
        key = "all"
        if countries:
            key = f"D.{'+'.join(countries)}"

        return await self.get_data(
            flow="WS_CBPOL",
            key=key,
            start_period=start_period,
            end_period=end_period,
            detail=detail
        )

    async def get_long_term_interest_rates(
        self,
        countries: Optional[List[str]] = None,
        start_period: Optional[str] = None,
        end_period: Optional[str] = None,
        detail: str = "dataonly"
    ) -> Dict[str, Any]:
        """Get long-term interest rates (WS_LTINT)"""
        key = "all"
        if countries:
            key = f"M.{'+'.join(countries)}"

        return await self.get_data(
            flow="WS_LTINT",
            key=key,
            start_period=start_period,
            end_period=end_period,
            detail=detail
        )

    async def get_short_term_interest_rates(
        self,
        countries: Optional[List[str]] = None,
        start_period: Optional[str] = None,
        end_period: Optional[str] = None,
        detail: str = "dataonly"
    ) -> Dict[str, Any]:
        """Get short-term interest rates (WS_STINT)"""
        key = "all"
        if countries:
            key = f"M.{'+'.join(countries)}"

        return await self.get_data(
            flow="WS_STINT",
            key=key,
            start_period=start_period,
            end_period=end_period,
            detail=detail
        )

    async def get_exchange_rates(
        self,
        currency_pairs: Optional[List[str]] = None,
        start_period: Optional[str] = None,
        end_period: Optional[str] = None,
        detail: str = "dataonly"
    ) -> Dict[str, Any]:
        """
        Get exchange rates (WS_XRU)

        Args:
            currency_pairs: List of currency pairs (e.g., ['USD', 'EUR', 'GBP'])
            start_period: Start period
            end_period: End period
            detail: Detail level
        """
        key = "all"
        if currency_pairs:
            key = f"D.{'+'.join(currency_pairs)}"

        return await self.get_data(
            flow="WS_XRU",
            key=key,
            start_period=start_period,
            end_period=end_period,
            detail=detail
        )

    async def get_credit_to_non_financial_sector(
        self,
        countries: Optional[List[str]] = None,
        sectors: Optional[List[str]] = None,
        start_period: Optional[str] = None,
        end_period: Optional[str] = None,
        detail: str = "dataonly"
    ) -> Dict[str, Any]:
        """
        Get credit to non-financial sector (WS_CRD)

        Args:
            countries: List of country codes
            sectors: List of sectors
            start_period: Start period
            end_period: End period
            detail: Detail level
        """
        key = "all"
        if countries and sectors:
            # Example format: Q.US.GBFC+GBFCNP+GBFCP
            key = f"Q.{'+'.join(countries)}.{'+'.join(sectors)}"
        elif countries:
            key = f"Q.{'+'.join(countries)}"

        return await self.get_data(
            flow="WS_CRD",
            key=key,
            start_period=start_period,
            end_period=end_period,
            detail=detail
        )

    async def get_house_prices(
        self,
        countries: Optional[List[str]] = None,
        start_period: Optional[str] = None,
        end_period: Optional[str] = None,
        detail: str = "dataonly"
    ) -> Dict[str, Any]:
        """Get house price indices (WS_HP)"""
        key = "all"
        if countries:
            key = f"Q.{'+'.join(countries)}"

        return await self.get_data(
            flow="WS_HP",
            key=key,
            start_period=start_period,
            end_period=end_period,
            detail=detail
        )

    async def get_available_datasets(self) -> Dict[str, Any]:
        """Get overview of all available dataflows (datasets)"""
        return await self.get_dataflows(agency_id="BIS", detail="allstubs")

    async def search_datasets(
        self,
        query: str,
        agency_id: str = "BIS"
    ) -> Dict[str, Any]:
        """
        Search for datasets matching a query

        Args:
            query: Search term
            agency_id: Agency to search in
        """
        # Get all dataflows and search through them
        result = await self.get_dataflows(agency_id=agency_id, detail="full")

        if result.get("success") and "data" in result:
            dataflows = result["data"]
            # Simple text search in the response
            matches = []
            if isinstance(dataflows, dict):
                for key, value in dataflows.items():
                    if query.lower() in str(value).lower():
                        matches.append({key: value})

            return {
                "success": True,
                "data": {
                    "query": query,
                    "matches": matches,
                    "total_matches": len(matches)
                },
                "endpoint": "search"
            }

        return result

    # COMPREHENSIVE ANALYSIS METHODS

    async def get_economic_overview(
        self,
        countries: Optional[List[str]] = None,
        start_period: Optional[str] = None,
        end_period: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Get comprehensive economic overview for countries

        Args:
            countries: List of country codes
            start_period: Start period
            end_period: End period
        """
        results = {}

        # Get multiple economic indicators
        indicators = [
            ("effective_exchange_rates", self.get_effective_exchange_rates),
            ("central_bank_policy_rates", self.get_central_bank_policy_rates),
            ("long_term_interest_rates", self.get_long_term_interest_rates),
            ("short_term_interest_rates", self.get_short_term_interest_rates),
            ("exchange_rates", self.get_exchange_rates),
            ("credit_to_non_financial_sector", self.get_credit_to_non_financial_sector)
        ]

        for name, method in indicators:
            try:
                result = await method(
                    countries=countries,
                    start_period=start_period,
                    end_period=end_period,
                    detail="dataonly"
                )
                results[name] = result
            except Exception as e:
                results[name] = {"success": False, "error": str(e)}

        return {
            "success": True,
            "data": {
                "economic_overview": results,
                "countries": countries or ["all"],
                "period": {
                    "start": start_period or "earliest",
                    "end": end_period or "latest"
                },
                "indicators_requested": len(indicators),
                "indicators_retrieved": len([r for r in results.values() if r.get("success", False)])
            },
            "endpoint": "economic_overview"
        }

    async def get_dataset_metadata(
        self,
        flow: str
    ) -> Dict[str, Any]:
        """
        Get comprehensive metadata for a specific dataset

        Args:
            flow: Data flow identifier (e.g., 'WS_EER')
        """
        results = {}

        try:
            # Get dataflow information
            dataflows = await self.get_dataflows(resource_id=flow)
            results["dataflow"] = dataflows

            # Get data structure
            structures = await self.get_data_structures(resource_id=flow)
            results["structure"] = structures

            # Get sample data to understand the format
            sample_data = await self.get_data(flow=flow, detail="serieskeysonly", last_n_observations=5)
            results["sample_keys"] = sample_data

            return {
                "success": True,
                "data": {
                    "flow": flow,
                    "metadata": results,
                    "description": self.known_flows.get(flow, "Unknown dataset")
                },
                "endpoint": "dataset_metadata"
            }

        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "endpoint": "dataset_metadata"
            }


# CLI Interface
async def main():
    """CLI interface for BIS API"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python bis_data.py <command> [args...]",
            "available_commands": [
                "get_data <flow> [key] [start_period] [end_period]",
                "get_effective_exchange_rates [countries...] [start_period] [end_period]",
                "get_central_bank_policy_rates [countries...] [start_period] [end_period]",
                "get_long_term_interest_rates [countries...] [start_period] [end_period]",
                "get_short_term_interest_rates [countries...] [start_period] [end_period]",
                "get_exchange_rates [currencies...] [start_period] [end_period]",
                "get_credit_to_non_financial_sector [countries...] [sectors...] [start_period] [end_period]",
                "get_house_prices [countries...] [start_period] [end_period]",
                "get_available_datasets",
                "get_data_structures [agency_id] [resource_id] [version]",
                "search_datasets <query>",
                "get_economic_overview [countries...] [start_period] [end_period]",
                "get_dataset_metadata <flow>"
            ]
        }))
        return

    command = sys.argv[1]

    async with BISAPI() as bis:
        try:
            if command == "get_data":
                if len(sys.argv) < 3:
                    raise ValueError("Usage: get_data <flow> [key] [start_period] [end_period]")

                flow = sys.argv[2]
                key = sys.argv[3] if len(sys.argv) > 3 else "all"
                start_period = sys.argv[4] if len(sys.argv) > 4 else None
                end_period = sys.argv[5] if len(sys.argv) > 5 else None

                result = await bis.get_data(flow, key, start_period, end_period)

            elif command == "get_effective_exchange_rates":
                countries = []
                other_args = []

                # Parse arguments - country codes are 2-letter, others are dates
                for arg in sys.argv[2:]:
                    if len(arg) == 2 and arg.isalpha():
                        countries.append(arg)
                    else:
                        other_args.append(arg)

                start_period = other_args[0] if len(other_args) > 0 else None
                end_period = other_args[1] if len(other_args) > 1 else None

                result = await bis.get_effective_exchange_rates(
                    countries=countries if countries else None,
                    start_period=start_period,
                    end_period=end_period
                )

            elif command == "get_central_bank_policy_rates":
                countries = []
                other_args = []

                for arg in sys.argv[2:]:
                    if len(arg) == 2 and arg.isalpha():
                        countries.append(arg)
                    else:
                        other_args.append(arg)

                start_period = other_args[0] if len(other_args) > 0 else None
                end_period = other_args[1] if len(other_args) > 1 else None

                result = await bis.get_central_bank_policy_rates(
                    countries=countries if countries else None,
                    start_period=start_period,
                    end_period=end_period
                )

            elif command == "get_long_term_interest_rates":
                countries = []
                other_args = []

                for arg in sys.argv[2:]:
                    if len(arg) == 2 and arg.isalpha():
                        countries.append(arg)
                    else:
                        other_args.append(arg)

                start_period = other_args[0] if len(other_args) > 0 else None
                end_period = other_args[1] if len(other_args) > 1 else None

                result = await bis.get_long_term_interest_rates(
                    countries=countries if countries else None,
                    start_period=start_period,
                    end_period=end_period
                )

            elif command == "get_short_term_interest_rates":
                countries = []
                other_args = []

                for arg in sys.argv[2:]:
                    if len(arg) == 2 and arg.isalpha():
                        countries.append(arg)
                    else:
                        other_args.append(arg)

                start_period = other_args[0] if len(other_args) > 0 else None
                end_period = other_args[1] if len(other_args) > 1 else None

                result = await bis.get_short_term_interest_rates(
                    countries=countries if countries else None,
                    start_period=start_period,
                    end_period=end_period
                )

            elif command == "get_exchange_rates":
                currencies = []
                other_args = []

                for arg in sys.argv[2:]:
                    if len(arg) == 3 and arg.isalpha():
                        currencies.append(arg)
                    else:
                        other_args.append(arg)

                start_period = other_args[0] if len(other_args) > 0 else None
                end_period = other_args[1] if len(other_args) > 1 else None

                result = await bis.get_exchange_rates(
                    currency_pairs=currencies if currencies else None,
                    start_period=start_period,
                    end_period=end_period
                )

            elif command == "get_credit_to_non_financial_sector":
                countries = []
                sectors = []
                other_args = []

                # Parse arguments - need to distinguish countries from sectors
                # This is simplified - in practice, you might need more sophisticated parsing
                for arg in sys.argv[2:]:
                    if len(arg) == 2 and arg.isalpha():
                        countries.append(arg)
                    else:
                        sectors.append(arg)

                # Last two arguments might be dates
                if len(other_args) >= 2:
                    start_period = other_args[-2]
                    end_period = other_args[-1]
                    sectors = sectors[:-2]
                elif len(other_args) == 1:
                    start_period = other_args[0]
                    end_period = None
                    sectors = sectors[:-1]
                else:
                    start_period = None
                    end_period = None

                result = await bis.get_credit_to_non_financial_sector(
                    countries=countries if countries else None,
                    sectors=sectors if sectors else None,
                    start_period=start_period,
                    end_period=end_period
                )

            elif command == "get_house_prices":
                countries = []
                other_args = []

                for arg in sys.argv[2:]:
                    if len(arg) == 2 and arg.isalpha():
                        countries.append(arg)
                    else:
                        other_args.append(arg)

                start_period = other_args[0] if len(other_args) > 0 else None
                end_period = other_args[1] if len(other_args) > 1 else None

                result = await bis.get_house_prices(
                    countries=countries if countries else None,
                    start_period=start_period,
                    end_period=end_period
                )

            elif command == "get_available_datasets":
                result = await bis.get_available_datasets()

            elif command == "get_data_structures":
                agency_id = sys.argv[2] if len(sys.argv) > 2 else "all"
                resource_id = sys.argv[3] if len(sys.argv) > 3 else "all"
                version = sys.argv[4] if len(sys.argv) > 4 else "all"

                result = await bis.get_data_structures(agency_id, resource_id, version)

            elif command == "search_datasets":
                if len(sys.argv) < 3:
                    raise ValueError("Usage: search_datasets <query>")
                query = sys.argv[2]
                result = await bis.search_datasets(query)

            elif command == "get_economic_overview":
                countries = []
                other_args = []

                for arg in sys.argv[2:]:
                    if len(arg) == 2 and arg.isalpha():
                        countries.append(arg)
                    else:
                        other_args.append(arg)

                start_period = other_args[0] if len(other_args) > 0 else None
                end_period = other_args[1] if len(other_args) > 1 else None

                result = await bis.get_economic_overview(
                    countries=countries if countries else None,
                    start_period=start_period,
                    end_period=end_period
                )

            elif command == "get_dataset_metadata":
                if len(sys.argv) < 3:
                    raise ValueError("Usage: get_dataset_metadata <flow>")
                flow = sys.argv[2]
                result = await bis.get_dataset_metadata(flow)

            elif command == "get_available_constraints":
                if len(sys.argv) < 5:
                    raise ValueError("Usage: get_available_constraints <flow> <key> <component_id> [mode] [references]")
                flow = sys.argv[2]
                key = sys.argv[3]
                component_id = sys.argv[4]
                mode = sys.argv[5] if len(sys.argv) > 5 else "exact"
                references = sys.argv[6] if len(sys.argv) > 6 else "none"
                result = await bis.get_available_constraints(flow, key, component_id, mode, references)

            elif command == "get_dataflows":
                agency_id = sys.argv[2] if len(sys.argv) > 2 else "all"
                resource_id = sys.argv[3] if len(sys.argv) > 3 else "all"
                version = sys.argv[4] if len(sys.argv) > 4 else "all"
                references = sys.argv[5] if len(sys.argv) > 5 else "none"
                detail = sys.argv[6] if len(sys.argv) > 6 else "full"
                result = await bis.get_dataflows(agency_id, resource_id, version, references, detail)

            elif command == "get_categorisations":
                agency_id = sys.argv[2] if len(sys.argv) > 2 else "all"
                resource_id = sys.argv[3] if len(sys.argv) > 3 else "all"
                version = sys.argv[4] if len(sys.argv) > 4 else "all"
                references = sys.argv[5] if len(sys.argv) > 5 else "none"
                detail = sys.argv[6] if len(sys.argv) > 6 else "full"
                result = await bis.get_categorisations(agency_id, resource_id, version, references, detail)

            elif command == "get_content_constraints":
                agency_id = sys.argv[2] if len(sys.argv) > 2 else "all"
                resource_id = sys.argv[3] if len(sys.argv) > 3 else "all"
                version = sys.argv[4] if len(sys.argv) > 4 else "all"
                references = sys.argv[5] if len(sys.argv) > 5 else "none"
                detail = sys.argv[6] if len(sys.argv) > 6 else "full"
                result = await bis.get_content_constraints(agency_id, resource_id, version, references, detail)

            elif command == "get_actual_constraints":
                agency_id = sys.argv[2] if len(sys.argv) > 2 else "all"
                resource_id = sys.argv[3] if len(sys.argv) > 3 else "all"
                version = sys.argv[4] if len(sys.argv) > 4 else "all"
                references = sys.argv[5] if len(sys.argv) > 5 else "none"
                detail = sys.argv[6] if len(sys.argv) > 6 else "full"
                result = await bis.get_actual_constraints(agency_id, resource_id, version, references, detail)

            elif command == "get_allowed_constraints":
                agency_id = sys.argv[2] if len(sys.argv) > 2 else "all"
                resource_id = sys.argv[3] if len(sys.argv) > 3 else "all"
                version = sys.argv[4] if len(sys.argv) > 4 else "all"
                references = sys.argv[5] if len(sys.argv) > 5 else "none"
                detail = sys.argv[6] if len(sys.argv) > 6 else "full"
                result = await bis.get_allowed_constraints(agency_id, resource_id, version, references, detail)

            elif command == "get_structures":
                agency_id = sys.argv[2] if len(sys.argv) > 2 else "all"
                resource_id = sys.argv[3] if len(sys.argv) > 3 else "all"
                version = sys.argv[4] if len(sys.argv) > 4 else "all"
                references = sys.argv[5] if len(sys.argv) > 5 else "none"
                detail = sys.argv[6] if len(sys.argv) > 6 else "full"
                result = await bis.get_structures(agency_id, resource_id, version, references, detail)

            elif command == "get_concept_schemes":
                agency_id = sys.argv[2] if len(sys.argv) > 2 else "all"
                resource_id = sys.argv[3] if len(sys.argv) > 3 else "all"
                version = sys.argv[4] if len(sys.argv) > 4 else "all"
                references = sys.argv[5] if len(sys.argv) > 5 else "none"
                detail = sys.argv[6] if len(sys.argv) > 6 else "full"
                result = await bis.get_concept_schemes(agency_id, resource_id, version, references, detail)

            elif command == "get_codelists":
                agency_id = sys.argv[2] if len(sys.argv) > 2 else "all"
                resource_id = sys.argv[3] if len(sys.argv) > 3 else "all"
                version = sys.argv[4] if len(sys.argv) > 4 else "all"
                references = sys.argv[5] if len(sys.argv) > 5 else "none"
                detail = sys.argv[6] if len(sys.argv) > 6 else "full"
                result = await bis.get_codelists(agency_id, resource_id, version, references, detail)

            elif command == "get_category_schemes":
                agency_id = sys.argv[2] if len(sys.argv) > 2 else "all"
                resource_id = sys.argv[3] if len(sys.argv) > 3 else "all"
                version = sys.argv[4] if len(sys.argv) > 4 else "all"
                references = sys.argv[5] if len(sys.argv) > 5 else "none"
                detail = sys.argv[6] if len(sys.argv) > 6 else "full"
                result = await bis.get_category_schemes(agency_id, resource_id, version, references, detail)

            elif command == "get_hierarchical_codelists":
                agency_id = sys.argv[2] if len(sys.argv) > 2 else "all"
                resource_id = sys.argv[3] if len(sys.argv) > 3 else "all"
                version = sys.argv[4] if len(sys.argv) > 4 else "all"
                references = sys.argv[5] if len(sys.argv) > 5 else "none"
                detail = sys.argv[6] if len(sys.argv) > 6 else "full"
                result = await bis.get_hierarchical_codelists(agency_id, resource_id, version, references, detail)

            elif command == "get_agency_schemes":
                agency_id = sys.argv[2] if len(sys.argv) > 2 else "all"
                resource_id = sys.argv[3] if len(sys.argv) > 3 else "all"
                version = sys.argv[4] if len(sys.argv) > 4 else "all"
                references = sys.argv[5] if len(sys.argv) > 5 else "none"
                detail = sys.argv[6] if len(sys.argv) > 6 else "full"
                result = await bis.get_agency_schemes(agency_id, resource_id, version, references, detail)

            elif command == "get_concepts":
                if len(sys.argv) < 4:
                    raise ValueError("Usage: get_concepts <agency_id> <resource_id> <version> [item_id] [references] [detail]")
                agency_id = sys.argv[2]
                resource_id = sys.argv[3]
                version = sys.argv[4]
                item_id = sys.argv[5] if len(sys.argv) > 5 else "all"
                references = sys.argv[6] if len(sys.argv) > 6 else "none"
                detail = sys.argv[7] if len(sys.argv) > 7 else "full"
                result = await bis.get_concepts(agency_id, resource_id, version, item_id, references, detail)

            elif command == "get_codes":
                if len(sys.argv) < 4:
                    raise ValueError("Usage: get_codes <agency_id> <resource_id> <version> [item_id] [references] [detail]")
                agency_id = sys.argv[2]
                resource_id = sys.argv[3]
                version = sys.argv[4]
                item_id = sys.argv[5] if len(sys.argv) > 5 else "all"
                references = sys.argv[6] if len(sys.argv) > 6 else "none"
                detail = sys.argv[7] if len(sys.argv) > 7 else "full"
                result = await bis.get_codes(agency_id, resource_id, version, item_id, references, detail)

            elif command == "get_categories":
                if len(sys.argv) < 4:
                    raise ValueError("Usage: get_categories <agency_id> <resource_id> <version> [item_id] [references] [detail]")
                agency_id = sys.argv[2]
                resource_id = sys.argv[3]
                version = sys.argv[4]
                item_id = sys.argv[5] if len(sys.argv) > 5 else "all"
                references = sys.argv[6] if len(sys.argv) > 6 else "none"
                detail = sys.argv[7] if len(sys.argv) > 7 else "full"
                result = await bis.get_categories(agency_id, resource_id, version, item_id, references, detail)

            elif command == "get_hierarchies":
                if len(sys.argv) < 4:
                    raise ValueError("Usage: get_hierarchies <agency_id> <resource_id> <version> [item_id] [references] [detail]")
                agency_id = sys.argv[2]
                resource_id = sys.argv[3]
                version = sys.argv[4]
                item_id = sys.argv[5] if len(sys.argv) > 5 else "all"
                references = sys.argv[6] if len(sys.argv) > 6 else "none"
                detail = sys.argv[7] if len(sys.argv) > 7 else "full"
                result = await bis.get_hierarchies(agency_id, resource_id, version, item_id, references, detail)

            else:
                result = {
                    "success": False,
                    "error": f"Unknown command: {command}",
                    "available_commands": [
                        "get_data", "get_effective_exchange_rates", "get_central_bank_policy_rates",
                        "get_long_term_interest_rates", "get_short_term_interest_rates",
                        "get_exchange_rates", "get_credit_to_non_financial_sector", "get_house_prices",
                        "get_available_datasets", "get_data_structures", "search_datasets",
                        "get_economic_overview", "get_dataset_metadata", "get_available_constraints",
                        "get_dataflows", "get_categorisations", "get_content_constraints",
                        "get_actual_constraints", "get_allowed_constraints", "get_structures",
                        "get_concept_schemes", "get_codelists", "get_category_schemes",
                        "get_hierarchical_codelists", "get_agency_schemes", "get_concepts",
                        "get_codes", "get_categories", "get_hierarchies"
                    ]
                }

            print(json.dumps(result, indent=2))

        except BISError as e:
            error_response = {
                "success": False,
                "error": str(e),
                "endpoint": e.endpoint,
                "status_code": e.status_code
            }
            print(json.dumps(error_response, indent=2))

        except Exception as e:
            error_response = {
                "success": False,
                "error": str(e),
                "command": command
            }
            print(json.dumps(error_response, indent=2))


if __name__ == "__main__":
    asyncio.run(main())