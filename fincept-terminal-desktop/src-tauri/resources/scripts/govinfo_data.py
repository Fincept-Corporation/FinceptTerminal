"""
GovInfo API Data Fetcher
Comprehensive wrapper for the U.S. Government Publishing Office (GPO) GovInfo API.
Provides access to official Federal Government publications across all three branches.

API Documentation: https://api.govinfo.gov/docs/
GitHub: https://github.com/usgpo/api
Developer Hub: https://www.govinfo.gov/developers

Base URL: https://api.govinfo.gov
Authentication: API key via api.data.gov (free registration)
  - Pass as query param: ?api_key=YOUR_KEY
  - Or header: X-Api-Key: YOUR_KEY
Rate Limits: 36,000/hour, 1,200/minute, 40/second

Main Service Groups:
- Collections Service: List/browse all GPO collections
- Packages Service: Package-level summaries, content, and metadata
- Granules Service: Sub-document components within packages
- Published Service: Browse packages by publication date
- Search Service: Full-text search across GovInfo content
- Related Service: Find related documents (bills, laws, reports, etc.)
"""

import sys
import json
import os
import requests
import time
from datetime import datetime, timedelta, timezone
from typing import Dict, Any, Optional, List, Union


BASE_URL = "https://api.govinfo.gov"

# Common collection codes for reference
COLLECTION_CODES = {
    "BILLS": "Congressional Bills",
    "BILLSTATUS": "Congressional Bill Status",
    "BILLSUM": "Congressional Bill Summaries",
    "BUDGET": "United States Budget",
    "CDIR": "Congressional Directory",
    "CFR": "Code of Federal Regulations",
    "CHRG": "Congressional Hearings",
    "CMR": "Congresssional Member Record",
    "COMPS": "Statute Compilations",
    "CPD": "Compilation of Presidential Documents",
    "CREC": "Congressional Record",
    "CRECB": "Congressional Record Bound",
    "CRI": "Congressional Record Index",
    "CZIC": "Coastal Zone Information Center",
    "ECONI": "Economic Indicators",
    "ERP": "Economic Report of the President",
    "ES": "Economic Statement",
    "FR": "Federal Register",
    "GAOREPORTS": "GAO Reports and Testimonies",
    "GOVMAN": "United States Government Manual",
    "GOVPUB": "Government Publications",
    "GPO": "GPO Publications",
    "HJOURNAL": "Journal of the House of Representatives",
    "HOB": "History of Bills",
    "HMAN": "House Rules and Manual",
    "HMEMO": "House Committee Memoranda",
    "HPRT": "House Committee Prints",
    "JJOURNAL": "Joint Session Journal",
    "LSA": "List of CFR Sections Affected",
    "PAI": "Privacy Act Issuances",
    "PLAW": "Public and Private Laws",
    "PPP": "Public Papers of the Presidents",
    "USCOURTS": "United States Courts Opinions",
    "USCODE": "United States Code",
    "SERIALSET": "Congressional Serial Set",
    "SJOURNAL": "Journal of the Senate",
    "SMAN": "Senate Manual",
    "SPRT": "Senate Committee Prints",
    "SRES": "Senate Resolutions",
    "STATUTE": "Statutes at Large",
    "WCPD": "Weekly Compilation of Presidential Documents",
}


class GovInfoError:
    """Error handling wrapper for GovInfo API responses"""

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
            "timestamp": self.timestamp,
        }


class GovInfoWrapper:
    """Comprehensive GovInfo API wrapper with fault tolerance"""

    def __init__(self, api_key: Optional[str] = None):
        self.api_key = api_key or os.environ.get("GOVINFO_API_KEY", "DEMO_KEY")
        self.base_url = BASE_URL
        self.session = requests.Session()
        self.session.headers.update(
            {
                "User-Agent": "Fincept-Terminal/1.0",
                "Accept": "application/json",
            }
        )

    # ==================== INTERNAL HELPERS ====================

    def _make_request(
        self,
        path: str,
        params: Optional[Dict[str, Any]] = None,
        method: str = "GET",
        json_body: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        """Centralized request handler with comprehensive error handling"""
        try:
            params = params or {}
            params["api_key"] = self.api_key

            url = f"{self.base_url}{path}"

            if method == "GET":
                response = self.session.get(url, params=params, timeout=30)
            elif method == "POST":
                response = self.session.post(
                    url, params=params, json=json_body, timeout=30
                )
            else:
                return GovInfoError(path, f"Unsupported HTTP method: {method}").to_dict()

            # Handle rate limiting
            if response.status_code == 429:
                retry_after = int(response.headers.get("Retry-After", 5))
                return GovInfoError(
                    path,
                    f"Rate limit exceeded. Retry after {retry_after} seconds.",
                    status_code=429,
                ).to_dict()

            # Handle file generation in progress
            if response.status_code == 503:
                retry_after = int(response.headers.get("Retry-After", 10))
                return GovInfoError(
                    path,
                    f"File generation in progress. Retry after {retry_after} seconds.",
                    status_code=503,
                ).to_dict()

            response.raise_for_status()

            data = response.json()
            return {"success": True, "endpoint": path, "data": data, "timestamp": int(datetime.now().timestamp())}

        except requests.exceptions.HTTPError as e:
            return GovInfoError(path, f"HTTP error: {str(e)}", status_code=e.response.status_code if e.response else None).to_dict()
        except requests.exceptions.ConnectionError as e:
            return GovInfoError(path, f"Connection error: {str(e)}").to_dict()
        except requests.exceptions.Timeout:
            return GovInfoError(path, "Request timed out after 30 seconds.").to_dict()
        except requests.exceptions.RequestException as e:
            return GovInfoError(path, f"Network error: {str(e)}").to_dict()
        except json.JSONDecodeError as e:
            return GovInfoError(path, f"JSON decode error: {str(e)}").to_dict()
        except Exception as e:
            return GovInfoError(path, f"Unexpected error: {str(e)}").to_dict()

    def _extract_list(self, result: Dict[str, Any], list_key: str) -> Dict[str, Any]:
        """Helper to pull a named list out of a successful API response"""
        if not result.get("success"):
            return result
        data = result.get("data", {})
        return {
            "success": True,
            "endpoint": result.get("endpoint"),
            "data": data.get(list_key, []),
            "count": data.get("count", None),
            "total_count": data.get("totalCount", None),
            "next_page": data.get("nextPage", None),
            "previous_page": data.get("previousPage", None),
            "timestamp": result.get("timestamp"),
        }

    # ==================== COLLECTIONS SERVICE ====================

    def get_collections(self) -> Dict[str, Any]:
        """
        GET /collections
        Returns a list of all collections available in GovInfo, including
        collectionCode, collectionName, packageCount, and granuleCount.
        """
        result = self._make_request("/collections")
        if not result.get("success"):
            return result
        data = result.get("data", {})
        return {
            "success": True,
            "endpoint": "/collections",
            "data": data.get("collections", data),
            "timestamp": result.get("timestamp"),
        }

    def get_collection_packages(
        self,
        collection_code: str,
        start_date: str,
        end_date: Optional[str] = None,
        page_size: int = 100,
        offset_mark: str = "*",
        doc_class: Optional[str] = None,
        congress: Optional[str] = None,
        court_code: Optional[str] = None,
        court_type: Optional[str] = None,
        nature_suit_code: Optional[str] = None,
        nature_suit: Optional[str] = None,
    ) -> Dict[str, Any]:
        """
        GET /collections/{collectionCode}/{lastModifiedStartDate}[/{lastModifiedEndDate}]
        List packages in a collection updated after a given date.

        Args:
            collection_code: e.g. 'BILLS', 'FR', 'CFR', 'CREC', 'USCOURTS'
            start_date: ISO 8601 datetime string e.g. '2025-01-01T00:00:00Z'
            end_date: Optional ISO 8601 end date
            page_size: Number of results per page (max 1000)
            offset_mark: Pagination cursor, use '*' for first page
            doc_class: Filter by document class (collection-specific)
            congress: Filter by congress number e.g. '119'
            court_code: Filter by court code (USCOURTS only)
            court_type: Filter by court type (USCOURTS only)
            nature_suit_code: Filter by nature of suit code (USCOURTS only)
            nature_suit: Filter by nature of suit description (USCOURTS only)
        """
        path = f"/collections/{collection_code}/{start_date}"
        if end_date:
            path += f"/{end_date}"

        params: Dict[str, Any] = {
            "pageSize": page_size,
            "offsetMark": offset_mark,
        }
        if doc_class:
            params["docClass"] = doc_class
        if congress:
            params["congress"] = congress
        if court_code:
            params["courtCode"] = court_code
        if court_type:
            params["courtType"] = court_type
        if nature_suit_code:
            params["natureSuitCode"] = nature_suit_code
        if nature_suit:
            params["natureSuit"] = nature_suit

        result = self._make_request(path, params=params)
        return self._extract_list(result, "packages")

    # ==================== PUBLISHED SERVICE ====================

    def get_published_packages(
        self,
        start_date: str,
        end_date: Optional[str] = None,
        collection: Optional[str] = None,
        page_size: int = 100,
        offset_mark: str = "*",
        doc_class: Optional[str] = None,
        congress: Optional[str] = None,
        court_code: Optional[str] = None,
        court_type: Optional[str] = None,
        modified_since: Optional[str] = None,
    ) -> Dict[str, Any]:
        """
        GET /published/{dateIssuedStartDate}[/{dateIssuedEndDate}]
        List packages by their official publication (dateIssued) date.
        This differs from collection updates — it reflects when content was published.

        Args:
            start_date: Publication start date 'YYYY-MM-DD'
            end_date: Optional publication end date 'YYYY-MM-DD'
            collection: Comma-separated collection codes e.g. 'BILLS,FR'
            page_size: Results per page (max 1000)
            offset_mark: Pagination cursor
            doc_class: Document class filter
            congress: Congress number filter
            court_code: Court code filter (USCOURTS)
            court_type: Court type filter (USCOURTS)
            modified_since: Only include packages modified since this date
        """
        path = f"/published/{start_date}"
        if end_date:
            path += f"/{end_date}"

        params: Dict[str, Any] = {
            "pageSize": page_size,
            "offsetMark": offset_mark,
        }
        if collection:
            params["collection"] = collection
        if doc_class:
            params["docClass"] = doc_class
        if congress:
            params["congress"] = congress
        if court_code:
            params["courtCode"] = court_code
        if court_type:
            params["courtType"] = court_type
        if modified_since:
            params["modifiedSince"] = modified_since

        result = self._make_request(path, params=params)
        return self._extract_list(result, "packages")

    # ==================== PACKAGES SERVICE ====================

    def get_package_summary(self, package_id: str) -> Dict[str, Any]:
        """
        GET /packages/{packageId}/summary
        Retrieve summary metadata and available download links for a package.
        Returns: title, dateIssued, congress, publisher, download links (pdf, htm, xml, mods, premis, zip)

        Args:
            package_id: e.g. 'BILLS-119hr1enr', 'CREC-2025-01-06', 'FR-2025-01-20'
        """
        result = self._make_request(f"/packages/{package_id}/summary")
        if not result.get("success"):
            return result
        return {
            "success": True,
            "endpoint": f"/packages/{package_id}/summary",
            "package_id": package_id,
            "data": result.get("data", {}),
            "timestamp": result.get("timestamp"),
        }

    def get_package_content_detail(self, package_id: str) -> Dict[str, Any]:
        """
        GET /packages/{packageId}/contentdetail
        Returns content detail for a package including links to individual files.

        Args:
            package_id: e.g. 'BILLS-119hr1enr'
        """
        result = self._make_request(f"/packages/{package_id}/contentdetail")
        if not result.get("success"):
            return result
        return {
            "success": True,
            "endpoint": f"/packages/{package_id}/contentdetail",
            "package_id": package_id,
            "data": result.get("data", {}),
            "timestamp": result.get("timestamp"),
        }

    # ==================== GRANULES SERVICE ====================

    def get_package_granules(
        self,
        package_id: str,
        page_size: int = 100,
        offset_mark: str = "*",
        granule_class: Optional[str] = None,
    ) -> Dict[str, Any]:
        """
        GET /packages/{packageId}/granules
        List individual granule (sub-document) components within a package.
        Useful for packages like Congressional Record (CREC) with many sections.

        Args:
            package_id: e.g. 'CREC-2025-01-06'
            page_size: Results per page (max 1000)
            offset_mark: Pagination cursor
            granule_class: Filter by granule class
        """
        params: Dict[str, Any] = {
            "pageSize": page_size,
            "offsetMark": offset_mark,
        }
        if granule_class:
            params["granuleClass"] = granule_class

        result = self._make_request(f"/packages/{package_id}/granules", params=params)
        return self._extract_list(result, "granules")

    def get_granule_summary(self, package_id: str, granule_id: str) -> Dict[str, Any]:
        """
        GET /packages/{packageId}/granules/{granuleId}/summary
        Retrieve summary metadata for a specific granule within a package.

        Args:
            package_id: Parent package ID e.g. 'CREC-2025-01-06'
            granule_id: Granule ID e.g. 'CREC-2025-01-06-pt1-PgS1'
        """
        result = self._make_request(
            f"/packages/{package_id}/granules/{granule_id}/summary"
        )
        if not result.get("success"):
            return result
        return {
            "success": True,
            "endpoint": f"/packages/{package_id}/granules/{granule_id}/summary",
            "package_id": package_id,
            "granule_id": granule_id,
            "data": result.get("data", {}),
            "timestamp": result.get("timestamp"),
        }

    # ==================== SEARCH SERVICE ====================

    def search(
        self,
        query: str,
        page_size: int = 10,
        offset_mark: str = "*",
        sorts: Optional[List[Dict[str, str]]] = None,
        historical: Optional[bool] = None,
        result_level: Optional[str] = None,
        date_issued_start: Optional[str] = None,
        date_issued_end: Optional[str] = None,
        congress: Optional[List[str]] = None,
        doc_class: Optional[List[str]] = None,
        category: Optional[List[str]] = None,
        collection: Optional[List[str]] = None,
        modified_start: Optional[str] = None,
        modified_end: Optional[str] = None,
        publishdate_start: Optional[str] = None,
        publishdate_end: Optional[str] = None,
        government_author1: Optional[List[str]] = None,
        government_author2: Optional[List[str]] = None,
        subject: Optional[List[str]] = None,
        court_code: Optional[List[str]] = None,
        court_type: Optional[List[str]] = None,
        nature_suit_code: Optional[List[str]] = None,
        nature_suit: Optional[List[str]] = None,
    ) -> Dict[str, Any]:
        """
        POST /search
        Full-text search across GovInfo content. Returns results similar to
        govinfo.gov search UI.

        Args:
            query: Search terms e.g. 'infrastructure bill', 'federal reserve'
            page_size: Results per page
            offset_mark: Pagination cursor (start with '*')
            sorts: List of sort dicts e.g. [{'field': 'score', 'sortOrder': 'DESC'}]
                   Supported fields: score, publishdate, lastModified, title
            historical: Include historical content
            result_level: 'default' or 'granule'
            date_issued_start: Filter by publication start date 'YYYY-MM-DD'
            date_issued_end: Filter by publication end date 'YYYY-MM-DD'
            congress: Filter by congress numbers e.g. ['119', '118']
            doc_class: Filter by document classes
            category: Filter by categories
            collection: Filter by collection codes e.g. ['BILLS', 'FR']
            modified_start: Filter by modification start date
            modified_end: Filter by modification end date
            publishdate_start: Filter by publish date start
            publishdate_end: Filter by publish date end
            government_author1: Filter by primary government author
            government_author2: Filter by secondary government author
            subject: Filter by subject terms
            court_code: Filter by court code (USCOURTS)
            court_type: Filter by court type
            nature_suit_code: Filter by nature of suit code
            nature_suit: Filter by nature of suit
        """
        body: Dict[str, Any] = {
            "query": query,
            "pageSize": page_size,
            "offsetMark": offset_mark,
        }

        if sorts:
            body["sorts"] = sorts
        if historical is not None:
            body["historical"] = historical
        if result_level:
            body["resultLevel"] = result_level

        # Build facet/filter fields
        fields: Dict[str, Any] = {}
        if date_issued_start:
            fields["dateIssued"] = {"from": date_issued_start}
        if date_issued_end:
            if "dateIssued" not in fields:
                fields["dateIssued"] = {}
            fields["dateIssued"]["to"] = date_issued_end
        if congress:
            fields["congress"] = congress
        if doc_class:
            fields["docClass"] = doc_class
        if category:
            fields["category"] = category
        if collection:
            fields["collection"] = collection
        if modified_start:
            fields["lastModified"] = {"from": modified_start}
        if modified_end:
            if "lastModified" not in fields:
                fields["lastModified"] = {}
            fields["lastModified"]["to"] = modified_end
        if publishdate_start:
            fields["publishdate"] = {"from": publishdate_start}
        if publishdate_end:
            if "publishdate" not in fields:
                fields["publishdate"] = {}
            fields["publishdate"]["to"] = publishdate_end
        if government_author1:
            fields["governmentAuthor1"] = government_author1
        if government_author2:
            fields["governmentAuthor2"] = government_author2
        if subject:
            fields["subject"] = subject
        if court_code:
            fields["courtCode"] = court_code
        if court_type:
            fields["courtType"] = court_type
        if nature_suit_code:
            fields["natureSuitCode"] = nature_suit_code
        if nature_suit:
            fields["natureSuit"] = nature_suit

        if fields:
            body["fields"] = fields

        result = self._make_request("/search", method="POST", json_body=body)
        if not result.get("success"):
            return result

        data = result.get("data", {})
        return {
            "success": True,
            "endpoint": "/search",
            "query": query,
            "data": data.get("results", []),
            "count": data.get("count", 0),
            "total_count": data.get("totalCount", 0),
            "next_page": data.get("nextPage", None),
            "previous_page": data.get("previousPage", None),
            "timestamp": result.get("timestamp"),
        }

    # ==================== RELATED SERVICE ====================

    def get_related_documents(
        self,
        access_id: str,
        collection: Optional[str] = None,
    ) -> Dict[str, Any]:
        """
        GET /related/{accessId}[/{collection}]
        Find documents related to a given access ID (BILLS or BILLSTATUS packages).
        Returns related bill versions, bill history, signing statements, laws,
        Congressional reports, U.S. Code sections, Statutes at Large, etc.

        Args:
            access_id: Package or granule access ID e.g. 'BILLS-119hr1enr'
            collection: Optional collection to filter relationships
                        e.g. 'PLAW', 'STATUTE', 'USCODE', 'HPRT', 'SPRT'
        """
        path = f"/related/{access_id}"
        if collection:
            path += f"/{collection}"

        result = self._make_request(path)
        if not result.get("success"):
            return result

        data = result.get("data", {})
        return {
            "success": True,
            "endpoint": path,
            "access_id": access_id,
            "data": data.get("relatedPackages", data.get("packages", data)),
            "timestamp": result.get("timestamp"),
        }

    # ==================== HIGH-LEVEL CONVENIENCE METHODS ====================

    def get_recent_bills(
        self,
        days_back: int = 7,
        page_size: int = 50,
        congress: Optional[str] = None,
    ) -> Dict[str, Any]:
        """
        Convenience: Fetch recently updated Congressional Bills from the BILLS collection.

        Args:
            days_back: How many days back to look for updates
            page_size: Results per page
            congress: Filter by congress e.g. '119'
        """
        start = (datetime.now(timezone.utc) - timedelta(days=days_back)).strftime(
            "%Y-%m-%dT%H:%M:%SZ"
        )
        return self.get_collection_packages(
            collection_code="BILLS",
            start_date=start,
            page_size=page_size,
            congress=congress,
        )

    def get_federal_register_entries(
        self,
        start_date: str,
        end_date: Optional[str] = None,
        page_size: int = 50,
    ) -> Dict[str, Any]:
        """
        Convenience: Fetch Federal Register entries by publication date.

        Args:
            start_date: Start date 'YYYY-MM-DD'
            end_date: End date 'YYYY-MM-DD' (defaults to today)
            page_size: Results per page
        """
        if not end_date:
            end_date = datetime.now(timezone.utc).strftime("%Y-%m-%d")
        return self.get_published_packages(
            start_date=start_date,
            end_date=end_date,
            collection="FR",
            page_size=page_size,
        )

    def get_recent_court_opinions(
        self,
        days_back: int = 30,
        court_code: Optional[str] = None,
        page_size: int = 50,
    ) -> Dict[str, Any]:
        """
        Convenience: Fetch recent U.S. Court opinions from USCOURTS collection.

        Args:
            days_back: Days back to look for new opinions
            court_code: Specific court code e.g. 'ca9', 'ca4', 'dc'
            page_size: Results per page
        """
        start = (datetime.now(timezone.utc) - timedelta(days=days_back)).strftime(
            "%Y-%m-%dT%H:%M:%SZ"
        )
        return self.get_collection_packages(
            collection_code="USCOURTS",
            start_date=start,
            page_size=page_size,
            court_code=court_code,
        )

    def get_congressional_record_by_date(
        self,
        date: str,
        page_size: int = 100,
    ) -> Dict[str, Any]:
        """
        Convenience: Fetch Congressional Record (CREC) for a specific date.
        Returns granule listing for the day's record.

        Args:
            date: Date in format 'YYYY-MM-DD'
            page_size: Results per page
        """
        package_id = f"CREC-{date}"
        return self.get_package_granules(package_id=package_id, page_size=page_size)

    def get_cfr_titles(
        self,
        page_size: int = 50,
    ) -> Dict[str, Any]:
        """
        Convenience: Get recent Code of Federal Regulations packages.

        Args:
            page_size: Results per page
        """
        start = (datetime.now(timezone.utc) - timedelta(days=365)).strftime(
            "%Y-%m-%dT%H:%M:%SZ"
        )
        return self.get_collection_packages(
            collection_code="CFR",
            start_date=start,
            page_size=page_size,
        )

    def get_public_laws(
        self,
        congress: Optional[str] = None,
        start_date: Optional[str] = None,
        page_size: int = 50,
    ) -> Dict[str, Any]:
        """
        Convenience: Get Public and Private Laws (PLAW collection).

        Args:
            congress: Congress number e.g. '119'
            start_date: Start date 'YYYY-MM-DD' (defaults to 2 years ago)
            page_size: Results per page
        """
        if not start_date:
            start_date = (datetime.now(timezone.utc) - timedelta(days=730)).strftime(
                "%Y-%m-%dT%H:%M:%SZ"
            )
        return self.get_collection_packages(
            collection_code="PLAW",
            start_date=start_date,
            page_size=page_size,
            congress=congress,
        )

    def get_economic_indicators(
        self,
        start_date: Optional[str] = None,
        page_size: int = 50,
    ) -> Dict[str, Any]:
        """
        Convenience: Get Economic Indicators (ECONI collection).

        Args:
            start_date: Start date 'YYYY-MM-DD' (defaults to 1 year ago)
            page_size: Results per page
        """
        if not start_date:
            start_date = (datetime.now(timezone.utc) - timedelta(days=365)).strftime(
                "%Y-%m-%dT%H:%M:%SZ"
            )
        return self.get_collection_packages(
            collection_code="ECONI",
            start_date=start_date,
            page_size=page_size,
        )

    def get_gao_reports(
        self,
        start_date: Optional[str] = None,
        page_size: int = 50,
    ) -> Dict[str, Any]:
        """
        Convenience: Get GAO Reports and Testimonies (GAOREPORTS collection).

        Args:
            start_date: Start date (defaults to 90 days ago)
            page_size: Results per page
        """
        if not start_date:
            start_date = (datetime.now(timezone.utc) - timedelta(days=90)).strftime(
                "%Y-%m-%dT%H:%M:%SZ"
            )
        return self.get_collection_packages(
            collection_code="GAOREPORTS",
            start_date=start_date,
            page_size=page_size,
        )

    def get_presidential_documents(
        self,
        start_date: Optional[str] = None,
        page_size: int = 50,
    ) -> Dict[str, Any]:
        """
        Convenience: Get Compilation of Presidential Documents (CPD collection).

        Args:
            start_date: Start date (defaults to 90 days ago)
            page_size: Results per page
        """
        if not start_date:
            start_date = (datetime.now(timezone.utc) - timedelta(days=90)).strftime(
                "%Y-%m-%dT%H:%M:%SZ"
            )
        return self.get_collection_packages(
            collection_code="CPD",
            start_date=start_date,
            page_size=page_size,
        )

    def search_legislation(
        self,
        query: str,
        congress: Optional[str] = None,
        page_size: int = 20,
    ) -> Dict[str, Any]:
        """
        Convenience: Search for legislation (bills, laws, resolutions).

        Args:
            query: Search terms
            congress: Congress number e.g. '119'
            page_size: Results per page
        """
        return self.search(
            query=query,
            page_size=page_size,
            collection=["BILLS", "PLAW", "BILLSTATUS"],
            congress=[congress] if congress else None,
            sorts=[{"field": "score", "sortOrder": "DESC"}],
        )

    def search_federal_regulations(
        self,
        query: str,
        page_size: int = 20,
    ) -> Dict[str, Any]:
        """
        Convenience: Search across CFR and Federal Register.

        Args:
            query: Search terms e.g. 'bank capital requirements'
            page_size: Results per page
        """
        return self.search(
            query=query,
            page_size=page_size,
            collection=["CFR", "FR"],
            sorts=[{"field": "publishdate", "sortOrder": "DESC"}],
        )

    def search_court_opinions(
        self,
        query: str,
        court_type: Optional[str] = None,
        page_size: int = 20,
    ) -> Dict[str, Any]:
        """
        Convenience: Search U.S. court opinions.

        Args:
            query: Search terms
            court_type: e.g. 'United States Courts of Appeals'
            page_size: Results per page
        """
        return self.search(
            query=query,
            page_size=page_size,
            collection=["USCOURTS"],
            court_type=[court_type] if court_type else None,
            sorts=[{"field": "publishdate", "sortOrder": "DESC"}],
        )

    def get_bill_with_related(self, package_id: str) -> Dict[str, Any]:
        """
        Convenience: Get a bill's summary plus all related documents in one call.

        Args:
            package_id: Bill package ID e.g. 'BILLS-119hr1enr'
        """
        summary = self.get_package_summary(package_id)
        related = self.get_related_documents(package_id)

        return {
            "success": True,
            "package_id": package_id,
            "summary": summary.get("data", {}),
            "related_documents": related.get("data", []),
            "timestamp": int(datetime.now().timestamp()),
        }

    def get_collections_overview(self) -> Dict[str, Any]:
        """
        Convenience: Get all collections with their descriptions from the local
        COLLECTION_CODES reference, merged with live package/granule counts.
        """
        result = self.get_collections()
        if not result.get("success"):
            return result

        collections = result.get("data", [])
        enriched = []
        for col in collections:
            code = col.get("collectionCode", "")
            col["collectionDescription"] = COLLECTION_CODES.get(code, "")
            enriched.append(col)

        return {
            "success": True,
            "endpoint": "/collections",
            "data": enriched,
            "count": len(enriched),
            "timestamp": result.get("timestamp"),
        }


# ==================== MAIN CLI ENTRYPOINT ====================

def main():
    if len(sys.argv) < 2:
        print(json.dumps({
            "success": False,
            "error": "No command provided",
            "available_commands": [
                "collections",
                "collections_overview",
                "collection_packages <collection_code> <start_date> [end_date] [page_size] [congress]",
                "published <start_date> [end_date] [collection] [page_size] [congress]",
                "package_summary <package_id>",
                "package_granules <package_id> [page_size]",
                "granule_summary <package_id> <granule_id>",
                "search <query> [page_size] [collection] [congress]",
                "related <access_id> [collection]",
                "recent_bills [days_back] [page_size] [congress]",
                "federal_register <start_date> [end_date] [page_size]",
                "court_opinions [days_back] [court_code] [page_size]",
                "congressional_record <date>",
                "cfr [page_size]",
                "public_laws [congress] [start_date] [page_size]",
                "economic_indicators [start_date] [page_size]",
                "gao_reports [start_date] [page_size]",
                "presidential_documents [start_date] [page_size]",
                "search_legislation <query> [congress] [page_size]",
                "search_regulations <query> [page_size]",
                "search_court_opinions <query> [court_type] [page_size]",
                "bill_with_related <package_id>",
            ],
            "note": "Requires GOVINFO_API_KEY environment variable (free at https://api.data.gov/signup/)",
            "rate_limits": "36,000/hour, 1,200/minute, 40/second",
        }))
        sys.exit(1)

    command = sys.argv[1]
    wrapper = GovInfoWrapper()

    try:
        # ---- Collections ----
        if command == "collections":
            result = wrapper.get_collections()
            print(json.dumps(result, indent=2))

        elif command == "collections_overview":
            result = wrapper.get_collections_overview()
            print(json.dumps(result, indent=2))

        elif command == "collection_packages":
            if len(sys.argv) < 4:
                print(json.dumps({"success": False, "error": "Usage: govinfo_data.py collection_packages <collection_code> <start_date> [end_date] [page_size] [congress]"}))
                sys.exit(1)
            collection_code = sys.argv[2]
            start_date = sys.argv[3]
            end_date = sys.argv[4] if len(sys.argv) > 4 and sys.argv[4] else None
            page_size = int(sys.argv[5]) if len(sys.argv) > 5 and sys.argv[5] else 100
            congress = sys.argv[6] if len(sys.argv) > 6 and sys.argv[6] else None
            result = wrapper.get_collection_packages(
                collection_code=collection_code,
                start_date=start_date,
                end_date=end_date,
                page_size=page_size,
                congress=congress,
            )
            print(json.dumps(result, indent=2))

        # ---- Published ----
        elif command == "published":
            if len(sys.argv) < 3:
                print(json.dumps({"success": False, "error": "Usage: govinfo_data.py published <start_date> [end_date] [collection] [page_size] [congress]"}))
                sys.exit(1)
            start_date = sys.argv[2]
            end_date = sys.argv[3] if len(sys.argv) > 3 and sys.argv[3] else None
            collection = sys.argv[4] if len(sys.argv) > 4 and sys.argv[4] else None
            page_size = int(sys.argv[5]) if len(sys.argv) > 5 and sys.argv[5] else 100
            congress = sys.argv[6] if len(sys.argv) > 6 and sys.argv[6] else None
            result = wrapper.get_published_packages(
                start_date=start_date,
                end_date=end_date,
                collection=collection,
                page_size=page_size,
                congress=congress,
            )
            print(json.dumps(result, indent=2))

        # ---- Packages ----
        elif command == "package_summary":
            if len(sys.argv) < 3:
                print(json.dumps({"success": False, "error": "Usage: govinfo_data.py package_summary <package_id>"}))
                sys.exit(1)
            package_id = sys.argv[2]
            result = wrapper.get_package_summary(package_id)
            print(json.dumps(result, indent=2))

        elif command == "package_granules":
            if len(sys.argv) < 3:
                print(json.dumps({"success": False, "error": "Usage: govinfo_data.py package_granules <package_id> [page_size]"}))
                sys.exit(1)
            package_id = sys.argv[2]
            page_size = int(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[3] else 100
            result = wrapper.get_package_granules(package_id=package_id, page_size=page_size)
            print(json.dumps(result, indent=2))

        elif command == "granule_summary":
            if len(sys.argv) < 4:
                print(json.dumps({"success": False, "error": "Usage: govinfo_data.py granule_summary <package_id> <granule_id>"}))
                sys.exit(1)
            package_id = sys.argv[2]
            granule_id = sys.argv[3]
            result = wrapper.get_granule_summary(package_id=package_id, granule_id=granule_id)
            print(json.dumps(result, indent=2))

        # ---- Search ----
        elif command == "search":
            if len(sys.argv) < 3:
                print(json.dumps({"success": False, "error": "Usage: govinfo_data.py search <query> [page_size] [collection] [congress]"}))
                sys.exit(1)
            query = sys.argv[2]
            page_size = int(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[3] else 10
            collection_filter = sys.argv[4].split(",") if len(sys.argv) > 4 and sys.argv[4] else None
            congress_filter = [sys.argv[5]] if len(sys.argv) > 5 and sys.argv[5] else None
            result = wrapper.search(
                query=query,
                page_size=page_size,
                collection=collection_filter,
                congress=congress_filter,
            )
            print(json.dumps(result, indent=2))

        # ---- Related ----
        elif command == "related":
            if len(sys.argv) < 3:
                print(json.dumps({"success": False, "error": "Usage: govinfo_data.py related <access_id> [collection]"}))
                sys.exit(1)
            access_id = sys.argv[2]
            collection = sys.argv[3] if len(sys.argv) > 3 and sys.argv[3] else None
            result = wrapper.get_related_documents(access_id=access_id, collection=collection)
            print(json.dumps(result, indent=2))

        # ---- Convenience methods ----
        elif command == "recent_bills":
            days_back = int(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[2] else 7
            page_size = int(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[3] else 50
            congress = sys.argv[4] if len(sys.argv) > 4 and sys.argv[4] else None
            result = wrapper.get_recent_bills(days_back=days_back, page_size=page_size, congress=congress)
            print(json.dumps(result, indent=2))

        elif command == "federal_register":
            if len(sys.argv) < 3:
                print(json.dumps({"success": False, "error": "Usage: govinfo_data.py federal_register <start_date> [end_date] [page_size]"}))
                sys.exit(1)
            start_date = sys.argv[2]
            end_date = sys.argv[3] if len(sys.argv) > 3 and sys.argv[3] else None
            page_size = int(sys.argv[4]) if len(sys.argv) > 4 and sys.argv[4] else 50
            result = wrapper.get_federal_register_entries(start_date=start_date, end_date=end_date, page_size=page_size)
            print(json.dumps(result, indent=2))

        elif command == "court_opinions":
            days_back = int(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[2] else 30
            court_code = sys.argv[3] if len(sys.argv) > 3 and sys.argv[3] else None
            page_size = int(sys.argv[4]) if len(sys.argv) > 4 and sys.argv[4] else 50
            result = wrapper.get_recent_court_opinions(days_back=days_back, court_code=court_code, page_size=page_size)
            print(json.dumps(result, indent=2))

        elif command == "congressional_record":
            if len(sys.argv) < 3:
                print(json.dumps({"success": False, "error": "Usage: govinfo_data.py congressional_record <date> (YYYY-MM-DD)"}))
                sys.exit(1)
            date = sys.argv[2]
            result = wrapper.get_congressional_record_by_date(date=date)
            print(json.dumps(result, indent=2))

        elif command == "cfr":
            page_size = int(sys.argv[2]) if len(sys.argv) > 2 and sys.argv[2] else 50
            result = wrapper.get_cfr_titles(page_size=page_size)
            print(json.dumps(result, indent=2))

        elif command == "public_laws":
            congress = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] else None
            start_date = sys.argv[3] if len(sys.argv) > 3 and sys.argv[3] else None
            page_size = int(sys.argv[4]) if len(sys.argv) > 4 and sys.argv[4] else 50
            result = wrapper.get_public_laws(congress=congress, start_date=start_date, page_size=page_size)
            print(json.dumps(result, indent=2))

        elif command == "economic_indicators":
            start_date = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] else None
            page_size = int(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[3] else 50
            result = wrapper.get_economic_indicators(start_date=start_date, page_size=page_size)
            print(json.dumps(result, indent=2))

        elif command == "gao_reports":
            start_date = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] else None
            page_size = int(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[3] else 50
            result = wrapper.get_gao_reports(start_date=start_date, page_size=page_size)
            print(json.dumps(result, indent=2))

        elif command == "presidential_documents":
            start_date = sys.argv[2] if len(sys.argv) > 2 and sys.argv[2] else None
            page_size = int(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[3] else 50
            result = wrapper.get_presidential_documents(start_date=start_date, page_size=page_size)
            print(json.dumps(result, indent=2))

        elif command == "search_legislation":
            if len(sys.argv) < 3:
                print(json.dumps({"success": False, "error": "Usage: govinfo_data.py search_legislation <query> [congress] [page_size]"}))
                sys.exit(1)
            query = sys.argv[2]
            congress = sys.argv[3] if len(sys.argv) > 3 and sys.argv[3] else None
            page_size = int(sys.argv[4]) if len(sys.argv) > 4 and sys.argv[4] else 20
            result = wrapper.search_legislation(query=query, congress=congress, page_size=page_size)
            print(json.dumps(result, indent=2))

        elif command == "search_regulations":
            if len(sys.argv) < 3:
                print(json.dumps({"success": False, "error": "Usage: govinfo_data.py search_regulations <query> [page_size]"}))
                sys.exit(1)
            query = sys.argv[2]
            page_size = int(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[3] else 20
            result = wrapper.search_federal_regulations(query=query, page_size=page_size)
            print(json.dumps(result, indent=2))

        elif command == "search_court_opinions":
            if len(sys.argv) < 3:
                print(json.dumps({"success": False, "error": "Usage: govinfo_data.py search_court_opinions <query> [court_type] [page_size]"}))
                sys.exit(1)
            query = sys.argv[2]
            court_type = sys.argv[3] if len(sys.argv) > 3 and sys.argv[3] else None
            page_size = int(sys.argv[4]) if len(sys.argv) > 4 and sys.argv[4] else 20
            result = wrapper.search_court_opinions(query=query, court_type=court_type, page_size=page_size)
            print(json.dumps(result, indent=2))

        elif command == "bill_with_related":
            if len(sys.argv) < 3:
                print(json.dumps({"success": False, "error": "Usage: govinfo_data.py bill_with_related <package_id>"}))
                sys.exit(1)
            package_id = sys.argv[2]
            result = wrapper.get_bill_with_related(package_id)
            print(json.dumps(result, indent=2))

        else:
            print(json.dumps({
                "success": False,
                "error": f"Unknown command: {command}",
                "available_commands": [
                    "collections", "collections_overview",
                    "collection_packages <collection_code> <start_date> [end_date] [page_size] [congress]",
                    "published <start_date> [end_date] [collection] [page_size] [congress]",
                    "package_summary <package_id>",
                    "package_granules <package_id> [page_size]",
                    "granule_summary <package_id> <granule_id>",
                    "search <query> [page_size] [collection] [congress]",
                    "related <access_id> [collection]",
                    "recent_bills [days_back] [page_size] [congress]",
                    "federal_register <start_date> [end_date] [page_size]",
                    "court_opinions [days_back] [court_code] [page_size]",
                    "congressional_record <date>",
                    "cfr [page_size]",
                    "public_laws [congress] [start_date] [page_size]",
                    "economic_indicators [start_date] [page_size]",
                    "gao_reports [start_date] [page_size]",
                    "presidential_documents [start_date] [page_size]",
                    "search_legislation <query> [congress] [page_size]",
                    "search_regulations <query> [page_size]",
                    "search_court_opinions <query> [court_type] [page_size]",
                    "bill_with_related <package_id>",
                ],
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
