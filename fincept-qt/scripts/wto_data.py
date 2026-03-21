#!/usr/bin/env python3
"""
WTO (World Trade Organization) Data Wrapper

This script provides access to multiple WTO APIs:
- Quantitative Restrictions (QR) API
- ePing (SPS/TBT notifications) API
- Timeseries API (trade statistics)
- Trade Facilitation Agreement Database (TFAD) API

Data sources:
- https://api.wto.org/qrs - Quantitative Restrictions
- https://api.wto.org/eping - ePing notifications
- https://api.wto.org/timeseries/v1 - Trade statistics
- https://api.wto.org/tfad - Trade Facilitation Agreement

Usage:
    python wto_data.py available_apis
    python wto_data.py qr_members
    python wto_data.py eping_search --language 1 --domainIds 1
    python wto_data.py timeseries_data --i TP_A_0010 --r US
    python wto_data.py tfad_data --countries 840,124
"""

import sys
import json
import asyncio
import aiohttp
from datetime import datetime
from typing import Dict, List, Optional, Union, Literal
import urllib.parse

class WTOError(Exception):
    """Custom exception for WTO data errors"""
    pass

class WTODataWrapper:
    """World Trade Organization data wrapper"""

    def __init__(self, subscription_key: str = None):
        self.subscription_key = subscription_key
        self.headers = {}

        if subscription_key:
            self.headers['Ocp-Apim-Subscription-Key'] = subscription_key

        # API base URLs
        self.qr_url = "https://api.wto.org/qrs"
        self.eping_url = "https://api.wto.org/eping"
        self.timeseries_url = "https://api.wto.org/timeseries/v1"
        self.tfad_url = "https://api.wto.org/tfad"

    async def fetch_data(self, session: aiohttp.ClientSession, url: str, params: Dict = None) -> Union[Dict, List]:
        """Fetch data from WTO API"""
        try:
            async with session.get(url, params=params, headers=self.headers) as response:
                if response.status == 200:
                    content_type = response.headers.get('content-type', '')
                    if 'application/json' in content_type:
                        data = await response.json()
                        # Timeseries API returns JSON string in 'data', parse it
                        if isinstance(data, str):
                            try:
                                import json
                                return json.loads(data)
                            except:
                                return data
                        return data
                    else:
                        text = await response.text()
                        # Try to parse as JSON anyway (some endpoints return JSON without correct content-type)
                        try:
                            import json
                            return json.loads(text)
                        except:
                            return text
                else:
                    error_text = await response.text()
                    raise WTOError(f"HTTP {response.status}: Failed to fetch data from {url}. Error: {error_text}")
        except aiohttp.ClientError as e:
            raise WTOError(f"Network error: {str(e)}")
        except Exception as e:
            raise WTOError(f"Unexpected error: {str(e)}")

    # QUANTITATIVE RESTRICTIONS (QR) API

    async def get_qr_members(self, member_code: str = None, name: str = None, page: int = None) -> Dict:
        """Get QR members list"""
        params = {}
        if member_code:
            params['member_code'] = member_code
        if name:
            params['name'] = name
        if page:
            params['page'] = page

        try:
            async with aiohttp.ClientSession() as session:
                result = await self.fetch_data(session, f"{self.qr_url}/members", params)
                return {
                    "success": True,
                    "data": result,
                    "source": "WTO QR API",
                    "description": "WTO Quantitative Restrictions members list"
                }
        except Exception as e:
            return {"success": False, "error": str(e)}

    async def get_qr_products(self, hs_version: str, code: str = None, description: str = None, page: int = None) -> Dict:
        """Get QR products list"""
        params = {'hs_version': hs_version}
        if code:
            params['code'] = code
        if description:
            params['description'] = description
        if page:
            params['page'] = page

        try:
            async with aiohttp.ClientSession() as session:
                result = await self.fetch_data(session, f"{self.qr_url}/products", params)
                return {
                    "success": True,
                    "data": result,
                    "source": "WTO QR API",
                    "description": f"WTO Quantitative Restrictions products - HS {hs_version}"
                }
        except Exception as e:
            return {"success": False, "error": str(e)}

    async def get_qr_notifications(self, reporter_member_code: str = None, notification_year: int = None, page: int = None) -> Dict:
        """Get QR notifications list"""
        params = {}
        if reporter_member_code:
            params['reporter_member_code'] = reporter_member_code
        if notification_year:
            params['notification_year'] = notification_year
        if page:
            params['page'] = page

        try:
            async with aiohttp.ClientSession() as session:
                result = await self.fetch_data(session, f"{self.qr_url}/notifications", params)
                return {
                    "success": True,
                    "data": result,
                    "source": "WTO QR API",
                    "description": "WTO Quantitative Restrictions notifications"
                }
        except Exception as e:
            return {"success": False, "error": str(e)}

    async def get_qr_details(self, qr_id: int) -> Dict:
        """Get QR details by ID"""
        try:
            async with aiohttp.ClientSession() as session:
                result = await self.fetch_data(session, f"{self.qr_url}/qrs/{qr_id}")
                return {
                    "success": True,
                    "data": result,
                    "source": "WTO QR API",
                    "description": f"WTO Quantitative Restrictions details - ID {qr_id}"
                }
        except Exception as e:
            return {"success": False, "error": str(e)}

    async def get_qr_list(self, reporter_member_code: str = None, in_force_only: bool = None,
                          year_of_entry_into_force: int = None, product_codes: str = None,
                          product_ids: str = None, page: int = None) -> Dict:
        """Get QR list with filters"""
        params = {}
        if reporter_member_code:
            params['reporter_member_code'] = reporter_member_code
        if in_force_only is not None:
            params['in_force_only'] = str(in_force_only).lower()
        if year_of_entry_into_force:
            params['year_of_entry_into_force'] = year_of_entry_into_force
        if product_codes:
            params['product_codes'] = product_codes
        if product_ids:
            params['product_ids'] = product_ids
        if page:
            params['page'] = page

        try:
            async with aiohttp.ClientSession() as session:
                result = await self.fetch_data(session, f"{self.qr_url}/qrs", params)
                return {
                    "success": True,
                    "data": result,
                    "source": "WTO QR API",
                    "description": "WTO Quantitative Restrictions list"
                }
        except Exception as e:
            return {"success": False, "error": str(e)}

    async def get_qr_hs_versions(self) -> Dict:
        """Get QR HS versions"""
        try:
            async with aiohttp.ClientSession() as session:
                result = await self.fetch_data(session, f"{self.qr_url}/hs-versions")
                return {
                    "success": True,
                    "data": result,
                    "source": "WTO QR API",
                    "description": "WTO Quantitative Restrictions HS versions"
                }
        except Exception as e:
            return {"success": False, "error": str(e)}

    # EPING (SPS/TBT NOTIFICATIONS) API

    async def get_eping_members(self, language: int = 1) -> Dict:
        """Get ePing members list"""
        params = {'language': str(language)}

        try:
            async with aiohttp.ClientSession() as session:
                result = await self.fetch_data(session, f"{self.eping_url}/members", params)
                return {
                    "success": True,
                    "data": result,
                    "source": "WTO ePing API",
                    "description": "WTO ePing members list"
                }
        except Exception as e:
            return {"success": False, "error": str(e)}

    async def search_eping_notifications(self, language: int = 1, domain_ids: int = None,
                                       document_symbol: str = None, distribution_date_from: str = None,
                                       distribution_date_to: str = None, country_ids: List[str] = None,
                                       hs_codes: str = None, ics_codes: str = None,
                                       free_text: str = None, page: int = None, page_size: int = None) -> Dict:
        """Search ePing notifications"""
        params = {'language': str(language)}

        if domain_ids:
            params['domainIds'] = domain_ids
        if document_symbol:
            params['documentSymbol'] = document_symbol
        if distribution_date_from:
            params['distributionDateFrom'] = distribution_date_from
        if distribution_date_to:
            params['distributionDateTo'] = distribution_date_to
        if country_ids:
            params['countryIds'] = ','.join(country_ids)
        if hs_codes:
            params['hs'] = hs_codes
        if ics_codes:
            params['ics'] = ics_codes
        if free_text:
            params['freeText'] = free_text
        if page:
            params['page'] = page
        if page_size:
            params['pageSize'] = page_size

        try:
            async with aiohttp.ClientSession() as session:
                result = await self.fetch_data(session, f"{self.eping_url}/notifications/search", params)
                return {
                    "success": True,
                    "data": result,
                    "source": "WTO ePing API",
                    "description": "WTO ePing notifications search results"
                }
        except Exception as e:
            return {"success": False, "error": str(e)}

    # TIMESERIES API

    async def get_timeseries_topics(self, language: int = 1) -> Dict:
        """Get timeseries topics"""
        params = {'lang': str(language)}

        try:
            async with aiohttp.ClientSession() as session:
                result = await self.fetch_data(session, f"{self.timeseries_url}/topics", params)
                return {
                    "success": True,
                    "data": result,
                    "source": "WTO Timeseries API",
                    "description": "WTO Timeseries topics"
                }
        except Exception as e:
            return {"success": False, "error": str(e)}

    async def get_timeseries_indicators(self, indicator: str = "all", name: str = None,
                                       topics: str = None, product_classifications: str = None,
                                       trade_partner: str = None, frequency: str = None,
                                       language: int = 1) -> Dict:
        """Get timeseries indicators"""
        params = {'i': indicator, 'lang': str(language)}

        if name:
            params['name'] = name
        if topics:
            params['t'] = topics
        if product_classifications:
            params['pc'] = product_classifications
        if trade_partner:
            params['tp'] = trade_partner
        if frequency:
            params['frq'] = frequency

        try:
            async with aiohttp.ClientSession() as session:
                result = await self.fetch_data(session, f"{self.timeseries_url}/indicators", params)
                return {
                    "success": True,
                    "data": result,
                    "source": "WTO Timeseries API",
                    "description": "WTO Timeseries indicators"
                }
        except Exception as e:
            return {"success": False, "error": str(e)}

    async def get_timeseries_data(self, indicator: str, reporters: str = "all", partners: str = "default",
                                 periods: str = "default", products: str = "default",
                                 include_sub_products: bool = False, format_type: str = "json",
                                 mode: str = "full", decimals: str = "default", offset: int = 0,
                                 max_records: int = 500, heading_style: str = "H",
                                 language: int = 1, include_metadata: bool = False) -> Dict:
        """Get timeseries data points"""
        params = {
            'i': indicator,
            'r': reporters,
            'p': partners,
            'ps': periods,
            'pc': products,
            'spc': str(include_sub_products).lower(),
            'fmt': format_type,
            'mode': mode,
            'dec': decimals,
            'off': str(offset),
            'max': str(max_records),
            'head': heading_style,
            'lang': str(language),
            'meta': str(include_metadata).lower()
        }

        try:
            async with aiohttp.ClientSession() as session:
                result = await self.fetch_data(session, f"{self.timeseries_url}/data", params)
                return {
                    "success": True,
                    "data": result,
                    "source": "WTO Timeseries API",
                    "description": f"WTO Timeseries data - {indicator}"
                }
        except Exception as e:
            return {"success": False, "error": str(e)}

    async def get_timeseries_data_count(self, indicator: str, reporters: str = "all", partners: str = "default",
                                       periods: str = "default", products: str = "default",
                                       include_sub_products: bool = False) -> Dict:
        """Get timeseries data count"""
        params = {
            'i': indicator,
            'r': reporters,
            'p': partners,
            'ps': periods,
            'pc': products,
            'spc': str(include_sub_products).lower()
        }

        try:
            async with aiohttp.ClientSession() as session:
                result = await self.fetch_data(session, f"{self.timeseries_url}/data_count", params)
                return {
                    "success": True,
                    "data": result,
                    "source": "WTO Timeseries API",
                    "description": f"WTO Timeseries data count - {indicator}"
                }
        except Exception as e:
            return {"success": False, "error": str(e)}

    async def get_timeseries_reporters(self, name: str = None, individual_group: str = None,
                                      regions: str = None, groups: str = None, language: int = 1) -> Dict:
        """Get timeseries reporting economies"""
        params = {'lang': str(language)}

        if name:
            params['name'] = name
        if individual_group:
            params['ig'] = individual_group
        if regions:
            params['reg'] = regions
        if groups:
            params['gp'] = groups

        try:
            async with aiohttp.ClientSession() as session:
                result = await self.fetch_data(session, f"{self.timeseries_url}/reporters", params)
                return {
                    "success": True,
                    "data": result,
                    "source": "WTO Timeseries API",
                    "description": "WTO Timeseries reporting economies"
                }
        except Exception as e:
            return {"success": False, "error": str(e)}

    async def get_timeseries_partners(self, name: str = None, individual_group: str = None,
                                     regions: str = None, groups: str = None, language: int = 1) -> Dict:
        """Get timeseries partner economies"""
        params = {'lang': str(language)}

        if name:
            params['name'] = name
        if individual_group:
            params['ig'] = individual_group
        if regions:
            params['reg'] = regions
        if groups:
            params['gp'] = groups

        try:
            async with aiohttp.ClientSession() as session:
                result = await self.fetch_data(session, f"{self.timeseries_url}/partners", params)
                return {
                    "success": True,
                    "data": result,
                    "source": "WTO Timeseries API",
                    "description": "WTO Timeseries partner economies"
                }
        except Exception as e:
            return {"success": False, "error": str(e)}

    # TRADE FACILITATION AGREEMENT DATABASE (TFAD) API

    async def get_tfad_data(self, countries: List[str] = None) -> Dict:
        """Get TFAD transparency procedures, contacts, and single window data"""
        params = {}
        if countries:
            params['countries[]'] = countries

        try:
            async with aiohttp.ClientSession() as session:
                result = await self.fetch_data(session, f"{self.tfad_url}/transparency/procedures_contacts_single_window", params)
                return {
                    "success": True,
                    "data": result,
                    "source": "WTO TFAD API",
                    "description": "WTO Trade Facilitation Agreement Database - procedures, contacts, single window"
                }
        except Exception as e:
            return {"success": False, "error": str(e)}

    # REFERENCE DATA

    def get_available_apis(self) -> Dict:
        """Get list of available WTO APIs and their endpoints"""
        return {
            "success": True,
            "data": {
                "apis": {
                    "quantitative_restrictions": {
                        "name": "Quantitative Restrictions API",
                        "base_url": self.qr_url,
                        "endpoints": [
                            "members", "products", "notifications", "qrs", "qrs/{id}", "hs-versions"
                        ]
                    },
                    "eping": {
                        "name": "ePing Notifications API",
                        "base_url": self.eping_url,
                        "endpoints": [
                            "members", "notifications/search"
                        ]
                    },
                    "timeseries": {
                        "name": "Timeseries API",
                        "base_url": self.timeseries_url,
                        "endpoints": [
                            "topics", "frequencies", "periods", "units", "indicators",
                            "reporters", "partners", "products", "data", "data_count", "metadata"
                        ]
                    },
                    "tfad": {
                        "name": "Trade Facilitation Agreement Database API",
                        "base_url": self.tfad_url,
                        "endpoints": [
                            "transparency/procedures_contacts_single_window"
                        ]
                    }
                },
                "authentication": "API Key (Ocp-Apim-Subscription-Key header or subscription-key query)",
                "data_types": [
                    "Trade restrictions and prohibitions",
                    "SPS/TBT notifications",
                    "Trade statistics and indicators",
                    "Trade facilitation procedures"
                ]
            }
        }

    # OVERVIEW AND ANALYSIS COMMANDS

    async def get_wto_overview(self) -> Dict:
        """Get overview of WTO data across all APIs"""
        try:
            results = {}

            # Get QR members
            qr_members = await self.get_qr_members(page=1)
            results['qr_members'] = qr_members

            # Get ePing members
            eping_members = await self.get_eping_members()
            results['eping_members'] = eping_members

            # Get timeseries topics
            timeseries_topics = await self.get_timeseries_topics()
            results['timeseries_topics'] = timeseries_topics

            # Get available APIs info
            available_apis = self.get_available_apis()
            results['available_apis'] = available_apis

            overview = {
                "success": True,
                "data": results,
                "source": "WTO APIs",
                "description": "WTO comprehensive data overview",
                "timestamp": datetime.now().isoformat()
            }

            return overview

        except Exception as e:
            return {"success": False, "error": str(e)}

    async def get_trade_restrictions_analysis(self, member_code: str = None) -> Dict:
        """Get comprehensive trade restrictions analysis"""
        try:
            results = {}

            # Get QR data
            if member_code:
                qr_list = await self.get_qr_list(reporter_member_code=member_code, page=1)
                qr_notifications = await self.get_qr_notifications(reporter_member_code=member_code, page=1)
                results['qr_list'] = qr_list
                results['qr_notifications'] = qr_notifications
            else:
                qr_list = await self.get_qr_list(page=1)
                qr_notifications = await self.get_qr_notifications(page=1)
                results['qr_list'] = qr_list
                results['qr_notifications'] = qr_notifications

            # Get HS versions for context
            hs_versions = await self.get_qr_hs_versions()
            results['hs_versions'] = hs_versions

            return {
                "success": True,
                "data": results,
                "source": "WTO QR API",
                "description": "WTO trade restrictions analysis",
                "member_filter": member_code
            }

        except Exception as e:
            return {"success": False, "error": str(e)}

    async def get_notifications_analysis(self, language: int = 1, domain_ids: int = None,
                                       date_from: str = None, date_to: str = None) -> Dict:
        """Get comprehensive notifications analysis"""
        try:
            results = {}

            # Search recent notifications
            recent_notifications = await self.search_eping_notifications(
                language=language,
                domain_ids=domain_ids,
                distribution_date_from=date_from,
                distribution_date_to=date_to,
                page=1,
                page_size=50
            )
            results['recent_notifications'] = recent_notifications

            return {
                "success": True,
                "data": results,
                "source": "WTO ePing API",
                "description": "WTO notifications analysis",
                "filters": {
                    "language": language,
                    "domain_ids": domain_ids,
                    "date_from": date_from,
                    "date_to": date_to
                }
            }

        except Exception as e:
            return {"success": False, "error": str(e)}

    async def get_trade_statistics_analysis(self, indicator: str = "TP_A_0010",
                                           reporter: str = "US", years: str = "2020-2023") -> Dict:
        """Get trade statistics analysis"""
        try:
            results = {}

            # Get data for specified indicator and period
            trade_data = await self.get_timeseries_data(
                indicator=indicator,
                reporters=reporter,
                periods=years,
                max_records=1000
            )
            results['trade_data'] = trade_data

            # Get data count
            data_count = await self.get_timeseries_data_count(
                indicator=indicator,
                reporters=reporter,
                periods=years
            )
            results['data_count'] = data_count

            # Get indicator info
            indicator_info = await self.get_timeseries_indicators(indicator=indicator)
            results['indicator_info'] = indicator_info

            return {
                "success": True,
                "data": results,
                "source": "WTO Timeseries API",
                "description": "WTO trade statistics analysis",
                "parameters": {
                    "indicator": indicator,
                    "reporter": reporter,
                    "years": years
                }
            }

        except Exception as e:
            return {"success": False, "error": str(e)}

    async def get_comprehensive_wto_analysis(self, member_code: str = None,
                                           indicator: str = "TP_A_0010") -> Dict:
        """Get comprehensive WTO analysis across all APIs"""
        try:
            results = {}

            # Trade restrictions analysis
            trade_restrictions = await self.get_trade_restrictions_analysis(member_code)
            results['trade_restrictions'] = trade_restrictions

            # Notifications analysis
            notifications = await self.get_notifications_analysis()
            results['notifications'] = notifications

            # Trade statistics analysis
            trade_stats = await self.get_trade_statistics_analysis(
                indicator=indicator,
                reporter=member_code if member_code else "US"
            )
            results['trade_statistics'] = trade_stats

            # TFAD data if member specified
            if member_code:
                tfad_data = await self.get_tfad_data([member_code])
                results['tfad_data'] = tfad_data

            return {
                "success": True,
                "data": results,
                "source": "WTO APIs",
                "description": "WTO comprehensive analysis",
                "member_focus": member_code,
                "timestamp": datetime.now().isoformat()
            }

        except Exception as e:
            return {"success": False, "error": str(e)}

def main():
    """Main function for CLI interface"""
    # Check if API key is provided as last argument
    api_key = None
    if len(sys.argv) > 2 and not sys.argv[-1].startswith('--'):
        # Last argument is likely the API key if it's not a flag
        potential_key = sys.argv[-1]
        if len(potential_key) == 32 and not potential_key.startswith('--'):  # API key format check
            api_key = potential_key
            sys.argv = sys.argv[:-1]  # Remove API key from args

    wrapper = WTODataWrapper(subscription_key=api_key)

    if len(sys.argv) < 2:
        print(json.dumps({
            "success": False,
            "error": "Usage: python wto_data.py <command> [options]",
            "commands": [
                "available_apis",
                "qr_members", "qr_products", "qr_notifications", "qr_details", "qr_list", "qr_hs_versions",
                "eping_members", "eping_search",
                "timeseries_topics", "timeseries_indicators", "timeseries_data", "timeseries_reporters",
                "tfad_data",
                "overview", "trade_restrictions_analysis", "notifications_analysis",
                "trade_statistics_analysis", "comprehensive_analysis"
            ]
        }))
        sys.exit(1)

    command = sys.argv[1]

    # Parse command line arguments
    args = {}
    for i, arg in enumerate(sys.argv[2:], 2):
        if arg.startswith('--'):
            if '=' in arg:
                key, value = arg[2:].split('=', 1)
                args[key] = value
            elif i + 1 < len(sys.argv) and not sys.argv[i + 1].startswith('--'):
                key = arg[2:]
                value = sys.argv[i + 1]
                args[key] = value
                i += 1
            else:
                args[arg[2:]] = True

    try:
        # REFERENCE COMMANDS
        if command == "available_apis":
            result = wrapper.get_available_apis()

        # QUANTITATIVE RESTRICTIONS COMMANDS
        elif command == "qr_members":
            result = asyncio.run(wrapper.get_qr_members(
                args.get("member_code"),
                args.get("name"),
                int(args.get("page")) if args.get("page") else None
            ))

        elif command == "qr_products":
            result = asyncio.run(wrapper.get_qr_products(
                args["hs_version"],
                args.get("code"),
                args.get("description"),
                int(args.get("page")) if args.get("page") else None
            ))

        elif command == "qr_notifications":
            result = asyncio.run(wrapper.get_qr_notifications(
                args.get("reporter_member_code"),
                int(args.get("notification_year")) if args.get("notification_year") else None,
                int(args.get("page")) if args.get("page") else None
            ))

        elif command == "qr_details":
            if "qr_id" not in args:
                raise ValueError("qr_id parameter is required")
            result = asyncio.run(wrapper.get_qr_details(int(args["qr_id"])))

        elif command == "qr_list":
            result = asyncio.run(wrapper.get_qr_list(
                args.get("reporter_member_code"),
                args.get("in_force_only") == "true" if args.get("in_force_only") else None,
                int(args.get("year_of_entry_into_force")) if args.get("year_of_entry_into_force") else None,
                args.get("product_codes"),
                args.get("product_ids"),
                int(args.get("page")) if args.get("page") else None
            ))

        elif command == "qr_hs_versions":
            result = asyncio.run(wrapper.get_qr_hs_versions())

        # EPING COMMANDS
        elif command == "eping_members":
            result = asyncio.run(wrapper.get_eping_members(
                int(args.get("language", 1))
            ))

        elif command == "eping_search":
            result = asyncio.run(wrapper.search_eping_notifications(
                int(args.get("language", 1)),
                int(args.get("domainIds")) if args.get("domainIds") else None,
                args.get("document_symbol"),
                args.get("distribution_date_from"),
                args.get("distribution_date_to"),
                args.get("country_ids", "").split(",") if args.get("country_ids") else None,
                args.get("hs_codes"),
                args.get("ics_codes"),
                args.get("free_text"),
                int(args.get("page")) if args.get("page") else None,
                int(args.get("page_size")) if args.get("page_size") else None
            ))

        # TIMESERIES COMMANDS
        elif command == "timeseries_topics":
            result = asyncio.run(wrapper.get_timeseries_topics(
                int(args.get("language", 1))
            ))

        elif command == "timeseries_indicators":
            result = asyncio.run(wrapper.get_timeseries_indicators(
                args.get("indicator", "all"),
                args.get("name"),
                args.get("topics"),
                args.get("product_classifications"),
                args.get("trade_partner"),
                args.get("frequency"),
                int(args.get("language", 1))
            ))

        elif command == "timeseries_data":
            if "i" not in args:
                raise ValueError("i (indicator) parameter is required")
            result = asyncio.run(wrapper.get_timeseries_data(
                args["i"],
                args.get("reporters", "all"),
                args.get("partners", "default"),
                args.get("periods", "default"),
                args.get("products", "default"),
                args.get("include_sub_products") == "true",
                args.get("format_type", "json"),
                args.get("mode", "full"),
                args.get("decimals", "default"),
                int(args.get("offset", 0)),
                int(args.get("max_records", 500)),
                args.get("heading_style", "H"),
                int(args.get("language", 1)),
                args.get("include_metadata") == "true"
            ))

        elif command == "timeseries_reporters":
            result = asyncio.run(wrapper.get_timeseries_reporters(
                args.get("name"),
                args.get("individual_group"),
                args.get("regions"),
                args.get("groups"),
                int(args.get("language", 1))
            ))

        # TFAD COMMANDS
        elif command == "tfad_data":
            result = asyncio.run(wrapper.get_tfad_data(
                args.get("countries", "").split(",") if args.get("countries") else None
            ))

        # ANALYSIS COMMANDS
        elif command == "overview":
            result = asyncio.run(wrapper.get_wto_overview())

        elif command == "trade_restrictions_analysis":
            result = asyncio.run(wrapper.get_trade_restrictions_analysis(
                args.get("member_code")
            ))

        elif command == "notifications_analysis":
            result = asyncio.run(wrapper.get_notifications_analysis(
                int(args.get("language", 1)),
                int(args.get("domain_ids")) if args.get("domain_ids") else None,
                args.get("date_from"),
                args.get("date_to")
            ))

        elif command == "trade_statistics_analysis":
            result = asyncio.run(wrapper.get_trade_statistics_analysis(
                args.get("indicator", "TP_A_0010"),
                args.get("reporter", "US"),
                args.get("years", "2020-2023")
            ))

        elif command == "comprehensive_analysis":
            result = asyncio.run(wrapper.get_comprehensive_wto_analysis(
                args.get("member_code"),
                args.get("indicator", "TP_A_0010")
            ))

        else:
            result = {
                "success": False,
                "error": f"Unknown command: {command}"
            }

        print(json.dumps(result, indent=2))

    except Exception as e:
        print(json.dumps({
            "success": False,
            "error": str(e)
        }))

if __name__ == "__main__":
    main()